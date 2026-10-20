#include "SeedSearch.h"
#include "seed_map_fixture.h"
#include <cassert>
#include <iostream>
using namespace practice;
static MapData World(int scene) {
    MapData d;d.pid=1234;d.scene=scene;d.epoch=1;d.spawn=d.liveSpawn=0;d.keep=1;d.flags=1;d.stage=1;
    d.physicalCaptured=true;d.agentRadius=.4f;d.agentHeight=2;d.agentSlope=45;
    FixtureTile(d,-10,-10,40,40);d.spawns={{0,1,0}};d.points={{10,1,0}};d.selected=d.liveSelected={0};
    return d;
}
static PhysicalPart Solid(float x0,float z0,float x1,float z1) {return {3,{{x0,0,z0},{x1,0,z0},{x1,0,z1},{x0,0,z1}}};}
static double Arc(const std::vector<Vec3>& line) {double length=0;for(size_t i=1;i<line.size();i++)length+=Distance(line[i-1],line[i]);return length;}
static void Check(const MapData& d,double expected=-1) {
    NavGraph graph(d);auto route=AnalyzeMap(std::make_shared<MapData>(d),graph).route;
    assert(route.complete&&!route.estimated&&!route.line.empty());
    assert(std::abs(Arc(route.line)-route.length)<1e-5);
    if(expected>=0)assert(std::abs(route.length-expected)<.002);
    for(size_t i=1;i<route.line.size();i++)assert(graph.Visible(route.line[i-1],route.line[i]));
    SearchDistances distances(d,graph,[]{return false;},[](int,int){});Selection selection{0,d.selected};
    auto score=distances.Score(selection,false,[]{return false;});auto drawn=distances.Draw(selection,score,[]{return false;});
    assert(score.complete&&!score.estimated&&score.length==drawn.length&&std::abs(Arc(drawn.line)-score.length)<1e-5);
    assert(std::abs(score.length-route.length)<1e-5);
    for(auto estimated:drawn.estimatedLine)assert(!estimated);
    RouteWorkspace work;int count=distances.FillBoundMatrix(selection,work);
    assert(MstRouteLowerBound(work.matrix.data(),count,[]{return false;})<=score.length+1e-6);
}
static void PickupAcrossWall() {
    for(int scene:{3,4,5,7,8,12,13,14}) {
        auto d=World(scene);d.points={{4.7f,1,0}};d.pickupRegions={{true,d.points[0],1.4f}};
        // The wall spans the whole modeled floor. Its center is unreachable,
        // but the real trigger extends into the player's accessible room.
        d.physicalParts={Solid(4,-11,4.2f,31)};Check(d,3.3);
        d.points.push_back({4.7f,1,10});d.pickupRegions.push_back({true,d.points.back(),1.4f});d.keep=2;d.selected=d.liveSelected={0,1};Check(d);
    }
}
static void SegmentDetour() {
    for(int scene:{3,4,5,7,8,12,13,14}) {
        auto d=World(scene);d.physicalParts={{3,{{5,0,-4},{5,0,4}}}};Check(d);
        std::reverse(d.physicalParts[0].points.begin(),d.physicalParts[0].points.end());Check(d);
        // Joining two clipped triangles must preserve the wall's full extent.
        d.physicalParts.push_back({3,{{5,0,4},{5,0,7}}});Check(d);
    }
}
static void MissingCenterAnchor() {
    for(int scene:{3,4,5,7,8,12,13,14}) {
        auto d=World(scene);d.pickupRegions={{true,d.points[0],1.4f}};
        d.navigationModel=1;d.navigationStatus=2;d.navigationNodes={{0,0,0}};d.navigationAnchors={0,-1};
        NavGraph graph(d);assert(graph.UsesUnityCorridors()&&graph.CatalogProject(1,d.points[0]).triangle<0);
        Check(d,8.6);
    }
}
static void UnknownContactsStayUnknown() {
    auto d=World(5);d.points={{10,1,0},{0,2,5}};d.pickupRegions={{true,d.points[0],.1f},{true,d.points[1],1.4f}};d.keep=2;d.selected=d.liveSelected={0,1};
    d.navigationModel=1;d.navigationStatus=2;d.navigationNodes={{0,0,0},{0,0,5}};d.navigationEdges={0,1};d.navigationAnchors={0,-1,1};
    d.physicalParts={Solid(9,-2,11,2)};NavGraph graph(d);
    auto route=AnalyzeMap(std::make_shared<MapData>(d),graph).route;
    assert(route.complete&&route.estimated&&route.pickupOptimized&&std::abs(Arc(route.line)-route.length)<1e-5);
    assert(std::any_of(route.estimatedLine.begin(),route.estimatedLine.end(),[](auto flag){return flag!=0;}));
    for(size_t i=1;i<route.line.size();i++)if(!route.estimatedLine[i-1])assert(graph.Visible(route.line[i-1],route.line[i]));
    SearchDistances distances(d,graph,[]{return false;},[](int,int){});Selection selection{0,d.selected};auto score=distances.Score(selection,false,[]{return false;});auto drawn=distances.Draw(selection,score,[]{return false;});
    assert(score.estimated&&std::abs(Arc(drawn.line)-score.length)<1e-5&&std::abs(route.length-score.length)<1e-5);
    // Unsupported or omitted physical data cannot authorize contact shortcuts.
    auto incomplete=World(5);incomplete.pickupRegions={{true,incomplete.points[0],1.4f}};incomplete.physicalOmitted=1;
    assert(!AnalyzeMap(std::make_shared<MapData>(incomplete),NavGraph(incomplete)).route.pickupOptimized);
    auto cancelData=World(5);cancelData.pickupRegions={{true,cancelData.points[0],1.4f}};NavGraph cancelGraph(cancelData);bool canceled=false;
    try{PickupRouting table(cancelData,cancelGraph,{0,1},[]{return true;});}catch(const std::runtime_error&){canceled=true;}assert(canceled);
    PickupRouting table(cancelData,cancelGraph,{0,1});canceled=false;
    try{table.SetLeg(0,1,{{table.Anchor(0),table.Anchor(1)},10},false,[]{return true;});}catch(const std::runtime_error&){canceled=true;}assert(canceled);
}
int main(int argc,char** argv) {
    std::string test=argc>1?argv[1]:"all";
    if(test=="all"||test=="pickup")PickupAcrossWall();
    if(test=="all"||test=="segment")SegmentDetour();
    if(test=="all"||test=="anchor")MissingCenterAnchor();
    if(test=="all"||test=="unknown")UnknownContactsStayUnknown();
    std::cout<<"Pickup across unreachable centers, missing corridor anchors, clipped wall segments, shared eight-mode routes: PASS\n";
}
