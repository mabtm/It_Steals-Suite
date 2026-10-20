#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ostream>
#include <string>
#include <thread>
#include "SeedSearchOptions.h"

inline int SeedWorkerLimit(){unsigned cpus=std::thread::hardware_concurrency();return static_cast<int>(std::clamp(cpus>1?cpus-1:1u,1u,32u));}

struct PlannerPreferences {
    int level=0,seed=0,recipe=2;
    uint64_t searchCount=100;
    int searchWorkers=std::min(4,static_cast<int>(std::max(1u,std::thread::hardware_concurrency()/2)));
    float generationMs=2;
    bool automaticLoad=true,calculateRoutes=true;
    bool portableGeneration=true;
    bool searchFast=false;
    int searchObjective=0;
    std::array<practice::SearchMapFilter,kMapLevelCount> filters;
    bool candidates=true,minimapRoute=false;
    void Validate(){level=std::clamp(level,0,kMapLevelCount-1);recipe=std::clamp(recipe,1,2);searchCount=std::clamp(searchCount,uint64_t{1},uint64_t{1}<<32);
        searchWorkers=std::clamp(searchWorkers,1,SeedWorkerLimit());generationMs=std::isfinite(generationMs)?std::clamp(generationMs,.25f,8.f):2.f;
        searchObjective=std::clamp(searchObjective,0,1);}
    practice::SearchCriteria Criteria() const {
        practice::SearchCriteria value;const auto& filter=filters.at(level);
        value.objective=static_cast<practice::SearchObjective>(searchObjective);
        value.spawn=filter.spawn;value.catalog=filter.catalog;return value;
    }
    void Encode(std::ostream& out) const {
        out<<"planner.level="<<level<<"\nplanner.seed="<<seed<<"\nplanner.recipe="<<recipe
           <<"\nplanner.candidates="<<candidates<<"\nplanner.minimapRoute="<<minimapRoute
           <<"\nplanner.searchCount="<<searchCount<<"\nplanner.searchFast="<<searchFast
           <<"\nplanner.searchWorkers="<<searchWorkers<<"\nplanner.generationMs="<<generationMs
           <<"\nplanner.searchGoal="<<searchObjective<<"\nplanner.automaticLoad="<<automaticLoad<<"\nplanner.calculateRoutes="<<calculateRoutes<<"\nplanner.portableGeneration="<<portableGeneration<<'\n';
        for(int i=0;i<kMapLevelCount;i++)filters[i].Encode(out,i);
    }
    void Decode(const std::string& key,double value) {
        if(!std::isfinite(value))return;
        int n=static_cast<int>(std::clamp(value,-2147483648.,2147483647.));
        if(key=="level")level=n;else if(key=="seed")seed=n;else if(key=="recipe")recipe=n;
        else if(key=="searchCount")searchCount=static_cast<uint64_t>(std::clamp(value,1.,4294967296.));
        else if(key=="searchWorkers")searchWorkers=n;
        else if(key=="generationMs")generationMs=static_cast<float>(value);
        else if(key=="searchObjective")searchObjective=n==2?1:0;
        else if(key=="searchGoal")searchObjective=n;
        else if(key=="calculateRoutes"&&(value==0||value==1))calculateRoutes=value==1;
        else if(key=="portableGeneration"&&(value==0||value==1))portableGeneration=value==1;
        else if(key=="automaticLoad"&&(value==0||value==1))automaticLoad=value==1;
        else if(key.size()>8&&key.rfind("filter",0)==0&&key[6]>='0'&&key[6]<='0'+kMapLevelCount-1&&key[7]=='.')filters[key[6]-'0'].Decode(key.substr(8),value);
        else if(key=="searchFast"&&(value==0||value==1))searchFast=value==1;
        else if(key=="candidates"&&(value==0||value==1))candidates=value==1;
        else if(key=="minimapRoute"&&(value==0||value==1))minimapRoute=value==1;
    }
};
