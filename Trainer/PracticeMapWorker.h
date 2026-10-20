#pragma once
#include "PracticeRouting.h"
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

namespace practice {
// One sleeping worker, latest-request wins. Navigation, route search, decoding
// and atomic cache writes are never performed by the render thread.
class MapWorker {
    struct Task {std::vector<uint8_t> bytes;std::shared_ptr<const MapData> prepared;int cacheLevel=-1;bool allCaches=false,live=true;uint64_t generation=0;uint32_t session=0;};
    std::array<std::shared_ptr<const MapAnalysis>,kMapLevelCount> results{};
    std::shared_ptr<const std::string> lastError;
    std::mutex gate;std::condition_variable ready;std::thread thread;
    std::atomic<uint64_t> generation{0};std::atomic<bool> stopping{false};
    Task pending;bool hasTask=false;std::filesystem::path directory;
    static bool SameLayout(const MapData& a,const MapData& b) {
        return !a.review&&!b.review&&a.spawn==b.spawn&&a.selected==b.selected&&a.points.size()==b.points.size()&&a.spawns.size()==b.spawns.size()&&
            (a.points.empty()||std::memcmp(a.points.data(),b.points.data(),a.points.size()*sizeof(Vec3))==0)&&
            (a.spawns.empty()||std::memcmp(a.spawns.data(),b.spawns.data(),a.spawns.size()*sizeof(Vec3))==0);
    }
    static std::filesystem::path CachePath(const std::filesystem::path& root,int level) {return root/"SeedMaps"/("scene_"+std::to_string(kMapScenes[level])+".ismap");}
    static std::vector<uint8_t> Load(const std::filesystem::path& file) {
        std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in)return {};
        auto size=in.tellg();if(size<0||size>static_cast<std::streamoff>(MapCapacity-64))throw std::runtime_error("Cached map exceeds file limit");
        std::vector<uint8_t> bytes(static_cast<size_t>(size));in.seekg(0);
        if(!in.read(reinterpret_cast<char*>(bytes.data()),size))throw std::runtime_error("Could not read cached map");
        return bytes;
    }
    static void Save(const std::filesystem::path& file,const std::vector<uint8_t>& bytes) {
        std::filesystem::create_directories(file.parent_path());auto temporary=file;temporary+=".tmp";
        {
            std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("Map cache is not writable");
            out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));out.flush();
            if(!out)throw std::runtime_error("Map cache write failed");
        }
#ifdef _WIN32
        if(!MoveFileExW(temporary.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Map cache replacement failed");
#else
        std::filesystem::rename(temporary,file);
#endif
    }
    void Work() {
        std::shared_ptr<const MapData> graphData;std::unique_ptr<NavGraph> graph;
        std::shared_ptr<const std::vector<std::pair<Vec3,Vec3>>> outline;
        while(!stopping) {
            Task task;std::filesystem::path root;
            {
                std::unique_lock<std::mutex> lock(gate);ready.wait(lock,[&]{return stopping||hasTask;});if(stopping)return;
                task=std::move(pending);hasTask=false;root=directory;
            }
            auto canceled=[&]{return stopping||generation.load()!=task.generation;};
            int begin=task.allCaches?0:task.cacheLevel,end=task.allCaches?kMapLevelCount:task.cacheLevel+1;
            if(task.cacheLevel<0&&!task.allCaches){begin=-1;end=0;}
            for(int cache=begin;cache<end;cache++) {
                if(canceled())break;
                std::shared_ptr<const MapData> data;int level=cache;
                try {
                    data=task.prepared;
                    if(!data) {
                        auto bytes=cache>=0?Load(CachePath(root,cache)):std::move(task.bytes);if(bytes.empty())continue;
                        if(cache<0&&bytes.size()>=16){int32_t scene;std::memcpy(&scene,bytes.data()+12,4);level=MapLevelIndex(scene);}
                        std::string error;data=DecodeMap(std::move(bytes),error);if(!data)throw std::runtime_error(error);
                    }
                    level=MapLevelIndex(data->scene);if(cache>=0&&level!=cache)throw std::runtime_error("Cached map belongs to another level");
                    std::atomic_store(&lastError,std::shared_ptr<const std::string>());
                    if(task.allCaches) {
                        auto saved=std::make_shared<MapAnalysis>();saved->data=data;saved->needsAnalysis=true;
                        if(data->physicalCaptured||IsHideSeekScene(data->scene)){NavGraph floorGraph(*data,canceled,true);saved->floor=std::make_shared<const std::vector<std::array<Vec3,3>>>(floorGraph.Floor());saved->outline=std::make_shared<const std::vector<std::pair<Vec3,Vec3>>>(floorGraph.Boundary());}
                        saved->route.message="Saved layout; open the Seed Map to calculate its route.";
                        if(!canceled())std::atomic_store(&results[level],std::shared_ptr<const MapAnalysis>(saved));
                        continue;
                    }
                    bool live=cache<0&&task.live&&!data->review;auto previous=Get(level);
                    if(data->spawns.empty()&&previous&&previous->data&&!previous->data->spawns.empty()) {
                        auto saved=std::make_shared<MapAnalysis>(*previous);saved->live=false;saved->working=false;saved->error=data->message;
                        if(!canceled())std::atomic_store(&results[level],std::shared_ptr<const MapAnalysis>(saved));
                        continue;
                    }
                    std::string cacheWarning;
                    if(live&&!root.empty()&&!data->spawns.empty()&&data->stage==1&&!(data->flags&128)&&!canceled()) {
                        try {Save(CachePath(root,level),data->bytes);}catch(const std::exception& e){cacheWarning=e.what();}
                    }
                    auto initial=std::make_shared<MapAnalysis>();initial->data=data;initial->live=live;initial->working=true;initial->session=task.session;initial->cacheWarning=cacheWarning;
                    bool sameCatalog=previous&&previous->data&&SameNavigationCatalog(*previous->data,*data);
                    bool reusable=sameCatalog&&previous->route.complete&&SameLayout(*previous->data,*data);
                    if(sameCatalog){initial->floor=previous->floor;initial->outline=previous->outline;}
                    if(reusable)initial->route=previous->route;
                    if(!canceled())std::atomic_store(&results[level],std::shared_ptr<const MapAnalysis>(initial));
                    if(!graphData||!SameNavigationCatalog(*graphData,*data)) {graph=std::make_unique<NavGraph>(*data,canceled);graphData=data;outline=std::make_shared<const std::vector<std::pair<Vec3,Vec3>>>(graph->Boundary());}
                    MapAnalysis analysis;
                    if(reusable) {analysis.data=data;analysis.route=previous->route;if(data->review)analysis=AnalyzeMap(data,*graph,canceled);}
                    else analysis=AnalyzeMap(data,*graph,canceled);
                    analysis.outline=outline;if((data->physicalCaptured||IsHideSeekScene(data->scene))&&!analysis.floor)analysis.floor=std::make_shared<const std::vector<std::array<Vec3,3>>>(graph->Floor());analysis.live=live;analysis.session=task.session;analysis.cacheWarning=cacheWarning;
                    if(canceled())break;
                    if(!canceled())std::atomic_store(&results[level],std::shared_ptr<const MapAnalysis>(std::make_shared<MapAnalysis>(std::move(analysis))));
                } catch(const std::exception& e) {
                    if(canceled())break;
                    std::atomic_store(&lastError,std::make_shared<const std::string>(e.what()));
                    if(level>=0&&level<kMapLevelCount){auto old=Get(level);auto failed=old?std::make_shared<MapAnalysis>(*old):std::make_shared<MapAnalysis>();failed->data=old?old->data:data;failed->working=false;failed->error=e.what();failed->live=false;std::atomic_store(&results[level],std::shared_ptr<const MapAnalysis>(failed));}
                }
            }
        }
    }
    void Queue(Task task) {
        task.generation=++generation;
        {std::lock_guard<std::mutex> lock(gate);pending=std::move(task);hasTask=true;if(!thread.joinable())thread=std::thread([this]{Work();});}
        ready.notify_one();
    }
public:
    MapWorker()=default;MapWorker(const MapWorker&)=delete;
    ~MapWorker(){stopping=true;++generation;ready.notify_one();if(thread.joinable())thread.join();}
    void SetDirectory(std::filesystem::path root){std::lock_guard<std::mutex> lock(gate);directory=std::move(root);}
    void LoadCaches(){Task task;task.allCaches=true;task.live=false;Queue(std::move(task));}
    void LoadCache(int level){if(level<0||level>=kMapLevelCount)return;Task task;task.cacheLevel=level;task.live=false;Queue(std::move(task));}
    void Submit(std::vector<uint8_t> bytes,uint32_t session=0){Task task;task.bytes=std::move(bytes);task.session=session;Queue(std::move(task));}
    void Cancel(){++generation;}
    void Pause(){Cancel();for(int i=0;i<kMapLevelCount;i++){auto old=Get(i);if(old&&old->working){auto copy=std::make_shared<MapAnalysis>(*old);copy->working=false;copy->needsAnalysis=true;std::atomic_store(&results[i],std::shared_ptr<const MapAnalysis>(copy));}}}
    void InvalidateLive(){Pause();for(int i=0;i<kMapLevelCount;i++){auto old=Get(i);if(old&&old->live){auto copy=std::make_shared<MapAnalysis>(*old);copy->live=false;std::atomic_store(&results[i],std::shared_ptr<const MapAnalysis>(copy));}}}
    void AnalyzeSaved(int level){auto old=Get(level);if(!old||!old->data||!old->needsAnalysis)return;auto copy=std::make_shared<MapAnalysis>(*old);copy->needsAnalysis=false;copy->working=true;std::atomic_store(&results[level],std::shared_ptr<const MapAnalysis>(copy));Task task;task.prepared=old->data;task.live=old->live;task.session=old->session;Queue(std::move(task));}
    std::shared_ptr<const MapAnalysis> Get(int level) const {return level>=0&&level<kMapLevelCount?std::atomic_load(&results[level]):nullptr;}
    std::string Error() const {auto error=std::atomic_load(&lastError);return error?*error:std::string();}
};
}
