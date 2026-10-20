#pragma once
#include "windows.h"
#include <cstring>
using HRESULT=int;
#define SUCCEEDED(x) ((x)>=0)
inline constexpr UINT DXGI_FORMAT_B8G8R8A8_UNORM=1,D3D11_USAGE_IMMUTABLE=2,D3D11_BIND_SHADER_RESOURCE=4;
struct D3D11_TEXTURE2D_DESC{UINT Width=0,Height=0,MipLevels=0,ArraySize=0,Format=0;struct{UINT Count=0;}SampleDesc;UINT Usage=0,BindFlags=0;};
struct D3D11_SUBRESOURCE_DATA{const void* pSysMem=nullptr;UINT SysMemPitch=0;};
inline int liveTextures=0,liveViews=0;
struct ID3D11Texture2D{int references=1;std::vector<uint32_t> pixels;void Release(){if(!--references){liveTextures--;delete this;}}};
struct ID3D11ShaderResourceView{ID3D11Texture2D* texture;void Release(){texture->Release();liveViews--;delete this;}};
struct ID3D11Device{
    HRESULT CreateTexture2D(const D3D11_TEXTURE2D_DESC* d,const D3D11_SUBRESOURCE_DATA* initial,ID3D11Texture2D** out){assert(flushed&&d->Width<=384&&d->Height<=256&&d->Usage==D3D11_USAGE_IMMUTABLE&&d->BindFlags==D3D11_BIND_SHADER_RESOURCE&&initial->SysMemPitch==d->Width*4);if(failure==7)return -1;auto t=new ID3D11Texture2D;t->pixels.resize(size_t(d->Width)*d->Height);std::memcpy(t->pixels.data(),initial->pSysMem,t->pixels.size()*4);for(auto p:t->pixels)assert((p>>24)==255);*out=t;liveTextures++;return 0;}
    HRESULT CreateShaderResourceView(ID3D11Texture2D* t,void*,ID3D11ShaderResourceView** out){if(failure==8)return -1;t->references++;*out=new ID3D11ShaderResourceView{t};liveViews++;return 0;}
};
