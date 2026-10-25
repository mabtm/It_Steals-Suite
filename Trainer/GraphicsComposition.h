#pragma once
#include <dxgi1_2.h>
#include <dcomp.h>

// Optional per-pixel alpha path. The existing DX11/layered renderer is retained
// as a fallback if the composition device cannot be created.
class GraphicsComposition
{
    HMODULE module = nullptr;
    IDCompositionDevice *device = nullptr;
    IDCompositionTarget *target = nullptr;
    IDCompositionVisual *visual = nullptr;
    IDCompositionEffectGroup *opacityEffect = nullptr;
    float opacity = 1;

  public:
    ~GraphicsComposition()
    {
        Reset();
    }

    GraphicsComposition() = default;
    GraphicsComposition(const GraphicsComposition &) = delete;

    void Reset()
    {
        opacity = 1;
        if (visual)
        {
            visual->SetEffect(nullptr);
            visual->SetContent(nullptr);
            visual->Release();
            visual = nullptr;
        }
        if (opacityEffect)
        {
            opacityEffect->Release();
            opacityEffect = nullptr;
        }
        if (target)
        {
            target->SetRoot(nullptr);
            target->Release();
            target = nullptr;
        }
        if (device)
        {
            device->Commit();
            device->Release();
            device = nullptr;
        }
        if (module)
        {
            FreeLibrary(module);
            module = nullptr;
        }
    }

    void SetCaptured(bool captured)
    {
        float next = captured ? 0.f : 1.f;
        if (opacityEffect && device && opacity != next && SUCCEEDED(opacityEffect->SetOpacity(next)))
        {
            if (SUCCEEDED(device->Commit()))
                opacity = next;
        }
    }

    bool Attach(ID3D11Device *renderer, HWND window, UINT width, UINT height, IDXGISwapChain **output)
    {
        Reset();
        wchar_t system[MAX_PATH]{};
        UINT count = GetSystemDirectoryW(system, MAX_PATH);
        if (!count || count >= MAX_PATH)
            return false;
        std::wstring name = std::wstring(system) + L"\\dcomp.dll";
        module = LoadLibraryW(name.c_str());
        using CreateDevice = HRESULT(WINAPI *)(IDXGIDevice *, REFIID, void **);
        CreateDevice create = module ? reinterpret_cast<CreateDevice>(reinterpret_cast<void *>(
                                           GetProcAddress(module, "DCompositionCreateDevice")))
                                     : nullptr;
        if (!create)
        {
            Reset();
            return false;
        }
        IDXGIDevice *dxgi = nullptr;
        IDXGIAdapter *adapter = nullptr;
        IDXGIFactory2 *factory = nullptr;
        IDXGISwapChain1 *chain = nullptr;
        HRESULT hr = renderer->QueryInterface(IID_PPV_ARGS(&dxgi));
        if (SUCCEEDED(hr))
            hr = dxgi->GetAdapter(&adapter);
        if (SUCCEEDED(hr))
            hr = adapter->GetParent(IID_PPV_ARGS(&factory));
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = std::max(1u, width);
        desc.Height = std::max(1u, height);
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        if (SUCCEEDED(hr))
            hr = factory->CreateSwapChainForComposition(renderer, &desc, nullptr, &chain);
        if (SUCCEEDED(hr))
            hr = create(dxgi, IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr))
            hr = device->CreateTargetForHwnd(window, TRUE, &target);
        if (SUCCEEDED(hr))
            hr = device->CreateVisual(&visual);
        if (SUCCEEDED(hr))
            hr = device->CreateEffectGroup(&opacityEffect);
        if (SUCCEEDED(hr))
            hr = visual->SetEffect(opacityEffect);
        if (SUCCEEDED(hr))
            hr = visual->SetContent(chain);
        if (SUCCEEDED(hr))
            hr = target->SetRoot(visual);
        if (SUCCEEDED(hr))
            hr = device->Commit();
        if (factory)
            factory->Release();
        if (adapter)
            adapter->Release();
        if (dxgi)
            dxgi->Release();
        if (FAILED(hr))
        {
            if (chain)
                chain->Release();
            Reset();
            return false;
        }
        *output = chain;
        return true;
    }
};
