#pragma once
#include "LayeredHud.h"
#include "CaptureGpu.h"
#include <string>
#include <vector>

// The game composites these exact HUD pixels, so window/game capture includes
// the overlay. The normal desktop overlay remains the fallback until acknowledged.
class CaptureOverlay
{
    static constexpr uint32_t Magic = 0x48505349, Capacity = 32 * 1024 * 1024 + 256;
    HANDLE mapping = nullptr, mutex = nullptr;
    uint8_t *data = nullptr;
    DWORD pid = 0;
    uint32_t acknowledged = 0;
    ULONGLONG lastAck = 0, nextConnect = 0;
    const LayeredHud *lastSource = nullptr;
    uint64_t lastFrame = 0;
    CaptureGpu gpu;
    LayeredHud menuFallback;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> fallbackTarget;
    DXGI_FORMAT fallbackFormat = DXGI_FORMAT_UNKNOWN;
    ULONGLONG nextFallback = 0;

    static const std::array<uint8_t, 65536> &StraightAlpha()
    {
        static const auto values = [] {
            std::array<uint8_t, 65536> result{};
            for (unsigned a = 1; a < 256; ++a)
                for (unsigned c = 0; c < 256; ++c)
                    result[a * 256 + c] = uint8_t(std::min(255u, (c * 255 + a / 2) / a));
            return result;
        }();
        return values;
    }

    bool Lock()
    {
        DWORD result = WaitForSingleObject(mutex, 0);
        if (result == WAIT_ABANDONED)
            ReleaseMutex(mutex);
        return result == WAIT_OBJECT_0;
    }

    template <class T> T Read(size_t at) const
    {
        T value{};
        std::memcpy(&value, data + at, sizeof(value));
        return value;
    }

    template <class T> void Write(size_t at, T value)
    {
        std::memcpy(data + at, &value, sizeof(value));
    }

  public:
    bool enabled = false;

    ~CaptureOverlay()
    {
        Close();
    }

    bool Active() const
    {
        return enabled && lastAck && GetTickCount64() - lastAck < 350;
    }

    void ResetGraphics()
    {
        gpu.Reset();
        fallbackTarget.Reset();
        menuFallback.Reset();
        fallbackFormat = DXGI_FORMAT_UNKNOWN;
        nextFallback = 0;
        if (data && Lock())
        {
            Write(36, 0u);
            Write(44, 0u);
            ReleaseMutex(mutex);
        }
        lastAck = 0;
        lastSource = nullptr;
        lastFrame = 0;
    }

    void Close()
    {
        ResetGraphics();
        if (data)
            UnmapViewOfFile(data);
        if (mapping)
            CloseHandle(mapping);
        if (mutex)
            CloseHandle(mutex);
        data = nullptr;
        mapping = mutex = nullptr;
        pid = acknowledged = 0;
        lastAck = 0;
    }

    void Tick(DWORD process, bool visible)
    {
        if (!enabled || pid != process)
        {
            if (data && Lock())
            {
                Write(36, 0u);
                ReleaseMutex(mutex);
            }
            Close();
        }
        if (!enabled || !process)
            return;
        if (!data && GetTickCount64() >= nextConnect)
        {
            nextConnect = GetTickCount64() + 1000;
            auto name = L"Local\\ItSteals.Capture.v1." + std::to_wstring(process);
            mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name.c_str());
            mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (name + L".Lock").c_str());
            if (mapping && mutex)
                data = static_cast<uint8_t *>(
                    MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, Capacity));
            if (!data)
                Close();
            else
                pid = process;
        }
        if (!data || !Lock())
            return;
        bool valid = Read<uint32_t>(0) == Magic && Read<uint32_t>(4) == 1 && Read<uint32_t>(8) == Capacity &&
                     Read<DWORD>(12) == process;
        if (valid)
        {
            auto ack = Read<uint32_t>(20);
            if (ack && ack != acknowledged)
            {
                acknowledged = ack;
                lastAck = GetTickCount64();
            }
            if (!visible)
            {
                Write(36, 0u);
                lastAck = 0;
            }
        }
        ReleaseMutex(mutex);
        if (!valid)
            Close();
    }

    void Publish(const LayeredHud &source, bool rgba = false)
    {
        if (!enabled || !data || !source.Generation() ||
            (lastSource == &source && lastFrame == source.Generation()) || !Lock())
            return;
        const auto &alphaTable = StraightAlpha();
        uint32_t count = 0, offset = 256;
        bool valid = true;
        source.VisitPixels([&](HudRegion region, const uint8_t *pixels) {
            uint64_t size = uint64_t(region.Width()) * region.Height() * 4;
            if (count == 4 || size > Capacity - offset)
            {
                valid = false;
                return;
            }
            size_t desc = 64 + count * 24;
            Write(desc, region.left);
            Write(desc + 4, region.top);
            Write(desc + 8, region.Width());
            Write(desc + 12, region.Height());
            Write(desc + 16, offset);
            Write(desc + 20, uint32_t(size));
            // ImGui's target is premultiplied; Unity's GUI expects straight alpha.
            for (size_t i = 0; i < size; i += 4)
            {
                auto alpha = pixels[i + 3];
                if (alpha == 255 && !rgba)
                {
                    std::memcpy(data + offset + i, pixels + i, 4);
                    continue;
                }
                const auto *row = alphaTable.data() + unsigned(alpha) * 256;
                for (int c = 0; c < 3; c++)
                    data[offset + i + c] = row[pixels[i + (rgba && c != 1 ? 2 - c : c)]];
                data[offset + i + 3] = alpha;
            }
            offset += uint32_t(size);
            count++;
        });
        Write(24, source.Width());
        Write(28, source.Height());
        Write(32, count);
        Write(40, 0u);
        Write(36, valid && count ? 1u : 0u);
        auto serial = Read<uint32_t>(16) + 1;
        Write(16, serial ? serial : 1u);
        lastSource = &source;
        lastFrame = source.Generation();
        ReleaseMutex(mutex);
    }

    // Called after the one ImGui render and before Present. Even if Unity is
    // behind, no Map/readback or waiting is introduced into menu rendering.
    bool PublishGpu(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11RenderTargetView *rendered)
    {
        if (!enabled || !data || !Lock())
            return false;
        if (Read<uint32_t>(44) == 2)
        {
            ReleaseMutex(mutex);
            return false;
        }
        bool published = gpu.Publish(device, context, rendered);
        bool available = gpu.Handle() != nullptr;
        if (!available)
            Write(44, 2u); // Do not recreate shaders/resources on every failed frame.
        if (published)
        {
            Write(24, gpu.Width());
            Write(28, gpu.Height());
            Write(32, 0u);
            Write(40, 1u);
            Write(48, uint64_t(reinterpret_cast<uintptr_t>(gpu.Handle())));
            auto serial = Read<uint32_t>(16) + 1;
            Write(16, serial ? serial : 1u);
            Write(36, 1u);
        }
        ReleaseMutex(mutex);
        return available;
    }

    void PublishMenu(ID3D11Device *device, ID3D11DeviceContext *context, ID3D11RenderTargetView *rendered,
                     const ImDrawData &draw)
    {
        // Different adapters or a non-DX11 game retain the bounded CPU route.
        // This copies the existing target; it never renders the UI a second time.
        if (!enabled || !data || GetTickCount64() < nextFallback)
            return;
        nextFallback = GetTickCount64() + 16;
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        rendered->GetResource(&resource);
        if (FAILED(resource.As(&texture)))
            return;
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        if (!fallbackTarget || menuFallback.Width() != desc.Width || menuFallback.Height() != desc.Height ||
            fallbackFormat != desc.Format)
        {
            fallbackTarget.Reset();
            fallbackFormat = desc.Format;
            if (!menuFallback.Resize(device, desc.Width, desc.Height, fallbackTarget.GetAddressOf(),
                                     desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM))
                return;
        }
        context->CopyResource(menuFallback.Texture(), texture.Get());
        RECT bounds{0, 0, LONG(desc.Width), LONG(desc.Height)};
        WNDCLASSEXW wc{};
        if (menuFallback.Present(device, context, nullptr, nullptr, wc, bounds,
                                 HudContentRegions(draw, int(desc.Width), int(desc.Height)), {}, 0, false))
            Publish(menuFallback, desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM);
    }
};
