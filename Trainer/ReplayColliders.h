#pragma once
#include "PracticeMapData.h"
#include "ReplayTextValidation.h"
namespace practice {
struct ReplayColliderShape {Vec3 center,size;float radius=0,height=0;int direction=0;std::array<float,12> matrix{};};
struct ReplayColliderSample {double seconds=0;uint32_t state=0;ReplayColliderShape shape;};
struct ReplayColliderTrack {int group=0,owner=0,kind=0;std::string name;std::vector<ReplayColliderSample> samples;};
struct ReplayColliderData {bool recorded=false;uint32_t flags=0;int dropped=0,count=0;std::vector<ReplayColliderTrack> tracks;};
inline ReplayColliderData ReadReplayColliders(MapReader& r,double duration,size_t bytes){
    ReplayColliderData d;auto recorded=r.Read<uint8_t>();d.recorded=recorded==1;d.flags=r.Read<uint32_t>();d.dropped=r.Read<int32_t>();d.count=r.Count(262144);int n=r.Count(256);
    if(recorded>1||(d.flags&~7u)||d.dropped<0||(!d.recorded&&(d.count||n))||bytes<size_t(d.count)*96+size_t(n)*20+25)throw std::runtime_error("Invalid collider header");
    int total=0;auto vec=[&](){return Vec3{r.Coordinate(),r.Coordinate(),r.Coordinate()};};
    for(int i=0;i<n;i++){
        ReplayColliderTrack t;t.group=r.Read<int32_t>();t.owner=r.Read<int32_t>();t.kind=r.Read<int32_t>();t.name=r.Text(128);int count=r.Count(262144);
        if(t.group<1||t.group>3||t.owner<0||(t.group==1&&t.owner!=0)||t.kind<1||t.kind>4||!ReplayUtf8(t.name,128)||count<1||count>d.count-total)throw std::runtime_error("Invalid collider track");
        double previous=0;t.samples.reserve(count);
        for(int j=0;j<count;j++){ReplayColliderSample s;s.seconds=r.Read<double>();s.state=r.Read<uint32_t>();if(!std::isfinite(s.seconds)||s.seconds<previous||s.seconds<0||s.seconds>duration+.000001||(s.state&~3u))throw std::runtime_error("Invalid collider timeline");previous=s.seconds;
            auto& shape=s.shape;shape.center=vec();shape.size=vec();shape.radius=r.Coordinate();shape.height=r.Coordinate();shape.direction=r.Read<int32_t>();if(shape.size.x<0||shape.size.y<0||shape.size.z<0||shape.radius<0||shape.height<0||shape.direction<0||shape.direction>2)throw std::runtime_error("Invalid collider dimensions");for(auto& f:shape.matrix)f=r.Coordinate();t.samples.push_back(s);
        }total+=count;d.tracks.push_back(std::move(t));
    }
    if(total!=d.count)throw std::runtime_error("Collider sample count mismatch");
    return d;
}
}
