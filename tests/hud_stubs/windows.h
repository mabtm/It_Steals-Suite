#pragma once
#include <cstdint>
#include <vector>
#include <memory>
#include <cstring>
#include <cassert>
using UINT=unsigned;using DWORD=uint32_t;using LONG=int32_t;using LONG_PTR=intptr_t;using BYTE=uint8_t;using BOOL=int;using ULONGLONG=uint64_t;using HRESULT=int32_t;using HINSTANCE=void*;
inline constexpr HRESULT S_OK=0,E_FAIL=-1,DXGI_ERROR_WAS_STILL_DRAWING=-2;
#define SUCCEEDED(x) ((x)>=0)
#define FAILED(x) ((x)<0)
struct POINT{LONG x,y;};struct SIZE{LONG cx,cy;};struct RECT{LONG left,top,right,bottom;};
struct FakeWindow{bool alive=true,visible=false;LONG_PTR styles=0;FakeWindow* owner=nullptr;RECT rect{};int uploads=0;BYTE blend=0;std::vector<uint32_t> pixels;};using HWND=FakeWindow*;
inline std::vector<std::unique_ptr<FakeWindow>> fakeWindows;inline ULONGLONG fakeClock=100;
inline constexpr DWORD WS_EX_LAYERED=1,WS_EX_TRANSPARENT=2,WS_EX_NOACTIVATE=4,WS_EX_TOOLWINDOW=8,WS_POPUP=16;
inline constexpr int SW_HIDE=0,GWL_EXSTYLE=0;inline constexpr DWORD SWP_NOSIZE=1,SWP_NOZORDER=2,SWP_NOACTIVATE=4,SWP_SHOWWINDOW=8;
struct WNDCLASSEXW{UINT cbSize=0;const wchar_t* lpszClassName=nullptr;HINSTANCE hInstance=nullptr;};
inline HWND CreateWindowExW(DWORD styles,const wchar_t*,const wchar_t*,DWORD,int x,int y,int w,int h,HWND owner,void*,HINSTANCE,void*){auto window=std::make_unique<FakeWindow>();window->styles=styles;window->owner=owner;window->rect={x,y,x+w,y+h};auto result=window.get();fakeWindows.push_back(std::move(window));return result;}
inline BOOL IsWindow(HWND w){return w&&w->alive;}inline BOOL IsWindowVisible(HWND w){return IsWindow(w)&&w->visible;}inline BOOL DestroyWindow(HWND w){assert(IsWindow(w));w->alive=w->visible=false;return 1;}inline LONG_PTR GetWindowLongPtrW(HWND w,int){return w->styles;}
inline BOOL ShowWindow(HWND w,int value){assert(IsWindow(w));w->visible=value!=SW_HIDE;return 1;}inline BOOL GetWindowRect(HWND w,RECT* r){assert(IsWindow(w));*r=w->rect;return 1;}
inline BOOL SetWindowPos(HWND w,HWND,int x,int y,int width,int height,UINT flags){assert(IsWindow(w)&&(flags&SWP_NOACTIVATE));int oldW=w->rect.right-w->rect.left,oldH=w->rect.bottom-w->rect.top;w->rect={x,y,x+((flags&SWP_NOSIZE)?oldW:width),y+((flags&SWP_NOSIZE)?oldH:height)};if(flags&SWP_SHOWWINDOW)w->visible=true;return 1;}
inline ULONGLONG GetTickCount64(){return fakeClock;}
struct BITMAPINFOHEADER{DWORD biSize=0;LONG biWidth=0,biHeight=0;uint16_t biPlanes=0,biBitCount=0;DWORD biCompression=0;};struct BITMAPINFO{BITMAPINFOHEADER bmiHeader;};
struct FakeBitmap{int width,height;std::vector<uint32_t> pixels;};struct FakeDC{FakeBitmap* bitmap=nullptr;};using HDC=FakeDC*;using HBITMAP=FakeBitmap*;using HGDIOBJ=void*;
inline HGDIOBJ HGDI_ERROR=reinterpret_cast<void*>(-1);inline constexpr DWORD BI_RGB=0,DIB_RGB_COLORS=0;inline constexpr BYTE AC_SRC_OVER=0,AC_SRC_ALPHA=1;inline constexpr DWORD ULW_ALPHA=1;
struct BLENDFUNCTION{BYTE BlendOp,BlendFlags,SourceConstantAlpha,AlphaFormat;};
inline HDC CreateCompatibleDC(HDC){return new FakeDC;}
inline HBITMAP CreateDIBSection(HDC,const BITMAPINFO* info,UINT,void** pixels,void*,DWORD){auto b=new FakeBitmap{info->bmiHeader.biWidth,-info->bmiHeader.biHeight,{}};b->pixels.resize(size_t(b->width)*b->height);*pixels=b->pixels.data();return b;}
inline HGDIOBJ SelectObject(HDC dc,HGDIOBJ item){auto previous=dc->bitmap?static_cast<void*>(dc->bitmap):reinterpret_cast<void*>(1);dc->bitmap=reinterpret_cast<FakeBitmap*>(item);return previous;}
inline BOOL DeleteObject(HBITMAP b){delete b;return 1;}inline BOOL DeleteDC(HDC dc){delete dc;return 1;}
inline BOOL UpdateLayeredWindow(HWND window,HDC,const POINT* destination,const SIZE* size,HDC dc,const POINT* source,DWORD,const BLENDFUNCTION* blend,DWORD flags){assert(IsWindow(window)&&source->x==0&&source->y==0&&blend->AlphaFormat==AC_SRC_ALPHA&&flags==ULW_ALPHA&&size->cx==dc->bitmap->width&&size->cy==dc->bitmap->height);window->rect={destination->x,destination->y,destination->x+size->cx,destination->y+size->cy};window->pixels=dc->bitmap->pixels;window->blend=blend->SourceConstantAlpha;window->uploads++;return 1;}
