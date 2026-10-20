#pragma once
#include "ClipComposition.h"
#include "PngSequence.h"
#include "ClipFrameWire.h"
#include <deque>
#ifdef _WIN32
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include <d3d11.h>
#include <windows.h>
namespace suite_clip {
class FrameBridge {
    struct Pending {int slot=0,width=0,height=0;uint32_t serial=0;uint64_t frame=0;double seconds=0;int64_t run=0;ULONGLONG since=0;};
    HANDLE mapping=nullptr,mutex=nullptr;uint8_t* data=nullptr;DWORD pid=0;
    std::deque<Pending> waiting;
    template<class T>T Read(int o)const{T v;std::memcpy(&v,data+o,sizeof(v));return v;}
    template<class T>void Write(int o,T v){std::memcpy(data+o,&v,sizeof(v));}
    bool Lock(){DWORD r=WaitForSingleObject(mutex,0);return r==WAIT_OBJECT_0||r==WAIT_ABANDONED;}
    bool Valid()const{return Read<uint32_t>(0)==0x504c4349&&Read<int>(4)==FrameVersion&&Read<int>(8)==FrameSlots&&Read<int>(12)==FrameSlotBytes;}
public:
    ~FrameBridge(){Close();}
    bool pending=false;CameraPose renderedPose;uint64_t renderedFrame=0;int readbackMode=0;
    double renderMilliseconds=0,readbackMilliseconds=0,transferMilliseconds=0;
    size_t Count()const{return waiting.size();}
    bool Available()const{return data&&waiting.size()<FrameSlots;}
    void Close(){
        if(data&&mutex&&Lock()){
            for(const auto& p:waiting){int o=FrameOffset(p.slot);if(Read<uint32_t>(o+8)==p.serial){uint32_t cancel=p.serial+1u;if(!cancel)cancel=1;Write(o+16,-1);Write(o+8,cancel);}}
            ReleaseMutex(mutex);
        }
        waiting.clear();pending=false;if(data)UnmapViewOfFile(data);if(mapping)CloseHandle(mapping);if(mutex)CloseHandle(mutex);data=nullptr;mapping=mutex=nullptr;pid=0;
    }
    bool Connect(DWORD process){
        if(data&&pid==process)return true;
        Close();auto name=L"Local\\ItSteals.Clip.v4."+std::to_wstring(process);
        mapping=OpenFileMappingW(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,name.c_str());if(!mapping)return false;
        data=static_cast<uint8_t*>(MapViewOfFile(mapping,FILE_MAP_READ|FILE_MAP_WRITE,0,0,FrameCapacity));mutex=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,(name+L".Lock").c_str());
        if(!data||!mutex){Close();return false;}pid=process;return true;
    }
    bool RequestIndexed(uint64_t frame,int64_t id,double seconds,int width,int height,uint32_t options,const CameraPose* custom=nullptr,float audioSpeed=0,float audioVolume=1){
        if(custom)custom->Validate();
        if(frame>uint64_t(INT64_MAX)||!data||!Available()||!Lock())return false;
        struct Unlock {HANDLE gate;~Unlock(){ReleaseMutex(gate);}} unlock{mutex};
        bool valid=Valid();
        if(valid){
            int slot=0;for(;slot<FrameSlots;slot++){bool used=false;for(const auto& p:waiting)used|=p.slot==slot;if(!used)break;}
            int o=FrameOffset(slot);uint32_t serial=Read<uint32_t>(o+8)+1u;if(!serial)serial=1;
            waiting.push_back({slot,width,height,serial,frame,seconds,id,GetTickCount64()});
            Write(o+20,width);Write(o+24,height);Write(o+32,seconds);Write(o+40,id);Write(o+48,options);Write(o+52,custom?1:0);Write(o+136,audioSpeed);Write(o+140,audioVolume);Write(o+144,int64_t(frame));
            if(custom){Write(o+56,custom->position.x);Write(o+60,custom->position.y);Write(o+64,custom->position.z);Write(o+68,custom->rotation.x);Write(o+72,custom->rotation.y);Write(o+76,custom->rotation.z);Write(o+80,custom->rotation.w);Write(o+84,custom->fov);}
            Write(o+16,0);Write(o+8,serial);pending=true;
        }
        return valid;
    }
    bool Request(int64_t id,double seconds,int width,int height,uint32_t options,const CameraPose* custom=nullptr,float audioSpeed=0,float audioVolume=1){
        return !pending&&RequestIndexed(0,id,seconds,width,height,options,custom,audioSpeed,audioVolume);
    }
    int Poll(std::vector<uint8_t>& pixels,std::string& error){
        if(waiting.empty()||!data)return 0;
        const auto p=waiting.front();
        if(GetTickCount64()-p.since>10000){error="Game did not provide the requested replay frame";return -1;}
        if(!Lock())return 0;
        try{
            int result=0,o=FrameOffset(p.slot);
            if(Read<uint32_t>(o+12)==p.serial){
                int status=Read<int>(o+16),width=Read<int>(o+20),height=Read<int>(o+24);
                if(status==1&&Valid()&&Read<uint32_t>(o+8)==p.serial&&Read<int64_t>(o+144)==int64_t(p.frame)&&Read<int64_t>(o+40)==p.run&&Read<double>(o+32)==p.seconds&&width==p.width&&height==p.height&&width>=320&&width<=2560&&height>=180&&height<=1440){
                    renderedPose={{Read<float>(o+96),Read<float>(o+100),Read<float>(o+104)},{Read<float>(o+108),Read<float>(o+112),Read<float>(o+116),Read<float>(o+120)},Read<float>(o+124)};renderedPose.Validate();
                    pixels.resize(size_t(width)*height*4);std::memcpy(pixels.data(),data+o+1024,pixels.size());renderedFrame=p.frame;readbackMode=Read<int>(o+152);renderMilliseconds=Read<float>(o+156);readbackMilliseconds=Read<float>(o+160);transferMilliseconds=Read<float>(o+164);result=1;
                }else{
                    size_t n=0;while(n<511&&data[o+256+n])n++;error.assign(reinterpret_cast<const char*>(data+o+256),n);if(error.empty())error="Replay frame identity or rendering failed";result=-1;
                }
                waiting.pop_front();pending=!waiting.empty();
            }
            ReleaseMutex(mutex);return result;
        }catch(...){ReleaseMutex(mutex);throw;}
    }
};
class Surface {
    struct ReadSlot {
        ID3D11Texture2D* texture=nullptr;ID3D11Query* query=nullptr;uint64_t frame=0;ULONGLONG since=0;
        bool gif=false,transparent=false;
    };
    std::array<ReadSlot,FrameSlots> readbacks{};std::deque<int> reading;
    ID3D11Texture2D *target = nullptr, *staging = nullptr, *scene = nullptr, *preview = nullptr;
    ID3D11RenderTargetView *rtv = nullptr;
    ID3D11ShaderResourceView *sceneView = nullptr, *previewView = nullptr;
    ID3D11Device *device = nullptr;
    ImGuiContext *context = nullptr;
    ImFontAtlas *atlas = nullptr;
    int width = 0, height = 0;
    template <class T> void Release(T *&p) {
        if (p) {
            p->Release();
            p = nullptr;
        }
    }
    void Check(HRESULT hr) {
        if (FAILED(hr))
            throw std::runtime_error("Offscreen clip renderer could not allocate or read a frame");
    }
    void Prepare(ID3D11Device *d, int w, int h) {
        if (device == d && width == w && height == h && target)
            return;
        Reset();
        device = d;
        device->AddRef();
        width = w;
        height = h;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        Check(d->CreateTexture2D(&desc, nullptr, &target));
        Check(d->CreateRenderTargetView(target, nullptr, &rtv));
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        Check(d->CreateTexture2D(&desc, nullptr, &scene));
        Check(d->CreateShaderResourceView(scene, nullptr, &sceneView));
        Check(d->CreateTexture2D(&desc, nullptr, &preview));
        Check(d->CreateShaderResourceView(preview, nullptr, &previewView));
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        Check(d->CreateTexture2D(&desc, nullptr, &staging));
    }

  public:
    ~Surface() { Reset(); }
    void Reset() {
        CancelReadbacks();
        if (context) {
            auto current = ImGui::GetCurrentContext();
            ImGui::SetCurrentContext(context);
            ImGui::GetIO().BackendRendererUserData = nullptr;
            ImGui::DestroyContext(context);
            ImGui::SetCurrentContext(current);
            context = nullptr;
            atlas = nullptr;
        }
        Release(target);
        Release(staging);
        Release(scene);
        Release(preview);
        Release(rtv);
        Release(sceneView);
        Release(previewView);
        Release(device);
        width = height = 0;
    }
    void CancelReadbacks(){
        reading.clear();for(auto& slot:readbacks){Release(slot.query);Release(slot.texture);slot={};}
    }
    bool CanQueue()const{return reading.size()<FrameSlots;}
    size_t PendingReadbacks()const{return reading.size();}
    void QueueReadback(ID3D11Device* d,ID3D11DeviceContext* gpu,uint64_t frame,const Options& o){
        if(!CanQueue()||!target||device!=d||o.width!=width||o.height!=height)throw std::runtime_error("Clip GPU readback window is full or mismatched");
        int index=0;for(;index<FrameSlots;index++)if(std::find(reading.begin(),reading.end(),index)==reading.end())break;
        auto& slot=readbacks[size_t(index)];
        if(!slot.texture){D3D11_TEXTURE2D_DESC desc{};target->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;Check(d->CreateTexture2D(&desc,nullptr,&slot.texture));D3D11_QUERY_DESC query{D3D11_QUERY_EVENT,0};Check(d->CreateQuery(&query,&slot.query));}
        slot.frame=frame;slot.since=GetTickCount64();slot.gif=o.format==1;slot.transparent=o.transparent;
        gpu->CopyResource(slot.texture,target);gpu->End(slot.query);reading.push_back(index);
        // Submit the copy without waiting for it, also when no Present follows.
        gpu->Flush();
    }
    template<class Acquire> int PollReadback(ID3D11DeviceContext* gpu,std::vector<uint8_t>& pixels,uint64_t& frame,double& milliseconds,Acquire&& acquire){
        if(reading.empty())return 0;
        auto& slot=readbacks[size_t(reading.front())];
        if(GetTickCount64()-slot.since>10000)throw std::runtime_error("Clip GPU composition readback timed out");
        BOOL ready=FALSE;HRESULT status=gpu->GetData(slot.query,&ready,sizeof(ready),D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if(status==S_FALSE||(!ready&&SUCCEEDED(status)))return 0;
        Check(status);
        D3D11_MAPPED_SUBRESOURCE mapped{};status=gpu->Map(slot.texture,0,D3D11_MAP_READ,D3D11_MAP_FLAG_DO_NOT_WAIT,&mapped);
        if(status==DXGI_ERROR_WAS_STILL_DRAWING)return 0;
        Check(status);
        try{pixels=acquire(size_t(width)*height*4);for(int y=0;y<height;y++)std::memcpy(pixels.data()+size_t(y)*width*4,static_cast<const uint8_t*>(mapped.pData)+size_t(y)*mapped.RowPitch,size_t(width)*4);}
        catch(...){gpu->Unmap(slot.texture,0);throw;}
        gpu->Unmap(slot.texture,0);if(slot.gif)Quantize(pixels.data(),size_t(width)*height);if(slot.transparent)StraightAlpha(pixels);
        frame=slot.frame;milliseconds=double(GetTickCount64()-slot.since);reading.pop_front();return 1;
    }
    ImTextureID View() const { return reinterpret_cast<ImTextureID>(previewView); }
    std::vector<uint8_t> Render(ID3D11Device *d, ID3D11DeviceContext *gpu, const Options &o, const Content &content,
                                double at, const std::vector<uint8_t> *scenePixels, bool readback = true, std::vector<uint8_t> reusable = {}, bool deferred = false) {
        Prepare(d, o.width, o.height);
        if (scenePixels) {
            if (scenePixels->size() != size_t(width) * height * 4)
                throw std::runtime_error("Unexpected replay frame size");
            gpu->UpdateSubresource(scene, 0, nullptr, scenePixels->data(), UINT(width * 4), 0);
        }
        ImGuiContext *previous = ImGui::GetCurrentContext();
        auto &source = ImGui::GetIO();
        if (!source.Fonts->IsBuilt() || !source.BackendRendererUserData)
            throw std::runtime_error("Clip renderer is waiting for GUI fonts / graphics");
        if (atlas != source.Fonts) {
            if (context)
                ImGui::DestroyContext(context);
            context = ImGui::CreateContext(source.Fonts);
            atlas = source.Fonts;
            ImGui::SetCurrentContext(previous);
        }
        void *backend = source.BackendRendererUserData;
        auto flags = source.BackendFlags;
        bool locked = source.Fonts->Locked;
        ID3D11RenderTargetView *oldTarget = nullptr;
        ID3D11DepthStencilView *oldDepth = nullptr;
        gpu->OMGetRenderTargets(1, &oldTarget, &oldDepth);
        try {
            ImGui::SetCurrentContext(context);
            auto &io = ImGui::GetIO();
            io.IniFilename = io.LogFilename = nullptr;
            io.DisplaySize = {float(width), float(height)};
            io.DisplayFramebufferScale = {1, 1};
            io.DeltaTime = 1.f / o.fps;
            io.BackendRendererUserData = backend;
            io.BackendFlags = flags;
            io.FontDefault = content.font ? content.font : atlas->Fonts[0];
            ImGui::NewFrame();
            Compose(ImGui::GetBackgroundDrawList(), o, content, at,
                    o.scene3d ? reinterpret_cast<ImTextureID>(sceneView) : ImTextureID{});
            ImGui::Render();
            gpu->OMSetRenderTargets(1, &rtv, nullptr);
            float clear[] = {0, 0, 0, o.transparent ? 0.f : 1.f};
            gpu->ClearRenderTargetView(rtv, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            std::vector<uint8_t> pixels=std::move(reusable);
            if (!deferred && (readback || o.format == 1)) {
                gpu->CopyResource(staging, target);
                pixels.resize(size_t(width) * height * 4);
                D3D11_MAPPED_SUBRESOURCE mapped{};
                Check(gpu->Map(staging, 0, D3D11_MAP_READ, 0, &mapped));
                for (int y = 0; y < height; y++)
                    std::memcpy(pixels.data() + size_t(y) * width * 4,
                                static_cast<const uint8_t *>(mapped.pData) + size_t(y) * mapped.RowPitch,
                                size_t(width) * 4);
                gpu->Unmap(staging, 0);
                if (o.format == 1)
                    Quantize(pixels.data(), size_t(width) * height);
                if (o.transparent)
                    StraightAlpha(pixels);
                if(o.format==1)gpu->UpdateSubresource(preview,0,nullptr,pixels.data(),UINT(width*4),0);
                else gpu->CopyResource(preview,target);
            } else
                gpu->CopyResource(preview, target);
            io.BackendRendererUserData = nullptr;
            ImGui::SetCurrentContext(previous);
            atlas->Locked = locked;
            gpu->OMSetRenderTargets(1, &oldTarget, oldDepth);
            Release(oldTarget);
            Release(oldDepth);
            return pixels;
        } catch (...) {
            if (context && context->WithinFrameScope)
                ImGui::EndFrame();
            if (context)
                context->IO.BackendRendererUserData = nullptr;
            ImGui::SetCurrentContext(previous);
            if (atlas)
                atlas->Locked = locked;
            gpu->OMSetRenderTargets(1, &oldTarget, oldDepth);
            Release(oldTarget);
            Release(oldDepth);
            throw;
        }
    }
};
} // namespace suite_clip
#endif
