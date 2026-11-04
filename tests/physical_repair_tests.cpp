#include "PhysicalNavigation.h"
#include "physical_navigation_reference.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
using namespace practice;
using Floor = std::vector<std::array<Vec3, 3>>;
static PhysicalPart Solid(float x0, float z0, float x1, float z1)
{
    return {3, {{x0, 0, z0}, {x1, 0, z0}, {x1, 0, z1}, {x0, 0, z1}}};
}
static void Tile(Floor &f, float x0, float z0, float x1, float z1, float y = 0)
{
    f.push_back({Vec3{x0,y,z0},Vec3{x1,y,z0},Vec3{x1,y,z1}});
    f.push_back({Vec3{x0,y,z0},Vec3{x1,y,z1},Vec3{x0,y,z1}});
}
static double Length(const std::vector<Vec3> &p)
{
    double result = 0;
    for (size_t i = 1; i < p.size(); i++) result += Distance(p[i-1], p[i]);
    return result;
}
static void Equal(PhysicalNavigation &nav, PhysicalNavigationReference &reference, Vec3 a, Vec3 b,
                  int &count, double &oldSeconds, double &newSeconds)
{
    auto start=std::chrono::steady_clock::now();
    auto old=reference.Repair(a,b,[]{return false;});
    oldSeconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    start=std::chrono::steady_clock::now();
    auto now=nav.Repair(a,b,[]{return false;});
    newSeconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    if(old.empty()!=now.empty() || std::abs(Length(old)-Length(now))>1e-9)
        std::cerr<<"Mismatch: "<<count<<" from "<<a.x<<","<<a.z<<" to "<<b.x<<","<<b.z
                 <<" old "<<old.size()<<"/"<<Length(old)<<" new "<<now.size()<<"/"<<Length(now)<<"\n";
    assert(old.empty()==now.empty());
    assert(std::abs(Length(old)-Length(now))<1e-9);
    if(!now.empty())
    {
        assert(Distance(now.front(),a)==0 && Distance(now.back(),b)==0);
        for(size_t i=1;i<now.size();i++) assert(nav.Visible(now[i-1],now[i]));
    }
    count++;
}
int main()
{
    Floor floor; Tile(floor,-30,-30,30,30);
    MapData data; data.physicalCaptured=true; data.agentRadius=.4f;
    // Synthetic Timer-like bunker: three closed sides and a narrow east exit.
    // Opening just the door must never erase any part of the fixed enclosure.
    data.physicalParts={Solid(-4.2f,-4.2f,-4,4.2f),Solid(-4,-4.2f,4.2f,-4),
                        Solid(-4,4,4.2f,4.2f),Solid(4,-4,4.2f,-1),Solid(4,1,4.2f,4)};
    PhysicalNavigation bunker(data,floor);
    assert(!bunker.Visible({0,0,0},{-8,0,0}));
    auto route=bunker.Repair({0,0,0},{-8,0,0},[]{return false;});
    assert(!route.empty());
    bool exited=false;
    for(size_t i=1;i<route.size();i++)
    {
        assert(bunker.Visible(route[i-1],route[i]));
        auto a=route[i-1],b=route[i];
        if(a.x<4 && b.x>=4)
        {
            double z=a.z+(b.z-a.z)*(4-a.x)/(b.x-a.x);
            assert(std::abs(z)<.6); exited=true;
        }
    }
    assert(exited);
    data.physicalParts.push_back(Solid(4,-1,4.2f,1));
    PhysicalNavigation shut(data,floor);
    assert(shut.Repair({0,0,0},{-8,0,0},[]{return false;}).empty());
    // Floor gaps and overlapping walls remain unverified; no search heuristic
    // may manufacture a traversable segment.
    Floor split; Tile(split,-20,-20,-1,20); Tile(split,1,-20,20,20);
    data.physicalParts={Solid(-2,-1,2,1)};
    PhysicalNavigation gap(data,split);
    assert(gap.Repair({-5,0,0},{5,0,0},[]{return false;}).empty());
    int count=0; double oldSeconds=0,newSeconds=0;
    std::mt19937 random(7210);
    for(int world=0;world<10;world++)
    {
        data.physicalParts.clear();
        for(int k=0;k<14;k++)
        {
            float x=float(int(random()%240)-120)/10.f,z=float(int(random()%240)-120)/10.f;
            if(k%4==0) data.physicalParts.push_back({3,{{x,0,z-2},{x,0,z+2}}});
            else data.physicalParts.push_back(Solid(x,z,x+.5f+float(random()%20)/10,z+.5f+float(random()%25)/10));
        }
        const Floor &surface=world%3==0?split:floor;
        PhysicalNavigation nav(data,surface);
        PhysicalNavigationReference reference(data,surface);
        for(int k=0;k<28;k++)
        {
            Vec3 a{float(int(random()%300)-150)/10,0,float(int(random()%300)-150)/10};
            Vec3 b{float(int(random()%300)-150)/10,0,float(int(random()%300)-150)/10};
            Equal(nav,reference,a,b,count,oldSeconds,newSeconds);
        }
    }
    // Cancellation remains responsive during actual cold repair work, after
    // entering graph expansion, rather than only at the outer call boundary.
    data.physicalParts={Solid(-2,-8,2,8)};
    PhysicalNavigation cancel(data,floor); int polls=0; bool stopped=false;
    try { cancel.Repair({-8,0,0},{8,0,0},[&]{return ++polls==3;}); }
    catch(const std::runtime_error &e){stopped=std::string(e.what())=="Canceled";}
    assert(stopped && polls==3);
    std::cout<<count<<" repairs match frozen exhaustive reachability/lengths; bunker single exit, closed door, floors, thin walls, multiple rounds and cancellation: PASS\n";
    std::cout<<"Controlled synthetic repair time: exhaustive "<<oldSeconds<<" s, lazy "<<newSeconds<<" s (not live Timer performance)\n";
}
