#pragma once
#include "PracticeRouting.h"
#include "RouteTravelTime.h"
namespace practice {
struct PlaybackSlice {std::vector<Vec3> trail;Vec3 marker;bool visible=false;std::vector<uint8_t> hidden;bool markerVisible=true;};
inline Vec3 Interpolate(Vec3 a,Vec3 b,double t){return {float(a.x+(b.x-a.x)*t),float(a.y+(b.y-a.y)*t),float(a.z+(b.z-a.z)*t)};}
// Slice by recorded time, interpolating both boundaries. Do not invent a
// connecting segment across a missing/reduced trace sample.
inline PlaybackSlice TraceSlice(const std::vector<RunPose>& poses,double duration,double time,double tail=2.,double gap=.25,bool keyframes=false){
    PlaybackSlice out;if(poses.empty())return out;
    if(!std::isfinite(time))time=0;
    time=std::clamp(time,0.,std::max(0.,duration));double begin=std::max(0.,time-tail);
    auto locate=[&](double at){auto next=std::upper_bound(poses.begin(),poses.end(),at,[](double t,const RunPose& p){return t<p.seconds;});if(next==poses.begin())return poses.front().point;if(next==poses.end())return poses.back().point;auto prev=next-1;if(keyframes?!next->continuous:next->seconds-prev->seconds>gap)return prev->point;return next->seconds>prev->seconds?Interpolate(prev->point,next->point,(at-prev->seconds)/(next->seconds-prev->seconds)):next->point;};
    out.marker=locate(time);out.visible=true;out.trail.push_back(locate(begin));
    auto first=std::upper_bound(poses.begin(),poses.end(),begin,[](double t,const RunPose& p){return t<p.seconds;});
    for(auto it=first;it!=poses.end()&&it->seconds<time;it++){if(it!=poses.begin()&&(keyframes?!it->continuous:it->seconds-(it-1)->seconds>gap))out.trail.clear();out.trail.push_back(it->point);}
    if(out.trail.empty()||!Same(out.trail.back(),out.marker))out.trail.push_back(out.marker);
    return out;
}
inline PlaybackSlice PlayedSlice(const RunReview& review,double time,double tail=2.){return TraceSlice(review.trace,review.seconds,time,tail);}
struct MonsterPlayback {PlaybackSlice pose;const std::vector<Vec3>* path=nullptr;int id=0;};
inline std::vector<MonsterPlayback> MonstersAt(const RunReview& r,double at,bool paths){
    std::vector<MonsterPlayback> out;
    for(const auto& m:r.monsters){if(m.trace.empty()||at<m.trace.front().seconds)continue;
        auto next=std::upper_bound(m.trace.begin(),m.trace.end(),at,[](double time,const RunPose& p){return time<p.seconds;});
        if(next==m.trace.begin()||(at-(next-1)->seconds>.5&&!(next!=m.trace.end()&&next->continuous)))continue;
        MonsterPlayback shown;shown.id=m.id;shown.pose=TraceSlice(m.trace,r.seconds,at,1.,.3,true);
        if(paths){auto path=std::upper_bound(m.paths.begin(),m.paths.end(),at,[](double time,const ReplayPath& p){return time<p.seconds;});if(path!=m.paths.begin())shown.path=&(path-1)->corners;}
        out.push_back(std::move(shown));
    }return out;
}
// Verified pieces remain drawable even when another leg has missing capture
// data. Missing metadata never turns a Euclidean estimate into a checked line.
inline bool RouteLegVerified(const RouteResult& route,size_t leg){return leg<route.estimatedLine.size()?route.estimatedLine[leg]==0:!route.estimated;}
inline PlaybackSlice SuggestedSlice(const RouteResult& route,double fraction,double tailFraction=.04){
    PlaybackSlice out;if(!route.complete||route.line.empty())return out;
    if(!std::isfinite(fraction))fraction=0;
    fraction=std::clamp(fraction,0.,1.);tailFraction=std::isfinite(tailFraction)?std::clamp(tailFraction,0.,1.):0.;
    double length=0;for(size_t i=1;i<route.line.size();i++)length+=Distance(route.line[i-1],route.line[i]);
    double end=length*fraction,begin=length*std::max(0.,fraction-tailFraction),at=0;
    out.marker=route.line.front();out.markerVisible=false;
    auto append=[&](Vec3 point,bool hidden){if(out.trail.empty()){out.trail.push_back(point);return;}if(!Same(out.trail.back(),point)){out.hidden.push_back(hidden);out.trail.push_back(point);}};
    for(size_t i=1;i<route.line.size();i++){
        auto a=route.line[i-1],b=route.line[i];double leg=Distance(a,b),next=at+leg;
        if(leg>1e-9&&next>=begin&&at<=end){
            auto start=Interpolate(a,b,std::clamp((begin-at)/leg,0.,1.));auto stop=Interpolate(a,b,std::clamp((end-at)/leg,0.,1.));bool known=RouteLegVerified(route,i-1);
            append(start,true);append(stop,!known);out.visible|=known;out.marker=stop;out.markerVisible=known;
        }
        at=next;if(at>=end&&!out.trail.empty())break;
    }
    if(out.trail.empty()){out.marker=fraction>=1?route.line.back():route.line.front();out.trail.push_back(out.marker);out.visible=out.markerVisible=!route.estimated;}
    return out;
}
inline bool SameSeedPreviewLayout(const MapData& a,const MapData& b){
    return a.pid==b.pid&&a.scene==b.scene&&a.epoch==b.epoch&&a.hash==b.hash&&a.seed==b.seed&&a.spawn==b.spawn&&a.selected==b.selected&&
        (a.recipe?a.recipe:a.liveRecipe)==(b.recipe?b.recipe:b.liveRecipe)&&a.spawns.size()==b.spawns.size()&&a.points.size()==b.points.size()&&
        (a.spawns.empty()||std::memcmp(a.spawns.data(),b.spawns.data(),a.spawns.size()*sizeof(Vec3))==0)&&
        (a.points.empty()||std::memcmp(a.points.data(),b.points.data(),a.points.size()*sizeof(Vec3))==0);
}
// Seed previews are a drawing aid, not playback at the predicted run speed.
// The revealed prefix never shrinks; the pulse has its own repeating clock.
struct SeedRoutePlayback {
    static constexpr double DrawSeconds=5.,PulseSeconds=4.;
    double seconds=0,pulseSeconds=0;bool suggested=false,playing=true;
    void Advance(double delta,bool motion){
        if(!suggested)return;
        if(!motion){seconds=DrawSeconds;return;}
        if(!playing||!std::isfinite(delta)||delta<=0)return;
        double step=std::min(delta,.1);bool complete=Complete();seconds=std::min(DrawSeconds,seconds+step);
        pulseSeconds=complete?std::fmod(pulseSeconds+step,PulseSeconds):0;
    }
    void ShowAll(){seconds=DrawSeconds;pulseSeconds=0;}
    void Restart(){seconds=pulseSeconds=0;playing=true;}
    bool Complete()const{return seconds>=DrawSeconds;}
    double Fraction()const{return std::clamp(seconds/DrawSeconds,0.,1.);}
    double Pulse()const{return Complete()?pulseSeconds/PulseSeconds:Fraction();}
};
struct ReviewPlayback {
    int64_t run=0;double seconds=0;float speed=2,travelSpeed=9;bool playing=true,played=true,suggested=true,collectedOnly=false,monsters=true,monsterPaths=true,flashes=true;
    void Bind(int64_t id){if(id!=run){run=id;seconds=0;playing=true;}}
    double Duration(double recorded,double modeled,bool available)const{
        if(suggested&&available)return played?std::max(recorded,modeled):modeled;
        return recorded;
    }
    void Advance(double delta,double duration){if(!std::isfinite(duration)||duration<0)duration=0;if(playing&&std::isfinite(delta)&&delta>0)seconds+=std::min(delta,.1)*speed;seconds=std::clamp(seconds,0.,duration);if(seconds>=duration)playing=false;}
    void Restart(){seconds=0;playing=true;}
};
}
