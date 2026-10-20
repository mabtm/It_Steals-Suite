#include "SeedSearch.h"
#include "RoutePlayback.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace practice;
int main(){
    for(int scene:{4,13}){
        auto d=std::make_shared<MapData>();d->scene=scene;d->spawn=0;d->keep=52;d->agentRadius=.4f;d->agentHeight=1.8f;d->agentSlope=45;d->physicalCaptured=true;d->spawns={{1,1,1}};
        d->vertices={{0,0,0},{90,0,0},{90,0,20},{0,0,20}};d->indices={0,1,2,0,2,3};
        for(int i=0;i<52;i++){Vec3 p{float(3+(i%26)*3),1,float(3+(i/26)*10)};d->points.push_back(p);d->selected.push_back(i);d->pickupRegions.push_back({true,p,.5f});}
        NavGraph graph(*d);auto started=std::chrono::steady_clock::now();SearchDistances distances(*d,graph,[]{return false;},[](int,int){});Selection selection;selection.spawn=0;selection.crystals=d->selected;
        auto scored=distances.Score(selection,true,[]{return false;});assert(scored.complete&&!scored.estimated&&scored.pickupOptimized&&scored.contacts.size()==52);
        auto route=distances.Draw(selection,scored,[]{return false;});assert(SuggestedSlice(route,1,1).visible);double length=0;for(size_t i=1;i<route.line.size();i++){assert(graph.Visible(route.line[i-1],route.line[i]));length+=Distance(route.line[i-1],route.line[i]);}assert(std::abs(length-route.length)<.001);
        auto preview=AnalyzeMap(d,graph);assert(preview.route.pickupOptimized&&!preview.route.estimated&&std::abs(preview.route.length-route.length)<.001);
        std::cout<<"Synthetic full-contact scene "<<scene<<": 52 crystals / 52 continuous contacts / checked drawing / preview-search agreement / "<<route.length<<" units / preparation+evaluation "<<std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count()<<" s: PASS\n";
    }
}
