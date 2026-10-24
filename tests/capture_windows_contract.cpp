#include <windows.h>
#include <string>
#include <iostream>
#include <stdexcept>
#include <wrl/client.h>
#include "CaptureOverlay.h"

static void Check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}

int main()
{
    try
    {
        constexpr uint32_t capacity = 32 * 1024 * 1024 + 256;
        auto name = L"Local\\ItSteals.Capture.v1." + std::to_wstring(GetCurrentProcessId());
        HANDLE mapping =
            CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, capacity, name.c_str());
        HANDLE mutex = CreateMutexW(nullptr, FALSE, (name + L".Lock").c_str());
        auto *bytes = static_cast<uint8_t *>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, capacity));
        Check(mapping && mutex && bytes, "Could not create capture fixture");
        auto write = [&](size_t at, uint32_t value) { std::memcpy(bytes + at, &value, 4); };
        auto read = [&](size_t at) {
            uint32_t value;
            std::memcpy(&value, bytes + at, 4);
            return value;
        };
        write(0, 0x48505349);
        write(4, 1);
        write(8, capacity);
        write(12, GetCurrentProcessId());
        using Microsoft::WRL::ComPtr;
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> gpu;
        Check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
                                          D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                                          &device, nullptr, &gpu)),
              "WARP device creation failed");
        {
            CaptureOverlay capture;
            capture.Tick(GetCurrentProcessId(), true);
            Check(!capture.Active() && read(16) == 0, "Disabled capture must not publish");
            capture.enabled = true;
            capture.Tick(GetCurrentProcessId(), true);
            LayeredHud hud;
            ComPtr<ID3D11RenderTargetView> target;
            Check(hud.Resize(device.Get(), 4, 4, target.GetAddressOf()), "Capture target creation failed");
            const float color[4]{.25f, .125f, 0, .5f};
            gpu->ClearRenderTargetView(target.Get(), color);
            RECT client{0, 0, 4, 4};
            WNDCLASSEXW wc{};
            for (int i = 0; i < 300 && !hud.LastPresented(); i++)
            {
                Check(hud.Present(device.Get(), gpu.Get(), nullptr, nullptr, wc, client, {{0, 0, 4, 4}}, {},
                                  0, false),
                      "Real DX11 readback failed");
                Sleep(1);
            }
            Check(hud.LastPresented() != 0, "Real DX11 readback did not complete");
            capture.Publish(hud);
            auto serial = read(16);
            capture.Publish(hud);
            Check(read(16) == serial, "Unchanged readback must not copy or publish again");
            Check(read(24) == 4 && read(28) == 4 && read(32) == 1 && read(36) == 1,
                  "Capture source dimensions and region header differ");
            Check(read(64) == 0 && read(68) == 0 && read(72) == 4 && read(76) == 4 && read(84) == 64,
                  "Capture region placement or length differs");
            uint32_t offset = read(80);
            Check(offset >= 256 && offset + 64 <= capacity, "Capture payload is outside mapping");
            for (size_t i = 0; i < 64; i += 4)
                Check(bytes[offset + i] == 0 && bytes[offset + i + 1] == 64 && bytes[offset + i + 2] == 128 &&
                          bytes[offset + i + 3] == 128,
                      "BGRA / straight alpha conversion differs");
            Check(!capture.Active(), "Native overlay must stay visible before game acknowledgement");
            write(20, read(16));
            capture.Tick(GetCurrentProcessId(), true);
            Check(capture.Active(), "Presented game frame was not acknowledged");
            capture.Tick(GetCurrentProcessId(), false);
            Check(!capture.Active() && read(36) == 0, "Alt-Tab must clear capture immediately");
            capture.enabled = false;
            capture.Tick(GetCurrentProcessId(), true);
            Check(!capture.Active(), "Disabling capture did not restore the desktop path");
        }
        UnmapViewOfFile(bytes);
        CloseHandle(mutex);
        CloseHandle(mapping);
        std::cout << "Windows DX11 capture: real GPU readback, BGRA/alpha, bounded shared memory,"
                     " acknowledgement, Alt-Tab and fallback: PASS\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
