#include "PhysicalNavigation.h"
#include <cassert>
#include <random>
#include <thread>
#include <iostream>
using namespace practice;

// Frozen 7.20.0 floor-interval calculation, using a brute-force triangle scan
// and the original broad phase. It is independent of the new segment cache.
static bool FloorReference(const std::vector<std::array<Vec3, 3>> &floor, Vec3 a, Vec3 b)
{
    auto cross = [](Vec3 p, Vec3 q, Vec3 r) { return (q.x-p.x)*double(r.z-p.z)-(q.z-p.z)*double(r.x-p.x); };
    std::vector<std::pair<double,double>> spans;
    for (const auto &t : floor)
    {
        double start=0,end=1;
        for(int k=0;k<2;k++)
        {
            double at=k?a.z:a.x,delta=k?b.z-a.z:b.x-a.x;
            double lo=(k?std::min({t[0].z,t[1].z,t[2].z}):std::min({t[0].x,t[1].x,t[2].x}))-.0001;
            double hi=(k?std::max({t[0].z,t[1].z,t[2].z}):std::max({t[0].x,t[1].x,t[2].x}))+.0001;
            if(std::abs(delta)<1e-12){if(at<lo||at>hi){start=2;break;}}
            else{double x=(lo-at)/delta,y=(hi-at)/delta;if(x>y)std::swap(x,y);start=std::max(start,x);end=std::min(end,y);}
        }
        if(start>end)continue;
        double area=cross(t[0],t[1],t[2]),sign=area<0?-1:1,lo=0,hi=1;
        for(int e=0;e<3;e++)
        {
            double begin=cross(t[e],t[(e+1)%3],a)*sign,finish=cross(t[e],t[(e+1)%3],b)*sign,delta=finish-begin;
            if(begin<-.0001&&finish<-.0001){lo=2;break;}
            if(std::abs(delta)>1e-10){double at=(-.0001-begin)/delta;if(delta>0)lo=std::max(lo,at);else hi=std::min(hi,at);}
        }
        if(lo>hi||lo>1||hi<0)continue;
        double at=(std::max(lo,0.)+std::min(hi,1.))*.5;
        Vec3 p{float(a.x+(b.x-a.x)*at),float(a.y+(b.y-a.y)*at),float(a.z+(b.z-a.z)*at)};
        double u=cross(t[1],t[2],p)/area,v=cross(t[2],t[0],p)/area;
        double y=u*t[0].y+v*t[1].y+(1-u-v)*t[2].y;
        if(std::abs(y-p.y)<.3)spans.emplace_back(std::max(lo,0.),std::min(hi,1.));
    }
    std::sort(spans.begin(),spans.end());double end=0;
    for(auto span:spans){if(span.first>end+.0001)return false;end=std::max(end,span.second);if(end>=.9999)return true;}
    return false;
}
int main()
{
    MapData d;d.physicalCaptured=true;d.agentRadius=.4f;
    d.physicalParts={{3,{{3,0,-4},{5,0,-4},{5,0,4},{3,0,4}}},{3,{{17,0,-5},{17,0,5}}}};
    std::vector<std::array<Vec3,3>> floor;
    auto rect=[&](float x,float z,float w,float h,float y){floor.push_back({Vec3{x,y,z},{x+w,y,z},{x+w,y,z+h}});floor.push_back({Vec3{x,y,z},{x+w,y,z+h},{x,y,z+h}});};
    rect(-10,-10,18,20,0);rect(12,-10,18,20,0);rect(-10,12,18,18,2);
    PhysicalNavigation nav(d,floor);
    std::vector<std::pair<Vec3,Vec3>> cases;
    std::mt19937 random(7201);
    for(int i=0;i<80000;i++)
    {
        auto point=[&]{return Vec3{float(int(random()%401)-100)/10, i%3==0?2.f:0.f,float(int(random()%421)-100)/10};};
        auto a=point(),b=point();if(i%7==0)b=a;if(i%11==0)b.y+=.30001f;cases.push_back({a,b});
    }
    for(auto [a,b]:std::vector<std::pair<Vec3,Vec3>>{{{0,0,0},{-0.f,0,0}},{{-10,0,-10},{8,0,10}},{{0,0,0},{20,0,0}},{{0,0,0},{0,2,20}},{{0,0,0},{3-.408f,0,0}}})cases.push_back({a,b});
    std::vector<uint8_t> expected;
    for(auto [a,b]:cases){bool value=nav.Clear(a,b)&&FloorReference(floor,a,b);expected.push_back(value);assert(nav.Visible(a,b)==value);}
    // Revisit after more distinct segments than cache slots, in both orders.
    // Parallel readers/writers must never accept a colliding endpoint key.
    std::vector<std::thread> threads;
    for(size_t worker=0;worker<4;worker++)threads.emplace_back([&,worker]{
        for(size_t i=worker;i<cases.size();i+=4){auto [a,b]=cases[cases.size()-1-i];assert(nav.Visible(a,b)==bool(expected[cases.size()-1-i]));assert(nav.Visible(b,a)==(nav.Clear(b,a)&&FloorReference(floor,b,a)));}
    });
    for(auto &thread:threads)thread.join();
    auto changed=d;changed.physicalParts.clear();PhysicalNavigation separate(changed,floor);
    assert(!nav.Visible({0,0,0},{7,0,0})&&separate.Visible({0,0,0},{7,0,0}));
    assert(!nav.Visible({0,0,0},{20,0,0})); // a floor hole remains a hole
    std::cout<<cases.size()<<" segment comparisons against frozen floor clipping; walls, thin segments, holes, heights, boundaries, collisions, parallel access and geometry isolation: PASS\n";
}
