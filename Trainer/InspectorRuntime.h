#pragma once
#include "InspectorModel.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
#include <chrono>

namespace inspector {
using Clock=std::chrono::steady_clock;
struct State {Header identity;std::shared_ptr<const Catalog> catalog;std::shared_ptr<const Detail> detail;std::shared_ptr<const Hover> hover;std::shared_ptr<const Probe> probe;bool probing=false;std::string error,hoverError;Clock::time_point received{},hoverReceived{};};
class Worker {
    struct ProbeJob {std::shared_ptr<const practice::MapData> data;practice::Vec3 a,b;uint64_t token=0;};
    mutable std::mutex gate;std::condition_variable wake;bool closing=false;std::thread thread;
    std::shared_ptr<const State> state=std::make_shared<State>();std::vector<uint8_t> catalog,detail,hover;std::optional<ProbeJob> probe;
    std::atomic<uint64_t> generation{0};uint64_t binding=0;
    void Run(){
        for(;;){std::vector<uint8_t> cat,det,hov;std::optional<ProbeJob> job;Header identity;uint64_t bind;
            {std::unique_lock<std::mutex> lock(gate);wake.wait(lock,[&]{return closing||!catalog.empty()||!detail.empty()||!hover.empty()||probe.has_value();});if(closing)return;cat.swap(catalog);det.swap(detail);hov.swap(hover);job=std::move(probe);probe.reset();identity=state->identity;bind=binding;}
            std::shared_ptr<const Catalog> nextCatalog;std::shared_ptr<const Detail> nextDetail;std::shared_ptr<const Probe> nextProbe;std::shared_ptr<const Hover> nextHover;std::string error,hoverError;
            try{if(!cat.empty()){nextCatalog=DecodeCatalog(cat);if(!SameScene(nextCatalog->header,identity))throw std::runtime_error("Inspector catalog belongs to another scene/session");}
                if(!det.empty()){nextDetail=DecodeDetail(det);if(!SameScene(nextDetail->header,identity))throw std::runtime_error("Inspector details belong to another scene/session");}
                if(job){auto cancel=[&]{return generation.load()!=job->token;};if(!cancel()){if(!job->data||job->data->pid!=identity.pid||job->data->scene!=identity.scene||uint32_t(job->data->epoch)!=identity.epoch)throw std::runtime_error("Probe map does not match the live scene");nextProbe=std::make_shared<const Probe>(ProbeRoute(*job->data,job->a,job->b,cancel));}}
            }catch(const std::exception& e){error=e.what();}
            if(!hov.empty())try{nextHover=DecodeHover(hov);if(!SameScene(nextHover->header,identity))throw std::runtime_error("Hover belongs to another scene/session");}catch(const std::exception& e){nextHover.reset();hoverError=e.what();}
            {std::lock_guard<std::mutex> lock(gate);if(closing||bind!=binding)continue;auto next=std::make_shared<State>(*state);
                if(!hov.empty()){next->hover=std::move(nextHover);next->hoverError=hoverError;next->hoverReceived=Clock::now();}
                if(nextCatalog&&SameScene(nextCatalog->header,identity))next->catalog=std::move(nextCatalog);
                if(nextDetail&&SameScene(nextDetail->header,identity)){
                    if(next->detail&&(next->detail->hasA!=nextDetail->hasA||next->detail->hasB!=nextDetail->hasB||!SamePoint(next->detail->a,nextDetail->a)||!SamePoint(next->detail->b,nextDetail->b))){generation++;probe.reset();next->probe.reset();next->probing=false;}
                    next->detail=std::move(nextDetail);next->received=Clock::now();}
                if(job&&generation.load()==job->token){next->probing=false;if(nextProbe)next->probe=std::move(nextProbe);}
                if(!error.empty()&&(!job||generation.load()==job->token))next->error=error;else if(next->detail)next->error.clear();state=next;
            }
        }
    }
public:
    Worker(){thread=std::thread([this]{Run();});}Worker(const Worker&)=delete;~Worker(){Shutdown();}
    void Shutdown(){{std::lock_guard<std::mutex> lock(gate);if(closing)return;closing=true;generation++;}wake.notify_all();if(thread.joinable())thread.join();}
    void Bind(Header identity){std::lock_guard<std::mutex> lock(gate);if(SameScene(identity,state->identity))return;binding++;generation++;catalog.clear();detail.clear();hover.clear();probe.reset();auto next=std::make_shared<State>();next->identity=identity;state=next;}
    void Submit(std::vector<uint8_t> bytes,bool isCatalog){if(bytes.size()>(isCatalog?CatalogCapacity:DetailCapacity))throw std::runtime_error("Inspector publication exceeds queue budget");{std::lock_guard<std::mutex> lock(gate);if(closing)return;(isCatalog?catalog:detail)=std::move(bytes);}wake.notify_one();}
    void SubmitHover(std::vector<uint8_t> bytes){if(bytes.size()>HoverCapacity)throw std::runtime_error("Hover exceeds queue budget");{std::lock_guard<std::mutex> lock(gate);if(closing)return;hover=std::move(bytes);}wake.notify_one();}
    void ProbeMap(std::shared_ptr<const practice::MapData> data,practice::Vec3 a,practice::Vec3 b){{std::lock_guard<std::mutex> lock(gate);if(closing)return;uint64_t token=++generation;probe=ProbeJob{std::move(data),a,b,token};auto next=std::make_shared<State>(*state);next->probing=true;next->probe.reset();next->error.clear();state=next;}wake.notify_one();}
    void CancelProbe(){std::lock_guard<std::mutex> lock(gate);generation++;probe.reset();auto next=std::make_shared<State>(*state);next->probing=false;next->probe.reset();state=next;}
    std::shared_ptr<const State> Get()const{std::lock_guard<std::mutex> lock(gate);return state;}
};
inline Worker worker;
inline bool open=false,picking=false,dock=false,dockInteractive=false,pinned=false,pickFromMenu=false,pickFromDock=false,pickWasVisible=false,pickWasCompact=false,pickWasAll=true,resumeCompact=false,pickResumeCompact=false;inline uint32_t focusSerial=0;
inline int pickAction=3,pickerArmFrame=0,detailsFrame=-1,geometryFrame=-1;inline uint32_t options=12,serial=0,pointerSerial=0,commitSerial=0,lastSubmittedOptions=~0u;
inline Pointer pointer;inline std::optional<Pointer> clickPointer,nativeClick;inline Clock::time_point pointerAt{};
inline bool (*pointerProvider)(Pointer&)=nullptr;inline std::optional<Command> pending;
inline Header identity;inline Clock::time_point pendingAt{};inline std::string transportError,lastReport;
inline std::shared_ptr<const Detail> baseline;
inline std::filesystem::path reportFolder;
inline Header Identity(int pid,uint32_t session,int scene,uint32_t epoch){Header h;h.pid=pid;h.session=session;h.scene=scene;h.epoch=epoch;return h;}
inline void Bind(Header h){if(SameScene(identity,h))return;if(identity.pid){open=picking=dock=dockInteractive=false;resumeCompact=false;options=12;}identity=h;pending.reset();baseline.reset();focusSerial=commitSerial=0;clickPointer.reset();nativeClick.reset();pointer={};lastSubmittedOptions=~0u;transportError.clear();worker.Bind(h);}
inline bool Request(int action,int object=0,uint64_t key=0,float x=0,float y=0){if(pending||!open||identity.pid<=0)return false;Command c;c.serial=++serial;if(!c.serial)c.serial=++serial;c.epoch=identity.epoch;c.flags=options;c.action=action;c.object=object;c.key=key;c.x=x;c.y=y;if(!c.Valid())return false;pending=c;lastSubmittedOptions=options;pendingAt=Clock::now();return true;}
// Reconnecting a trainer must not re-use the helper's last acknowledged ID.
inline void AvoidSerialCollision(uint32_t last){if(!pending||pending->serial!=last)return;uint32_t old=pending->serial;pending->serial=++serial;if(!pending->serial)pending->serial=++serial;if(focusSerial==old)focusSerial=pending->serial;if(commitSerial==old)commitSerial=pending->serial;}
inline double Age(const State& s){return s.received==Clock::time_point{}?1e9:std::chrono::duration<double>(Clock::now()-s.received).count();}
inline double HoverAge(const State& s){return s.hoverReceived==Clock::time_point{}?1e9:std::chrono::duration<double>(Clock::now()-s.hoverReceived).count();}
inline void Point(Pointer p,bool force=false){if(clickPointer&&!force)return;auto now=Clock::now();p.epoch=identity.epoch;bool same=p.active==pointer.active&&p.x==pointer.x&&p.y==pointer.y&&p.width==pointer.width&&p.height==pointer.height&&p.action==pointer.action&&p.epoch==pointer.epoch&&p.preferred==pointer.preferred;if(!force&&same&&std::chrono::duration<double>(now-pointerAt).count()<.1)return;p.serial=++pointerSerial;if(!p.serial)p.serial=++pointerSerial;p.epoch=identity.epoch;pointer=p;pointerAt=now;}
inline void Prefer(int id){if(!picking||clickPointer||pending)return;Pointer p=pointer;p.preferred=id;Point(p,true);}
inline int CycleTarget(const Hover& h,int direction){if(h.targets.empty())return 0;int at=0;for(size_t i=0;i<h.targets.size();i++)if(h.targets[i].id==h.object){at=int(i);break;}return h.targets[size_t((at+direction+int(h.targets.size()))%int(h.targets.size()))].id;}
inline void StopPointer(){clickPointer.reset();nativeClick.reset();Pointer p=pointer;p.active=false;p.preferred=0;Point(p,true);commitSerial=0;}
inline void QueueClick(Pointer p){if(!picking||pending||clickPointer)return;p.active=true;p.action=pickAction;bool same=p.width==pointer.width&&p.height==pointer.height&&std::abs(p.x-pointer.x)*p.width<=4&&std::abs(p.y-pointer.y)*p.height<=4;if(same&&p.action==3)p.preferred=pointer.preferred;else p.preferred=0;Point(p,true);clickPointer=pointer;}
inline void Close(){open=picking=dock=dockInteractive=false;focusSerial=0;resumeCompact=false;pending.reset();baseline.reset();options=12;lastSubmittedOptions=~0u;StopPointer();worker.CancelProbe();}

}
