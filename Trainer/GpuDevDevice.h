#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <map>
#include "GpuDevReport.h"
#include "GpuDevCatalog.h"

namespace gpu_dev
{
using Microsoft::WRL::ComPtr;

inline void Check(HRESULT result, const char *operation)
{
    if (FAILED(result))
        throw std::runtime_error(std::string(operation) + " failed (HRESULT " +
                                 std::to_string(uint32_t(result)) + ")");
}

inline Adapter Describe(IDXGIAdapter1 *adapter, int index)
{
    DXGI_ADAPTER_DESC1 info{};
    Check(adapter->GetDesc1(&info), "Read GPU adapter");
    int bytes = WideCharToMultiByte(CP_UTF8, 0, info.Description, -1, nullptr, 0, nullptr, nullptr);
    std::string name(size_t(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, info.Description, -1, name.data(), bytes, nullptr, nullptr);
    if (!name.empty()) name.pop_back();
    Adapter result;
    result.index = index;
    result.name = name;
    result.vendor = info.VendorId;
    result.device = info.DeviceId;
    result.dedicatedBytes = info.DedicatedVideoMemory;
    // Some CI Basic Render adapters omit DXGI_ADAPTER_FLAG_SOFTWARE.
    result.software = (info.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 ||
                      (info.VendorId == 0x1414 && info.DeviceId == 0x8c);
    LARGE_INTEGER driver{};
    if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver)))
        result.driver = std::to_string(HIWORD(driver.HighPart)) + "." +
                        std::to_string(LOWORD(driver.HighPart)) + "." +
                        std::to_string(HIWORD(driver.LowPart)) + "." +
                        std::to_string(LOWORD(driver.LowPart));
    return result;
}

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
        result.push_back(Describe(adapter.Get(), int(i)));
    }
    return result;
}

inline std::string Resource(int id)
{
    HRSRC resource = FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (!resource) throw std::runtime_error("Embedded GPU shader is missing from this build");
    HGLOBAL data = LoadResource(nullptr, resource);
    const char *bytes = static_cast<const char *>(LockResource(data));
    if (!bytes) throw std::runtime_error("Could not read the embedded GPU shader");
    return {bytes, SizeofResource(nullptr, resource)};
}

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
    std::string source;
    int fixedCrystals = 0;
    Buffer matrices, neighbors, sizes, orders, lengths;
    Buffer selected, catalogEdges, catalogNearest;
    ComPtr<ID3D11Buffer> catalogInfo;
    std::shared_ptr<const Catalog> catalog;
    bool compact;
    uint64_t staticBytes = 0;
    int capacity;
    std::vector<double> packedMatrices, resetLengths;
    std::vector<uint32_t> packedNeighbors, packedSizes, resetOrders;
    std::vector<uint32_t> packedIds;
    ComPtr<ID3D11Query> disjoint;
    std::vector<std::array<ComPtr<ID3D11Query>, 4>> timestamps;
    size_t timedBatches = 0;
    int pending = 0;
    bool recording = false;

    static double Elapsed(std::chrono::steady_clock::time_point start)
    {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }

    ComPtr<ID3D11ComputeShader> Compile(int crystals)
    {
        std::string count = std::to_string(crystals ? crystals : 52);
        std::string threads = std::to_string(crystals && crystals <= 32 ? 32 : 64);
        std::vector<D3D_SHADER_MACRO> macros{{"GPU_MAX_CRYSTALS", count.c_str()}, {"GPU_THREADS", threads.c_str()}};
        if (compact) macros.push_back({"GPU_CATALOG_INPUT", "1"});
        if (crystals) macros.push_back({"GPU_FIXED_CRYSTALS", "1"});
        macros.push_back({nullptr, nullptr});
        ComPtr<ID3DBlob> code, errors;
        HRESULT compiled = D3DCompile(source.data(), source.size(), "gpu_dev", macros.data(), nullptr,
                                      "Main", "cs_5_0", D3DCOMPILE_ENABLE_STRICTNESS |
                                      D3DCOMPILE_IEEE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
                                      0, &code, &errors);
        if (FAILED(compiled) && errors)
            throw std::runtime_error(std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize()));
        Check(compiled, "Compile embedded GPU shader");
        ComPtr<ID3D11ComputeShader> result;
        Check(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &result), "Create GPU compute shader");
        return result;
    }

    Buffer Make(UINT stride, UINT count, bool output, const void *initial = nullptr)
    {
        Buffer result;
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = stride * count;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = output ? D3D11_BIND_UNORDERED_ACCESS : D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        desc.StructureByteStride = stride;
        D3D11_SUBRESOURCE_DATA data{initial, 0, 0};
        Check(device->CreateBuffer(&desc, initial ? &data : nullptr, &result.buffer), "Allocate GPU buffer");
        if (output)
        {
            D3D11_UNORDERED_ACCESS_VIEW_DESC view{};
            view.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
            view.Buffer.NumElements = count;
            Check(device->CreateUnorderedAccessView(result.buffer.Get(), &view, &result.write), "Create GPU output view");
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = desc.MiscFlags = desc.StructureByteStride = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            Check(device->CreateBuffer(&desc, nullptr, &result.staging), "Allocate GPU readback");
        }
        else
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC view{};
            view.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
            view.Buffer.NumElements = count;
            Check(device->CreateShaderResourceView(result.buffer.Get(), &view, &result.read), "Create GPU input view");
        }
        return result;
    }

    template <class T> std::vector<T> Read(Buffer &source, size_t count)
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(context->Map(source.staging.Get(), 0, D3D11_MAP_READ, 0, &mapped), "Read GPU result");
        std::vector<T> values(count);
        std::memcpy(values.data(), mapped.pData, count * sizeof(T));
        context->Unmap(source.staging.Get(), 0);
        return values;
    }

  public:
    explicit Device(Report &report, bool allowSoftware = false)
        : compact(report.options.cachedCatalog), capacity(report.options.batch)
    {
        ComPtr<IDXGIFactory1> factory;
        ComPtr<IDXGIAdapter1> adapter;
        Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)), "Create DXGI factory");
        Check(factory->EnumAdapters1(report.options.adapter, &adapter), "Select GPU adapter");
        report.adapter = Describe(adapter.Get(), report.options.adapter);
        if (report.adapter.software && !allowSoftware)
            throw std::runtime_error("The selected adapter is a software renderer. Select your physical GPU.");
        D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        Check(D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                               D3D11_CREATE_DEVICE_SINGLETHREADED, &level, 1, D3D11_SDK_VERSION,
                               &device, nullptr, &context), "Create dedicated GPU test device");
        D3D11_FEATURE_DATA_DOUBLES support{};
        report.fp64 = SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_DOUBLES, &support, sizeof(support))) &&
                      support.DoublePrecisionFloatShaderOps;
        if (!report.fp64)
            throw std::runtime_error("This adapter/driver does not expose the required D3D11 FP64 operations.");
        source = Resource(105);
        std::string kernel = Resource(106);
        const std::string include = "#include \"gpu_order_kernel.hlsli\"";
        auto at = source.find(include);
        if (at == std::string::npos) throw std::runtime_error("Embedded shader include is invalid");
        source.replace(at, include.size(), kernel);
        shader = kernels[0] = Compile(0);
        if (!compact) Resize(capacity, report);
        // Allocate once outside all scoring timings. One disjoint interval spans
        // a complete GPU pass, rather than beginning/ending one per batch/frame.
        D3D11_QUERY_DESC query{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
        report.timestampsSupported = report.options.gpuTiming && SUCCEEDED(device->CreateQuery(&query, &disjoint));
        if (report.timestampsSupported)
        {
            auto batches = Batches(report.options);
            int minimumBatch = *std::min_element(batches.begin(), batches.end());
            timestamps.resize(size_t((report.options.seeds + minimumBatch - 1) / minimumBatch) + 1);
            query.Query = D3D11_QUERY_TIMESTAMP;
            for (auto &batch : timestamps)
                for (auto &marker : batch)
                    if (FAILED(device->CreateQuery(&query, &marker))) report.timestampsSupported = false;
            if (!report.timestampsSupported) { disjoint.Reset(); timestamps.clear(); }
        }
    }

    double PrepareKernel(int crystals)
    {
        if (crystals < 1 || crystals > 52 || pending || recording) throw std::runtime_error("Invalid shader specialization");
        if (kernels.contains(crystals)) return 0;
        auto start = std::chrono::steady_clock::now();
        kernels[crystals] = Compile(crystals);
        return Elapsed(start);
    }

    void SelectKernel(int crystals)
    {
        auto found = kernels.find(crystals);
        if (pending || recording || found == kernels.end()) throw std::runtime_error("GPU shader was not prepared");
        fixedCrystals = crystals; shader = found->second;
        context->CSSetShader(shader.Get(), nullptr, 0);
    }

    int KernelThreads() const { return fixedCrystals && fixedCrystals <= 32 ? 32 : 64; }
    int KernelSharedBytes() const
    {
        int n = fixedCrystals ? fixedCrystals : 52;
        return 8 * (n + 1) * (n + 1) + 12 * KernelThreads() + 4 * n + (compact ? 256 + 4 * n * ((n + 1) / 2) : 0);
    }

    void SetCatalog(std::shared_ptr<const Catalog> value, Report &report)
    {
        if (!compact || !value) throw std::runtime_error("GPU device was not created for catalog inputs");
        catalog = std::move(value);
        catalogEdges = Make(8, UINT(catalog->edges.size()), false, catalog->edges.data());
        catalogNearest = Make(4, UINT(catalog->nearest.size()), false, catalog->nearest.data());
        uint32_t info[]{uint32_t(catalog->points), uint32_t(catalog->spawns), uint32_t(catalog->side), 0};
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = sizeof(info); desc.Usage = D3D11_USAGE_DEFAULT; desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        D3D11_SUBRESOURCE_DATA initial{info, 0, 0};
        catalogInfo.Reset();
        Check(device->CreateBuffer(&desc, &initial, &catalogInfo), "Allocate GPU catalog constants");
        staticBytes = catalog->edges.size() * 8 + catalog->nearest.size() * 4 + sizeof(info);
        report.catalogBytes = staticBytes;
        Resize(capacity, report);
    }

    void Resize(int count, Report &report)
    {
        if (pending) throw std::runtime_error("Cannot resize GPU buffers while a batch is pending");
        if (count < 1 || count > 2048) throw std::runtime_error("Invalid GPU buffer capacity");
        capacity = count;
        if (compact)
        {
            if (!catalog) throw std::runtime_error("GPU catalog has not been prepared");
            packedIds.resize(size_t(capacity) * 53);
            selected = Make(4, UINT(packedIds.size()), false);
        }
        else
        {
            packedMatrices.resize(size_t(capacity) * 53 * 53);
            packedNeighbors.resize(size_t(capacity) * 52 * 52);
            matrices = Make(8, UINT(packedMatrices.size()), false);
            neighbors = Make(4, UINT(packedNeighbors.size()), false);
        }
        packedSizes.resize(capacity);
        resetOrders.assign(size_t(capacity) * 52 + 8, 0xcdcdcdcdu);
        resetLengths.assign(size_t(capacity) + 8, -12345.);
        sizes = Make(4, UINT(packedSizes.size()), false);
        orders = Make(4, UINT(resetOrders.size()), true);
        lengths = Make(8, UINT(resetLengths.size()), true);
        report.inputBytesPerLayout = compact ? 53 * 4 + 4 : 53 * 53 * 8 + 52 * 52 * 4 + 4;
        uint64_t bytes = staticBytes + packedIds.size() * 4 + packedMatrices.size() * 8 + packedNeighbors.size() * 4 + packedSizes.size() * 4 +
                         2 * (resetOrders.size() * 4 + resetLengths.size() * 8);
        report.gpuBufferBytes = std::max(report.gpuBufferBytes, bytes);
        context->CSSetShader(shader.Get(), nullptr, 0);
        ID3D11ShaderResourceView *inputs[]{compact ? catalogEdges.read.Get() : matrices.read.Get(),
                                         compact ? catalogNearest.read.Get() : neighbors.read.Get(),
                                         sizes.read.Get(), compact ? selected.read.Get() : nullptr};
        context->CSSetShaderResources(0, 4, inputs);
        ID3D11Buffer *constants = catalogInfo.Get();
        context->CSSetConstantBuffers(0, 1, &constants);
    }

    void BeginPass()
    {
        timedBatches = 0;
        recording = bool(disjoint);
        if (recording) context->Begin(disjoint.Get());
    }

    void EndPass(PairTiming &timing, const practice::Cancel &canceled)
    {
        // A cancellation can leave the batch submitted ahead of pickup scoring.
        if (pending) Collect(&timing);
        if (!recording) return;
        recording = false;
        auto start = std::chrono::steady_clock::now();
        context->End(disjoint.Get());
        context->Flush();
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT range{};
        HRESULT ready;
        while ((ready = context->GetData(disjoint.Get(), &range, sizeof(range), D3D11_ASYNC_GETDATA_DONOTFLUSH)) == S_FALSE &&
               !canceled() && Elapsed(start) < 2000.)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Check(ready, "Collect GPU timing range");
        for (size_t i = 0; i < timedBatches; i++)
        {
            UINT64 ticks[4]{};
            bool valid = ready == S_OK && !range.Disjoint && range.Frequency;
            for (int j = 0; j < 4 && valid; j++)
                valid = context->GetData(timestamps[i][j].Get(), &ticks[j], sizeof(ticks[j]), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_OK;
            valid = valid && ticks[0] <= ticks[1] && ticks[1] <= ticks[2] && ticks[2] <= ticks[3];
            if (!valid) { timing.timestampInvalid++; continue; }
            double scale = 1000. / double(range.Frequency);
            timing.gpuUploadMs += double(ticks[1] - ticks[0]) * scale;
            timing.gpuKernelMs += double(ticks[2] - ticks[1]) * scale;
            timing.gpuCopyMs += double(ticks[3] - ticks[2]) * scale;
            timing.timestampBatches++;
        }
        timing.queryCollectMs += Elapsed(start);
    }

    void Pack(int slot, const double *matrix, int n)
    {
        if (compact || slot < 0 || slot >= capacity || n < 1 || n > 52 || (fixedCrystals && n != fixedCrystals))
            throw std::runtime_error("GPU input exceeds the supported batch/crystal range");
        int side = n + 1;
        for (int i = 0; i < side * side; i++)
            if (!std::isfinite(matrix[i])) throw std::runtime_error("Captured map has incomplete route distances");
        std::copy_n(matrix, side * side, packedMatrices.data() + size_t(slot) * 53 * 53);
        packedSizes[slot] = n;
        for (int from = 0; from < n; from++)
        {
            auto *row = packedNeighbors.data() + size_t(slot) * 52 * 52 + from * 52;
            std::iota(row, row + n, uint32_t{0});
            std::sort(row, row + n, [&](uint32_t a, uint32_t b) {
                double x = matrix[(from + 1) * side + a + 1], y = matrix[(from + 1) * side + b + 1];
                return x < y || (x == y && a < b);
            });
        }
    }

    void PackSelection(int slot, const practice::Selection &selection)
    {
        if (!compact || !catalog || slot < 0 || slot >= capacity ||
            (fixedCrystals && selection.crystals.size() != size_t(fixedCrystals)))
            throw std::runtime_error("Invalid GPU catalog input slot");
        catalog->Pack(selection, packedIds.data() + size_t(slot) * 53);
        packedSizes[slot] = uint32_t(selection.crystals.size());
    }

    void Submit(int count, PairTiming *timing = nullptr)
    {
        if (pending || count < 1 || count > capacity) throw std::runtime_error("Invalid or already pending GPU batch");
        auto start = std::chrono::steady_clock::now();
        bool measured = recording && timedBatches < timestamps.size();
        if (measured) context->End(timestamps[timedBatches][0].Get());
        if (compact) context->UpdateSubresource(selected.buffer.Get(), 0, nullptr, packedIds.data(), 0, 0);
        else
        {
            context->UpdateSubresource(matrices.buffer.Get(), 0, nullptr, packedMatrices.data(), 0, 0);
            context->UpdateSubresource(neighbors.buffer.Get(), 0, nullptr, packedNeighbors.data(), 0, 0);
        }
        context->UpdateSubresource(sizes.buffer.Get(), 0, nullptr, packedSizes.data(), 0, 0);
        context->UpdateSubresource(orders.buffer.Get(), 0, nullptr, resetOrders.data(), 0, 0);
        context->UpdateSubresource(lengths.buffer.Get(), 0, nullptr, resetLengths.data(), 0, 0);
        ID3D11UnorderedAccessView *outputs[]{orders.write.Get(), lengths.write.Get()};
        context->CSSetUnorderedAccessViews(0, 2, outputs, nullptr);
        if (timing) timing->hostUploadMs += Elapsed(start);
        if (measured) context->End(timestamps[timedBatches][1].Get());
        context->Dispatch(UINT(count), 1, 1);
        if (measured) context->End(timestamps[timedBatches][2].Get());
        ID3D11UnorderedAccessView *empty[]{nullptr, nullptr};
        context->CSSetUnorderedAccessViews(0, 2, empty, nullptr);
        start = std::chrono::steady_clock::now();
        // Queue both copies before the first blocking Map.
        context->CopyResource(orders.staging.Get(), orders.buffer.Get());
        context->CopyResource(lengths.staging.Get(), lengths.buffer.Get());
        if (measured) { context->End(timestamps[timedBatches][3].Get()); timedBatches++; }
        context->Flush();
        pending = count;
        if (timing) timing->hostReadbackMs += Elapsed(start);
    }

    std::vector<practice::RouteResult> Collect(PairTiming *timing = nullptr)
    {
        if (!pending) throw std::runtime_error("No GPU batch is pending");
        int count = pending;
        auto start = std::chrono::steady_clock::now();
        auto order = Read<uint32_t>(orders, resetOrders.size());
        auto length = Read<double>(lengths, resetLengths.size());
        if (timing) timing->hostReadbackMs += Elapsed(start);
        for (size_t i = size_t(count) * 52; i < order.size(); i++)
            if (order[i] != 0xcdcdcdcdu) throw std::runtime_error("GPU output guard was overwritten");
        for (size_t i = size_t(count); i < length.size(); i++)
            if (length[i] != -12345.) throw std::runtime_error("GPU length guard was overwritten");
        std::vector<practice::RouteResult> result(count);
        for (int i = 0; i < count; i++)
        {
            int n = int(packedSizes[i]);
            auto &r = result[i];
            for (int j = 0; j < n; j++) r.order.push_back(int(order[size_t(i) * 52 + j]));
            for (int j = n; j < 52; j++)
                if (order[size_t(i) * 52 + j] != 0xffffffffu) throw std::runtime_error("GPU output padding is invalid");
            r.length = length[i];
            r.complete = ValidOrder(r.order, size_t(n)) && std::isfinite(r.length);
            r.message = r.complete ? "GPU multi-start nearest neighbor + 2-opt + relocation."
                                   : "GPU returned an invalid route.";
        }
        pending = 0;
        return result;
    }

    std::vector<practice::RouteResult> Execute(int count, PairTiming *timing = nullptr)
    {
        Submit(count, timing);
        return Collect(timing);
    }
};
} // namespace gpu_dev
