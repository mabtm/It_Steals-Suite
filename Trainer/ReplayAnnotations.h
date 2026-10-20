#pragma once
#include "ReplayMotion.h"
#include <set>
namespace practice {
struct ReplayMarker {uint32_t id=0;double seconds=0;std::string name;};
struct ReplayAnnotations {
    int64_t run=0;double duration=0,a=0,b=0;bool hasA=false,hasB=false,loop=false;
    double clipA=0,clipB=0;bool hasClipA=false,hasClipB=false;
    std::vector<ReplayMarker> markers;
    bool Matches(const MapData& d)const{return d.review&&run==d.review->id&&std::abs(duration-d.review->seconds)<.000001;}
    bool LoopValid()const{return hasA&&hasB&&a>=0&&b<=duration&&b-a>=.02;}
};
inline void ValidateAnnotations(const ReplayAnnotations& a){
    auto time=[&](double t){return std::isfinite(t)&&t>=0&&t<=a.duration;};
    if(a.run<=0||!std::isfinite(a.duration)||a.duration<0||a.duration>315360000||!time(a.a)||!time(a.b)||!time(a.clipA)||!time(a.clipB)||a.markers.size()>256||(a.loop&&!a.LoopValid()))throw std::runtime_error("Invalid annotation identity / loop");
    std::set<uint32_t> ids;for(const auto& m:a.markers)if(m.id==0||!ids.insert(m.id).second||!time(m.seconds)||m.name.empty()||!ReplayUtf8(m.name,128)||std::any_of(m.name.begin(),m.name.end(),[](unsigned char c){return c<32||c==127;}))throw std::runtime_error("Invalid named marker");
}
inline std::vector<uint8_t> EncodeAnnotations(const ReplayAnnotations& a){
    ValidateAnnotations(a);std::vector<uint8_t> bytes;auto put=[&](auto v){auto p=reinterpret_cast<const uint8_t*>(&v);bytes.insert(bytes.end(),p,p+sizeof(v));};
    put(uint32_t{0x4b4d5349});put(int32_t{2});put(a.run);put(a.duration);put(a.a);put(a.b);put(uint32_t((a.hasA?1:0)|(a.hasB?2:0)|(a.loop?4:0)));put(int32_t(a.markers.size()));
    for(const auto& m:a.markers){put(m.id);put(m.seconds);put(int32_t(m.name.size()));bytes.insert(bytes.end(),m.name.begin(),m.name.end());}put(uint32_t((a.hasClipA?1:0)|(a.hasClipB?2:0)));put(a.clipA);put(a.clipB);put(ReplayHash(bytes,bytes.size()));return bytes;
}
inline std::shared_ptr<const ReplayAnnotations> DecodeAnnotations(const std::vector<uint8_t>& bytes){
    if(bytes.size()<56||bytes.size()>65536)throw std::runtime_error("Invalid annotation size");
    MapReader r(bytes);
    if(r.Read<uint32_t>()!=0x4b4d5349)throw std::runtime_error("Unsupported annotation file");
    int format=r.Read<int32_t>();if(format<1||format>2)throw std::runtime_error("Unsupported annotation file");
    auto a=std::make_shared<ReplayAnnotations>();a->run=r.Read<int64_t>();a->duration=r.Read<double>();a->a=r.Read<double>();a->b=r.Read<double>();uint32_t flags=r.Read<uint32_t>();if(flags&~7u)throw std::runtime_error("Invalid annotation flags");a->hasA=flags&1;a->hasB=flags&2;a->loop=flags&4;int n=r.Count(256);
    for(int i=0;i<n;i++){ReplayMarker m;m.id=r.Read<uint32_t>();m.seconds=r.Read<double>();m.name=r.Text(128);a->markers.push_back(std::move(m));}if(format>=2){uint32_t clip=r.Read<uint32_t>();if(clip&~3u)throw std::runtime_error("Invalid clip markers");a->hasClipA=clip&1;a->hasClipB=clip&2;a->clipA=r.Read<double>();a->clipB=r.Read<double>();}
    if(r.Read<uint64_t>()!=ReplayHash(bytes,bytes.size()-8)||!r.Done())throw std::runtime_error("Annotation checksum mismatch");
    ValidateAnnotations(*a);return a;
}
}
