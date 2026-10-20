#pragma once
#include "InspectorRuntime.h"

class InspectorClient {
    HANDLE map=nullptr,mutex=nullptr;uint8_t* view=nullptr;inspector::Header identity;
    uint32_t catRevision=0,detailRevision=0,hoverRevision=0,sent=0,sentPointer=0;ULONGLONG nextOpen=0,nextPoll=0;
public:
    ~InspectorClient(){Close();}
    void Close(){if(view)UnmapViewOfFile(view);if(map)CloseHandle(map);if(mutex)CloseHandle(mutex);view=nullptr;map=mutex=nullptr;catRevision=detailRevision=hoverRevision=sent=sentPointer=0;nextOpen=nextPoll=0;}
    void Poll(inspector::Header h,bool enabled){
        if(!inspector::SameScene(h,identity)){Close();identity=h;inspector::Bind(h);}
        if(!enabled||h.pid<=0){if(!enabled)sent=0;return;}
        auto now=GetTickCount64();auto state=inspector::worker.Get();
        if(inspector::pending&&sent==inspector::pending->serial&&state->detail&&state->detail->ack==inspector::pending->serial)inspector::pending.reset();
        if(inspector::pending&&std::chrono::duration<double>(inspector::Clock::now()-inspector::pendingAt).count()>3){inspector::transportError="Inspector request timed out. Check the game connection, then try again.";inspector::pending.reset();inspector::focusSerial=inspector::commitSerial=0;inspector::clickPointer.reset();sent=0;}
        if(!view&&now>=nextOpen){nextOpen=now+500;std::string name="ItStealsInspector.v3."+std::to_string(h.pid);map=OpenFileMappingA(FILE_MAP_READ|FILE_MAP_WRITE,FALSE,name.c_str());mutex=OpenMutexA(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,(name+".Lock").c_str());if(map&&mutex)view=static_cast<uint8_t*>(MapViewOfFile(map,FILE_MAP_READ|FILE_MAP_WRITE,0,0,inspector::Capacity));if(!view){if(map)CloseHandle(map);if(mutex)CloseHandle(mutex);map=mutex=nullptr;return;}}
        bool pointerDirty=inspector::pointer.Valid()&&inspector::pointer.serial!=sentPointer;
        if(!view||(now<nextPoll&&!inspector::pending&&!pointerDirty))return;
        nextPoll=now+(inspector::picking?16:100);
        DWORD lock=WaitForSingleObject(mutex,0);if(lock!=WAIT_OBJECT_0&&lock!=WAIT_ABANDONED)return;
        std::vector<uint8_t> catalog,detail,hover;
        if(lock==WAIT_OBJECT_0){uint32_t magic=0,version=0,session=0;int pid=0;std::memcpy(&magic,view,4);std::memcpy(&version,view+4,4);std::memcpy(&session,view+8,4);std::memcpy(&pid,view+12,4);
            if(magic==inspector::Magic&&version==inspector::Version&&session==h.session&&pid==h.pid){
                auto copy=[&](size_t lengthAt,size_t revisionAt,size_t start,size_t capacity,uint32_t& revision,std::vector<uint8_t>& bytes){int len=0;uint32_t current=0;std::memcpy(&len,view+lengthAt,4);std::memcpy(&current,view+revisionAt,4);if(current!=revision&&len>0&&size_t(len)<=capacity){bytes.assign(view+start,view+start+len);revision=current;}};
                copy(32,36,inspector::CatalogOffset,inspector::CatalogCapacity,catRevision,catalog);copy(40,44,inspector::DetailOffset,inspector::DetailCapacity,detailRevision,detail);copy(48,52,inspector::HoverOffset,inspector::HoverCapacity,hoverRevision,hover);
                if(pointerDirty){const auto& p=inspector::pointer;uint32_t active=p.active?1:0;std::memcpy(view+132,&p.epoch,4);std::memcpy(view+136,&active,4);std::memcpy(view+140,&p.x,4);std::memcpy(view+144,&p.y,4);std::memcpy(view+148,&p.width,4);std::memcpy(view+152,&p.height,4);std::memcpy(view+156,&p.action,4);std::memcpy(view+160,&p.preferred,4);std::memcpy(view+128,&p.serial,4);sentPointer=p.serial;}
                if(inspector::pending&&inspector::pending->serial!=sent){uint32_t lastCommand=0;std::memcpy(&lastCommand,view+64,4);inspector::AvoidSerialCollision(lastCommand);const auto& c=*inspector::pending;std::memcpy(view+68,&c.action,4);std::memcpy(view+72,&c.object,4);std::memcpy(view+76,&c.flags,4);std::memcpy(view+80,&c.x,4);std::memcpy(view+84,&c.y,4);std::memcpy(view+88,&c.epoch,4);std::memcpy(view+96,&c.key,8);std::memcpy(view+64,&c.serial,4);sent=c.serial;inspector::transportError.clear();}
            }
        }
        ReleaseMutex(mutex);if(!catalog.empty())inspector::worker.Submit(std::move(catalog),true);if(!detail.empty())inspector::worker.Submit(std::move(detail),false);if(!hover.empty())inspector::worker.SubmitHover(std::move(hover));
    }
};
