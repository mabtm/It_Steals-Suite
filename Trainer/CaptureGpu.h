#pragma once
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <climits>

// One GPU image shared with Unity. A busy consumer drops a capture frame;
// it never waits on the trainer's input/render thread.
class CaptureGpu
{
    template <class T> using Com = Microsoft::WRL::ComPtr<T>;
    Com<ID3D11Texture2D> input, shared;
    Com<ID3D11ShaderResourceView> source;
    Com<ID3D11RenderTargetView> target;
    Com<IDXGIKeyedMutex> gate;
    Com<ID3D11VertexShader> vertex;
    Com<ID3D11PixelShader> pixel;
    Com<ID3D11BlendState> blend;
    Com<ID3D11DepthStencilState> depth;
    Com<ID3D11RasterizerState> raster;
    UINT width = 0, height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    HANDLE handle = nullptr;

    bool Shaders(ID3D11Device *device)
    {
        if (vertex && pixel && blend && depth && raster)
            return true;
        constexpr char code[] =
            "float4 VS(uint id:SV_VertexID):SV_Position {"
            "float2 uv=float2((id<<1)&2,id&2);return float4(uv*float2(2,-2)+float2(-1,1),0,1);}"
            "Texture2D<float4> image:register(t0);"
            "float4 PS(float4 p:SV_Position):SV_Target {"
            "float4 c=image.Load(int3(int2(p.xy),0));"
            "return c.a>0 ? float4(saturate(c.rgb/c.a),c.a) : 0;}";
        Com<ID3DBlob> vs, ps, errors;
        if (FAILED(D3DCompile(code, sizeof(code), nullptr, nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vs,
                              &errors)) ||
            FAILED(D3DCompile(code, sizeof(code), nullptr, nullptr, nullptr, "PS", "ps_4_0", 0, 0, &ps,
                              &errors)) ||
            FAILED(
                device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &vertex)) ||
            FAILED(device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &pixel)))
            return false;
        D3D11_BLEND_DESC b{};
        b.RenderTarget[0].SrcBlend = b.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        b.RenderTarget[0].DestBlend = b.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
        b.RenderTarget[0].BlendOp = b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        b.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        D3D11_DEPTH_STENCIL_DESC d{};
        d.DepthFunc = D3D11_COMPARISON_ALWAYS;
        D3D11_RASTERIZER_DESC r{};
        r.FillMode = D3D11_FILL_SOLID;
        r.CullMode = D3D11_CULL_NONE;
        r.DepthClipEnable = TRUE;
        return SUCCEEDED(device->CreateBlendState(&b, &blend)) &&
               SUCCEEDED(device->CreateDepthStencilState(&d, &depth)) &&
               SUCCEEDED(device->CreateRasterizerState(&r, &raster));
    }

    bool Prepare(ID3D11Device *device, const D3D11_TEXTURE2D_DESC &original)
    {
        if (shared && width == original.Width && height == original.Height && format == original.Format)
            return true;
        Reset();
        if (original.SampleDesc.Count != 1 || !Shaders(device))
            return false;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = original.Width;
        desc.Height = original.Height;
        desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
        desc.Format = original.Format;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &input)) ||
            FAILED(device->CreateShaderResourceView(input.Get(), nullptr, &source)))
            return false;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
        Com<IDXGIResource> resource;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &shared)) ||
            FAILED(device->CreateRenderTargetView(shared.Get(), nullptr, &target)) ||
            FAILED(shared.As(&gate)) || FAILED(shared.As(&resource)) ||
            FAILED(resource->GetSharedHandle(&handle)))
        {
            Reset();
            return false;
        }
        width = original.Width;
        height = original.Height;
        format = original.Format;
        return true;
    }

  public:
    void Reset()
    {
        source.Reset();
        target.Reset();
        gate.Reset();
        shared.Reset();
        input.Reset();
        vertex.Reset();
        pixel.Reset();
        blend.Reset();
        depth.Reset();
        raster.Reset();
        width = height = 0;
        format = DXGI_FORMAT_UNKNOWN;
        handle = nullptr;
    }

    HANDLE Handle() const
    {
        return handle;
    }

    UINT Width() const
    {
        return width;
    }

    UINT Height() const
    {
        return height;
    }

    bool Publish(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11RenderTargetView *rendered)
    {
        Com<ID3D11Resource> original;
        Com<ID3D11Texture2D> texture;
        rendered->GetResource(&original);
        if (FAILED(original.As(&texture)))
            return false;
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        if (!Prepare(device, desc) || gate->AcquireSync(0, 0) != S_OK)
            return false;
        context->CopyResource(input.Get(), texture.Get());
        auto *rt = target.Get();
        auto *srv = source.Get();
        context->OMSetRenderTargets(1, &rt, nullptr);
        context->OMSetBlendState(blend.Get(), nullptr, UINT_MAX);
        context->OMSetDepthStencilState(depth.Get(), 0);
        context->RSSetState(raster.Get());
        D3D11_VIEWPORT viewport{0, 0, float(width), float(height), 0, 1};
        context->RSSetViewports(1, &viewport);
        context->IASetInputLayout(nullptr);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->VSSetShader(vertex.Get(), nullptr, 0);
        context->PSSetShader(pixel.Get(), nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->HSSetShader(nullptr, nullptr, 0);
        context->DSSetShader(nullptr, nullptr, 0);
        context->PSSetShaderResources(0, 1, &srv);
        context->Draw(3, 0);
        srv = nullptr;
        context->PSSetShaderResources(0, 1, &srv);
        context->OMSetRenderTargets(1, &rendered, nullptr);
        // Submit shared-resource writes before handing ownership to Unity.
        context->Flush();
        return gate->ReleaseSync(1) == S_OK;
    }
};
