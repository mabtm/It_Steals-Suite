#pragma once
#include "SharedProtocol.h"
#include <cstring>

struct DisplayPose { float x=0,y=0,z=0,heading=0,monsterX=0,monsterZ=0,speed=0; };
class TelemetryPresentation {
    DisplayPose previous{}, current{};
    double received = 0, gameTime = 0, interval = 1./60;
    int scene = -1;
    int epoch=0,teleport=0;
    bool ready = false, monster = false;
public:
    void Reset() { ready = false; }
    void Push(const GameData& data, double published, double now) {
        if(!std::isfinite(published) || !std::isfinite(now)) {ready=false;published=0;now=0;}
        DisplayPose next{data.playerX,data.playerY,data.playerZ,data.playerRotY,data.monsterX,data.monsterZ,data.smoothedSpeed};
        double gap = published - gameTime;
        float dx=next.x-current.x,dz=next.z-current.z;
        bool snap = !ready || data.currentSceneIndex != scene || data.sceneEpoch!=epoch || data.lastTeleportSerial!=teleport ||
            gap <= 0 || gap > .12 || dx*dx+dz*dz > 144;
        previous = snap ? next : current;
        float mx=next.monsterX-current.monsterX,mz=next.monsterZ-current.monsterZ;
        if (monster != bool(data.monsterActive) || mx*mx+mz*mz>144) { previous.monsterX=next.monsterX; previous.monsterZ=next.monsterZ; }
        current = next; interval = std::clamp(gap,1./240,.1); received=now; gameTime=published;
        scene=data.currentSceneIndex;epoch=data.sceneEpoch;teleport=data.lastTeleportSerial;monster=data.monsterActive!=0;ready=true;
    }
    DisplayPose Sample(double now) const {
        if (!ready) return {};
        float a=static_cast<float>(std::clamp((now-received)/interval,0.,1.));
        auto lerp=[&](float old,float value){return old+(value-old)*a;};
        float turn=std::remainder(current.heading-previous.heading,360.f);
        return {lerp(previous.x,current.x),lerp(previous.y,current.y),lerp(previous.z,current.z),
            previous.heading+turn*a,lerp(previous.monsterX,current.monsterX),lerp(previous.monsterZ,current.monsterZ),lerp(previous.speed,current.speed)};
    }
};
inline double DisplayRunTime(const GameData& data, double age) {
    return data.currentRunTimeSeconds + (data.timerState == 1 ? std::clamp(age,0.,.15) : 0);
}
#pragma pack(push,1)
struct HistoryRecord {
    int64_t id, startedUtc, endedUtc;
    double seconds;
    int32_t scene, seed, initialRngSeed;
    float progress;
    byte seedKind, seedKnown, outcome, pb;
    uint32_t aids, reseeds, reserved;
    bool Valid()const{return (reserved&0x80000000u)!=0;}
    uint32_t Warnings()const{return reserved&0x7fffffffu;}
};
#pragma pack(pop)
static_assert(sizeof(HistoryRecord)==64);
inline HistoryRecord ReadHistory(const GameData& data,int index) {
    HistoryRecord result{};
    if (index>=0 && index<data.historyCount && index<16) std::memcpy(&result,data.historyBytes+index*64,sizeof(result));
    if(result.startedUtc<0 || result.startedUtc>3155378975999999999LL) result.startedUtc=0;
    if(!std::isfinite(result.seconds) || result.seconds<0) result.seconds=0;
    result.progress=std::isfinite(result.progress)?std::clamp(float(result.progress),0.f,1.f):0;
    if(result.seedKind>2 || !result.seedKnown) result.seedKind=0;
    if(result.outcome>5) result.outcome=4;
    if(result.id<0) result.id=0;
    return result;
}
