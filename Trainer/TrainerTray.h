#pragma once
#include <windows.h>
#include <shellapi.h>
#include <functional>
#include <string>

// A stable control window survives game/overlay recreation. It never covers
// the game and gives a waiting trainer an explicit, reachable Exit command.
class TrainerTray {
    HWND window = nullptr;
    UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    bool present = false, canOpen = false;
    ULONGLONG nextRetry = 0;
    std::wstring tooltip;
    std::function<void()> open, quit;
    static constexpr UINT callback = WM_APP + 42;
    NOTIFYICONDATAW Data() const {
        NOTIFYICONDATAW data{}; data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;
        data.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;data.uCallbackMessage=callback;
        data.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
        wcsncpy(data.szTip,tooltip.c_str(),127);data.szTip[127]=0;return data;
    }
    void Add() {
        auto data=Data();present=Shell_NotifyIconW(NIM_ADD,&data)!=FALSE;
        if(present) {data.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&data);}
        nextRetry=GetTickCount64()+2000;
    }
    static LRESULT CALLBACK Proc(HWND hwnd,UINT message,WPARAM wParam,LPARAM lParam) {
        auto* self=reinterpret_cast<TrainerTray*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(message==WM_NCCREATE) {
            self=static_cast<TrainerTray*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
            self->window=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));
        }
        if(self) {
            if(message==self->taskbarCreated) {self->present=false;self->Add();return 0;}
            if(message==callback && LOWORD(lParam)==WM_CONTEXTMENU) {
                POINT cursor{};GetCursorPos(&cursor);HMENU menu=CreatePopupMenu();
                if(menu) {
                    AppendMenuW(menu,MF_STRING|(self->canOpen?0:MF_GRAYED),1,L"Open PracticeSuite menu");
                    AppendMenuW(menu,MF_STRING,2,L"Exit PracticeSuite");
                    SetForegroundWindow(hwnd);
                    UINT selected=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY|TPM_RIGHTBUTTON,cursor.x,cursor.y,0,hwnd,nullptr);
                    DestroyMenu(menu);PostMessageW(hwnd,WM_NULL,0,0);
                    if(selected==1 && self->canOpen) self->open();
                    if(selected==2) self->quit();
                }
                return 0;
            }
            if(message==WM_CLOSE) {self->quit();return 0;}
        }
        return DefWindowProcW(hwnd,message,wParam,lParam);
    }
public:
    TrainerTray(std::function<void()> onOpen,std::function<void()> onQuit):open(onOpen),quit(onQuit) {
        WNDCLASSW type{};type.lpfnWndProc=Proc;type.hInstance=GetModuleHandleW(nullptr);type.lpszClassName=L"ItStealsPracticeControl.v6";
        if(RegisterClassW(&type)) {
            window=CreateWindowExW(WS_EX_TOOLWINDOW,type.lpszClassName,L"It Steals PracticeSuite",WS_OVERLAPPED,0,0,0,0,nullptr,nullptr,type.hInstance,this);
            tooltip=L"PracticeSuite 6 | waiting for It Steals";if(window) Add();
        }
    }
    TrainerTray(const TrainerTray&)=delete;
    ~TrainerTray() {
        if(present) {auto data=Data();Shell_NotifyIconW(NIM_DELETE,&data);}
        if(window) DestroyWindow(window);
        UnregisterClassW(L"ItStealsPracticeControl.v6",GetModuleHandleW(nullptr));
    }
    void Update(const std::string& status,bool gameExists) {
        canOpen=gameExists;
        std::wstring next=L"PracticeSuite 6 | "+std::wstring(status.begin(),status.end());
        if(next!=tooltip) {tooltip=next;if(present) {auto data=Data();if(!Shell_NotifyIconW(NIM_MODIFY,&data)) present=false;}}
        if(window && !present && GetTickCount64()>=nextRetry) Add();
    }
};
