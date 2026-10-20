#include "SeedSearch.h"
#include "seed_map_fixture.h"
#include <cassert>
#include <iostream>
#include <fstream>
using namespace practice;
static MapData World(int scene=3){MapData d;d.pid=1234;d.scene=scene;d.epoch=1;d.spawn=d.liveSpawn=0;d.keep=2;d.flags=1;d.stage=1;d.physicalCaptured=true;d.agentRadius=.4f;d.agentHeight=2;d.agentSlope=45;FixtureTile(d,-10,-10,40,40);d.spawns={{0,1,0}};d.points={{10,1,0},{20,1,0}};d.selected=d.liveSelected={0,1};d.pickupRegions={{true,d.points[0],1.4f},{true,d.points[1],1.4f}};return d;}
static PhysicalPart Solid(float a,float b,float c,float e){return {3,{{a,0,b},{c,0,b},{c,0,e},{a,0,e}}};}
static double Arc(const std::vector<Vec3>& line){double length=0;for(size_t i=1;i<line.size();i++)length+=Distance(line[i-1],line[i]);return length;}
int main(int argc,char** argv){
    if(argc>1){std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};std::string error;auto wire=DecodeMap(bytes,error);assert(wire&&wire->scene==5&&wire->physicalCaptured&&wire->physicalParts.size()==1&&wire->pickupRegions[0].radius==1.2f);}
    for(int scene:{3,4,5,7,8,12,13,14}){
        auto d=World(scene);std::string error;auto decoded=DecodeMap(EncodeFixture(d),error);assert(decoded&&error.empty()&&decoded->pickupRegions.size()==2&&decoded->physicalCaptured);NavGraph nav(d);auto route=AnalyzeMap(decoded,nav).route;
        assert(route.complete&&!route.estimated&&route.pickupOptimized&&!route.exact);assert(std::abs(route.length-18.6)<.002);assert(std::abs(Arc(route.line)-route.length)<1e-5);
        assert(std::hypot(route.line.back().x-20,route.line.back().z)<1.401);
        SearchDistances distances(d,nav,[]{return false;},[](int,int){});Selection selection{0,{0,1}};auto score=distances.Score(selection,false,[]{return false;});auto drawn=distances.Draw(selection,score,[]{return false;});assert(score.length==drawn.length&&std::abs(Arc(drawn.line)-score.length)<1e-5);assert(std::abs(drawn.length-route.length)<1e-5);
        RouteWorkspace work;int count=distances.FillBoundMatrix(selection,work);assert(MstRouteLowerBound(work.matrix.data(),count,[]{return false;})<=score.length+1e-6);
        // A complete AI floor containing a real wall is insufficient. All modes
        // must detour using the player's radius, even Outside's smaller AI bake.
        d=World(scene);d.points={{10,1,0}};d.keep=1;d.selected=d.liveSelected={0};d.pickupRegions={{true,d.points[0],1.4f}};d.physicalParts={Solid(4,-4,6,4)};NavGraph wall(d);auto checked=AnalyzeMap(std::make_shared<MapData>(d),wall).route;
        assert(checked.complete&&!checked.estimated&&checked.pickupOptimized);assert(checked.length>10);assert(std::abs(Arc(checked.line)-checked.length)<1e-5);
        for(size_t i=1;i<checked.line.size();i++)assert(wall.Visible(checked.line[i-1],checked.line[i]));
        assert(!wall.Visible({0,0,0},{10,0,0}));
        // Small-radius enemy meshes can project a crystal center too close to
        // a wall. The safe anchor still lies inside the actual pickup region.
        d.points[0]={6.1f,1,0};d.pickupRegions[0].center=d.points[0];NavGraph nearWall(d);auto anchor=nearWall.CatalogProject(1,d.points[0]);assert(anchor.triangle>=0&&nearWall.Visible(anchor.point,anchor.point));assert(std::hypot(anchor.point.x-6.1,anchor.point.z)<1.4);
        auto touch=AnalyzeMap(std::make_shared<MapData>(d),nearWall).route;assert(touch.complete&&!touch.estimated&&touch.pickupOptimized);for(size_t i=1;i<touch.line.size();i++)assert(nearWall.Visible(touch.line[i-1],touch.line[i]));
    }
    auto fallback=World(5);fallback.points={{10,1,0}};fallback.pickupRegions={{true,fallback.points[0],1.4f}};fallback.keep=1;fallback.selected=fallback.liveSelected={0};fallback.navigationModel=1;fallback.navigationStatus=2;fallback.navigationNodes={{0,0,0},{10,0,0}};fallback.navigationEdges={0,1};fallback.navigationAnchors={0,1};NavGraph corridors(fallback);assert(corridors.UsesUnityCorridors());auto contact=AnalyzeMap(std::make_shared<MapData>(fallback),corridors).route;assert(contact.pickupOptimized&&!contact.estimated&&std::abs(contact.length-8.6)<.001);SearchDistances corridorCosts(fallback,corridors,[]{return false;},[](int,int){});auto score=corridorCosts.Score({0,{0}},false,[]{return false;});auto drawn=corridorCosts.Draw({0,{0}},score,[]{return false;});assert(score.pickupOptimized&&std::abs(score.length-Arc(drawn.line))<1e-5);
    auto through=World();through.keep=1;through.points={{4.7f,1,0}};through.selected=through.liveSelected={0};through.pickupRegions={{true,through.points[0],1.4f}};through.physicalParts={Solid(4,-4,4.2f,4)};NavGraph thinWall(through);auto edge=AnalyzeMap(std::make_shared<MapData>(through),thinWall).route;assert(edge.complete&&!edge.estimated&&edge.pickupOptimized&&std::abs(edge.length-3.3)<.001);for(size_t i=1;i<edge.line.size();i++)assert(thinWall.Visible(edge.line[i-1],edge.line[i]));
    auto ai=World(13);ai.vertices.clear();ai.indices.clear();ai.spawns={{2,1,2}};ai.points={{18,1,2}};ai.selected=ai.liveSelected={0};ai.keep=1;ai.pickupRegions={{true,ai.points[0],1.4f}};ai.physicalParts={Solid(8,0,12,12)};
    float left=7.85f,right=12.15f,top=12.15f;FixtureTile(ai,0,0,left,top);FixtureTile(ai,0,top,left,20-top);FixtureTile(ai,left,top,right-left,20-top);FixtureTile(ai,right,0,20-right,top);FixtureTile(ai,right,top,20-right,20-top);
    NavGraph smallerAgent(ai);auto safe=AnalyzeMap(std::make_shared<MapData>(ai),smallerAgent).route;assert(safe.complete&&!safe.estimated&&safe.pickupOptimized);for(size_t i=1;i<safe.line.size();i++)assert(smallerAgent.Visible(safe.line[i-1],safe.line[i]));
    auto d=World();auto overlap=World();overlap.keep=1;overlap.points={{.5f,1,0}};overlap.pickupRegions={{true,overlap.points[0],1.4f}};overlap.selected=overlap.liveSelected={0};auto instant=AnalyzeMap(std::make_shared<MapData>(overlap),NavGraph(overlap)).route;assert(instant.complete&&instant.pickupOptimized&&instant.length<1e-6);
    // A waypoint cannot jump from independent closest entry to a different
    // closest exit. One shared contact per crystal keeps score and line equal.
    d.points={{8,1,4},{16,1,0}};d.pickupRegions={{true,d.points[0],1.4f},{true,d.points[1],1.4f}};auto curve=AnalyzeMap(std::make_shared<MapData>(d),NavGraph(d)).route;assert(curve.pickupOptimized&&std::abs(Arc(curve.line)-curve.length)<1e-5);
    auto original=d;original.physicalCaptured=false;original.physicalParts.clear();original.pickupRegions.clear();auto legacy=AnalyzeMap(std::make_shared<MapData>(original),NavGraph(original)).route;assert(!legacy.pickupOptimized&&curve.length<=legacy.length+1e-5);
    // Trigger eligibility is explicit; unknown shapes retain center routing.
    d.pickupRegions[0].verified=d.pickupRegions[1].verified=false;assert(!AnalyzeMap(std::make_shared<MapData>(d),NavGraph(d)).route.pickupOptimized);
    auto cache=World();assert(!SameNavigationCatalog(cache,original));auto changed=cache;changed.pickupRegions[0].radius-=.1f;assert(!SameNavigationCatalog(cache,changed));changed=cache;changed.physicalParts={Solid(4,-4,6,4)};assert(!SameNavigationCatalog(cache,changed));
    auto bytes=EncodeFixture(cache);bytes[bytes.size()-9]^=1;std::string error;assert(!DecodeMap(bytes,error));
    bool canceled=false;try{NavGraph nav(cache);AnalyzeMap(std::make_shared<MapData>(cache),nav,[]{return true;});}catch(const std::runtime_error&){canceled=true;}assert(canceled);
    std::cout<<"Actual pickup contacts, continuous costs/draw, physical detours, safe anchors, immutable cache and eight-mode parity: PASS\n";
}
