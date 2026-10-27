#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "gpu_order_fixture.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

using Microsoft::WRL::ComPtr;

static void Check(HRESULT result)
{
    if (FAILED(result))
        throw std::runtime_error("Direct3D failure: " + std::to_string(uint32_t(result)));
}

struct LabBuffer
{
    ComPtr<ID3D11Buffer> buffer;
    ComPtr<ID3D11ShaderResourceView> read;
    ComPtr<ID3D11UnorderedAccessView> write;
};

template <class T>
LabBuffer MakeBuffer(ID3D11Device *device, const std::vector<T> &values, bool output = false)
{
    LabBuffer result;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = UINT(values.size() * sizeof(T));
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = output ? D3D11_BIND_UNORDERED_ACCESS : D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(T);
    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = values.data();
    Check(device->CreateBuffer(&desc, &data, &result.buffer));
    if (output)
    {
        D3D11_UNORDERED_ACCESS_VIEW_DESC view{};
        view.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        view.Buffer.NumElements = UINT(values.size());
        Check(device->CreateUnorderedAccessView(result.buffer.Get(), &view, &result.write));
    }
    else
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC view{};
        view.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        view.Buffer.NumElements = UINT(values.size());
        Check(device->CreateShaderResourceView(result.buffer.Get(), &view, &result.read));
    }
    return result;
}

template <class T>
std::vector<T> ReadBuffer(ID3D11Device *device, ID3D11DeviceContext *context, const LabBuffer &source)
{
    D3D11_BUFFER_DESC desc{};
    source.buffer->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = desc.MiscFlags = desc.StructureByteStride = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> staging;
    Check(device->CreateBuffer(&desc, nullptr, &staging));
    context->CopyResource(staging.Get(), source.buffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<T> values(desc.ByteWidth / sizeof(T));
    std::memcpy(values.data(), mapped.pData, desc.ByteWidth);
    context->Unmap(staging.Get(), 0);
    return values;
}

int main(int argc, char **argv)
{
    if (argc < 3)
        return 2;
    try
    {
        std::filesystem::path shaderPath = argv[1], resultPath = argv[2];
        ComPtr<ID3DBlob> code, errors;
        HRESULT compiled = D3DCompileFromFile(
            shaderPath.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "Main", "cs_5_0",
            D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_IEEE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
            &code, &errors);
        if (errors)
            std::cout.write(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize());
        Check(compiled);
        auto binaryPath = resultPath;
        binaryPath.replace_extension(".cso");
        std::ofstream binary(binaryPath, std::ios::binary);
        binary.write(static_cast<const char *>(code->GetBufferPointer()), code->GetBufferSize());
        binary.close();

        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        D3D_FEATURE_LEVEL feature = D3D_FEATURE_LEVEL_11_0;
        std::string driver;
        for (auto type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_REFERENCE, D3D_DRIVER_TYPE_WARP})
        {
            if (argc > 3 && std::string(argv[3]) == "--hardware" && type != D3D_DRIVER_TYPE_HARDWARE)
                continue;
            device.Reset();
            context.Reset();
            HRESULT created = D3D11CreateDevice(nullptr, type, nullptr, 0, &feature, 1, D3D11_SDK_VERSION,
                                                &device, nullptr, &context);
            if (FAILED(created))
                continue;
            D3D11_FEATURE_DATA_DOUBLES support{};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_DOUBLES, &support, sizeof(support))) &&
                support.DoublePrecisionFloatShaderOps)
            {
                driver = type == D3D_DRIVER_TYPE_HARDWARE    ? "hardware"
                         : type == D3D_DRIVER_TYPE_REFERENCE ? "reference"
                                                             : "WARP";
                break;
            }
        }
        if (driver.empty())
        {
            std::ofstream(resultPath)
                << "{\"shaderCompiled\":true,\"execution\":\"unavailable\","
                   "\"reason\":\"No available D3D11 device supports the required FP64 operations\"}\n";
            std::cout
                << "FP64 shader compiled; execution unavailable on this machine. No GPU performance claim.\n";
            return 77;
        }
        ComPtr<ID3D11ComputeShader> shader;
        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIAdapter1> adapter1;
        Check(device.As(&dxgiDevice));
        Check(dxgiDevice->GetAdapter(&adapter));
        Check(adapter.As(&adapter1));
        DXGI_ADAPTER_DESC1 adapterInfo{};
        Check(adapter1->GetDesc1(&adapterInfo));
        bool software = (adapterInfo.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        std::wcout << L"Adapter: " << adapterInfo.Description << L"; reportedSoftwareFlag=" << software << L'\n';
        Check(device->CreateComputeShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &shader));
        std::vector<GpuOrderFixture> fixtures;
        std::vector<double> matrices;
        std::vector<uint32_t> neighbors, sizes;
        for (int n : {1, 2, 8, 16, 22, 32, 33, 40, 52})
            for (int profile = 0; profile < 5; profile++)
            {
                fixtures.emplace_back(n, profile, 0x720007);
                auto &f = fixtures.back();
                matrices.insert(matrices.end(), f.matrix.begin(), f.matrix.end());
                neighbors.insert(neighbors.end(), f.nearest.begin(), f.nearest.end());
                sizes.push_back(n);
            }
        auto matrix = MakeBuffer(device.Get(), matrices);
        auto nearest = MakeBuffer(device.Get(), neighbors);
        auto counts = MakeBuffer(device.Get(), sizes);
        auto orders =
            MakeBuffer(device.Get(), std::vector<uint32_t>(sizes.size() * 52 + 8, 0xcdcdcdcdu), true);
        auto lengths = MakeBuffer(device.Get(), std::vector<double>(sizes.size() + 8, -12345), true);
        ID3D11ShaderResourceView *inputs[]{matrix.read.Get(), nearest.read.Get(), counts.read.Get()};
        ID3D11UnorderedAccessView *outputs[]{orders.write.Get(), lengths.write.Get()};
        context->CSSetShader(shader.Get(), nullptr, 0);
        context->CSSetShaderResources(0, 3, inputs);
        context->CSSetUnorderedAccessViews(0, 2, outputs, nullptr);
        auto started = std::chrono::steady_clock::now();
        context->Dispatch(UINT(sizes.size()), 1, 1);
        ID3D11UnorderedAccessView *empty[]{nullptr, nullptr};
        context->CSSetUnorderedAccessViews(0, 2, empty, nullptr);
        auto returnedOrders = ReadBuffer<uint32_t>(device.Get(), context.Get(), orders);
        auto returnedLengths = ReadBuffer<double>(device.Get(), context.Get(), lengths);
        double elapsed =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        for (size_t i = 0; i < fixtures.size(); i++)
        {
            const auto &f = fixtures[i];
            for (int j = 0; j < f.n; j++)
                if (returnedOrders[i * 52 + j] != uint32_t(f.expected.order[j]))
                    throw std::runtime_error("Shader visit order differs from production solver at case " +
                                             std::to_string(i));
            for (int j = f.n; j < 52; j++)
                if (returnedOrders[i * 52 + j] != 0xffffffffu)
                    throw std::runtime_error("Invalid shader padding");
            if (returnedLengths[i] != f.expected.length)
                throw std::runtime_error("Shader length differs from production solver");
        }
        for (size_t i = fixtures.size() * 52; i < returnedOrders.size(); i++)
            if (returnedOrders[i] != 0xcdcdcdcdu)
                throw std::runtime_error("Shader wrote beyond output range");
        for (size_t i = fixtures.size(); i < returnedLengths.size(); i++)
            if (returnedLengths[i] != -12345)
                throw std::runtime_error("Shader wrote beyond length range");
        std::ofstream(resultPath) << "{\"shaderCompiled\":true,\"execution\":\"passed\",\"creationDriver\":\""
                                  << driver << "\",\"adapterVendorId\":" << adapterInfo.VendorId
                                  << ",\"adapterDeviceId\":" << adapterInfo.DeviceId
                                  << ",\"reportedSoftwareFlag\":" << (software ? "true" : "false")
                                  << ",\"cases\":" << fixtures.size()
                                  << ",\"dispatchAndReadbackMs\":" << elapsed
                                  << ",\"liveGameOrObsTested\":false}\n";
        std::cout << fixtures.size() << " FP64 shader/production comparisons and output guards; D3D11 creation driver=" << driver
                  << ": PASS; not a live game/OBS throughput measurement.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
