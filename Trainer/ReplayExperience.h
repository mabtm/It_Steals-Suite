#pragma once
#include "PracticeMapData.h"
#include <climits>
#include <filesystem>
#include <set>
#include <tuple>
namespace practice {
inline constexpr uint64_t AudioCapacity=256ull*1024*1024,AudioHeaderBytes=256;
struct ReplayAudioInfo {
    bool recorded=false;int rate=0,channels=0,flags=0,gaps=0;int64_t frames=0,bytes=0;uint64_t checksum=0;
    double Duration()const{return rate>0?double(frames)/rate:0;}
};
struct ReplayProbe {double seconds=0;int actor=0,site=0,mask=0;uint32_t flags=0;Vec3 origin,end;float limit=0;};
struct ReplayObservation {double seconds=0;int actor=0,kind=0,name=0;float value=0;Vec3 position;};
struct ReplayAnimationFrame {double seconds=0;int state=0,next=0,clip=0;uint32_t flags=0;float phase=0,nextPhase=0,transition=0,speed=0,weight=0;};
struct ReplayAnimationTrack {int group=0,actor=0,layer=0,kind=0,slot=0;std::string path,name;std::vector<ReplayAnimationFrame> frames;};
struct ReplayExperience {
    bool recorded=false;int dropped=0;ReplayAudioInfo audio;std::vector<std::string> names;
    std::vector<ReplayProbe> probes;std::vector<ReplayObservation> events;std::vector<ReplayAnimationTrack> animations;
    std::string Name(int id)const{return id>0&&size_t(id)<=names.size()?names[size_t(id)-1]:"Unknown observation";}
};
inline ReplayAudioInfo ReadReplayAudioInfo(MapReader& r){
    ReplayAudioInfo i;i.recorded=r.Flag();i.rate=r.Read<int32_t>();i.channels=r.Read<int32_t>();i.flags=r.Count(31);i.gaps=r.Count(INT_MAX);i.frames=r.Read<int64_t>();i.bytes=r.Read<int64_t>();i.checksum=r.Read<uint64_t>();
    if(i.frames<0||i.bytes<0||uint64_t(i.bytes)>AudioCapacity||(i.recorded&&(i.rate<8000||i.rate>192000||i.channels<1||i.channels>8||i.frames<1||uint64_t(i.frames)>(AudioCapacity-AudioHeaderBytes-8)/(uint64_t(i.channels)*4)||uint64_t(i.bytes)!=AudioHeaderBytes+uint64_t(i.frames)*i.channels*4+8))||(!i.recorded&&(i.frames||i.bytes)))throw std::runtime_error("Invalid recorded audio metadata");
    return i;
}
inline ReplayExperience ReadReplayExperience(MapReader& r,double duration){
    auto start=r.Offset();ReplayExperience d;d.recorded=r.Flag();d.dropped=r.Count(INT_MAX);d.audio=ReadReplayAudioInfo(r);if(d.audio.recorded&&d.audio.Duration()>duration+.001)throw std::runtime_error("Audio duration exceeds recording");
    auto number=[&](double limit=100000.){float v=r.Read<float>();if(!std::isfinite(v)||std::abs(v)>limit)throw std::runtime_error("Invalid experience number");return v;};
    auto vec=[&](){return Vec3{number(),number(),number()};};
    auto stamp=[&](double& previous){double t=r.Read<double>();if(!std::isfinite(t)||t<0||t<previous||t>duration+.000001)throw std::runtime_error("Invalid experience timestamp");previous=t;return t;};
    int n=r.Count(2048);std::set<std::string> names;for(int k=0;k<n;k++){auto text=r.Text(512);if(text.empty()||!names.insert(text).second)throw std::runtime_error("Duplicate experience name");d.names.push_back(std::move(text));}
    n=r.Count(131072);d.probes.reserve(n);double previous=0;
    for(int k=0;k<n;k++){ReplayProbe p;p.seconds=stamp(previous);p.actor=r.Count(128);p.site=r.Count(int(d.names.size()));p.mask=r.Read<int32_t>();p.flags=r.Read<uint32_t>();p.origin=vec();p.end=vec();p.limit=number();if(p.site<1||(p.flags&~15u)||(p.limit<0&&p.limit!=-1))throw std::runtime_error("Invalid observed query");d.probes.push_back(p);}
    n=r.Count(65536);d.events.reserve(n);previous=0;
    for(int k=0;k<n;k++){ReplayObservation e;e.seconds=stamp(previous);e.actor=r.Count(128);e.kind=r.Count(4);e.name=r.Count(int(d.names.size()));e.value=number();e.position=vec();if(e.kind<1||e.name<1||((e.kind==1||e.kind==4)&&e.value!=0&&e.value!=1))throw std::runtime_error("Invalid observed event");d.events.push_back(e);}
    n=r.Count(512);int samples=0;std::set<std::tuple<int,int,int,int,int,std::string>> tracks;
    for(int k=0;k<n;k++){ReplayAnimationTrack t;t.group=r.Count(3);t.actor=r.Count(1000000);t.layer=r.Count(127);t.kind=r.Count(2);t.slot=r.Count(1023);t.path=r.Text(1024);t.name=r.Text(512);if(t.kind<1||!tracks.insert({t.group,t.actor,t.kind,t.layer,t.slot,t.path}).second)throw std::runtime_error("Invalid animation track");int count=r.Count(65536-samples);samples+=count;t.frames.reserve(count);previous=0;
        for(int j=0;j<count;j++){ReplayAnimationFrame f;f.seconds=stamp(previous);f.state=r.Read<int32_t>();f.next=r.Read<int32_t>();f.clip=r.Count(int(d.names.size()));f.flags=r.Read<uint32_t>();f.phase=number(1e9);f.nextPhase=number(1e9);f.transition=number();f.speed=number();f.weight=number();if((f.flags&~3u)||f.transition<0||f.transition>1||(t.kind==1&&(f.weight<0||f.weight>1.001f))||(t.kind==2&&std::abs(f.weight)>10000))throw std::runtime_error("Invalid animation sample");t.frames.push_back(f);}
        d.animations.push_back(std::move(t));
    }
    if(r.Offset()-start>16*1024*1024)throw std::runtime_error("Experience data exceeds byte limit");
    if(!d.recorded&&(!d.probes.empty()||!d.events.empty()||!d.animations.empty()||d.audio.recorded))throw std::runtime_error("Unexpected experience observations");
    return d;
}
}
