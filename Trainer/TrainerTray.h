#pragma once
#include <windows.h>
#include <shellapi.h>
#include <functional>
#include <algorithm>
#include <string>
#include <string_view>
#include "SuiteBranding.h"

// Independent of the game's owned overlay: it can finish after that HWND dies.
inline void ShowSuiteExitGraphic(bool enabled, bool motion, COLORREF background, COLORREF accent,
                                 COLORREF ink, float scale)
{
    if (!enabled)
        return;
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HBRUSH fill = CreateSolidBrush(background), edge = CreateSolidBrush(accent);
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = L"PracticeSuite.Exit";
    wc.hbrBackground = fill;
    if (!RegisterClassW(&wc))
    {
        DeleteObject(fill);
        DeleteObject(edge);
        return;
    }
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int width = int(460 * scale), height = int(138 * scale);
    HWND window = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT, wc.lpszClassName,
        L"PracticeSuite", WS_POPUP, work.left + (work.right - work.left - width) / 2,
        work.top + (work.bottom - work.top - height) / 2, width, height, nullptr, nullptr, instance, nullptr);
    if (window)
    {
        HFONT title = CreateFontW(-int(24 * scale), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH, L"Segoe UI");
        HFONT body = CreateFontW(-int(15 * scale), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                                 L"Segoe UI");
        SetLayeredWindowAttributes(window, 0, motion ? 0 : 255, LWA_ALPHA);
        ShowWindow(window, SW_SHOWNOACTIVATE);
        ULONGLONG started = GetTickCount64();
        while (IsWindow(window) && GetTickCount64() - started < 650)
        {
            auto elapsed = GetTickCount64() - started;
            if (motion)
                SetLayeredWindowAttributes(
                    window, 0,
                    BYTE(255 * std::clamp(std::min(elapsed / 100., (650 - elapsed) / 180.), 0., 1.)),
                    LWA_ALPHA);
            HDC dc = GetDC(window);
            RECT bounds{0, 0, width, height}, stripe{0, 0, int(5 * scale), height};
            FillRect(dc, &bounds, fill);
            FillRect(dc, &stripe, edge);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, ink);
            auto original = SelectObject(dc, title);
            RECT line{int(28 * scale), int(25 * scale), width - int(20 * scale), int(66 * scale)};
            DrawTextW(dc, L"PRACTICE SUITE", -1, &line, DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, body);
            SetTextColor(dc, accent);
            line.top = int(77 * scale);
            line.bottom = height;
            DrawTextW(dc, L"Session closed. See you next run.", -1, &line, DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(dc, original);
            ReleaseDC(window, dc);
            MSG message{};
            while (PeekMessageW(&message, window, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            Sleep(16);
        }
        DestroyWindow(window);
        DeleteObject(title);
        DeleteObject(body);
    }
    UnregisterClassW(wc.lpszClassName, instance);
    DeleteObject(fill);
    DeleteObject(edge);
}

// A stable control window survives game/overlay recreation. It never covers
// the game and gives a waiting trainer an explicit, reachable Exit command.
class TrainerTray
{
    HWND window = nullptr;
    UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    bool present = false, canOpen = false;
    ULONGLONG nextRetry = 0;
    std::wstring tooltip;
    std::function<void()> open, quit, launch;
    std::wstring brand =
        L"PracticeSuite v" +
        std::wstring(suite_brand::Version, suite_brand::Version + sizeof(suite_brand::Version) - 1) +
        L" / mab";
    static constexpr UINT callback = WM_APP + 42;

    template <size_t N> static void CopyText(wchar_t (&destination)[N], std::wstring_view source)
    {
        size_t length = (std::min)(N - 1, source.size());
        std::copy_n(source.data(), length, destination);
        destination[length] = L'\0';
    }

    NOTIFYICONDATAW Data() const
    {
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof(data);
        data.hWnd = window;
        data.uID = 1;
        data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        data.uCallbackMessage = callback;
        data.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
        if (!data.hIcon)
            data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        CopyText(data.szTip, tooltip);
        return data;
    }

    void Add()
    {
        auto data = Data();
        present = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
        if (present)
        {
            data.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &data);
        }
        nextRetry = GetTickCount64() + 2000;
    }

    static LRESULT CALLBACK Proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        auto *self = reinterpret_cast<TrainerTray *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            self = static_cast<TrainerTray *>(reinterpret_cast<CREATESTRUCTW *>(lParam)->lpCreateParams);
            self->window = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self)
        {
            if (message == self->taskbarCreated)
            {
                self->present = false;
                self->Add();
                return 0;
            }
            if (message == callback && LOWORD(lParam) == WM_CONTEXTMENU)
            {
                POINT cursor{};
                GetCursorPos(&cursor);
                HMENU menu = CreatePopupMenu();
                if (menu)
                {
                    AppendMenuW(menu, MF_STRING | (self->canOpen ? 0 : MF_GRAYED), 1,
                                L"Open PracticeSuite menu");
                    AppendMenuW(menu, MF_STRING, 2, L"Exit PracticeSuite");
                    if (!self->canOpen && self->launch)
                        AppendMenuW(menu, MF_STRING, 3, L"Start It Steals");
                    SetForegroundWindow(hwnd);
                    UINT selected = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                                                   cursor.x, cursor.y, 0, hwnd, nullptr);
                    DestroyMenu(menu);
                    PostMessageW(hwnd, WM_NULL, 0, 0);
                    if (selected == 1 && self->canOpen)
                        self->open();
                    if (selected == 2)
                        self->quit();
                    if (selected == 3 && self->launch)
                        self->launch();
                }
                return 0;
            }
            if (message == WM_CLOSE)
            {
                self->quit();
                return 0;
            }
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

  public:
    TrainerTray(std::function<void()> onOpen, std::function<void()> onQuit,
                std::function<void()> onLaunch = {})
        : open(onOpen), quit(onQuit), launch(onLaunch)
    {
        WNDCLASSW type{};
        type.lpfnWndProc = Proc;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"ItStealsPracticeControl.v6";
        if (RegisterClassW(&type))
        {
            window = CreateWindowExW(WS_EX_TOOLWINDOW, type.lpszClassName, L"It Steals PracticeSuite",
                                     WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, type.hInstance, this);
            tooltip = brand + L" | waiting for It Steals";
            if (window)
                Add();
        }
    }

    TrainerTray(const TrainerTray &) = delete;

    ~TrainerTray()
    {
        if (present)
        {
            auto data = Data();
            Shell_NotifyIconW(NIM_DELETE, &data);
        }
        if (window)
            DestroyWindow(window);
        UnregisterClassW(L"ItStealsPracticeControl.v6", GetModuleHandleW(nullptr));
    }

    void Update(const std::string &status, bool gameExists)
    {
        canOpen = gameExists;
        std::wstring next = brand + L" | " + std::wstring(status.begin(), status.end());
        if (next != tooltip)
        {
            tooltip = next;
            if (present)
            {
                auto data = Data();
                if (!Shell_NotifyIconW(NIM_MODIFY, &data))
                    present = false;
            }
        }
        if (window && !present && GetTickCount64() >= nextRetry)
            Add();
    }

    void LaunchWarning()
    {
        if (!present)
            return;
        auto data = Data();
        data.uFlags = NIF_INFO;
        data.dwInfoFlags = NIIF_WARNING;
        CopyText(data.szInfoTitle, L"It Steals could not be started");
        CopyText(
            data.szInfo,
            L"Run PracticeSuite Setup or start the game manually. Details are available in Diagnostics.");
        Shell_NotifyIconW(NIM_MODIFY, &data);
    }
};
