#pragma once
#include <cassert>
#include <cstdint>
#include <vector>
using UINT=unsigned;using DWORD=uint32_t;using LONG=int32_t;using BOOL=int;
struct RECT{LONG left=0,top=0,right=0,bottom=0;};struct FakeGame{bool alive=true,iconic=false;int width=800,height=600;};using HWND=FakeGame*;
struct BITMAPINFOHEADER{DWORD biSize=0;LONG biWidth=0,biHeight=0;uint16_t biPlanes=0,biBitCount=0;DWORD biCompression=0;};struct BITMAPINFO{BITMAPINFOHEADER bmiHeader;};
struct FakeBitmap{int w,h;std::vector<uint32_t> pixels;};struct FakeDC{bool source;FakeBitmap* selected=nullptr;};using HDC=FakeDC*;using HBITMAP=FakeBitmap*;using HGDIOBJ=void*;
inline HGDIOBJ HGDI_ERROR=reinterpret_cast<void*>(-1);inline constexpr DWORD BI_RGB=0,DIB_RGB_COLORS=0,SRCCOPY=0;inline constexpr int HALFTONE=1;
inline int failure=0,liveDC=0,liveBitmap=0,captures=0;inline bool flushed=false;
inline BOOL IsWindow(HWND w){return w&&w->alive;}inline BOOL IsIconic(HWND w){return w->iconic;}
inline BOOL GetClientRect(HWND w,RECT* r){*r={0,0,w->width,w->height};return 1;}
inline HDC GetDC(HWND){if(failure==1)return nullptr;liveDC++;return new FakeDC{true};}
inline int ReleaseDC(HWND,HDC dc){assert(dc&&dc->source);liveDC--;delete dc;return 1;}
inline HDC CreateCompatibleDC(HDC source){assert(source&&source->source);if(failure==2)return nullptr;liveDC++;return new FakeDC{false};}
inline BOOL DeleteDC(HDC dc){assert(dc&&!dc->source&&!dc->selected);liveDC--;delete dc;return 1;}
inline HBITMAP CreateDIBSection(HDC source,const BITMAPINFO* info,UINT,void** data,void*,DWORD){assert(source->source&&info->bmiHeader.biBitCount==32&&info->bmiHeader.biHeight<0);if(failure==3)return nullptr;auto b=new FakeBitmap{int(info->bmiHeader.biWidth),int(-info->bmiHeader.biHeight),{}};b->pixels.resize(size_t(b->w)*b->h);*data=b->pixels.data();liveBitmap++;return b;}
inline HGDIOBJ SelectObject(HDC dc,HGDIOBJ item){if(failure==4)return HGDI_ERROR;if(item==reinterpret_cast<void*>(1)){assert(dc->selected);dc->selected=nullptr;return reinterpret_cast<void*>(1);}assert(!dc->selected);dc->selected=static_cast<FakeBitmap*>(item);return reinterpret_cast<void*>(1);}
inline BOOL DeleteObject(HBITMAP b){assert(b);liveBitmap--;delete b;return 1;}
inline int SetStretchBltMode(HDC,int){return 1;}inline BOOL SetBrushOrgEx(HDC,int,int,void*){return 1;}
inline BOOL StretchBlt(HDC dest,int,int,int w,int h,HDC source,int x,int y,int cropW,int cropH,DWORD){assert(source->source&&dest->selected&&w<=384&&h<=256&&x>=0&&y>=0&&cropW<=800&&cropH<=600);captures++;flushed=false;if(failure==5)return 0;for(int i=0;i<w*h;i++)dest->selected->pixels[i]=failure==6?0:(i%w<w/2?0x204080:0xa0c0e0);return 1;}
inline BOOL GdiFlush(){flushed=true;return failure==9?0:1;}
