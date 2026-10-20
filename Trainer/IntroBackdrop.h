#pragma once
#include "IntroBlur.h"
#include <windows.h>
#include <d3d11.h>
// Best-effort GDI game-client crop. It never reads game memory, writes an image
// file, captures another app, or keeps recording after startup. No PrintWindow
// call (which could block on a hung game). Black/unavailable captures fall back.
class IntroBackdrop {
    ID3D11ShaderResourceView* view=nullptr;bool attempted=false;
public:
    IntroBackdrop()=default;IntroBackdrop(const IntroBackdrop&)=delete;
    ~IntroBackdrop(){Reset();}
    void Reset(){if(view){view->Release();view=nullptr;}}
    ID3D11ShaderResourceView* View()const{return view;}
    const char* Status()const{return view?"cached intro crop":attempted?"released / capture unavailable":"not requested";}
    void Capture(ID3D11Device* device,HWND game,int cropW,int cropH){
        if(attempted||!device||!IsWindow(game)||IsIconic(game))return;
        attempted=true;RECT client{};if(!GetClientRect(game,&client)||client.right<1||client.bottom<1)return;
        cropW=std::clamp(cropW,1,int(client.right));cropH=std::clamp(cropH,1,int(client.bottom));
        int w=std::min(384,cropW),h=std::clamp(int(double(cropH)*w/cropW),1,256);
        HDC source=GetDC(game);if(!source)return;
        HDC dest=CreateCompatibleDC(source);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* data=nullptr;HBITMAP bitmap=dest?CreateDIBSection(source,&info,DIB_RGB_COLORS,&data,nullptr,0):nullptr;
        HGDIOBJ old=bitmap?SelectObject(dest,bitmap):nullptr;std::vector<uint32_t> pixels;
        if(old&&old!=HGDI_ERROR&&data){SetStretchBltMode(dest,HALFTONE);SetBrushOrgEx(dest,0,0,nullptr);bool ok=StretchBlt(dest,0,0,w,h,source,(client.right-cropW)/2,(client.bottom-cropH)/2,cropW,cropH,SRCCOPY)!=0;bool flushed=GdiFlush()!=0;if(ok&&flushed){const auto* begin=static_cast<uint32_t*>(data);pixels.assign(begin,begin+size_t(w)*h);}}
        if(old&&old!=HGDI_ERROR)SelectObject(dest,old);
        if(bitmap)DeleteObject(bitmap);
        if(dest)DeleteDC(dest);
        ReleaseDC(game,source);
        bool visible=false;for(auto p:pixels)visible|=(p&0x00ffffffu)!=0;if(!visible||!suite_ui::BlurIntroPixels(pixels,w,h))return;
        D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=1;desc.ArraySize=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.SampleDesc.Count=1;desc.Usage=D3D11_USAGE_IMMUTABLE;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA initial{};initial.pSysMem=pixels.data();initial.SysMemPitch=w*4;
        ID3D11Texture2D* texture=nullptr;if(SUCCEEDED(device->CreateTexture2D(&desc,&initial,&texture))){device->CreateShaderResourceView(texture,nullptr,&view);texture->Release();}
    }
};
