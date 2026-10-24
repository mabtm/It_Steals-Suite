// The production adapter is executed unchanged against typed Win32/D3D11
// doubles. This verifies calls/queue/alpha, not an actual Windows driver.
#include "LayeredHud.h"
#include <iostream>

static uint32_t Pixel(int x, int y)
{
    return 0x40000000u | (uint32_t(y) << 12) | uint32_t(x);
}

int main()
{
    ID3D11Device device;
    ID3D11DeviceContext context;
    WNDCLASSEXW wc{};
    wc.lpszClassName = L"test";
    auto game = CreateWindowExW(0, L"", L"", 0, 0, 0, 800, 600, nullptr, nullptr, nullptr, nullptr);
    auto main = CreateWindowExW(WS_EX_LAYERED, L"", L"", 0, 0, 0, 800, 600, game, nullptr, nullptr, nullptr);
    RECT client{-1400, 80, -600, 680};
    LayeredHud hud;
    ID3D11RenderTargetView *view = nullptr;
    assert(hud.Resize(&device, 800, 600, &view));
    auto color = view->texture;
    for (UINT y = 0; y < 600; y++)
        for (UINT x = 0; x < 800; x++)
        {
            auto p = Pixel(x, y);
            std::memcpy(color->pixels.data() + y * color->pitch + x * 4, &p, 4);
        }
    std::vector<HudRegion> regions{{16, 16, 144, 112}, {600, 400, 792, 592}};
    HudRegion badge{640, 420, 700, 445};
    assert(hud.Present(&device, &context, main, game, wc, client, regions, badge));
    assert(hud.SurfaceCount() == 2 && context.copies == 2 && context.flushes == 1 &&
           hud.LastPresented() == 0 && hud.Generation() == 0 && main->uploads == 0);
    auto aux = fakeWindows.back().get();
    assert(aux != main && aux->owner == game &&
           aux->styles == (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW));
    fakeClock = 110;
    assert(hud.Present(&device, &context, main, game, wc, client, regions, badge));
    assert(main->uploads == 1 && aux->uploads == 1 && hud.LastPresented() == 110 &&
           hud.LastBadgePresented() == 110 && main->blend == 255 && aux->blend == 255);
    assert(hud.Generation() == 2);
    assert(main->pixels.front() == Pixel(16, 16) && main->pixels[127] == Pixel(143, 16) &&
           main->pixels[128] == Pixel(16, 17) && main->pixels.back() == Pixel(143, 111));
    assert(aux->pixels.front() == Pixel(600, 400) && aux->pixels.back() == Pixel(791, 591));
    {
        LayeredHud capture;
        int references = color->references;
        assert(!capture.UseTexture(nullptr) && capture.UseTexture(color));
        assert(color->references == references + 1 && capture.Width() == 800 && capture.Height() == 600);
        assert(capture.Present(&device, &context, nullptr, nullptr, wc, client, {regions[0]}, {}, 0, false));
        assert(capture.Present(&device, &context, nullptr, nullptr, wc, client, {regions[0]}, {}, 0, false));
        auto generation = capture.Generation();
        assert(generation && capture.UseTexture(color) && capture.Generation() == generation &&
               color->references == references + 1);
        capture.Reset();
        assert(color->references == references && capture.Generation() == 0);
    }
    assert(hud.ReadbackBytes() == 128ull * 96 * 4 + 192ull * 192 * 4);
    assert(main->rect.left == client.left + 16 && main->rect.top == client.top + 16 &&
           aux->rect.left == client.left + 600);
    // Pending readbacks for the badge panel stay busy; a fresh minimap must
    // not masquerade as a newly presented VALID badge.
    auto aux0 = device.created[3], aux1 = device.created[4];
    aux0->busy = aux1->busy = true;
    fakeClock = 120;
    assert(hud.Present(&device, &context, main, game, wc, client, regions, badge));
    assert(hud.LastPresented() == 120 && hud.LastBadgePresented() == 110);
    fakeClock = 130;
    assert(hud.Present(&device, &context, main, game, wc, client, regions, badge));
    assert(hud.LastBadgePresented() == 110 && aux->uploads == 1);
    aux0->busy = aux1->busy = false;
    // Position metadata belongs to the queued frame, including equal-sized
    // moved regions. Follow the current game origin at upload time.
    auto moved = regions;
    moved[1] = {580, 380, 772, 572};
    client = {-1500, -60, -700, 540};
    fakeClock = 140;
    assert(hud.Present(&device, &context, main, game, wc, client, moved, badge));
    assert(aux->rect.left == client.left + 600 && aux->pixels.front() == Pixel(600, 400));
    fakeClock = 150;
    assert(hud.Present(&device, &context, main, game, wc, client, moved, badge));
    fakeClock = 160;
    assert(hud.Present(&device, &context, main, game, wc, client, moved, badge));
    assert(aux->rect.left == client.left + 580 && aux->pixels.front() == Pixel(580, 380));
    hud.SyncVisibility(false, client);
    assert(!aux->visible);
    client.left += 80;
    client.right += 80;
    hud.SyncVisibility(true, client);
    assert(aux->visible && aux->rect.left == client.left + 580);
    assert(hud.Present(&device, &context, main, game, wc, client, {regions[0]}, {}));
    assert(!aux->alive && hud.SurfaceCount() == 1);
    view->Release();
    view = nullptr;
    assert(hud.Resize(&device, 320, 200, &view) && hud.Width() == 320 && hud.Height() == 200 &&
           hud.LastPresented() == 0);
    assert(fakeTextureCount == 1);
    assert(hud.Present(&device, &context, main, game, wc, client, {{0, 0, 1, 1}}, {}));
    assert(hud.ReadbackBytes() == 4);
    hud.Reset();
    hud.Reset();
    view->Release();
    assert(fakeTextureCount == 0 && IsWindow(main) && IsWindow(game));
    std::cout
        << "Production LayeredHud: cropped CopySubresourceRegion, padded row pitch, per-pixel alpha, two nonblocking readback slots, stale badge exclusion, queued crop coordinates, negative/moving origins, owned NoActivate surfaces, hiding/shrink/resize and repeat cleanup: PASS\n";
}
