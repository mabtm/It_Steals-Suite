#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include "SeedGpuCatalog.h"

namespace seed_gpu
{
using Microsoft::WRL::ComPtr;
inline void Check(HRESULT result, const char *operation)
{
    if (FAILED(result))
        throw std::runtime_error(std::string(operation) + " failed (HRESULT " + std::to_string(uint32_t(result)) + ")");
}
struct Adapter
{
    int index = 0;
    std::string name;
    uint64_t dedicatedBytes = 0;
    bool software = false;
};
inline std::vector<Adapter> Adapters()
{
    ComPtr<IDXGIFactory1> factory;
    Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "Create DXGI factory");
    std::vector<Adapter> result;
    for (UINT i = 0;; i++)
    {
        ComPtr<IDXGIAdapter1> adapter;
        HRESULT found = factory->EnumAdapters1(i, &adapter);
        if (found == DXGI_ERROR_NOT_FOUND) break;
        Check(found, "Enumerate GPU adapter");
        DXGI_ADAPTER_DESC1 info{};
        Check(adapter->GetDesc1(&info), "Read GPU adapter");
        int bytes = WideCharToMultiByte(CP_UTF8, 0, info.Description, -1, nullptr, 0, nullptr, nullptr);
        std::string name(size_t(bytes), '\0');
        WideCharToMultiByte(CP_UTF8, 0, info.Description, -1, name.data(), bytes, nullptr, nullptr);
        if (!name.empty()) name.pop_back();
        bool software = (info.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 ||
                        (info.VendorId == 0x1414 && info.DeviceId == 0x8c);
        result.push_back({int(i), name, info.DedicatedVideoMemory, software});
    }
    std::stable_sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.dedicatedBytes > b.dedicatedBytes;
    });
    return result;
}
inline std::string Resource(int id)
{
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (!resource) throw std::runtime_error("Embedded seed shader is missing");
    const char *bytes = static_cast<const char *>(LockResource(LoadResource(nullptr, resource)));
    if (!bytes) throw std::runtime_error("Could not read the embedded seed shader");
    return {bytes, SizeofResource(nullptr, resource)};
}

// Separate from the overlay device. Only the search coordinator owns this context.
class Device
{
    struct Buffer
    {
        ComPtr<ID3D11Buffer> buffer, staging;
        ComPtr<ID3D11ShaderResourceView> read;
        ComPtr<ID3D11UnorderedAccessView> write;
    };
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<ID3D11ComputeShader> shader;
    std::map<int, ComPtr<ID3D11ComputeShader>> kernels;
    ComPtr<ID3D11Query> complete;
    ComPtr<ID3D11Buffer> catalogInfo;
    std::string source;
    std::shared_ptr<const Catalog> catalog;
    Buffer ids, sizes, orders, lengths, edges, nearest;
    std::vector<uint32_t> packedIds, packedSizes, resetOrders;
    std::vector<double> resetLengths;
    int capacity = 4096, fixedCrystals = 0;
    bool pending = false;
    std::chrono::seconds timeout{10};

    Buffer Make(UINT stride, UINT count, bool output, const void *initial = nullptr)
    {
        Buffer result;
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = stride * count; desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = output ? D3D11_BIND_UNORDERED_ACCESS : D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED; desc.StructureByteStride = stride;
        D3D11_SUBRESOURCE_DATA data{initial, 0, 0};
        Check(device->CreateBuffer(&desc, initial ? &data : nullptr, &result.buffer), "Allocate seed GPU buffer");
        if (output)
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC view{};
            view.ViewDimension = D3D11_UAV_DIMENSION_BUFFER; view.Buffer.NumElements = count;
            Check(device->CreateUnorderedAccessView(result.buffer.Get(), &view, &result.write), "Create seed output view");
            desc.Usage = D3D11_USAGE_STAGING; desc.BindFlags = desc.MiscFlags = desc.StructureByteStride = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            Check(device->CreateBuffer(&desc, nullptr, &result.staging), "Allocate seed GPU readback");
        }
        else
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC view{};
            view.ViewDimension = D3D11_SRV_DIMENSION_BUFFER; view.Buffer.NumElements = count;
            Check(device->CreateShaderResourceView(result.buffer.Get(), &view, &result.read), "Create seed input view");
        }
        return result;
    }
    void Bind()
    {
        context->CSSetShader(shader.Get(), nullptr, 0);
        ID3D11ShaderResourceView *inputs[]{edges.read.Get(), nearest.read.Get(), sizes.read.Get(), ids.read.Get()};
        context->CSSetShaderResources(0, 4, inputs);
        ID3D11Buffer *constants = catalogInfo.Get(); context->CSSetConstantBuffers(0, 1, &constants);
    }
    template <class T> std::vector<T> Read(Buffer &buffer, size_t count)
    {
        std::vector<T> result(count);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(context->Map(buffer.staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Read seed GPU result");
        std::memcpy(result.data(), mapped.pData, count * sizeof(T));
        context->Unmap(buffer.staging.Get(), 0);
        return result;
    }
  public:
    explicit Device(int index, bool allowSoftware = false)
    {
        ComPtr<IDXGIFactory1> factory; ComPtr<IDXGIAdapter1> adapter;
        Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "Create DXGI factory");
        Check(factory->EnumAdapters1(index, &adapter), "Select seed GPU");
        DXGI_ADAPTER_DESC1 info{}; Check(adapter->GetDesc1(&info), "Read seed GPU");
        if (!allowSoftware && ((info.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) || (info.VendorId == 0x1414 && info.DeviceId == 0x8c)))
            throw std::runtime_error("A hardware GPU is required");
        // WARP executes the large correctness batches on CI CPUs. Production
        // never admits it; physical adapters retain the ten-second deadline.
        if (allowSoftware && ((info.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) || (info.VendorId == 0x1414 && info.DeviceId == 0x8c)))
            timeout = std::chrono::seconds(120);
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        Check(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, D3D11_CREATE_DEVICE_SINGLETHREADED,
            &level, 1, D3D11_SDK_VERSION, &device, nullptr, &context), "Create seed compute device");
        D3D11_FEATURE_DATA_DOUBLES support{};
        if (FAILED(device->CheckFeatureSupport(D3D11_FEATURE_DOUBLES, &support, sizeof(support))) || !support.DoublePrecisionFloatShaderOps)
            throw std::runtime_error("The GPU/driver does not support the required FP64 operations");
        source = Resource(105);
        const std::string include = "#include \"SeedOrderKernel.hlsli\"";
        auto at = source.find(include);
        if (at == std::string::npos) throw std::runtime_error("Embedded seed shader include is invalid");
        source.replace(at, include.size(), Resource(106));
        D3D11_QUERY_DESC query{D3D11_QUERY_EVENT, 0};
        Check(device->CreateQuery(&query, &complete), "Create seed GPU completion event");
    }
    void Select(ShaderKind kind, int crystals)
    {
        if (pending) throw std::runtime_error("A seed GPU batch is still pending");
        auto shape = ShapeFor(kind, crystals);
        int key = int(kind) * 64 + (shape.fixed ? crystals : 0);
        if (!kernels.contains(key))
        {
            std::string count = std::to_string(shape.capacity), threads = std::to_string(shape.threads), bits = std::to_string(shape.neighborBits);
            std::vector<D3D_SHADER_MACRO> macros{{"GPU_MAX_CRYSTALS", count.c_str()}, {"GPU_THREADS", threads.c_str()},
                {"GPU_NEAREST_BITS", bits.c_str()}, {"GPU_CATALOG_INPUT", "1"}};
            if (shape.fixed) macros.push_back({"GPU_FIXED_CRYSTALS", "1"});
            if (shape.triangular) macros.push_back({"GPU_TRIANGULAR_TILE", "1"});
            macros.push_back({nullptr, nullptr});
            ComPtr<ID3DBlob> code, errors;
            HRESULT compiled = D3DCompile(source.data(), source.size(), "SeedOrder", macros.data(), nullptr, "Main", "cs_5_0",
                D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_IEEE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
            if (FAILED(compiled) && errors)
                throw std::runtime_error(std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()));
            Check(compiled, "Compile seed shader");
            ComPtr<ID3D11ComputeShader> value;
            Check(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &value), "Create seed shader");
            kernels[key] = value;
        }
        shader = kernels.at(key); fixedCrystals = shape.fixed ? crystals : 0; Bind();
    }
    void SetCatalog(std::shared_ptr<const Catalog> value)
    {
        if (pending || !value) throw std::runtime_error("Invalid seed GPU catalog change");
        catalog = std::move(value);
        edges = Make(8, UINT(catalog->edges.size()), false, catalog->edges.data());
        nearest = Make(4, UINT(catalog->nearest.size()), false, catalog->nearest.data());
        uint32_t info[]{uint32_t(catalog->points), uint32_t(catalog->spawns), uint32_t(catalog->side), 0};
        D3D11_BUFFER_DESC desc{}; desc.ByteWidth = sizeof(info); desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA initial{info, 0, 0}; catalogInfo.Reset();
        Check(device->CreateBuffer(&desc, &initial, &catalogInfo), "Allocate seed catalog constants");
        if (packedSizes.empty()) Resize(capacity);
        else Bind();
    }
    void Resize(int count)
    {
        if (pending || count < 1 || count > 8192) throw std::runtime_error("Invalid seed GPU batch capacity");
        capacity = count; packedIds.resize(size_t(count) * 53); packedSizes.resize(count);
        resetOrders.assign(size_t(count) * 52 + 8, 0xcdcdcdcdu); resetLengths.assign(size_t(count) + 8, -12345.);
        ids = Make(4, UINT(packedIds.size()), false); sizes = Make(4, UINT(packedSizes.size()), false);
        orders = Make(4, UINT(resetOrders.size()), true); lengths = Make(8, UINT(resetLengths.size()), true); Bind();
    }
    bool DiscardPending()
    {
        if (!pending) return true;
        auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
        do
        {
            HRESULT status = context->GetData(complete.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (status == S_OK) { pending = false; return true; }
            if (FAILED(status)) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < until);
        return false;
    }
    std::vector<practice::RouteResult> Execute(std::span<const practice::SeedLayout> layouts, const practice::Cancel &canceled)
    {
        int count = int(layouts.size());
        if (!shader || !catalog || pending || count < 1 || count > capacity) throw std::runtime_error("Invalid seed GPU dispatch");
        for (int i = 0; i < count; i++)
        {
            const auto &selection = layouts[i].selection;
            if (fixedCrystals && selection.crystals.size() != size_t(fixedCrystals)) throw std::runtime_error("Seed GPU crystal count changed");
            catalog->Pack(selection, packedIds.data() + size_t(i) * 53); packedSizes[i] = uint32_t(selection.crystals.size());
        }
        if (canceled()) throw std::runtime_error("Seed search stopped");
        context->UpdateSubresource(ids.buffer.Get(), 0, nullptr, packedIds.data(), 0, 0);
        context->UpdateSubresource(sizes.buffer.Get(), 0, nullptr, packedSizes.data(), 0, 0);
        context->UpdateSubresource(orders.buffer.Get(), 0, nullptr, resetOrders.data(), 0, 0);
        context->UpdateSubresource(lengths.buffer.Get(), 0, nullptr, resetLengths.data(), 0, 0);
        ID3D11UnorderedAccessView *outputs[]{orders.write.Get(), lengths.write.Get()}; context->CSSetUnorderedAccessViews(0, 2, outputs, nullptr);
        context->Dispatch(UINT(count), 1, 1);
        ID3D11UnorderedAccessView *empty[]{nullptr, nullptr}; context->CSSetUnorderedAccessViews(0, 2, empty, nullptr);
        context->CopyResource(orders.staging.Get(), orders.buffer.Get()); context->CopyResource(lengths.staging.Get(), lengths.buffer.Get());
        context->End(complete.Get()); context->Flush(); pending = true;
        auto start = std::chrono::steady_clock::now();
        // Poll completion before mapping, so Stop and a failed driver cannot leave
        // the coordinator blocked indefinitely in a staging-buffer Map.
        for (;;)
        {
            HRESULT status = context->GetData(complete.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (status == S_OK) break;
            Check(status, "Wait for seed GPU");
            if (canceled()) throw std::runtime_error("Seed search stopped");
            if (std::chrono::steady_clock::now() - start > timeout) throw std::runtime_error("Seed GPU timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        auto order = Read<uint32_t>(orders, resetOrders.size()); auto length = Read<double>(lengths, resetLengths.size());
        pending = false;
        for (size_t i = size_t(count) * 52; i < order.size(); i++)
            if (order[i] != 0xcdcdcdcdu) throw std::runtime_error("Seed GPU output guard was overwritten");
        for (size_t i = size_t(count); i < length.size(); i++)
            if (length[i] != -12345.) throw std::runtime_error("Seed GPU length guard was overwritten");
        std::vector<practice::RouteResult> result(count);
        for (int i = 0; i < count; i++)
        {
            auto &r = result[i]; int n = int(packedSizes[i]);
            for (int j = 0; j < n; j++) r.order.push_back(int(order[size_t(i) * 52 + j]));
            for (int j = n; j < 52; j++)
                if (order[size_t(i) * 52 + j] != 0xffffffffu) throw std::runtime_error("Seed GPU output padding is invalid");
            r.length = length[i]; r.complete = ValidOrder(r.order, size_t(n)) && std::isfinite(r.length);
            if (!r.complete) throw std::runtime_error("Seed GPU returned an invalid route");
        }
        return result;
    }
};
} // namespace seed_gpu
