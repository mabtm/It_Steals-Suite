#pragma once
#include <d3d11.h>
#include <windows.h>
#include <array>
#include <cstring>
#include <chrono>
#include "HudSurfaceBounds.h"
// Render once; transfer non-overlapping occupied regions with true panel alpha.
class LayeredHud {
    struct Surface {
        ID3D11Texture2D* readback[2]{};bool pending[2]{},badge[2]{};int readIndex=0,writeIndex=0;
        HudRegion queued[2],presented;HWND window=nullptr;bool owned=false;
        HDC dc=nullptr;HBITMAP bitmap=nullptr;HGDIOBJ previous=nullptr;void* pixels=nullptr;UINT width=0,height=0;
        void ResetBuffers(){
            for(auto& texture:readback)if(texture){texture->Release();texture=nullptr;}
            pending[0]=pending[1]=false;readIndex=writeIndex=0;
            if(dc&&previous)SelectObject(dc,previous);
            previous=nullptr;if(bitmap){DeleteObject(bitmap);bitmap=nullptr;}if(dc){DeleteDC(dc);dc=nullptr;}pixels=nullptr;width=height=0;
        }
        void Reset(){ResetBuffers();if(owned&&window&&IsWindow(window))DestroyWindow(window);window=nullptr;owned=false;presented={};}
        bool Prepare(ID3D11Device* device,UINT w,UINT h){
            if(w==width&&h==height&&readback[0]&&readback[1]&&pixels)return true;
            ResetBuffers();width=w;height=h;
            D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            for(auto& texture:readback)if(FAILED(device->CreateTexture2D(&desc,nullptr,&texture)))return false;
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-static_cast<LONG>(h);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
            dc=CreateCompatibleDC(nullptr);if(!dc)return false;
            bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);if(!bitmap||!pixels)return false;
            previous=SelectObject(dc,bitmap);return previous&&previous!=HGDI_ERROR;
        }
    };
    ID3D11Texture2D* color=nullptr;std::array<Surface,4> surfaces{};size_t active=0;
    ULONGLONG lastPresented=0,lastBadgePresented=0;UINT width=0,height=0;double uploadMs=0;
public:
    ~LayeredHud(){Reset();}
    void Reset(){for(auto& s:surfaces)s.Reset();active=0;lastPresented=lastBadgePresented=0;uploadMs=0;if(color){color->Release();color=nullptr;}width=height=0;}
    bool Resize(ID3D11Device* device,UINT w,UINT h,ID3D11RenderTargetView** view){
        Reset();width=std::max(1u,w);height=std::max(1u,h);D3D11_TEXTURE2D_DESC desc{};desc.Width=width;desc.Height=height;desc.MipLevels=desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
        return SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&color))&&SUCCEEDED(device->CreateRenderTargetView(color,nullptr,view));
    }
    void SyncVisibility(bool show,const RECT& gameClient){
        for(size_t i=1;i<active;i++){auto& s=surfaces[i];if(!s.window)continue;
            if(!show){if(IsWindowVisible(s.window))ShowWindow(s.window,SW_HIDE);continue;}if(!s.presented.Width()||!s.presented.Height())continue;
            RECT actual{};GetWindowRect(s.window,&actual);int x=gameClient.left+s.presented.left,y=gameClient.top+s.presented.top;
            if(!IsWindowVisible(s.window)||actual.left!=x||actual.top!=y)SetWindowPos(s.window,nullptr,x,y,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_SHOWWINDOW);
        }
    }
    bool Present(ID3D11Device* device,ID3D11DeviceContext* context,HWND mainWindow,HWND game,const WNDCLASSEXW& wc,const RECT& gameClient,const std::vector<HudRegion>& regions,HudRegion badgeBounds){
        if(!color||regions.empty()||regions.size()>surfaces.size())return false;
        auto started=std::chrono::steady_clock::now();
        for(size_t i=regions.size();i<active;i++)surfaces[i].Reset();
        active=regions.size();bool copied=false;
        for(size_t i=0;i<active;i++){
            auto& s=surfaces[i];auto region=ClampHudRegion(regions[i],int(width),int(height));if(!region.Width()||!region.Height())return false;
            if(i==0)s.window=mainWindow;
            else if(!s.window){s.window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,wc.lpszClassName,L"It Steals HUD",WS_POPUP,gameClient.left,gameClient.top,1,1,game,nullptr,wc.hInstance,nullptr);s.owned=true;if(!s.window)return false;constexpr LONG_PTR required=WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE;if((GetWindowLongPtrW(s.window,GWL_EXSTYLE)&required)!=required)return false;}
            if(!s.Prepare(device,UINT(region.Width()),UINT(region.Height())))return false;
            if(s.pending[s.readIndex]){
                D3D11_MAPPED_SUBRESOURCE mapped{};HRESULT hr=context->Map(s.readback[s.readIndex],0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
                if(SUCCEEDED(hr)){
                    for(UINT y=0;y<s.height;y++)std::memcpy(static_cast<unsigned char*>(s.pixels)+size_t(y)*s.width*4,static_cast<const unsigned char*>(mapped.pData)+size_t(y)*mapped.RowPitch,s.width*4);
                    context->Unmap(s.readback[s.readIndex],0);int slot=s.readIndex;s.pending[slot]=false;s.readIndex=(slot+1)%2;
                    auto frame=s.queued[slot];POINT destination{gameClient.left+frame.left,gameClient.top+frame.top},source{};SIZE size{static_cast<LONG>(s.width),static_cast<LONG>(s.height)};
                    BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};if(!UpdateLayeredWindow(s.window,nullptr,&destination,&size,s.dc,&source,0,&blend,ULW_ALPHA))return false;
                    lastPresented=GetTickCount64();if(s.badge[slot])lastBadgePresented=lastPresented;s.presented=frame;
                }else if(hr!=DXGI_ERROR_WAS_STILL_DRAWING)return false;
            }
            if(!s.pending[s.writeIndex]){D3D11_BOX box{UINT(region.left),UINT(region.top),0,UINT(region.right),UINT(region.bottom),1};context->CopySubresourceRegion(s.readback[s.writeIndex],0,0,0,0,color,0,&box);s.queued[s.writeIndex]=region;s.badge[s.writeIndex]=badgeBounds.Width()>0&&badgeBounds.left>=region.left&&badgeBounds.top>=region.top&&badgeBounds.right<=region.right&&badgeBounds.bottom<=region.bottom;s.pending[s.writeIndex]=true;s.writeIndex=(s.writeIndex+1)%2;copied=true;}
        }
        if(copied)context->Flush();
        SyncVisibility(true,gameClient);uploadMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();return true;
    }
    ULONGLONG LastPresented()const{return lastPresented;}ULONGLONG LastBadgePresented()const{return lastBadgePresented;}
    HudRegion PresentedRegion()const{return surfaces[0].presented;}
    UINT Width()const{return width;}UINT Height()const{return height;}size_t SurfaceCount()const{return active;}
    uint64_t ReadbackBytes()const{uint64_t bytes=0;for(size_t i=0;i<active;i++)bytes+=uint64_t(surfaces[i].width)*surfaces[i].height*4;return bytes;}
    double UploadMs()const{return uploadMs;}
};
