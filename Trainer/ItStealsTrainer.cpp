#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <tlhelp32.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <cmath>
#include <string>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <chrono>
#include <sstream>
#include "GameWindowBinding.h"
#include "TrainerSettings.h"
#include "HudEditorModel.h"
#include "TrainerTray.h"
#include "GameLaunch.h"
#include "SuiteUpdater.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#ifdef _MSC_VER
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

#include "SharedProtocol.h"
#include "MinimapGeometry.h"
#include "TelemetryPresentation.h"
#include "GraphicsComposition.h"
#include "PracticeMapClient.h"
#include "InspectorClient.h"
#include "SeedSearch.h"
#include "MovementTelemetry.h"
#include "PerformanceTiming.h"
#include "SuiteExperience.h"
#include "InterfaceAudio.h"
#include "DiagnosticScan.h"
#include "RunExperienceModel.h"
#include "IntroBackdrop.h"
#include "RunReviewState.h"
#include "ReplayArchive.h"
#include "Replay3DControls.h"

bool g_autoStartGame = true;
bool g_captureOverlayEnabled = false;
std::string g_gameLaunchStatus = "Not requested";
bool g_overlayVisible = false;
bool g_allOverlayEnabled = true, g_quit = false, g_resetLayout = false;
bool g_softwareRenderer = false;
bool g_alphaCompositing = false;
GraphicsComposition g_composition;
IntroBackdrop g_introBackdrop;
TelemetryPresentation g_presentation;
MinimapOptions g_mapOptions;
MapTrail g_trail;
bool g_reportsEnabled = false;
double g_snapshotReceived = 0;
ImFont *g_timerFont = nullptr;
OverlayLayout g_hudLayout{false}, g_speedrunLayout{true};
int g_renderFps = 60;
float g_dpiScale = 1;
std::string g_uiFontName = "Dear ImGui fallback";
UINT g_resizeWidth = 0, g_resizeHeight = 0;
uint32_t g_ackSerial = 0;
DWORD g_gamePid = 0;
unsigned g_hotkeyEvents = 0, g_hotkeysRegistered = 0, g_hotkeysAttempted = 0;
GameWindowBinding *g_binding = nullptr;
TrainerSettings g_settings;
bool g_overlayInteractive = false;
bool g_surfaceInteractive = false;
#include "LayeredHud.h"
LayeredHud g_layeredHud;
#include "CaptureOverlay.h"
CaptureOverlay g_captureOverlay;
InterfaceAudio g_interfaceAudio;
bool g_settingsFeedbackPending = false;
FrameCost g_menuFrameCost, g_hudFrameCost;
uint64_t g_lastHudBytes = 0;
size_t g_lastHudSurfaces = 0;
double g_lastHudUpload = 0;
bool g_validityBadgeDrawn = false;
ULONGLONG g_badgePresentedTick = 0;
int g_frameLimitMode = 0, g_gameFrameLimit = 240, g_vSyncMode = 0;
bool g_showMinimapWindow = true;
HWND g_hWnd = nullptr;
HWND g_gameWnd = nullptr;
WNDCLASSEXW g_wc = {};

ID3D11Device *g_pd3dDevice = nullptr;
ID3D11DeviceContext *g_pd3dDeviceContext = nullptr;
IDXGISwapChain *g_pSwapChain = nullptr;
ID3D11RenderTargetView *g_mainRenderTargetView = nullptr;

HANDLE g_hMapFile = nullptr;
HANDLE g_hMutex = nullptr;
byte *g_mapping = nullptr;
GameData g_snapshot{};
// UI reads only this local snapshot; no renderer touches mapped memory.
GameData *g_pShared = nullptr;
TrainerCommands g_commands{};
uint32_t g_sentSerial = 0, g_snapshotId = 0, g_session = 0;
bool g_handshake = false, g_hasSnapshot = false;
TrainerCommands g_lastWrittenCommands{};
bool g_hasWrittenCommands = false;
ULONGLONG g_lastHeartbeatWrite = 0;
ULONGLONG g_lastSnapshotTick = 0, g_nextConnectTick = 0;
bool g_seededPractice = false;
bool g_rawPracticeSpawn = false, g_plannerTabActive = false;
int g_plannerAutoLevel = -1;
PlannerPreferences g_plannerOptions;
practice::MapWorker g_mapWorker;
PracticeMapClient g_mapClient;
InspectorClient g_inspectorClient;
practice::SeedSearch g_seedSearch;
std::string g_plannerRandomError;
std::string g_connectionStatus = "Waiting for ItStealsHelper";

bool g_compactMode = false;
bool g_showPlayerPanel = true, g_showCrystalPanel = true, g_showMonsterPanel = true;
bool g_showVisualsPanel = true;
ImVec4 g_mainBgCol = ImVec4(0.065f, 0.075f, 0.095f, 1.0f);
ImVec4 g_textCol = ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
ImVec4 g_headerCol = ImVec4(0.34f, 0.70f, 0.95f, 1.0f);
ImVec4 g_accentCol = ImVec4(1.0f, 0.64f, 0.29f, 1.0f);
float g_minimapOpacity = 1.0f;
int g_minimapWidth = 250, g_minimapHeight = 250;
static bool minimapLockAspect = true;
static bool g_showPath = false;

// Cheat toggles (global for resetting when practice mode is disabled)
bool g_brightnessOn = false;

bool g_hitboxCheck = false;
bool g_hidePlayerHitbox = false;

// Speedrun globals
bool g_speedrunMode = false;
bool g_practiceMode = false;
int g_selectedSpeedrunLevel = 0;
int g_practiceSeed = 0;

inline bool UpdateInputRequested()
{
    const auto &p = suite_update::service.presentation;
    return (p.changelog || p.prompt ||
            suite_update::service.View()->phase == suite_update::Phase::Installing) &&
           !(g_pShared && g_pShared->timerState == 1);
}

bool OverlayRequested()
{
    bool menu = g_overlayVisible && !g_compactMode;
    bool hud = g_overlayVisible && g_compactMode && g_hudLayout.enabled &&
               CompactHudAllowed(g_speedrunMode, g_practiceMode);
    bool map = g_showMinimapWindow && !(g_speedrunMode && !g_practiceMode);
    bool result = suite_run::presentation.WantsInput(g_pShared, g_runReviewOpen);
    bool startup = suite_ui::lifecycle.WantsStartup(suite_ui::LifecycleNow(), suite_ui::preferences,
                                                    (g_pShared && g_pShared->timerState == 1) || result);
    bool replay = g_pShared && g_pShared->replay3dState >= 1 && g_pShared->replay3dState <= 3;
    return UpdateInputRequested() ||
           suite_update::service.View()->phase == suite_update::Phase::Downloading || inspector::picking ||
           (inspector::dock && g_allOverlayEnabled) || replay || result || startup ||
           suite_ui::LifecycleNow() - suite_run::presentation.toastAt < 3 || suite_ui::lifecycle.Exiting() ||
           (g_allOverlayEnabled && (menu || hud || map)) ||
           SpeedrunHudAllowed(g_speedrunMode, g_practiceMode);
}

inline bool ClipInputRequested();

bool MenuInputRequested()
{
    return UpdateInputRequested() || inspector::picking || inspector::dockInteractive || suite_hud::active ||
           ClipInputRequested() || suite_replay::ControlsRequested(g_pShared) ||
           suite_run::presentation.WantsInput(g_pShared, g_runReviewOpen) ||
           (g_allOverlayEnabled && g_overlayVisible && !g_compactMode);
}

void UpdateOverlayStyle()
{
    if (!g_hWnd || !IsWindow(g_hWnd))
        return;
    LONG_PTR style = GetWindowLongPtrW(g_hWnd, GWL_EXSTYLE);
    style |= WS_EX_TOOLWINDOW;
    if (g_alphaCompositing)
    {
        style |= WS_EX_NOREDIRECTIONBITMAP;
        style &= ~WS_EX_LAYERED;
    }
    else
    {
        style |= WS_EX_LAYERED;
        style &= ~WS_EX_NOREDIRECTIONBITMAP;
    }
    if (g_overlayInteractive)
        style &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    else
        style |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    if (style != GetWindowLongPtrW(g_hWnd, GWL_EXSTYLE))
        SetWindowLongPtrW(g_hWnd, GWL_EXSTYLE, style);
    // A disabled fullscreen popup swallows clicks; it is not a pass-through.
    // HUD surfaces use LAYERED + TRANSPARENT across process/thread boundaries.
    // Hide an outgoing composition menu until its layered HUD is ready.
    if (g_alphaCompositing && !g_overlayInteractive)
        ShowWindow(g_hWnd, SW_HIDE);
}

void SetOverlayInteractive(bool interactive, bool userAction = true)
{
    bool wasForeground = GetForegroundWindow() == g_hWnd;
    g_overlayInteractive =
        !suite_ui::lifecycle.Exiting() &&
        (interactive || UpdateInputRequested() || inspector::picking || inspector::dockInteractive ||
         suite_hud::active || ClipInputRequested() || suite_replay::controls ||
         suite_run::presentation.WantsInput(g_pShared, g_runReviewOpen)) &&
        MenuInputRequested();
    interactive = g_overlayInteractive;
    if (ImGui::GetCurrentContext())
    {
        ImGuiIO &io = ImGui::GetIO();
        if (!interactive)
        {
            io.MouseDrawCursor = false;
            io.ClearInputKeys();
        }
    }
    UpdateOverlayStyle();
    if (!userAction || !g_binding)
        return;
    g_binding->Refresh(OverlayRequested());
    if (interactive && g_binding->state.ShouldShow())
    {
        ClipCursor(nullptr);
        SetForegroundWindow(g_hWnd);
        SetFocus(g_hWnd);
    }
    else if (wasForeground && IsWindow(g_gameWnd))
    {
        if (GetCapture() == g_hWnd)
            ReleaseCapture();
        SetForegroundWindow(g_gameWnd);
    }
    g_binding->Invalidate();
}

DWORD GetProcessIdByName(const wchar_t *name)
{
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return 0;
    if (Process32FirstW(snapshot, &entry))
    {
        do
        {
            if (_wcsicmp(entry.szExeFile, name) == 0)
            {
                CloseHandle(snapshot);
                return entry.th32ProcessID;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return 0;
}

bool StartInstalledGame()
{
    try
    {
        if (GetProcessIdByName(L"It Steals.exe"))
        {
            g_gameLaunchStatus = "Game is already running";
            return true;
        }
        auto location = game_launch::Resolve(game_launch::ApplicationDirectory(), game_launch::SteamRoots());
        if (location.game.empty())
        {
            g_gameLaunchStatus = "Game not found. Run Suite Setup or configure installation.ini";
            return false;
        }
        if (GetProcessIdByName(L"It Steals.exe"))
            return true;
        DWORD error = 0;
        if (!game_launch::Start(location, error))
        {
            g_gameLaunchStatus = "Game launch failed (Windows error " + std::to_string(error) + ")";
            return false;
        }
        g_gameLaunchStatus =
            location.steam.empty() ? "Started configured game" : "Requested game through Steam";
        return true;
    }
    catch (const std::exception &)
    {
        g_gameLaunchStatus = "Game location could not be read. Run Suite Setup again";
        return false;
    }
}

void CleanupDeviceD3D();

bool CreateDeviceD3D(HWND hWnd, bool interactive)
{
    g_softwareRenderer = false;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    if (!interactive)
    {
        D3D_FEATURE_LEVEL actual;
        HRESULT hr =
            D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                              levels, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &actual, &g_pd3dDeviceContext);
        if (FAILED(hr))
        {
            CleanupDeviceD3D();
            hr =
                D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                  levels, 2, D3D11_SDK_VERSION, &g_pd3dDevice, &actual, &g_pd3dDeviceContext);
            g_softwareRenderer = true;
        }
        RECT rect{};
        GetClientRect(hWnd, &rect);
        if (FAILED(hr) ||
            !g_layeredHud.Resize(g_pd3dDevice, rect.right, rect.bottom, &g_mainRenderTargetView))
        {
            CleanupDeviceD3D();
            return false;
        }
        return true;
    }
    // DirectComposition has no per-pixel cross-process hit testing. Keep it for
    // interactive menus; every passive surface uses the proven layered path.
    if (interactive)
    {
        D3D_FEATURE_LEVEL compositionLevel;
        HRESULT composed = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                             D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 2, D3D11_SDK_VERSION,
                                             &g_pd3dDevice, &compositionLevel, &g_pd3dDeviceContext);
        RECT client{};
        GetClientRect(hWnd, &client);
        if (SUCCEEDED(composed) && g_composition.Attach(g_pd3dDevice, hWnd, static_cast<UINT>(client.right),
                                                        static_cast<UINT>(client.bottom), &g_pSwapChain))
        {
            ID3D11Texture2D *buffer = nullptr;
            g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&buffer));
            if (buffer)
            {
                g_pd3dDevice->CreateRenderTargetView(buffer, nullptr, &g_mainRenderTargetView);
                buffer->Release();
            }
            if (g_mainRenderTargetView)
            {
                g_alphaCompositing = true;
                return true;
            }
        }
    }
    CleanupDeviceD3D();
    LONG_PTR fallback = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
    SetWindowLongPtrW(hWnd, GWL_EXSTYLE, (fallback & ~WS_EX_NOREDIRECTIONBITMAP) | WS_EX_LAYERED);
    if (!SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY))
        return false;
    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = 0; // The overlay never switches display modes.
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL lvl[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL fl;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, lvl, 2,
                                               D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &fl,
                                               &g_pd3dDeviceContext);
    if (FAILED(hr))
    {
        CleanupDeviceD3D();
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, lvl, 2,
                                           D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &fl,
                                           &g_pd3dDeviceContext);
        g_softwareRenderer = SUCCEEDED(hr);
    }
    if (FAILED(hr))
    {
        CleanupDeviceD3D();
        return false;
    }
    IDXGIFactory *factory = nullptr;
    if (SUCCEEDED(g_pSwapChain->GetParent(IID_PPV_ARGS(&factory))))
    {
        factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
        factory->Release();
    }

    ID3D11Texture2D *pBB = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBB));
    if (pBB)
    {
        g_pd3dDevice->CreateRenderTargetView(pBB, nullptr, &g_mainRenderTargetView);
        pBB->Release();
    }
    return g_mainRenderTargetView != nullptr;
}

void CleanupDeviceD3D()
{
    g_captureOverlay.ResetGraphics();
    suite_ui::introBackdrop = 0;
    g_introBackdrop.Reset();
    g_composition.Reset();
    g_layeredHud.Reset();
    g_alphaCompositing = false;
    if (g_mainRenderTargetView)
    {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
    if (g_pSwapChain)
    {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext)
    {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice)
    {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

void CreateRenderTarget()
{
    if (!g_pSwapChain)
        return;
    ID3D11Texture2D *pBB = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBB));
    if (pBB)
    {
        g_pd3dDevice->CreateRenderTargetView(pBB, nullptr, &g_mainRenderTargetView);
        pBB->Release();
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_LBUTTONDOWN && inspector::picking && IsWindow(g_gameWnd) && !inspector::clickPointer &&
        !inspector::nativeClick)
    {
        POINT at{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
        RECT client{};
        if (ClientToScreen(hWnd, &at) && ScreenToClient(g_gameWnd, &at) && GetClientRect(g_gameWnd, &client))
            if (auto p = inspector::NormalizePointer(at.x, at.y, 0, 0, client.right - client.left,
                                                     client.bottom - client.top))
                inspector::nativeClick = *p;
    }
    if (g_hWnd && hWnd != g_hWnd)
    {
        if (msg == WM_NCHITTEST)
            return HTTRANSPARENT;
        if (msg == WM_MOUSEACTIVATE)
            return MA_NOACTIVATE;
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    if (msg == WM_HOTKEY && wParam >= 1 && wParam <= 8)
    {
        g_hotkeyEvents |= 1u << (wParam - 1);
        return 0;
    }
    if (msg == WM_SIZE && wParam != SIZE_MINIMIZED && g_surfaceInteractive)
    {
        // Resize outside the callback. SetWindowPos and DPI messages can reenter.
        g_resizeWidth = LOWORD(lParam);
        g_resizeHeight = HIWORD(lParam);
        return 0;
    }
    if (msg == WM_CLOSE)
    {
        g_quit = true;
        return 0;
    }
    if (msg == WM_DESTROY)
        return 0; // Owner recreation is handled by the main loop.
    if (msg == WM_ACTIVATEAPP && !wParam)
    {
        g_interfaceAudio.Policy(false, suite_ui::preferences.volume);
        if (GetCapture() == hWnd)
            ReleaseCapture();
        if (g_binding)
            g_binding->Invalidate();
    }
    if (msg == WM_DPICHANGED || msg == WM_DISPLAYCHANGE)
    {
        if (g_binding)
            g_binding->Invalidate();
        return 0; // Client geometry comes exclusively from the game window.
    }
    if (msg == WM_SYSCOMMAND && (wParam & 0xfff0) == SC_KEYMENU)
        return 0;
    if (msg == WM_NCHITTEST)
        return g_overlayInteractive ? HTCLIENT : HTTRANSPARENT;
    if (msg == WM_MOUSEACTIVATE)
        return g_overlayInteractive ? MA_ACTIVATE : MA_NOACTIVATE;
    if (msg == WM_SETCURSOR && g_overlayInteractive && GetForegroundWindow() == hWnd)
    {
        SetCursor(nullptr);
        return TRUE;
    }
    if (ImGui::GetCurrentContext() && ImGui::GetIO().BackendPlatformUserData &&
        ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return 1;
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

class SharedLock
{
    HANDLE handle;

  public:
    bool acquired, abandoned;

    explicit SharedLock(HANDLE value) : handle(value), acquired(false), abandoned(false)
    {
        if (!handle)
            return;
        DWORD result = WaitForSingleObject(handle, 0);
        acquired = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
        abandoned = result == WAIT_ABANDONED;
    }

    SharedLock(const SharedLock &) = delete;
    SharedLock &operator=(const SharedLock &) = delete;

    ~SharedLock()
    {
        if (acquired)
            ReleaseMutex(handle);
    }
};

void CloseSharedMemory()
{
    inspector::Close();
    g_inspectorClient.Close();
    inspector::Bind(inspector::Identity(0, 0, -1, 0));
    g_seedSearch.Stop();
    g_seedSearch.HidePreview();
    g_mapClient.Close();
    g_mapWorker.InvalidateLive();
    g_plannerAutoLevel = -1;
    g_trail.Clear();
    g_presentation.Reset();
    g_pShared = nullptr;
    g_handshake = false;
    g_hasSnapshot = false;
    g_hasWrittenCommands = false;
    g_lastHeartbeatWrite = 0;
    if (g_mapping)
    {
        UnmapViewOfFile(g_mapping);
        g_mapping = nullptr;
    }
    if (g_hMapFile)
    {
        CloseHandle(g_hMapFile);
        g_hMapFile = nullptr;
    }
    if (g_hMutex)
    {
        CloseHandle(g_hMutex);
        g_hMutex = nullptr;
    }
}

bool OpenSharedMemory()
{
    CloseSharedMemory();
    g_hMapFile = OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, kMapName);
    if (!g_hMapFile)
        return false;
    g_hMutex = OpenMutexA(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, kMutexName);
    g_mapping =
        static_cast<byte *>(MapViewOfFile(g_hMapFile, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, kMappingSize));
    if (!g_hMutex || !g_mapping)
    {
        CloseSharedMemory();
        return false;
    }
    g_lastSnapshotTick = GetTickCount64();
    // PullSnapshot adopts the session before writing any control state.
    // Reconnecting must not replay actions from an older helper session.
    return true;
}

bool Compatible(const SharedHeader &header)
{
    return header.magic == kMagic && header.version == kProtocolVersion &&
           header.payloadSize == sizeof(GameData) && header.commandSize == sizeof(TrainerCommands);
}

void PullSnapshot()
{
    ULONGLONG now = GetTickCount64();
    if (!g_gamePid)
    {
        g_pShared = nullptr;
        g_connectionStatus = "Waiting for It Steals (automatic reconnect)";
        return;
    }
    if (!g_mapping && now >= g_nextConnectTick)
    {
        OpenSharedMemory();
        g_nextConnectTick = now + 1000;
    }
    if (!g_mapping)
    {
        g_pShared = nullptr;
        g_connectionStatus = "Waiting for ItStealsHelper";
        return;
    }
    SharedLock lock(g_hMutex);
    if (lock.acquired && !lock.abandoned)
    {
        SharedHeader header{};
        memcpy(&header, g_mapping, sizeof(header));
        g_ackSerial = header.commandAck;
        if (!Compatible(header))
        {
            g_pShared = nullptr;
            g_connectionStatus = "Mod/trainer version mismatch";
            return;
        }
        if (header.pid != g_gamePid)
        {
            g_pShared = nullptr;
            g_connectionStatus = "Channel belongs to another game instance";
            return;
        }
        if (!g_handshake || g_session != header.session)
        {
            if (g_session && g_session != header.session)
            {
                g_trail.Clear();
                g_commands = {};
                g_commands.telemetryOptions = kTelemetryConfigured;
                g_commands.reportOptions = g_reportsEnabled ? 1u : 0u;
                g_commands.replayMinSeconds = suite_ui::replayPreferences.seconds;
                g_commands.replayMinCrystals = suite_ui::replayPreferences.mode == 1
                                                   ? uint32_t(suite_ui::replayPreferences.crystals)
                                                   : 0;
                g_brightnessOn = g_hitboxCheck = g_hidePlayerHitbox = false;
                g_rawPracticeSpawn = false;
                g_mapClient.Close();
                g_mapWorker.InvalidateLive();
                g_plannerAutoLevel = -1;
                g_seedSearch.Stop();
                g_seedSearch.HidePreview();
                g_speedrunMode = g_practiceMode = g_seededPractice = false;
            }
            g_sentSerial = header.commandAck;
            g_ackSerial = header.commandAck;
            g_commands.serial = g_sentSerial;
            memcpy(g_mapping + kCommandOffset, &g_commands, sizeof(g_commands));
            g_handshake = true;
            g_hasSnapshot = false;
            g_session = header.session;
            g_presentation.Reset();
            g_hasWrittenCommands = false;
        }
        if (!header.online)
        {
            g_pShared = nullptr;
            g_connectionStatus = "Waiting for a complete snapshot";
            return;
        }
        if (header.snapshotId != g_snapshotId || !g_hasSnapshot)
        {
            memcpy(&g_snapshot, g_mapping + kPayloadOffset, sizeof(g_snapshot));
            if (g_snapshot.pid != static_cast<int>(header.pid))
            {
                g_pShared = nullptr;
                g_hasSnapshot = false;
                g_connectionStatus = "Snapshot process identity mismatch";
                return;
            }
            SanitizeSnapshot(g_snapshot);
            ObserveMovementSnapshot(g_snapshot);
            g_pShared = &g_snapshot;
            g_hasSnapshot = true;
            g_snapshotReceived =
                std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
            g_presentation.Push(g_snapshot, header.gameTime, g_snapshotReceived);
            if (g_mapOptions.trail && (g_commands.telemetryOptions & kTelemetryMap))
                g_trail.Push(g_snapshot, g_session, header.gameTime);
            else
                g_trail.Clear();
            g_snapshotId = header.snapshotId;
            g_lastSnapshotTick = now;
            g_connectionStatus = "Connected";
        }
    }
    else if (lock.abandoned)
    {
        // A process died while copying; wait for a new complete publication.
        g_pShared = nullptr;
        g_connectionStatus = "Waiting for snapshot recovery";
        reinterpret_cast<SharedHeader *>(g_mapping)->online = 0;
    }
    if (now - g_lastSnapshotTick > 5000)
    {
        g_pShared = nullptr;
        g_connectionStatus = "Game telemetry paused";
    }
}

void FlushCommands()
{
    if (suite_ui::lifecycle.Exiting())
        return;
    if (!g_mapping)
        return;
    ULONGLONG now = GetTickCount64();
    bool heartbeatDue = !g_hasWrittenCommands || now - g_lastHeartbeatWrite >= 500;
    bool changed =
        !g_hasWrittenCommands || memcmp(&g_commands, &g_lastWrittenCommands, sizeof(g_commands)) != 0;
    if (!heartbeatDue && !changed)
        return;
    SharedLock lock(g_hMutex);
    if (!lock.acquired || lock.abandoned)
        return;
    SharedHeader header{};
    memcpy(&header, g_mapping, sizeof(header));
    if (!Compatible(header) || header.pid != g_gamePid || !g_handshake || g_session != header.session)
        return;
    g_ackSerial = header.commandAck;
    // A request stays in mapped memory until Unity acknowledges dequeuing it.
    // New UI pulses remain locally queued instead of overwriting that request.
    if (header.commandAck != g_sentSerial)
    {
        if (heartbeatDue)
        {
            ++g_commands.heartbeat;
            memcpy(g_mapping + kCommandOffset + offsetof(TrainerCommands, heartbeat), &g_commands.heartbeat,
                   sizeof(uint32_t));
            g_lastWrittenCommands.heartbeat = g_commands.heartbeat;
            g_lastHeartbeatWrite = now;
        }
        return;
    }
    bool request = g_commands.replay3dCmd || g_commands.runReviewRequest || g_commands.levelLoadCmd ||
                   g_commands.speedrunLoadLevelCmd || g_commands.setSeedCmd || g_commands.teleportCmd ||
                   g_commands.resetMapCmd || g_commands.windowModeCmd || g_commands.plannerRequest;
    if (request)
        ++g_sentSerial;
    g_commands.serial = g_sentSerial;
    ++g_commands.heartbeat;
    memcpy(g_mapping + kCommandOffset, &g_commands, sizeof(g_commands));
    g_lastWrittenCommands = g_commands;
    g_hasWrittenCommands = true;
    g_lastHeartbeatWrite = now;
    g_commands.levelLoadCmd = g_commands.speedrunLoadLevelCmd = g_commands.setSeedCmd =
        g_commands.teleportCmd = g_commands.resetMapCmd = 0;
    g_commands.windowModeCmd = 0;
    g_commands.plannerRequest = 0;
    g_commands.runReviewRequest = 0;
    g_commands.replay3dCmd = 0;
}

double SnapshotAge();
bool LivePlannerRoute(const practice::MapAnalysis &analysis);
std::shared_ptr<const practice::MapAnalysis> DisplayedPlanner(int level);
#include "MinimapUI.h"

bool HasPendingRequest()
{
    return g_sentSerial != g_ackSerial || g_commands.replay3dCmd || g_commands.levelLoadCmd ||
           g_commands.speedrunLoadLevelCmd || g_commands.setSeedCmd || g_commands.teleportCmd ||
           g_commands.resetMapCmd || g_commands.windowModeCmd || g_commands.plannerRequest;
}

TrainerPreferences CollectPreferences()
{
    TrainerPreferences p;
    p.inspector = inspector_fx::preferences;
    p.input = suite_input::preferences;
    p.replay = suite_ui::replayPreferences;
    p.experience = suite_ui::preferences;
    p.appearance = suite_ui::appearance;
    p.hud = g_hudLayout;
    p.speedrun = g_speedrunLayout;
    p.map = g_mapOptions;
    p.reports = g_reportsEnabled;
    p.autoStartGame = g_autoStartGame;
    p.captureOverlay = g_captureOverlayEnabled;
    p.planner = g_plannerOptions;
    p.compact = g_compactMode;
    p.minimap = g_showMinimapWindow;
    p.path = g_showPath;
    p.lockAspect = minimapLockAspect;
    p.player = g_showPlayerPanel;
    p.crystals = g_showCrystalPanel;
    p.monster = g_showMonsterPanel;
    p.visuals = g_showVisualsPanel;
    p.width = g_minimapWidth;
    p.height = g_minimapHeight;
    p.opacity = g_minimapOpacity;
    p.frameLimitMode = g_frameLimitMode;
    p.gameFrameLimit = g_gameFrameLimit;
    p.vSyncMode = g_vSyncMode;
    p.fps = g_renderFps;
    p.level = g_selectedSpeedrunLevel;
    p.seed = g_practiceSeed;
    p.rawSpawn = g_rawPracticeSpawn;
    memcpy(p.background, &g_mainBgCol.x, sizeof(p.background));
    memcpy(p.text, &g_textCol.x, sizeof(p.text));
    memcpy(p.header, &g_headerCol.x, sizeof(p.header));
    memcpy(p.accent, &g_accentCol.x, sizeof(p.accent));
    p.Validate();
    return p;
}

void ApplyPreferences(TrainerPreferences p)
{
    p.Validate();
    inspector_fx::preferences = p.inspector;
    suite_input::preferences = p.input;
    suite_ui::replayPreferences = p.replay;
    suite_ui::preferences = p.experience;
    suite_ui::fontsDirty = true;
    suite_ui::appearance = p.appearance;
    g_hudLayout = p.hud;
    g_speedrunLayout = p.speedrun;
    g_mapOptions = p.map;
    g_reportsEnabled = p.reports;
    g_autoStartGame = p.autoStartGame;
    g_captureOverlayEnabled = p.captureOverlay;
    g_plannerOptions = p.planner;
    g_compactMode = p.compact;
    g_showMinimapWindow = p.minimap;
    g_showPath = p.path;
    minimapLockAspect = p.lockAspect;
    g_showPlayerPanel = p.player;
    g_showCrystalPanel = p.crystals;
    g_showMonsterPanel = p.monster;
    g_showVisualsPanel = p.visuals;
    g_minimapWidth = p.width;
    g_minimapHeight = p.height;
    g_minimapOpacity = p.opacity;
    g_frameLimitMode = p.frameLimitMode;
    g_gameFrameLimit = p.gameFrameLimit;
    g_vSyncMode = p.vSyncMode;
    g_renderFps = p.fps;
    g_selectedSpeedrunLevel = p.level;
    g_practiceSeed = p.seed;
    g_rawPracticeSpawn = p.rawSpawn;
    g_mainBgCol = ImVec4(p.background[0], p.background[1], p.background[2], 1);
    g_textCol = ImVec4(p.text[0], p.text[1], p.text[2], p.text[3]);
    g_headerCol = ImVec4(p.header[0], p.header[1], p.header[2], p.header[3]);
    g_accentCol = ImVec4(p.accent[0], p.accent[1], p.accent[2], p.accent[3]);
    if (g_hWnd)
        SetOverlayInteractive(g_overlayVisible && !g_compactMode, false);
}

void ClearTrainerCommands();

void SavePreferencesNow(bool feedback = false)
{
    bool queued = g_settings.Save(CollectPreferences());
    if (feedback)
    {
        g_settingsFeedbackPending = queued && g_settings.Pending();
        if (!queued)
            suite_ui::Notify("Settings could not be saved", true);
        else if (!g_settingsFeedbackPending)
            suite_ui::Notify("Settings saved");
        else
            suite_ui::Notify("Saving settings...", false, 0, suite_ui::Tone::None);
    }
}

inline bool ActiveSpeedrun()
{
    return g_pShared && g_pShared->timerState == 1 && (g_speedrunMode || g_pShared->speedrunModeActive);
}

void PollExperienceFeedback()
{
    g_settings.Poll();
    if (g_settingsFeedbackPending && !g_settings.Pending())
    {
        g_settingsFeedbackPending = false;
        suite_ui::Notify(g_settings.error.empty() ? "Settings saved" : "Settings could not be saved",
                         !g_settings.error.empty());
    }
    static uint64_t saved = 0, finished = 0;
    auto archive = practice::g_replayArchive.Get();
    if (archive->savedRevision != saved)
    {
        saved = archive->savedRevision;
        suite_ui::Notify("Replay saved");
    }
    auto search = g_seedSearch.Get();
    uint64_t key = (uint64_t(search->request.session) << 32) | search->request.serial;
    if (key && key != finished && search->phase == practice::SearchPhase::Completed)
    {
        finished = key;
        suite_ui::Notify("Seed search complete", false, 0, suite_ui::Tone::Complete);
    }
}

bool DeferExit()
{
    if (suite_ui::lifecycle.Exiting())
        return true;
    bool visible = g_binding && g_binding->state.ShouldShow() && IsWindow(g_hWnd) && !IsIconic(g_gameWnd);
    if (!suite_ui::lifecycle.BeginExit(suite_ui::LifecycleNow(), suite_ui::preferences, visible,
                                       ActiveSpeedrun()))
        return false;
    g_interfaceAudio.Policy(false, suite_ui::preferences.volume);
    SetOverlayInteractive(false);
    g_overlayVisible = false;
    g_compactMode = false;
    g_allOverlayEnabled = true;
    g_commands.inputCapture = 0;
    ClearTrainerCommands();
    return true;
}

std::string BuildDiagnostics()
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "[Connection and build]\nAuthor: " << suite_brand::Author << "\nTrainer: " << suite_brand::Version
        << " / protocol " << kProtocolVersion << "\nMod: " << (g_pShared ? g_pShared->version : "--")
        << "\nAuto-start game: " << (g_autoStartGame ? "enabled" : "disabled")
        << "\nLast game launch: " << g_gameLaunchStatus << "\nStatus: " << g_connectionStatus
        << "\nGame PID: " << g_gamePid;
    if (g_pShared)
        out << "\nGame reported version: " << g_pShared->gameVersion << "; Unity: " << g_pShared->unityVersion
            << "\nGame assembly file SHA256: " << g_pShared->gameAssemblyHash << "\nModule base: 0x"
            << std::hex << g_pShared->baseAddress << std::dec;
    if (g_hasSnapshot)
        out << "\nSnapshot age: " << GetTickCount64() - g_lastSnapshotTick << " ms";
    out << "\nCommands: " << (HasPendingRequest() ? "awaiting acknowledgement" : "idle") << " ("
        << g_ackSerial << " / " << g_sentSerial << ")\n\n[Window and input]";
    if (g_binding)
    {
        const RECT &r = g_binding->clientRect;
        out << "\nClient: " << r.right - r.left << " x " << r.bottom - r.top << " at " << r.left << ", "
            << r.top << "\nWindow: "
            << (g_binding->state.minimized     ? "minimized"
                : g_binding->state.gameVisible ? "visible"
                                               : "hidden")
            << "\nFocus: "
            << (g_binding->state.gameForeground      ? "game"
                : g_binding->state.overlayForeground ? "trainer menu"
                                                     : "other application")
            << "\nWindow events: " << (g_binding->EventsAvailable() ? "active" : "polling fallback");
    }
    out << "\nMenu ownership requested: " << g_commands.inputCapture;
    if (g_pShared)
        out << "; observed: " << int(g_pShared->menuInputOwned)
            << "\nUnity cursor lock: " << g_pShared->cursorLock << " (0 released / 1 locked / 2 confined)";
    out << "\nInput surface: " << (g_surfaceInteractive ? "interactive menu" : "passive HUD")
        << "\n\n[Rendering and performance]\nDPI scale: " << g_dpiScale
        << "\nRenderer: " << (g_softwareRenderer ? "WARP (software)" : "D3D11 hardware") << "\nTransparency: "
        << (g_alphaCompositing     ? "DirectComposition menu alpha"
            : g_surfaceInteractive ? "color-key menu fallback"
                                   : "layered HUD with per-pixel alpha")
        << "\nFonts (menu / HUD / timer): " << suite_ui::loadedFonts[0] << " / " << suite_ui::loadedFonts[1]
        << " / " << suite_ui::loadedFonts[2] << "\nIntro backdrop: " << g_introBackdrop.Status()
        << "\nMenu frame limit: " << g_renderFps << " FPS; passive HUD: 30 FPS; hidden: no rendering";
    if (g_pShared)
        out << "\nGame FPS: " << g_pShared->gameFps << "\nGame targetFrameRate: " << g_pShared->gameTargetFps
            << "; VSync count: " << g_pShared->gameVSync << "; display: " << g_pShared->gameRefreshHz << " Hz"
            << "\nDisplay override: FPS mode " << g_commands.frameLimitMode << ", target "
            << g_commands.gameFrameLimit << ", VSync mode " << g_commands.vSyncMode
            << "\nSampler work: " << g_pShared->plannerGenerationMs << " ms per game frame";
    out << "\n\n[Player and map geometry]";
    if (g_pShared)
        out << "\nScene: " << g_pShared->currentSceneIndex << " / health: " << g_pShared->health
            << "\nObserved player: " << int(g_pShared->playerReady)
            << "; scene epoch: " << g_pShared->sceneEpoch << "\nView camera: " << g_pShared->viewCamera
            << "\nTimer enemies: " << g_pShared->timerMonsterCount
            << " / 128; omitted: " << g_pShared->timerMonsterOverflow
            << "\nWall boxes: " << g_pShared->wallCount << " / 200; omitted: " << g_pShared->wallOverflowCount
            << "\nHide & Seek wall contours: " << g_pShared->wallOutlineCount
            << "\nFreecam: " << (g_pShared->freecamActive ? "active" : "off")
            << "; hitboxes: " << int(g_pShared->hitboxEnabled)
            << "\nHitbox discovery: " << g_pShared->hitboxDetails;
    auto map = g_mapWorker.Get(g_pShared ? MapLevelIndex(g_pShared->currentSceneIndex) : -1);
    if (map && map->data)
    {
        const auto &d = *map->data;
        out << "\nStatic capture: " << (map->live ? "current process" : "saved / review") << "; "
            << d.wallOutlines.size() << " wall contours, " << d.obstacles.size() << " physical obstacles"
            << "\nCaptured triangles: " << d.indices.size() / 3
            << "; selected floor triangles: " << (map->floor ? map->floor->size() : 0)
            << "; boundary edges: " << (map->outline ? map->outline->size() : 0)
            << "\nPlayer profile: radius " << d.agentRadius << ", height " << d.agentHeight << ", step "
            << d.agentStep << ", slope " << d.agentSlope << ", physical bottom Y " << d.agentFloor
            << "\nNavigation: status " << d.navigationStatus << ", model " << d.navigationModel
            << ", queries " << d.navigationQueries << " / " << d.navigationTotal << "\n"
            << d.navigationNote;
    }
    if (g_pShared)
        out << "\n3D replay: " << g_pShared->replay3dState
            << " (0 off / 1 loading / 2 playing / 3 paused / 4 error); run " << g_pShared->replay3dRunId
            << "; " << g_pShared->replay3dSeconds << " s\n"
            << g_pShared->replay3dMessage;
    out << "\nGeometry scan: 1 Hz; enemy path: 10 Hz; trail: max 10 Hz / 128 samples\n\n[Run and local validation]";
    if (g_pShared)
        out << "\nLocal status: " << (g_pShared->runValidity == 1 ? "VALID" : "UNVALID")
            << "\nReason: " << g_pShared->validationDetail
            << "\nRNG hooks: " << (g_pShared->rngPatchComplete ? "22 / 22" : "incomplete")
            << "\nObserved seed kind: " << int(g_pShared->seedKind)
            << "; reseeds: " << g_pShared->rngReseedCount << "\nRun warnings: " << g_pShared->runWarnings
            << "; used aids: " << g_pShared->runUsedFeatures
            << "\nMandatory HUD presented: " << g_commands.runHudVisible
            << "\nExternal tool scan: " << int(g_pShared->externalScanStatus)
            << " (0 pending / 1 clear / 2 detected / 3 unavailable); " << g_pShared->externalTool
            << "\nPractice seed: " << (g_pShared->seedActive ? std::to_string(g_pShared->levelSeed) : "off");
    out << "\nMenu CPU/frame: mean " << g_menuFrameCost.Mean() << " ms; p95 " << g_menuFrameCost.P95()
        << " ms"
        << "\nHUD CPU/frame: mean " << g_hudFrameCost.Mean() << " ms; p95 " << g_hudFrameCost.P95() << " ms"
        << "\nHUD readback: " << g_lastHudBytes << " bytes/frame; regions " << g_lastHudSurfaces
        << "; last upload " << g_lastHudUpload << " ms";
    if (g_pShared)
    {
        out << "\nHelper Update/LateUpdate CPU: mean " << g_pShared->helperWorkMean << " ms; p95 "
            << g_pShared->helperWorkP95 << " ms"
            << "\nMovement model: scene " << g_pShared->movementScene << "; revision "
            << g_pShared->movementRevision << "; rejected " << g_pShared->movementRejected;
        for (int k = 0; k < 5; k++)
            out << "\nMovement class " << k << ": " << g_pShared->movementCounts[k] << " windows; p60 "
                << g_pShared->movementSpeeds[k * 3 + 1] << " u/s";
        out << "\nMovement storage: " << g_pShared->movementNote;
    }
    out << "\nUI motion: " << suite_ui::preferences.motion << "; UI sounds: " << suite_ui::preferences.sounds
        << "; volume " << int(suite_ui::preferences.volume * 100) << "%; audio device "
        << g_interfaceAudio.Status() << " (0 idle / 1 opened / 2 unavailable)";
    out << "\n\n[Recording and storage]";
    if (g_pShared)
        out << "\nLast review: " << g_pShared->reviewRunId << "; scene " << g_pShared->reviewScene
            << "; spawn seed " << g_pShared->reviewSpawnSeed << "; samples " << g_pShared->reviewSamples
            << "; ready " << int(g_pShared->reviewReady)
            << "\nReports / reviews: " << int(g_pShared->reportSaveStatus) << "; " << g_pShared->reportError
            << "\nRun data: " << g_pShared->statsSaveStatus << "; " << g_pShared->statsError
            << "\nData recovery: " << g_pShared->statsWarning;
    if (g_pShared)
    {
        auto map = g_mapWorker.Get(MapLevelIndex(g_pShared->currentSceneIndex));
        if (map && map->data)
        {
            const auto &d = *map->data;
            size_t regions = std::count_if(d.pickupRegions.begin(), d.pickupRegions.end(),
                                           [](const auto &p) { return p.verified; });
            out << "\n[Map geometry / routes]\nPhysical capture: "
                << (d.physicalCaptured ? "yes" : "legacy / waiting") << "; live: " << map->live
                << "; epoch: " << d.epoch << "\nPhysical parts: " << d.physicalParts.size()
                << "; omitted: " << d.physicalOmitted << "\nPickup regions: " << regions << " / "
                << d.points.size() << "; profile: standing / level camera\nMovement capsule: radius "
                << d.agentRadius << "; height " << d.agentHeight << "; floor " << d.agentFloor
                << "\nPickup contacts used: " << map->route.pickupOptimized
                << "; route estimated: " << map->route.estimated << "\nRoute: " << map->route.message;
        }
    }
    out << "\nDisplay settings writer: " << (g_settings.Pending() ? "pending" : "idle");
    if (!g_settings.error.empty())
        out << "\n" << g_settings.error;
    if (!g_settings.loadWarning.empty())
        out << "\n" << g_settings.loadWarning;
    return out.str();
}

void SyncCommands(bool visible)
{
    if (suite_ui::lifecycle.Exiting())
    {
        inspector::Close();
        ClearTrainerCommands();
        return;
    }
    bool restricted = g_speedrunMode && !g_practiceMode;
    bool replayReserved = g_pShared && g_pShared->replay3dState >= 1 && g_pShared->replay3dState <= 3;
    if ((restricted || replayReserved) && inspector::open)
        inspector::Close();
    bool inspectorWanted = inspector::open && !restricted && !replayReserved;
    const bool running = ActiveSpeedrun();
    static bool previouslyRestricted = false;
    if (running && !previouslyRestricted)
    {
        g_mapWorker.Pause();
        g_seedSearch.Stop();
        g_seedSearch.HidePreview();
        g_runReviewOpen = false;
        g_replayMapWorker.Pause();
    }
    previouslyRestricted = running;
    if (restricted)
        g_brightnessOn = g_hitboxCheck = g_hidePlayerHitbox = false;
    g_commands.hitboxEnabled = g_hitboxCheck;
    g_commands.brightnessEnabled = g_brightnessOn;
    g_commands.hidePlayerHitbox = g_hidePlayerHitbox;
    g_commands.accurateHitboxes = 1;
    g_commands.speedrunLevelIndex = static_cast<byte>(g_selectedSpeedrunLevel);
    g_commands.speedrunModeActive = g_speedrunMode;
    g_commands.practiceModeActive = g_practiceMode;
    g_commands.seededPractice = g_seededPractice;
    g_commands.practiceSeed = g_practiceSeed;
    g_commands.reportOptions = g_reportsEnabled ? 1u : 0u;
    g_commands.replayMinSeconds = suite_ui::replayPreferences.seconds;
    g_commands.replayMinCrystals =
        suite_ui::replayPreferences.mode == 1 ? uint32_t(suite_ui::replayPreferences.crystals) : 0;
    bool searchWanted = !restricted && (g_seedSearch.Busy() || (visible && g_plannerTabActive));
    bool plannerWanted =
        searchWanted || (visible && !restricted &&
                         (g_showMinimapWindow || g_runReviewOpen ||
                          (inspectorWanted && ((inspector::options & 2) ||
                                               inspector::geometryFrame == ImGui::GetFrameCount()))));
    auto search = g_seedSearch.Get();
    if (search->Busy() &&
        (!searchWanted || (search->request.live &&
                           (!g_pShared || !g_pShared->rngPatchComplete ||
                            search->request.pid != g_pShared->pid || search->request.session != g_session))))
        g_seedSearch.Stop();
    search = g_seedSearch.Get();
    if (search->Busy())
        g_seedSearch.Update(g_mapWorker.Get(MapLevelIndex(search->request.scene)));
    search = g_seedSearch.Get();
    static bool wasSearchBusy = false;
    static uint32_t lastSearchSerial = 0;
    static bool stopPending = false;
    if (wasSearchBusy && !search->Busy() &&
        (search->phase != practice::SearchPhase::Completed || search->portable))
        stopPending = true;
    if (search->Busy() && search->request.serial != lastSearchSerial)
        stopPending = false;
    if (stopPending && !HasPendingRequest())
    {
        g_commands.plannerRequest = 4;
        stopPending = false;
    }
    wasSearchBusy = search->Busy();
    lastSearchSerial = search->request.serial;
    g_commands.plannerSearchCount = g_plannerOptions.searchCount;
    g_commands.plannerBudgetMicros = static_cast<uint32_t>(g_plannerOptions.generationMs * 1000);
    g_commands.plannerOptions = (g_plannerOptions.recipe == 1 ? 1u : 0u) | (g_rawPracticeSpawn ? 2u : 0u) |
                                (g_plannerOptions.automaticLoad ? 4u : 0u) |
                                (g_plannerOptions.calculateRoutes ? 0u : 8u) |
                                (g_plannerOptions.portableGeneration ? 16u : 0u);
    g_commands.inputCapture = visible && MenuInputRequested() ? 1u : 0u;
    g_commands.frameLimitMode = g_frameLimitMode;
    g_commands.gameFrameLimit = g_gameFrameLimit;
    g_commands.vSyncMode = g_vSyncMode;
    g_commands.runHudVisible =
        visible && g_validityBadgeDrawn && GetTickCount64() - g_badgePresentedTick < 250 ? 1u : 0u;
    g_commands.telemetryOptions = kTelemetryConfigured;
    if (inspectorWanted)
        g_commands.telemetryOptions |= kTelemetryInspector;
    if (visible && suite_input::preferences.enabled && !suite_hud::active && !ClipInputRequested())
        g_commands.telemetryOptions |= kTelemetryInputs | kTelemetryLive;
    if (searchWanted || (!running && g_runReviewOpen))
        g_commands.telemetryOptions |= kTelemetryPlanner;
    if (visible)
    {
        g_commands.telemetryOptions |= kTelemetryLive;
        if (searchWanted || (!running && g_runReviewOpen))
            g_commands.telemetryOptions |= kTelemetryPlanner;
        if (plannerWanted)
            g_commands.telemetryOptions |= kTelemetryGeometry;
        auto hudValue = [&](OverlayField field) { return g_hudLayout.visible[static_cast<int>(field)]; };
        bool hudDetails =
            CompactHudAllowed(g_speedrunMode, g_practiceMode) && g_overlayVisible && g_compactMode &&
            g_hudLayout.enabled &&
            (hudValue(OverlayField::Mode) || hudValue(OverlayField::Speed) ||
             hudValue(OverlayField::PlayerState) || hudValue(OverlayField::MonsterState) ||
             hudValue(OverlayField::MonsterSpeed) || hudValue(OverlayField::Noise) ||
             hudValue(OverlayField::Health) || (!g_speedrunMode && hudValue(OverlayField::Progress)));
        if (!restricted && ((g_overlayVisible && !g_compactMode) || g_showMinimapWindow || hudDetails))
            g_commands.telemetryOptions |= kTelemetryDetails;
        if (g_showMinimapWindow && !restricted)
        {
            g_commands.telemetryOptions |= kTelemetryMap;
            if (g_mapOptions.walls)
                g_commands.telemetryOptions |= kTelemetryWalls;
            if (g_mapOptions.crystals)
                g_commands.telemetryOptions |= kTelemetryCrystals;
            if (g_mapOptions.monster)
                g_commands.telemetryOptions |= kTelemetryMonster;
            if (g_showPath)
                g_commands.telemetryOptions |= kTelemetryPath;
        }
        if (!restricted && hudDetails &&
            (hudValue(OverlayField::MonsterState) || hudValue(OverlayField::MonsterSpeed)))
            g_commands.telemetryOptions |= kTelemetryMonster;
    }
}

void InvalidateFontTexture()
{
    if (ImGui::GetIO().BackendRendererUserData)
        ImGui_ImplDX11_InvalidateDeviceObjects();
}

ImFont *LoadUiFont(float pixels, int choice = 0)
{
    wchar_t directory[MAX_PATH]{};
    UINT count = GetWindowsDirectoryW(directory, MAX_PATH);
    if (count && count < MAX_PATH)
    {
        auto fonts = std::filesystem::path(directory) / L"Fonts";
        for (int i = 0; i < kSuiteFontCount; i++)
            suite_ui::fontAvailable[i] =
                GetFileAttributesW((fonts / kSuiteFontFiles[i]).c_str()) != INVALID_FILE_ATTRIBUTES;
        choice = std::clamp(choice, 0, kSuiteFontCount - 1);
        int actual = suite_ui::fontAvailable[choice] ? choice : 0;
        auto path = fonts / kSuiteFontFiles[actual];
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
        {
            auto utf8 = path.u8string();
            std::string name(reinterpret_cast<const char *>(utf8.data()), utf8.size());
            if (auto *font = ImGui::GetIO().Fonts->AddFontFromFileTTF(name.c_str(), pixels))
            {
                g_uiFontName = kSuiteFonts[actual];
                return font;
            }
        }
    }
    ImFontConfig config;
    config.SizePixels = pixels;
    g_uiFontName = "Dear ImGui fallback";
    return ImGui::GetIO().Fonts->AddFontDefault(&config);
}

double SnapshotAge()
{
    return std::max(
        0., std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count() -
                g_snapshotReceived);
}

#include "TrainerUI.h"
#include "ClipRuntime.h"

DiagnosticInputs CaptureDiagnosticInputs()
{
    DiagnosticInputs in;
    in.data = g_pShared;
    in.age = SnapshotAge();
    in.menuP95 = g_menuFrameCost.P95();
    in.hudP95 = g_hudFrameCost.P95();
    in.software = g_softwareRenderer;
    in.menuRequested = MenuInputRequested();
    in.mapRequested = g_showMinimapWindow && !(g_speedrunMode && !g_practiceMode);
    in.sounds = suite_ui::preferences.sounds;
    in.audioStatus = g_interfaceAudio.Status();
    in.settingsError = g_settings.error;
    in.replayError = practice::g_replayArchive.Get()->error;
    auto map = g_mapWorker.Get(g_pShared ? MapLevelIndex(g_pShared->currentSceneIndex) : -1);
    in.mapReady = map && map->data && g_pShared && map->live &&
                  map->data->epoch == int(g_pShared->sceneEpoch) && map->session == g_session;
    auto displayed = DisplayedPlanner(g_plannerOptions.level);
    if (displayed && displayed->data)
    {
        in.routeKnown = displayed->data->spawn >= 0 && !displayed->data->selected.empty();
        in.routeWorking = displayed->working;
        in.routeComplete = displayed->route.complete;
        in.routeEstimated = displayed->route.estimated;
        in.routeDetail = displayed->error.empty() ? displayed->route.message : displayed->error;
        in.omittedGeometry = displayed->data->physicalOmitted;
        for (const auto &part : displayed->data->physicalParts)
            in.floorTriangles += (part.flags & 8) != 0;
    }
    auto search = g_seedSearch.Get();
    in.searchActive = search->Busy();
    in.checkedSeeds = search->evaluated;
    in.searchResults = int(search->top.size());
    in.searchRate = search->recentChecked;
    auto motion = practice::g_replayArchive.Motion();
    if (motion)
    {
        in.recordingRun = motion->id;
        in.recording = practice::RecordingCheckFor(*motion);
    }
    in.exportError = suite_clip::error;
    return in;
}

void ReleaseSurface()
{
    suite_clip::ResetGraphics();
    for (int i = 0; i < 8; i++)
        if (g_hotkeysRegistered & (1u << i))
            UnregisterHotKey(g_hWnd, i + 1);
    g_hotkeysRegistered = g_hotkeysAttempted = g_hotkeyEvents = 0;
    if (ImGui::GetIO().BackendRendererUserData)
        ImGui_ImplDX11_Shutdown();
    if (ImGui::GetIO().BackendPlatformUserData)
        ImGui_ImplWin32_Shutdown();
    CleanupDeviceD3D();
    if (g_hWnd && IsWindow(g_hWnd))
        DestroyWindow(g_hWnd);
    g_hWnd = nullptr;
    g_surfaceInteractive = false;
    if (g_binding)
        g_binding->SetOverlay(nullptr);
}

bool CreateSurface()
{
    if (!g_gameWnd || !IsWindow(g_gameWnd))
        return false;
    RECT client{};
    POINT origin{};
    GetClientRect(g_gameWnd, &client);
    ClientToScreen(g_gameWnd, &origin);
    bool interactive = MenuInputRequested();
    DWORD style = WS_EX_TOOLWINDOW | (interactive ? WS_EX_NOREDIRECTIONBITMAP
                                                  : WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    g_hWnd = CreateWindowExW(style, g_wc.lpszClassName, L"It Steals Trainer", WS_POPUP, origin.x, origin.y,
                             std::max(1L, client.right), std::max(1L, client.bottom), g_gameWnd, nullptr,
                             g_wc.hInstance, nullptr); // Actual owned popup, no desktop TOPMOST window.
    if (!g_hWnd)
        return false;
    if (!CreateDeviceD3D(g_hWnd, interactive))
    {
        ReleaseSurface();
        return false;
    }
    if (!ImGui_ImplWin32_Init(g_hWnd) || !ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext))
    {
        ReleaseSurface();
        return false;
    }
    // Never expose a passive surface whose actual Windows input styles differ.
    if (!interactive)
    {
        LONG_PTR actual = GetWindowLongPtrW(g_hWnd, GWL_EXSTYLE);
        LONG_PTR required = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
        if ((actual & required) != required || !IsWindowEnabled(g_hWnd))
        {
            ReleaseSurface();
            return false;
        }
    }
    g_surfaceInteractive = interactive;
    g_resizeWidth = g_resizeHeight = 0;
    g_binding->SetOverlay(g_hWnd);
    ApplyTheme(ImGui_ImplWin32_GetDpiScaleForHwnd(g_gameWnd));
    SetOverlayInteractive(g_overlayVisible && !g_compactMode, false);
    return true;
}

void ClearTrainerCommands()
{
    if (!g_mapping)
        return;
    SharedLock lock(g_hMutex);
    if (!lock.acquired || lock.abandoned)
        return;
    SharedHeader header{};
    memcpy(&header, g_mapping, sizeof(header));
    if (!Compatible(header) || header.pid != g_gamePid)
        return;
    TrainerCommands off{};
    off.serial = header.commandAck;
    off.heartbeat = g_commands.heartbeat + 1;
    off.telemetryOptions = kTelemetryConfigured;
    off.replay3dCmd = 2;
    memcpy(g_mapping + kCommandOffset, &off, sizeof(off));
}

class OwnedHandle
{
    HANDLE value;

  public:
    explicit OwnedHandle(HANDLE handle) : value(handle) {}

    OwnedHandle(const OwnedHandle &) = delete;

    ~OwnedHandle()
    {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
    }

    HANDLE Get() const
    {
        return value;
    }

    void Reset(HANDLE next = nullptr)
    {
        if (value && value != INVALID_HANDLE_VALUE)
            CloseHandle(value);
        value = next;
    }
};

HANDLE CreateFrameTimer()
{
    HANDLE value = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002, TIMER_ALL_ACCESS);
    return value ? value : CreateWaitableTimerW(nullptr, FALSE, nullptr);
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    OwnedHandle singleInstance(CreateMutexW(nullptr, FALSE, L"Local\\ItStealsPracticeSuite.Trainer"));
    if (!singleInstance.Get() || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        MessageBoxW(nullptr, L"The trainer is already running.", L"It Steals Trainer", MB_ICONINFORMATION);
        return 1;
    }
    OwnedHandle process(nullptr);
    OwnedHandle frameTimer(CreateFrameTimer());
    ImGui_ImplWin32_EnableDpiAwareness();
    GameWindowBinding binding(g_gamePid);
    g_binding = &binding;
    binding.Refresh(g_allOverlayEnabled);
    g_gameWnd = binding.Game();
    g_wc = {sizeof(g_wc),
            0,
            WndProc,
            0L,
            0L,
            GetModuleHandleW(nullptr),
            nullptr,
            LoadCursor(nullptr, IDC_ARROW),
            nullptr,
            nullptr,
            L"ItStealsPracticeOverlay.v6",
            nullptr};
    g_wc.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
    g_wc.hIconSm = static_cast<HICON>(
        LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, LR_SHARED));
    if (!RegisterClassExW(&g_wc))
    {
        MessageBoxW(nullptr, L"Could not create the window class.", L"Trainer", MB_ICONERROR);
        g_binding = nullptr;
        return 1;
    }
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    inspector::pointerProvider = [](inspector::Pointer &out) {
        POINT at{};
        RECT client{};
        if (!IsWindow(g_gameWnd) || !GetCursorPos(&at) || !ScreenToClient(g_gameWnd, &at) ||
            !GetClientRect(g_gameWnd, &client))
            return false;
        auto p = inspector::NormalizePointer(at.x, at.y, 0, 0, client.right - client.left,
                                             client.bottom - client.top);
        if (!p)
            return false;
        out = *p;
        return true;
    };
    ApplyPreferences(g_settings.Load());
    inspector::reportFolder = g_settings.Directory() / L"Diagnostics" / L"Inspector";
    g_mapWorker.SetDirectory(g_settings.Directory());
    g_mapWorker.LoadCaches();
    practice::g_replayArchive.SetDirectory(g_settings.Directory() / L"Replays");
    if (g_compactMode)
        g_overlayVisible = true; // Restore HUD without taking focus.
    std::error_code directoryError;
    std::filesystem::create_directories(g_settings.Directory(), directoryError);
    if (directoryError)
        g_settings.error = "Settings directory is not writable";
    suite_update::service.Init(g_settings.Directory());
    if (suite_update::service.autoCheck)
        suite_update::service.Check();
    auto iniUtf8 = (g_settings.Directory() / L"layout.ini").u8string();
    std::string iniPath(reinterpret_cast<const char *>(iniUtf8.data()), iniUtf8.size());
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = iniPath.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    ApplyTheme(1);
    using Clock = std::chrono::steady_clock;
    auto nextFrame = Clock::now();
    ULONGLONG nextExchange = 0, nextSave = 0, nextSurface = 0, nextProcessCheck = 0;
    KeyEdge f2, f3, f4, f5, f6, shiftT, f8, pickerKey, replayTab, replayEscape, replaySpace, replayLeft,
        replayRight, replayHome, replayEnd, replayFast, replaySlow, replayMark;
    bool occluded = false;
    game_launch::StartupOnce startup;
    bool launchRequested =
        startup.Plan(g_autoStartGame || game_launch::RestartAfterUpdateFlag(),
                     GetProcessIdByName(L"It Steals.exe") != 0, game_launch::NoLaunchFlag());
    TrainerTray tray(
        [] {
            if (!IsWindow(g_gameWnd))
                return;
            SetForegroundWindow(g_gameWnd);
            g_allOverlayEnabled = true;
            g_overlayVisible = true;
            g_compactMode = false;
            SetOverlayInteractive(true);
        },
        [] { g_quit = true; }, [&] { launchRequested = true; });
    uint32_t previousOptions = 0;
    HWND previousForeground = nullptr;
    suite_update::Phase previousUpdatePhase = suite_update::Phase::Idle;
    DWORD updateSessionPid = 0;
    std::string updateSession;
    ULONGLONG nextUpdateCheck = GetTickCount64() + 20 * 60 * 1000;
    ULONGLONG nextSessionUpdateCheck = GetTickCount64() + 60000;
    bool gameExited = false;
    while (!g_quit)
    {
        if (launchRequested)
        {
            launchRequested = false;
            if (!StartInstalledGame())
                tray.LaunchWarning();
        }
        suite_ui::lifecycleTime = GetTickCount64() / 1000.0;
        MSG message{};
        int messages = 0;
        while (messages++ < 256 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                g_quit = true;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (g_quit && DeferExit())
            g_quit = false;
        if (g_quit)
            break;
        if (suite_ui::lifecycle.Done(suite_ui::LifecycleNow()))
            break;
        ULONGLONG tick = GetTickCount64();
        bool lost = process.Get() && WaitForSingleObject(process.Get(), 0) == WAIT_OBJECT_0;
        if (!process.Get() && g_gamePid && tick >= nextProcessCheck)
            lost = GetProcessIdByName(L"It Steals.exe") != g_gamePid;
        if (lost)
        {
            gameExited = true;
            g_quit = true;
            break;
        }
        if (tick >= nextProcessCheck)
        {
            nextProcessCheck = tick + 1000;
            if (!g_gamePid)
            {
                g_gamePid = GetProcessIdByName(L"It Steals.exe");
                if (g_gamePid)
                {
                    process.Reset(OpenProcess(SYNCHRONIZE, FALSE, g_gamePid));
                    binding.BindProcess(g_gamePid);
                    nextExchange = 0;
                    HANDLE info = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, g_gamePid);
                    if (info)
                    {
                        wchar_t exe[32768]{};
                        DWORD count = 32768;
                        if (QueryFullProcessImageNameW(info, 0, exe, &count))
                            practice::g_replayArchive.SetSourceDirectory(
                                std::filesystem::path(exe).parent_path() / L"UserData" /
                                L"ItStealsPracticeSuite" / L"RunReviews");
                        CloseHandle(info);
                    }
                }
            }
            tray.Update(g_gamePid ? g_connectionStatus : "Waiting for It Steals (automatic reconnect)",
                        IsWindow(binding.Game()));
        }
        binding.Refresh(OverlayRequested());
        g_gameWnd = binding.Game();
        bool allowed = !suite_ui::lifecycle.Exiting() &&
                       (binding.state.gameForeground || binding.state.overlayForeground);
        for (int i = 0; i < 8; i++)
        {
            unsigned bit = 1u << i;
            if (!allowed)
            {
                if (g_hotkeysRegistered & bit)
                    UnregisterHotKey(g_hWnd, i + 1);
                g_hotkeysRegistered &= ~bit;
                g_hotkeysAttempted &= ~bit;
            }
            else if (IsWindow(g_hWnd) && !(g_hotkeysAttempted & bit))
            {
                g_hotkeysAttempted |= bit;
                if (RegisterHotKey(g_hWnd, i + 1, MOD_NOREPEAT | (i == 5 ? MOD_SHIFT : 0),
                                   i == 5   ? 'T'
                                   : i == 7 ? VK_F7
                                            : VK_F2 + i))
                    g_hotkeysRegistered |= bit;
            }
        }
        auto keyPressed = [&](KeyEdge &key, int index) {
            bool down = (GetAsyncKeyState(index == 5   ? 'T'
                                          : index == 7 ? VK_F7
                                                       : VK_F2 + index) &
                         0x8000) != 0;
            if (index == 5)
                down = down && (GetAsyncKeyState(VK_SHIFT) & 0x8000);
            bool fallback = key.Update(down, allowed);
            unsigned bit = 1u << index;
            return allowed && ((g_hotkeysRegistered & bit) ? (g_hotkeyEvents & bit) != 0 : fallback);
        };
        bool workspaceActive = suite_hud::active || suite_clip::open;
        if (keyPressed(f2, 0) && !workspaceActive)
        {
            if (inspector::picking)
                inspector_ui::EndPick();
            if (inspector::dockInteractive)
                inspector_ui::ReturnToGame();
            g_allOverlayEnabled = !g_allOverlayEnabled;
            if (!g_allOverlayEnabled)
            {
                suite_replay::controls = false;
                SetOverlayInteractive(false);
            }
            else
                SetOverlayInteractive(g_overlayVisible && !g_compactMode);
            binding.Invalidate();
        }
        if (keyPressed(f3, 1) && !workspaceActive)
        {
            if (suite_update_ui::Open())
                suite_update_ui::Close();
            suite_replay::controls = false;
            if (inspector::picking)
                inspector_ui::EndPick();
            else if (inspector::dockInteractive)
                inspector_ui::ReturnToGame();
            else if (g_compactMode)
            {
                g_compactMode = false;
                g_overlayVisible = true;
            }
            else
                g_overlayVisible = !g_overlayVisible;
            g_allOverlayEnabled = true;
            SetOverlayInteractive(g_overlayVisible && !g_compactMode);
            binding.Invalidate();
        }
        if (keyPressed(f4, 2) && !workspaceActive && CompactHudAllowed(g_speedrunMode, g_practiceMode))
        {
            if (inspector::open)
                inspector::Close();
            g_compactMode = !g_compactMode;
            g_overlayVisible = true;
            g_allOverlayEnabled = true;
            SetOverlayInteractive(!g_compactMode);
            binding.Invalidate();
        }
        if (keyPressed(f5, 3) && !workspaceActive)
        {
            g_commands.resetMapCmd = 1;
            if (!g_mapping)
                g_nextConnectTick = 0;
            nextExchange = 0;
        }
        if (keyPressed(f6, 4) && !workspaceActive && g_pShared && !HasPendingRequest())
        {
            g_commands.windowModeCmd = 1;
            nextExchange = 0;
        }
        if (keyPressed(shiftT, 5) && !workspaceActive && !ImGui::GetIO().WantTextInput &&
            !UpdateInputRequested())
            SetSpeedrunMode(!g_speedrunMode, true);
        if (keyPressed(f8, 6) && !workspaceActive &&
            suite_run::presentation.CanAnalyze(g_pShared, suite_ui::LifecycleNow()))
            AnalyseFinishedRun();
        if (keyPressed(pickerKey, 7) && !workspaceActive && !ImGui::GetIO().WantTextInput)
        {
            if (inspector::picking)
                inspector_ui::EndPick();
            else
                inspector_ui::StartPick(3);
            binding.Invalidate();
        }
        bool replayActive = g_pShared && g_pShared->timerState != 1 && g_pShared->replay3dState >= 2 &&
                            g_pShared->replay3dState <= 3;
        if (!replayActive && suite_replay::controls)
        {
            suite_replay::controls = false;
            SetOverlayInteractive(g_overlayVisible && !g_compactMode);
        }
        auto replayKey = [&](KeyEdge &edge, int code, bool enabled) {
            return edge.Update((GetAsyncKeyState(code) & 0x8000) != 0,
                               allowed && replayActive && !workspaceActive && enabled);
        };
        bool textInput = ImGui::GetCurrentContext() && ImGui::GetIO().WantTextInput;
        if (replayKey(replayTab, VK_TAB, !textInput && !(GetAsyncKeyState(VK_SHIFT) & 0x8000)))
        {
            suite_replay::controls = !suite_replay::controls;
            g_overlayVisible = false;
            g_compactMode = false;
            g_allOverlayEnabled = true;
            SetOverlayInteractive(suite_replay::controls);
            binding.Invalidate();
        }
        if (replayKey(replayEscape, VK_ESCAPE, suite_replay::controls && !textInput))
        {
            suite_replay::controls = false;
            SetOverlayInteractive(false);
            binding.Invalidate();
        }
        bool gameReplay =
            binding.state.gameForeground && !suite_replay::controls && !(g_overlayVisible && !g_compactMode);
        if (replayKey(replaySpace, VK_SPACE, gameReplay))
            Replay3DShortcut(0);
        if (replayKey(replayLeft, VK_LEFT, gameReplay))
            Replay3DShortcut(1);
        if (replayKey(replayRight, VK_RIGHT, gameReplay))
            Replay3DShortcut(2);
        if (replayKey(replayHome, VK_HOME, gameReplay))
            Replay3DShortcut(3);
        if (replayKey(replayEnd, VK_END, gameReplay))
            Replay3DShortcut(4);
        if (replayKey(replayFast, VK_OEM_PLUS, gameReplay))
            Replay3DShortcut(5);
        if (replayKey(replaySlow, VK_OEM_MINUS, gameReplay))
            Replay3DShortcut(6);
        if (replayKey(replayMark, 'M', gameReplay))
        {
            suite_replay::controls = true;
            suite_replay::requestMarker = true;
            suite_replay::markerAt = g_pShared->replay3dSeconds;
            g_overlayVisible = false;
            g_compactMode = false;
            g_allOverlayEnabled = true;
            SetOverlayInteractive(true);
            binding.Invalidate();
        }
        g_hotkeyEvents = 0;
        binding.Refresh(OverlayRequested());
        bool surfaceRecreated = false;
        if ((!IsWindow(g_hWnd) || GetWindow(g_hWnd, GW_OWNER) != g_gameWnd ||
             g_surfaceInteractive != MenuInputRequested()) &&
            tick >= nextSurface && binding.state.gameExists)
        {
            ReleaseSurface();
            if (!CreateSurface())
            {
                nextSurface = tick + 1000;
            }
            else
            {
                nextSurface = 0;
                nextFrame = Clock::now();
                occluded = false;
                surfaceRecreated = true;
            }
        }
        binding.Refresh(OverlayRequested());
        binding.Apply();
        bool visible = binding.state.ShouldShow() && IsWindow(g_hWnd);
        g_captureOverlay.enabled = g_captureOverlayEnabled;
        // Color-key fallback menus need their visible pixels for hit testing.
        g_captureOverlay.Tick(g_gamePid, visible && (!g_surfaceInteractive || g_alphaCompositing));
        bool introReady = visible && g_pShared && SnapshotAge() < .5;
        suite_ui::lifecycle.Consider(suite_ui::LifecycleNow(), introReady, suite_ui::preferences,
                                     ActiveSpeedrun() ||
                                         suite_run::presentation.WantsInput(g_pShared, g_runReviewOpen));
        auto &updater = suite_update::service;
        updater.Poll();
        if (g_gamePid != updateSessionPid)
        {
            updateSessionPid = g_gamePid;
            updateSession = suite_update::Service::Session(g_gamePid);
            if (g_gamePid && updater.autoCheck && tick >= nextSessionUpdateCheck)
            {
                updater.Check();
                nextSessionUpdateCheck = tick + 60000;
            }
        }
        if (tick >= nextUpdateCheck)
        {
            nextUpdateCheck = tick + 20 * 60 * 1000;
            if (updater.autoCheck)
                updater.Check();
        }
        auto update = updater.View();
        if (previousUpdatePhase == suite_update::Phase::Installing &&
            update->phase != suite_update::Phase::Installing)
            SetOverlayInteractive(MenuInputRequested());
        previousUpdatePhase = update->phase;
        bool updateBlocked = (g_pShared && g_pShared->timerState == 1) || suite_clip::open ||
                             suite_clip::exporting || suite_hud::active || inspector::picking ||
                             inspector::dockInteractive || suite_replay::ControlsRequested(g_pShared) ||
                             suite_run::presentation.WantsInput(g_pShared, g_runReviewOpen) ||
                             suite_ui::lifecycle.Exiting();
        updater.installBlocked = updateBlocked;
        bool introDone =
            !suite_ui::preferences.launchGraphic ||
            (suite_ui::lifecycle.presented && suite_ui::lifecycle.StartupAge(suite_ui::LifecycleNow()) >=
                                                  suite_ui::Lifecycle::StartupDuration);
        if (updater.presentation.Consider(
                suite_brand::Version, updateSession, introReady, introDone, updateBlocked,
                suite_update::Newer(update->latest, suite_brand::Version), suite_ui::LifecycleNow()))
        {
            suite_ui::PostGameTone(suite_ui::Tone::Confirm);
            SetOverlayInteractive(true);
            binding.Invalidate();
        }
        if (updater.presentation.installRequested && update->phase == suite_update::Phase::Ready &&
            !updateBlocked)
        {
            updater.presentation.installRequested = false;
            updater.Install(g_gamePid);
        }
        if (!g_surfaceInteractive)
            g_layeredHud.SyncVisibility(visible, binding.clientRect);
        HWND foreground = GetForegroundWindow();
        if (visible && MenuInputRequested() && binding.state.gameForeground &&
            (surfaceRecreated || foreground != previousForeground))
        {
            // Resume an already opened menu after Alt+Tab. An unowned visible
            // menu would leave Unity's locked cursor unusable over its controls.
            SetOverlayInteractive(true);
            if (GetForegroundWindow() != g_hWnd)
            {
                g_overlayVisible = false;
                suite_replay::controls = false;
                inspector::dockInteractive = false;
                if (inspector::picking)
                {
                    inspector::picking = false;
                    inspector::StopPointer();
                }
                SetOverlayInteractive(false, false);
                binding.Invalidate();
            }
        }
        previousForeground = GetForegroundWindow();
        SyncCommands(visible);
        if (suite_ui::lifecycle.Exiting())
        {
            suite_hud::active = false;
            if (suite_clip::open || suite_clip::exporting)
            {
                suite_clip::cancelRequested = true;
                suite_clip::open = false;
            }
        }
        suite_clip::Tick(g_pd3dDevice, g_pd3dDeviceContext, g_gamePid);
        g_mapClient.Poll(g_gamePid, g_session,
                         g_pShared &&
                             (g_commands.telemetryOptions & (kTelemetryPlanner | kTelemetryGeometry)),
                         g_mapWorker, g_seedSearch);
        g_inspectorClient.Poll(inspector::Identity(int(g_gamePid), g_session,
                                                   g_pShared ? g_pShared->currentSceneIndex : -1,
                                                   g_pShared ? g_pShared->sceneEpoch : 0),
                               visible && g_pShared && inspector::open);
        if ((!visible && tick >= nextExchange) || previousOptions != g_commands.telemetryOptions)
        {
            PullSnapshot();
            previousOptions = g_commands.telemetryOptions;
            nextExchange = tick + 500;
        }
        FlushCommands();
        if (tick >= nextSave)
        {
            SavePreferencesNow();
            nextSave = tick + 1500;
        }
        bool activeRun = ActiveSpeedrun();
        bool audioFocused =
            GetForegroundWindow() == g_hWnd && g_overlayInteractive && !suite_ui::lifecycle.Exiting();
        g_interfaceAudio.Policy(
            (suite_ui::AudioAllowed(suite_ui::preferences, MenuInputRequested(), audioFocused, activeRun) ||
             (visible && !activeRun && (GetForegroundWindow() == g_gameWnd || audioFocused) &&
              suite_ui::preferences.sounds && suite_ui::preferences.volume > 0 &&
              (suite_ui::LifecycleNow() - suite_ui::gameToneAt < .16 ||
               inspector_fx::feedback.Window(suite_ui::LifecycleNow(), inspector_fx::preferences,
                                             suite_ui::preferences)))),
            suite_ui::preferences.volume);
        suite_run::presentation.Observe(g_pShared, g_session, suite_ui::LifecycleNow());
        suite_run::presentation.scan.Tick(suite_ui::LifecycleNow(),
                                          visible &&
                                              suite_run::presentation.WantsInput(g_pShared, g_runReviewOpen));
        bool gameSound = visible && !activeRun && (GetForegroundWindow() == g_gameWnd || audioFocused) &&
                         suite_ui::preferences.sounds && suite_ui::preferences.volume > 0 &&
                         suite_ui::GameTonePending();
        if (gameSound)
        {
            g_interfaceAudio.Policy(true, suite_ui::preferences.volume);
            g_interfaceAudio.Submit(suite_ui::ConsumeGameTone(true));
        }
        else
            suite_ui::ConsumeGameTone(false);
        PollExperienceFeedback();
        g_interfaceAudio.Submit(suite_ui::ConsumeTone());
        auto now = Clock::now();
        if (visible && g_pd3dDevice && now >= nextFrame)
        {
            auto framePeriod = std::chrono::microseconds(
                1000000 / (g_surfaceInteractive ? ClampRenderRate(g_renderFps) : 30));
            nextFrame += framePeriod;
            if (nextFrame < now || nextFrame > now + framePeriod * 2)
                nextFrame = now + framePeriod;
            auto frameStarted = Clock::now();
            bool frameWasInteractive = g_surfaceInteractive;
            PullSnapshot();
            if (!g_surfaceInteractive)
            {
                UINT width = UINT(std::max<LONG>(1, binding.clientRect.right - binding.clientRect.left)),
                     height = UINT(std::max<LONG>(1, binding.clientRect.bottom - binding.clientRect.top));
                if (width != g_layeredHud.Width() || height != g_layeredHud.Height())
                {
                    g_resizeWidth = width;
                    g_resizeHeight = height;
                }
            }
            float dpi = std::clamp(ImGui_ImplWin32_GetDpiScaleForHwnd(g_gameWnd), .75f, 4.f);
            static float pendingScale = -1;
            static ULONGLONG scaleAt = 0;
            float desiredScale = g_speedrunMode && !(g_practiceMode && g_compactMode) ? g_speedrunLayout.scale
                                                                                      : g_hudLayout.scale;
            if (pendingScale != desiredScale)
            {
                pendingScale = desiredScale;
                scaleAt = tick;
            }
            if (std::fabs(dpi - g_dpiScale) > .01f || suite_ui::fontsDirty ||
                (desiredScale != suite_ui::loadedHudScale && tick - scaleAt >= 120))
                ApplyTheme(dpi);
            suite_ui::ApplyPalette(g_mainBgCol, g_textCol, g_headerCol);
            if (g_resizeWidth && g_resizeHeight)
            {
                g_pd3dDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
                if (g_mainRenderTargetView)
                {
                    g_mainRenderTargetView->Release();
                    g_mainRenderTargetView = nullptr;
                }
                HRESULT resize = g_surfaceInteractive
                                     ? g_pSwapChain->ResizeBuffers(0, g_resizeWidth, g_resizeHeight,
                                                                   DXGI_FORMAT_UNKNOWN, 0)
                                     : (g_layeredHud.Resize(g_pd3dDevice, g_resizeWidth, g_resizeHeight,
                                                            &g_mainRenderTargetView)
                                            ? S_OK
                                            : E_FAIL);
                g_resizeWidth = g_resizeHeight = 0;
                if (FAILED(resize))
                {
                    ReleaseSurface();
                    nextSurface = tick + 250;
                    continue;
                }
                if (g_surfaceInteractive)
                    CreateRenderTarget();
            }
            if (occluded && g_surfaceInteractive)
            {
                HRESULT probe = g_pSwapChain->Present(0, DXGI_PRESENT_TEST);
                if (probe == DXGI_STATUS_OCCLUDED)
                {
                    nextFrame = now + std::chrono::milliseconds(200);
                }
                else if (FAILED(probe))
                {
                    ReleaseSurface();
                    nextSurface = tick + 250;
                    continue;
                }
                else
                    occluded = false;
            }
            if (!occluded && g_mainRenderTargetView)
            {
                ImGui_ImplDX11_NewFrame();
                ImGui_ImplWin32_NewFrame();
                if (!g_surfaceInteractive)
                    io.DisplaySize = ImVec2(float(g_layeredHud.Width()), float(g_layeredHud.Height()));
                io.MouseDrawCursor = g_overlayInteractive && GetForegroundWindow() == g_hWnd;
                if (io.MouseDrawCursor)
                    SetCursor(nullptr);
                ImGui::NewFrame();
                suite_ui::BeginFrame(g_surfaceInteractive, ActiveSpeedrun());
                bool introBlocked =
                    ActiveSpeedrun() || suite_run::presentation.WantsInput(g_pShared, g_runReviewOpen);
                suite_ui::lifecycle.Consider(suite_ui::LifecycleNow(), introReady, suite_ui::preferences,
                                             introBlocked);
                if (introReady &&
                    suite_ui::lifecycle.Startup(suite_ui::LifecycleNow(), suite_ui::preferences,
                                                introBlocked) &&
                    suite_ui::appearance.introBlur)
                {
                    if (GetForegroundWindow() == g_gameWnd)
                        g_introBackdrop.Capture(g_pd3dDevice, g_gameWnd, int(640 * suite_ui::IntroUnit()),
                                                int(272 * suite_ui::IntroUnit()));
                    suite_ui::introBackdrop =
                        ImTextureID(reinterpret_cast<uintptr_t>(g_introBackdrop.View()));
                }
                else
                {
                    suite_ui::introBackdrop = 0;
                    g_introBackdrop.Reset();
                }
                float px = 0, py = 0, pz = 0, rotY = 0, monX = 0, monZ = 0, speed = 0;
                int objectives = 0, total = 0, crystals = 0, noise = 0, seed = -1;
                bool sprint = false, crouch = false, monster = false;
                std::string mode = "Menu";
                if (g_pShared)
                {
                    const auto &d = *g_pShared;
                    DisplayPose pose =
                        g_presentation.Sample(std::chrono::duration<double>(now.time_since_epoch()).count());
                    px = pose.x;
                    py = pose.y;
                    pz = pose.z;
                    rotY = pose.heading;
                    monX = pose.monsterX;
                    monZ = pose.monsterZ;
                    speed = pose.speed;
                    objectives = d.objectives;
                    total = d.totalObjectives;
                    crystals = d.crystalCount;
                    noise = d.playerNoise;
                    seed = d.levelSeed;
                    sprint = d.isSprinting != 0;
                    crouch = d.isCrouching != 0;
                    monster = d.monsterActive != 0;
                    int levelIndex = MapLevelIndex(d.currentSceneIndex);
                    int finishedIndex = MapLevelIndex(d.runSceneIndex);
                    if (levelIndex >= 0 && d.playerReady)
                        mode = kMapLevelNames[levelIndex];
                    else if (d.timerState == 2 && finishedIndex >= 0)
                        mode = kMapLevelNames[finishedIndex];
                    if (d.cameraMode)
                        mode = d.cameraBright ? "Camera Bright" : "Camera";
                    else if (d.hideAndSeekMode)
                        mode = d.hideAndSeekOutside ? "Hide & Seek Outside" : "Hide & Seek";
                    else if (d.timerMode)
                        mode = "Timer";
                    else if (d.ghostMode)
                        mode = "Ghost";
                    else if (d.classicBonus)
                        mode = "Classic Bonus";
                    else if (total > 0)
                        mode = "Classic";
                }
                if (g_pShared && !(g_pShared->replay3dState >= 1 && g_pShared->replay3dState <= 3))
                {
                    auto &d = *g_pShared;
                    RenderMinimapWindow(px, pz, rotY, monX, monZ, d.crystalCount, d.wallCount,
                                        d.pathCornerCount, d.pathCorners, d.mapMinX, d.mapMaxX, d.mapMinZ,
                                        d.mapMaxZ, d.mapBoundsValid != 0, monster, g_showPath);
                }
                g_validityBadgeDrawn = false;
                g_validityBadgeBounds = {};
                RenderTimerOverlay(mode);
                if (!suite_hud::active && !suite_clip::open)
                    suite_input::LiveHud();
                g_plannerTabActive = false;
                RenderMainWindow(mode, seed);
                inspector_ui::RenderOverlay();
                inspector_ui::FinishFrame();
                if (g_quit && DeferExit())
                    g_quit = false;
                RenderRunExperience();
                suite_hud::Render();
                suite_clip::Render();
                suite_ui::RenderLifecycle(visible, introBlocked, introReady);
                bool updateWasOpen = suite_update_ui::Open();
                suite_update_ui::Overlays(suite_update::service.View(), updateBlocked);
                if (updateWasOpen != suite_update_ui::Open())
                {
                    SetOverlayInteractive(MenuInputRequested());
                    binding.Invalidate();
                }
                bool soundFocused =
                    GetForegroundWindow() == g_hWnd && g_overlayInteractive && !suite_ui::lifecycle.Exiting();
                g_interfaceAudio.Policy(
                    (suite_ui::AudioAllowed(suite_ui::preferences, MenuInputRequested(), soundFocused,
                                            ActiveSpeedrun()) ||
                     (visible && !ActiveSpeedrun() && GetForegroundWindow() == g_gameWnd &&
                      suite_ui::preferences.sounds &&
                      (suite_ui::LifecycleNow() - suite_ui::gameToneAt < .16 ||
                       inspector_fx::feedback.Window(suite_ui::LifecycleNow(), inspector_fx::preferences,
                                                     suite_ui::preferences)))),
                    suite_ui::preferences.volume);
                auto inspectorCue = inspector_fx::feedback.Take(
                    suite_ui::LifecycleNow(),
                    visible && !ActiveSpeedrun() && (soundFocused || GetForegroundWindow() == g_gameWnd) &&
                        inspector_fx::feedback.Window(suite_ui::LifecycleNow(), inspector_fx::preferences,
                                                      suite_ui::preferences));
                auto interfaceCue = suite_ui::ConsumeTone();
                if (inspectorCue.tone != suite_ui::Tone::None)
                    g_interfaceAudio.Submit(inspectorCue.tone, inspectorCue.gain);
                else
                    g_interfaceAudio.Submit(interfaceCue);
                SyncCommands(visible);
                FlushCommands();
                ImGui::Render();
                g_resetLayout = false;
                binding.Refresh(OverlayRequested());
                // UI buttons may have changed input mode during this frame.
                // Recreate outside the ImGui frame; do not re-show an outgoing
                // composition menu after it has returned focus to the game.
                bool surfaceReady = g_surfaceInteractive == MenuInputRequested();
                if (surfaceReady)
                    binding.Apply();
                if (surfaceReady && binding.state.ShouldShow())
                {
                    const float clear[4] = {0, 0, 0, 0};
                    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
                    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
                    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
                    if (g_surfaceInteractive && g_captureOverlayEnabled && g_alphaCompositing)
                    {
                        g_captureOverlay.RenderMenu(g_pd3dDevice, g_pd3dDeviceContext, binding.clientRect,
                                                    g_wc);
                        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
                        if (g_captureOverlay.Active())
                            g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear);
                    }
                    HRESULT present =
                        g_surfaceInteractive
                            ? g_pSwapChain->Present(0, 0)
                            : (g_layeredHud.Present(
                                   g_pd3dDevice, g_pd3dDeviceContext, g_hWnd, g_gameWnd, g_wc,
                                   binding.clientRect,
                                   HudContentRegions(*ImGui::GetDrawData(), int(g_layeredHud.Width()),
                                                     int(g_layeredHud.Height())),
                                   g_validityBadgeBounds, g_captureOverlay.Active() ? 0 : 255)
                                   ? S_OK
                                   : E_FAIL);
                    suite_ui::CommitLifecycleFrame(present == S_OK, GetTickCount64() / 1000.0);
                    if (!g_surfaceInteractive && SUCCEEDED(present))
                    {
                        g_captureOverlay.Publish(g_layeredHud);
                        auto region = g_layeredHud.PresentedRegion();
                        if (region.Width() > 0 && region.Height() > 0)
                        {
                            binding.SetContentRegion(region);
                            binding.Apply();
                        }
                        g_lastHudBytes = g_layeredHud.ReadbackBytes();
                        g_lastHudSurfaces = g_layeredHud.SurfaceCount();
                        g_lastHudUpload = g_layeredHud.UploadMs();
                    }
                    if (SUCCEEDED(present) && g_validityBadgeDrawn)
                        g_badgePresentedTick =
                            g_surfaceInteractive ? GetTickCount64() : g_layeredHud.LastBadgePresented();
                    if (present == DXGI_STATUS_OCCLUDED)
                    {
                        occluded = true;
                        nextFrame = now + std::chrono::milliseconds(200);
                    }
                    else if (FAILED(present))
                    {
                        ReleaseSurface();
                        nextSurface = tick + 250;
                    }
                }
            }
            else if (!occluded)
            {
                if (g_surfaceInteractive)
                    CreateRenderTarget();
                else
                {
                    ReleaseSurface();
                    nextSurface = tick + 250;
                }
                nextFrame = now + std::chrono::milliseconds(100);
            }
            (frameWasInteractive ? g_menuFrameCost : g_hudFrameCost)
                .Observe(std::chrono::duration<double, std::milli>(Clock::now() - frameStarted).count());
        }
        auto deadline =
            Clock::now() + std::chrono::milliseconds(allowed && g_hotkeysRegistered != 127 ? 20 : 100);
        if (visible && g_pd3dDevice)
            deadline = std::min(deadline, nextFrame);
        auto duration = deadline - Clock::now();
        if (duration > Clock::duration::zero() && frameTimer.Get())
        {
            LARGE_INTEGER due{};
            due.QuadPart = -std::max<int64_t>(
                1, std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count() / 100);
            if (SetWaitableTimer(frameTimer.Get(), &due, 0, nullptr, nullptr, FALSE))
            {
                HANDLE timer = frameTimer.Get();
                MsgWaitForMultipleObjectsEx(1, &timer, 100, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                continue;
            }
        }
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        MsgWaitForMultipleObjectsEx(0, nullptr,
                                    static_cast<DWORD>(std::clamp<long long>(remaining + 1, 1, 100)),
                                    QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    g_interfaceAudio.Shutdown();
    ClearTrainerCommands();
    CloseSharedMemory();
    SavePreferencesNow();
    g_settings.Shutdown();
    practice::g_replayArchive.Shutdown();
    if (io.IniFilename)
        ImGui::SaveIniSettingsToDisk(io.IniFilename);
    ReleaseSurface();
    if (gameExited)
    {
        auto rgb = [](ImVec4 color) {
            return RGB(int(std::clamp(color.x, 0.f, 1.f) * 255), int(std::clamp(color.y, 0.f, 1.f) * 255),
                       int(std::clamp(color.z, 0.f, 1.f) * 255));
        };
        ShowSuiteExitGraphic(suite_ui::preferences.launchGraphic, suite_ui::preferences.motion,
                             rgb(g_mainBgCol), rgb(g_headerCol), rgb(g_textCol),
                             std::clamp(g_dpiScale, .75f, 2.f));
    }
    ImGui::DestroyContext();
    UnregisterClassW(g_wc.lpszClassName, g_wc.hInstance);
    g_binding = nullptr;
    return 0;
}
