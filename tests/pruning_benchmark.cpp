#include "seed_batch_fixture.h"
#include <cassert>
#include <iostream>
#include <random>
int main(int argc,char** argv){
    int n=argc>1?std::stoi(argv[1]):14;bool fast=argc>2&&std::stoi(argv[2]);
    bool uniform=argc>3&&std::stoi(argv[3]);
    auto f=SeedFixture();f.vertices.clear();f.indices.clear();f.walls.clear();FixtureTile(f,0,0,200,200);
    f.points.clear();for(int i=0;i<100;i++)f.points.push_back({float(2+i%10*(i<30?2:18)),1,float(2+i/10*(i<30?2:18))});
    f.spawns={{1,1,1},{195,1,195}};f.spawn=0;f.liveSpawn=0;f.keep=n;f.selected.clear();for(int i=0;i<n;i++)f.selected.push_back(i);f.liveSelected=f.selected;
    std::mt19937 random(1023);std::vector<practice::Selection> choices;
    for(int i=0;i<600;i++){
        std::vector<int> candidates(!uniform&&i<20?std::max(30,n+10):100);std::iota(candidates.begin(),candidates.end(),0);std::shuffle(candidates.begin(),candidates.end(),random);
        practice::Selection choice;choice.spawn=!uniform&&i<20?0:i%2;choice.crystals.assign(candidates.begin(),candidates.begin()+n);choices.push_back(std::move(choice));
    }
    auto source=StreamingSource(f);auto request=StreamingRequest(*source,choices.size(),1,fast);practice::SeedSearch search;
    auto start=std::chrono::steady_clock::now();search.Begin(request);search.Update(source);FeedStreaming(search,request,choices);auto result=AwaitStreaming(search);
    assert(result->phase==practice::SearchPhase::Completed&&result->evaluated==choices.size()&&result->top.size()==10);
    std::cout<<n<<" crystals uniform="<<uniform<<" fast="<<fast<<" seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" checksum=";
    for(auto& row:result->top)std::cout<<row.index<<":"<<row.route.length<<",";
#ifdef ADAPTIVE_PRUNING
    std::cout<<" pruned="<<result->pruned<<" scored="<<result->scored;
#endif
    std::cout<<"\n";
}
