#pragma once
#include "PracticeMapData.h"
#include <array>
#include <bit>
#include <charconv>
#include <ostream>

namespace practice {
enum class SearchObjective {ShortestRoute,LongestRoute};
inline const char* ObjectiveName(SearchObjective value) {return value==SearchObjective::LongestRoute?"Longest route (practice)":"Shortest complete route";}
// Candidate IDs depend on capture order. Never silently reuse a filter after
// that order or its coordinates changed, including a different game build.
inline uint64_t CatalogKey(const MapData& data) {
    uint64_t key=14695981039346656037ULL;
    auto add=[&](uint32_t value){for(int i=0;i<4;i++){key^=(value>>(8*i))&255;key*=1099511628211ULL;}};
    add(static_cast<uint32_t>(data.scene));
    for(unsigned char c:data.hash){key^=c;key*=1099511628211ULL;}
    for(const auto* points:{&data.spawns,&data.points}){
        add(static_cast<uint32_t>(points->size()));
        for(auto p:*points)for(float value:{p.x,p.y,p.z})add(value==0?0:std::bit_cast<uint32_t>(value));
    }
    return key?key:1;
}
struct SearchMapFilter {
    uint64_t catalog=0;int spawn=-1;
    bool HasRules() const {return spawn>=0;}
    void Clear(){spawn=-1;}
    bool Bind(uint64_t key) {if(key==catalog)return false;bool discarded=HasRules();Clear();catalog=key;return discarded;}
    void Encode(std::ostream& out,int level) const {
        std::string prefix="planner.filter"+std::to_string(level)+".";
        out<<prefix<<"low="<<uint32_t(catalog)<<'\n'<<prefix<<"high="<<uint32_t(catalog>>32)<<'\n'<<prefix<<"spawn="<<spawn<<'\n';
    }
    void Decode(const std::string& key,double value) {
        if(!std::isfinite(value)||std::floor(value)!=value)return;
        if(key=="low"&&value>=0&&value<=4294967295.)catalog=(catalog&0xffffffff00000000ULL)|static_cast<uint32_t>(value);
        else if(key=="high"&&value>=0&&value<=4294967295.)catalog=(catalog&0xffffffffULL)|(uint64_t(static_cast<uint32_t>(value))<<32);
        else if(key=="spawn"&&value>=-1&&value<=127)spawn=static_cast<int>(value);
    }
};
struct SearchCriteria {
    SearchObjective objective=SearchObjective::ShortestRoute;
    int spawn=-1;uint64_t catalog=0;
    bool Valid() const {return int(objective)>=0&&int(objective)<=1&&spawn>=-1&&spawn<128;}
    std::string CheckCatalog(const MapData& data,bool currentGeneration=false) const {
        if(spawn>=0&&catalog!=CatalogKey(data))return "The spawn filter belongs to a different catalog; select its spawn again.";
        if(spawn>=static_cast<int>(data.spawns.size()))return "The selected player spawn is not in this catalog.";
        if(currentGeneration&&(data.flags&16)&&data.spawns.size()>4&&spawn>=0&&spawn!=4)return "The first Timer start forces spawn #5; select Any or spawn #5.";
        return {};
    }
};
}
