#pragma once
#include <windows.h>
#include <dwmapi.h>
#include "OverlayPolicy.h"
#include "HudSurfaceBounds.h"

class GameWindowBinding
{
    inline static GameWindowBinding *instance = nullptr;
    DWORD pid = 0;
    HWND game = nullptr, overlay = nullptr;
    HWINEVENTHOOK foregroundHook = nullptr, objectHook = nullptr, locationHook = nullptr,
                  minimizeHook = nullptr;
    bool dirty = true;
    ULONGLONG nextCheck = 0;
    HWND previousForeground = nullptr;
    RECT previousRect{};
    bool previousRectValid = false;
    HudRegion content{};
    bool cropped = false;

    static void CALLBACK Event(HWINEVENTHOOK, DWORD event, HWND window, LONG object, LONG child, DWORD, DWORD)
    {
        // OUTOFCONTEXT callbacks run on the message-loop thread. Keep them free
        // of window mutations, rendering and mutex acquisition (reentrancy).
        if (!instance)
            return;
        if (event == EVENT_SYSTEM_FOREGROUND || event == EVENT_SYSTEM_MINIMIZESTART ||
            event == EVENT_SYSTEM_MINIMIZEEND ||
            (object == OBJID_WINDOW && child == CHILDID_SELF &&
             (window == instance->game || !instance->game)))
            instance->dirty = true;
    }

    struct Candidate
    {
        DWORD pid;
        HWND best = nullptr;
        long long score = 0;
    };

    static BOOL CALLBACK Enumerate(HWND window, LPARAM parameter)
    {
        auto &candidate = *reinterpret_cast<Candidate *>(parameter);
        DWORD windowPid = 0;
        GetWindowThreadProcessId(window, &windowPid);
        if (windowPid != candidate.pid || GetWindow(window, GW_OWNER))
            return TRUE;
        wchar_t name[128]{};
        GetClassNameW(window, name, 128);
        if (wcscmp(name, L"UnityWndClass") != 0)
            return TRUE;
        RECT client{};
        if (!GetClientRect(window, &client))
            return TRUE;
        long long score = static_cast<long long>(client.right) * client.bottom;
        if (IsWindowVisible(window))
            score += 1LL << 40;
        if (!candidate.best || score > candidate.score)
        {
            candidate.best = window;
            candidate.score = score;
        }
        return TRUE;
    }

  public:
    OverlayState state;
    RECT clientRect{};

    explicit GameWindowBinding(DWORD processId)
    {
        instance = this;
        foregroundHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, Event, 0,
                                         0, WINEVENT_OUTOFCONTEXT);
        BindProcess(processId);
    }

    GameWindowBinding(const GameWindowBinding &) = delete;
    GameWindowBinding &operator=(const GameWindowBinding &) = delete;

    ~GameWindowBinding()
    {
        instance = nullptr;
        for (HWINEVENTHOOK hook : {foregroundHook, objectHook, locationHook, minimizeHook})
            if (hook)
                UnhookWinEvent(hook);
    }

    HWND Game() const
    {
        return game;
    }

    void BindProcess(DWORD processId)
    {
        for (HWINEVENTHOOK hook : {objectHook, locationHook, minimizeHook})
            if (hook)
                UnhookWinEvent(hook);
        objectHook = locationHook = minimizeHook = nullptr;
        pid = processId;
        game = nullptr;
        state = {};
        clientRect = {};
        previousRectValid = false;
        dirty = true;
        nextCheck = 0;
        if (!pid)
            return;
        objectHook = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE, nullptr, Event, pid, 0,
                                     WINEVENT_OUTOFCONTEXT);
        locationHook = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
                                       Event, pid, 0, WINEVENT_OUTOFCONTEXT);
        minimizeHook = SetWinEventHook(EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND, nullptr, Event,
                                       pid, 0, WINEVENT_OUTOFCONTEXT);
    }

    bool EventsAvailable() const
    {
        return foregroundHook && objectHook && locationHook && minimizeHook;
    }

    void SetOverlay(HWND window)
    {
        cropped = false;
        content = {};
        overlay = window;
        previousRectValid = false;
        dirty = true;
    }

    void SetContentRegion(HudRegion region)
    {
        if (!cropped || !(region == content))
        {
            content = region;
            cropped = true;
            previousRectValid = false;
        }
    }

    void Invalidate()
    {
        dirty = true;
    }

    void Refresh(bool enabled)
    {
        ULONGLONG now = GetTickCount64();
        HWND foreground = GetForegroundWindow();
        if (foreground != previousForeground || enabled != state.enabled)
            dirty = true;
        previousForeground = foreground;
        if (!dirty && now < nextCheck)
            return;
        dirty = false;
        nextCheck = now + 100; // Fallback for missing/filtered events.
        if (pid && !IsWindow(game))
        {
            Candidate candidate{pid};
            EnumWindows(Enumerate, reinterpret_cast<LPARAM>(&candidate));
            game = candidate.best;
        }
        state = {};
        state.enabled = enabled;
        state.gameExists = game && IsWindow(game);
        if (!state.gameExists)
            return;
        state.gameVisible = IsWindowVisible(game) != FALSE;
        state.minimized = IsIconic(game) != FALSE;
        DWORD cloaked = 0;
        if (SUCCEEDED(DwmGetWindowAttribute(game, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))))
            state.cloaked = cloaked != 0;
        RECT rect{};
        POINT origin{};
        if (GetClientRect(game, &rect) && ClientToScreen(game, &origin))
        {
            clientRect = {origin.x, origin.y, origin.x + rect.right, origin.y + rect.bottom};
            state.clientValid = rect.right > 0 && rect.bottom > 0;
        }
        // Only the actual Unity window and this overlay count. The loader's
        // console or unrelated windows belonging to the same PID do not.
        HWND root = foreground ? GetAncestor(foreground, GA_ROOT) : nullptr;
        state.gameForeground = foreground == game || root == game;
        state.overlayForeground = overlay && (foreground == overlay || root == overlay);
    }

    void Apply()
    {
        if (!overlay || !IsWindow(overlay))
            return;
        bool visible = state.ShouldShow();
        bool currentlyVisible = IsWindowVisible(overlay) != FALSE;
        if (!visible)
        {
            if (currentlyVisible)
                ShowWindow(overlay, SW_HIDE);
            return;
        }
        RECT target = clientRect;
        if (cropped)
        {
            auto region = ClampHudRegion(content, clientRect.right - clientRect.left,
                                         clientRect.bottom - clientRect.top);
            if (region.Width() > 0 && region.Height() > 0)
                target = {clientRect.left + region.left, clientRect.top + region.top,
                          clientRect.left + region.right, clientRect.top + region.bottom};
        }
        bool moved = !previousRectValid || !EqualRect(&target, &previousRect);
        if (moved || !currentlyVisible)
        {
            // The owned popup stays above the game automatically. Do not change
            // TOPMOST on it: Windows may propagate that change to the owner.
            SetWindowPos(overlay, nullptr, target.left, target.top, target.right - target.left,
                         target.bottom - target.top,
                         SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW |
                             (moved ? 0 : SWP_NOMOVE | SWP_NOSIZE));
            previousRect = target;
            previousRectValid = true;
        }
    }
};
