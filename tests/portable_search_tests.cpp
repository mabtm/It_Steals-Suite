#include "seed_batch_fixture.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <random>

static practice::MapData CpuFixture(int crystals,int recipe) {
    auto f=SeedFixture();f.vertices.clear();f.indices.clear();f.walls.clear();f.spawns.clear();f.points.clear();f.selected.clear();f.liveSelected.clear();f.samples.clear();f.requested=f.completed=0;
    for(int i=0;i<9;i++)f.spawns.push_back({float(i*3+.17),1,float(i*i%19+.31)});
    for(int i=0;i<34;i++)f.points.push_back({float(i*1.73+.28),1,float(i*i%31*1.31+.47)});
    f.keep=crystals;for(int i=0;i<crystals;i++)f.selected.push_back(i);f.liveSelected=f.selected;
    f.rngStatus=2;f.rngProbes=4160;f.rngInit=0;f.rngRange=0;f.rngAdvance=1;f.rngSite=static_cast<int>(0xbe00001f);f.searchBackend=1;f.flags=1;
    f.recipe=recipe;f.seed=practice::RandomSeedOrder(FixtureSearchKey).SeedAt(0);f.requestSerial=100;f.stage=2;return f;
}
static std::shared_ptr<practice::MapAnalysis> Source(practice::MapData fixture) {
    std::string error;auto d=practice::DecodeMap(EncodeFixture(fixture,4),error);assert(d);
    auto source=std::make_shared<practice::MapAnalysis>();source->data=d;source->session=77;source->live=true;return source;
}
int main(int argc,char** argv) {
    if(argc>1) {
        std::ifstream in(std::string(argv[1])+"/portable-rng-wire.bin",std::ios::binary);auto read=[&]{int32_t v;in.read(reinterpret_cast<char*>(&v),4);assert(in);return v;};
        assert(read()==0x52474e50);int rows=read();assert(rows==320);
        for(int i=0;i<rows;i++){
            auto f=CpuFixture(18,2);f.rngInit=read();f.rngRange=read();f.rngAdvance=read();int seed=read();f.rngSite=read();int applied=read();
            assert(practice::DerivedPracticeSeed(seed,12,f.rngSite)==applied);f.scene=12;practice::PortableSelectionGenerator generator(f);practice::Selection actual;generator.Generate(seed,2,actual);
            assert(actual.spawn==read());for(int id:actual.crystals)assert(id==read());
        }
        std::ifstream map(std::string(argv[1])+"/portable-map-wire.bin",std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(map),std::istreambuf_iterator<char>()};
        std::string error;auto decoded=practice::DecodeMap(bytes,error);assert(decoded&&decoded->rngStatus==2&&decoded->searchBackend==1&&decoded->scene==12);
        std::mt19937 random(777);for(int i=0;i<5000;i++){auto corrupt=bytes;if(i%3==0)corrupt.resize(random()%(bytes.size()+1));else corrupt[random()%corrupt.size()]=static_cast<uint8_t>(random());auto d=practice::DecodeMap(std::move(corrupt),error);if(d)assert(d->rngStatus!=2||d->rngProbes==4160);}
    }
    for(int recipe:{1,2})for(int crystals:{8,18}) {
        auto source=Source(CpuFixture(crystals,recipe));auto request=StreamingRequest(*source,2000,1,true);
        practice::PortableSelectionGenerator generator(*source->data);practice::RandomSeedOrder order(request.key);std::vector<practice::Selection> layouts;
        for(uint32_t i=0;i<request.count;i++){practice::Selection layout;generator.Generate(order.SeedAt(i),recipe,layout);layouts.push_back(layout);}
        auto streamData=std::make_shared<practice::MapData>(*source->data);streamData->searchBackend=0;auto streamSource=std::make_shared<practice::MapAnalysis>(*source);streamSource->data=streamData;
        practice::SeedSearch original;original.Begin(request);original.Update(streamSource);FeedStreaming(original,request,layouts);auto expected=AwaitStreaming(original);assert(expected->phase==practice::SearchPhase::Completed&&expected->top.size()==10);
        for(int workers:{1,3,6}) {
            request.workers=workers;practice::SeedSearch cpu;cpu.Begin(request);cpu.Update(source);auto actual=AwaitStreaming(cpu);
            assert(actual->phase==practice::SearchPhase::Completed&&actual->portable&&actual->generated==request.count&&actual->evaluated==request.count&&actual->filtered==0&&actual->estimated==request.count);
            assert(actual->scored+actual->pruned==actual->evaluated&&actual->top.size()==expected->top.size());
            for(size_t i=0;i<actual->top.size();i++){assert(actual->top[i].seed==expected->top[i].seed&&practice::SameLayout(actual->top[i].selection,expected->top[i].selection));assert(std::abs(actual->top[i].route.length-expected->top[i].route.length)<1e-7);}
            assert(actual->preview&&actual->preview->data->seed==actual->top[0].seed&&actual->preview->route.complete&&!actual->preview->route.line.empty());
            request.criteria.spawn=2;request.criteria.catalog=practice::CatalogKey(*source->data);cpu.Begin(request);cpu.Update(source);auto filtered=AwaitStreaming(cpu);assert(filtered->phase==practice::SearchPhase::Completed&&filtered->evaluated==request.count&&filtered->filtered>0&&filtered->filtered<request.count);for(const auto& row:filtered->top)assert(row.selection.spawn==2);request.criteria.spawn=-1;
        }
    }
    auto source=Source(CpuFixture(14,2));auto request=StreamingRequest(*source,practice::SeedSpace,6,false);
    practice::SeedSearch cancel;cancel.Begin(request);cancel.Update(source);auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(std::chrono::steady_clock::now()<deadline&&cancel.Get()->evaluated==0)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(cancel.Get()->evaluated>0);cancel.Stop();auto stopped=cancel.Get();assert(stopped->phase==practice::SearchPhase::Stopped);auto done=stopped->evaluated;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));assert(cancel.Get()->evaluated==done);
    request.count=128;request.fast=true;request.layoutOnly=true;cancel.Begin(request);cancel.Update(source);auto restart=AwaitStreaming(cancel);assert(restart->phase==practice::SearchPhase::Completed&&restart->evaluated==128&&restart->top.size()==10);
    auto bad=std::make_shared<practice::MapData>(*source->data);bad->rngStatus=3;auto badSource=std::make_shared<practice::MapAnalysis>(*source);badSource->data=bad;cancel.Begin(request);cancel.Update(badSource);assert(AwaitStreaming(cancel)->phase==practice::SearchPhase::Failed);
    auto altered=CpuFixture(18,1);altered.rngProbes=4159;std::string error;assert(!practice::DecodeMap(EncodeFixture(altered,4),error));
    std::cout<<"Portable C#/C++ generation, old-pipeline ranking parity, 1/3/6 workers, seed counting, spawn filter, cancellation/restart and strict validation: PASS\n";
}
