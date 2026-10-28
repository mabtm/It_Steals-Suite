#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
#include "PracticeMapWorker.h"
#include "seed_route_fixture.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
using namespace practice;
static void WireFootprints(MapData &data)
{
    // Asset-derived fixtures contain double hulls. Rebuild their float hulls
    // before IPC, matching the live capture's strict convex-footprint invariant.
    auto cross=[](Vec3 a,Vec3 b,Vec3 c){return (b.x-a.x)*double(c.z-a.z)-(b.z-a.z)*double(c.x-a.x);};
    for(auto &part:data.physicalParts)if((part.flags&2)&&part.points.size()>2)
    {
        auto points=part.points;
        std::sort(points.begin(),points.end(),[](Vec3 a,Vec3 b){return a.x<b.x||(a.x==b.x&&a.z<b.z);});
        points.erase(std::unique(points.begin(),points.end(),[](Vec3 a,Vec3 b){return a.x==b.x&&a.z==b.z;}),points.end());
        std::vector<Vec3> hull;
        for(auto p:points){while(hull.size()>1&&cross(hull[hull.size()-2],hull.back(),p)<=1e-8)hull.pop_back();hull.push_back(p);}
        size_t lower=hull.size();
        for(size_t i=points.size()-1;i-->0;){auto p=points[i];while(hull.size()>lower&&cross(hull[hull.size()-2],hull.back(),p)<=1e-8)hull.pop_back();hull.push_back(p);}
        if(hull.size()>2)hull.pop_back();part.points=std::move(hull);
    }
}
static bool TopEqual(const SeedSearchState &a,const SeedSearchState &b)
{
    if(a.top.size()!=b.top.size())return false;
    for(size_t i=0;i<a.top.size();i++)if(a.top[i].seed!=b.top[i].seed||a.top[i].index!=b.top[i].index||
        !SameLayout(a.top[i].selection,b.top[i].selection)||a.top[i].route.length!=b.top[i].route.length||
        a.top[i].route.order!=b.top[i].route.order||a.top[i].route.contacts!=b.top[i].route.contacts)return false;
    return true;
}
static void GameStopCases()
{
    auto fixture = SeedFixture();
    auto choices = fixture.samples;
    auto source = StreamingSource(fixture);
    auto request = StreamingRequest(*source, 32);
    SeedSearch search;
    assert(!search.TakeGameStop(false));
    search.Begin(request);
    assert(!search.TakeGameStop(true));
    search.Stop();
    assert(!search.Busy());
    for (int i = 0; i < 3; i++) assert(!search.TakeGameStop(true));
    assert(search.TakeGameStop(false) && !search.TakeGameStop(false));

    // A new request retires an older, unsent stop.
    search.Begin(request);
    assert(!search.TakeGameStop(false));
    search.Stop();
    assert(!search.TakeGameStop(true));
    auto newer = request;
    newer.serial++;
    search.Begin(newer);
    assert(!search.TakeGameStop(true) && !search.TakeGameStop(false));
    search.Stop();
    assert(search.TakeGameStop(false) && !search.TakeGameStop(false));

    // Losing the map context uses the same deferred cancellation.
    search.Begin(request);
    assert(!search.TakeGameStop(false));
    auto changed = std::make_shared<MapAnalysis>(*source);
    changed->session++;
    search.Update(changed);
    assert(!search.Busy() && !search.TakeGameStop(true));
    assert(search.TakeGameStop(false) && !search.TakeGameStop(false));

    // A finished Unity stream has already stopped its own sampler.
    search.Begin(request);
    assert(!search.TakeGameStop(false));
    search.Update(source);
    FeedStreaming(search, request, choices);
    auto result = AwaitStreaming(search);
    assert(result->phase == SearchPhase::Completed && !result->portable);
    assert(!search.TakeGameStop(false) && !search.TakeGameStop(false));
    std::cout << "Immediate local cancellation, delayed/one-time game stop, restart/context change and completed Unity stream: PASS\n";
}
int main(int argc,char **argv)
{
    assert(argc==2);std::filesystem::path fixtures(argv[1]);
    GameStopCases();
    for(int scene:kMapScenes)
    {
        auto data=LoadRouteBenchmarkFixture((fixtures/(scene==5?"hide-seek-boxes.routefixture":"hide-seek-outside-boxes.routefixture")).string().c_str(),scene==5?22:20,1.35f);
        data.scene=scene;data.spawn=data.liveSpawn=0;data.stage=2;
        WireFootprints(data);
        MapWorker maps;maps.Submit(EncodeFixture(data),77);
        auto until=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        std::shared_ptr<const MapAnalysis> source;
        while(std::chrono::steady_clock::now()<until){source=maps.Get(MapLevelIndex(scene));if(source&&!source->working)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        if(!source||!source->live||source->working)std::cerr<<"Scene "<<scene<<" map error "<<maps.Error()<<"\n";
        assert(source&&source->live&&!source->working&&source->data->stage==2&&source->floor&&source->outline);
        assert(!source->route.complete); // no duplicate full preview preparation during a live search
        auto request=StreamingRequest(*source,96,2);request.backend=SearchBackend::Cpu;
        SeedSearch search;
        search.Begin(request);assert(!search.TakeGameStop(false));search.Update(source);
        until=std::chrono::steady_clock::now()+std::chrono::seconds(15);
        bool preparing=false;
        while(std::chrono::steady_clock::now()<until)
        {
            auto state=search.Get();
            if(state->phase==SearchPhase::Preparing&&state->preparationStep=="navigation paths"){preparing=true;break;}
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(preparing); // cancel the expensive cold preparation, not only a warm search
        search.Stop();assert(!search.Busy());
        assert(!search.TakeGameStop(true));
        assert(search.TakeGameStop(false)&&!search.TakeGameStop(false));
        for(int round=0;round<2;round++)
        {
            search.Begin(request);assert(!search.TakeGameStop(false));search.Update(source);
            auto result=AwaitStreaming(search);
            assert(result->phase==SearchPhase::Completed&&result->evaluated==request.count&&result->generated==request.count);
            assert(result->scored+result->pruned+result->filtered==request.count&&result->preview&&result->top.size()==10);
            assert(result->distanceCacheReused==(round!=0));
            assert(!search.TakeGameStop(true));
            assert(search.TakeGameStop(false)&&!search.TakeGameStop(false));
            NavGraph nav(*source->data);int last=0,total=0;std::vector<std::string> stages;
            SearchDistances distances(*source->data,nav,[]{return false;},[&](int done,int count){assert(done>=last&&done<=count);last=done;total=count;},[&](const char *stage){stages.emplace_back(stage);});
            assert(last==total&&total>int(source->data->spawns.size()+source->data->points.size())*2&&stages.size()==3);
            auto reference=SeedSearch::RunFrozen(source,distances,request,[]{return false;});
            for(auto &row:reference.top)row.route=distances.Draw(row.selection,std::move(row.route),[]{return false;});
            assert(TopEqual(reference,*result));
        }
        request.count=SeedSpace;
        search.Begin(request);search.Update(source);search.Stop();assert(!search.Busy());
        request.count=96;search.Begin(request);search.Update(source);auto restarted=AwaitStreaming(search);
        assert(restarted->phase==SearchPhase::Completed&&restarted->evaluated==96);
        data.stage=1;data.requestSerial++;
        maps.Submit(EncodeFixture(data),77);
        until=std::chrono::steady_clock::now()+std::chrono::seconds(30);
        std::shared_ptr<const MapAnalysis> preview;
        while(std::chrono::steady_clock::now()<until){preview=maps.Get(MapLevelIndex(scene));if(preview&&preview->data->requestSerial==data.requestSerial&&!preview->working)break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        assert(preview&&preview->data->stage==1&&!preview->working&&preview->route.complete);
        std::cout<<"Scene "<<scene<<": live map -> cold/warm CPU search -> complete identical Top10/preview -> Stop/restart: PASS\n";
    }
}
