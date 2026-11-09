#pragma once
#include "MapLevels.h"
#include "SharedProtocol.h"
#include <array>
#include <charconv>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace suite_full
{
inline constexpr int LevelCount = 8;
inline int64_t focusReplayGroup=0;
inline constexpr int64_t MaxIdentity = 3155378975999999999ll;
inline const char *Name(int mode) { return mode == 2 ? "SSFG" : "RSFG"; }
inline const char *LongName(int mode) { return mode == 2 ? "Set Seed Full Game" : "Random Seed Full Game"; }
inline bool Active(const GameData &d) { return d.fullGameRunId > 0 && d.fullGameMode > 0 && (d.fullGameState == 1 || d.fullGameState == 2); }
inline uint64_t Hash(std::string_view bytes)
{
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char byte : bytes) { hash ^= byte; hash *= 1099511628211ull; }
    return hash;
}
inline std::string PlanKey(int mode, const std::array<int, 8> &seeds, uint32_t raw)
{
    uint64_t hash = 14695981039346656037ull;
    auto word = [&](uint32_t value) { for (int i = 0; i < 4; i++) { hash ^= uint8_t(value >> (8 * i)); hash *= 1099511628211ull; } };
    word(uint32_t(mode)); word(mode == 2 ? raw : 0);
    for (int seed : seeds) word(mode == 2 ? uint32_t(seed) : 0);
    std::ostringstream out; out << mode << '-' << std::hex << std::setfill('0') << std::setw(16) << hash;
    return out.str();
}
struct Record
{
    int64_t id = 0, started = 0, ended = 0;
    int mode = 0, state = 0, stage = 0;
    double seconds = 0, loads = 0;
    bool eligible = false, checked = false;
    uint32_t aids = 0, warnings = 0, raw = 0;
    std::array<int,8> seeds{}, retries{};
    std::array<int64_t,8> replays{};
    std::array<double,8> sectors{}, cumulative{}, attempts{};
    std::filesystem::path file;
    std::string key;
    bool Ranked() const { return state == 3 && stage == 8 && eligible && checked; }
};
inline std::vector<std::string_view> Fields(std::string_view row, char delimiter)
{
    std::vector<std::string_view> fields;
    size_t begin = 0;
    while (true)
    {
        auto end = row.find(delimiter,begin);
        fields.push_back(row.substr(begin,end == std::string_view::npos ? end : end-begin));
        if (end == std::string_view::npos) return fields;
        begin=end+1;
    }
}
template<class T> inline T Number(std::string_view text)
{
    T value{}; auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{} || end!=text.data()+text.size() || text.empty()) throw std::runtime_error("Invalid full-game number");
    return value;
}
inline bool Time(double n) { return std::isfinite(n) && n>=0 && n<=315360000; }
inline Record Decode(std::string_view bytes)
{
    if(bytes.size()>16384)throw std::runtime_error("Full-game metadata exceeds limit");
    if(!bytes.starts_with("ELITE_FULL_GAME|1\n"))
    {
        if(bytes.starts_with("ELITE_FULL_GAME|"))throw std::runtime_error("Unsupported full-game metadata version");
        throw std::runtime_error("Invalid full-game metadata header");
    }
    auto lines=Fields(bytes,'\n');
    if(lines.size()!=13 || !lines.back().empty()) throw std::runtime_error("Incomplete full-game metadata");
    auto checksum=Fields(lines[11],'|');
    if(checksum.size()!=2 || checksum[0]!="checksum" || checksum[1].size()!=16) throw std::runtime_error("Missing full-game checksum");
    uint64_t expected=0;auto [end,error]=std::from_chars(checksum[1].data(),checksum[1].data()+16,expected,16);
    if(error!=std::errc{} || end!=checksum[1].data()+16 || expected!=Hash(bytes.substr(0,bytes.rfind("checksum|"))))
        throw std::runtime_error("Full-game checksum mismatch");
    auto a=Fields(lines[1],'|'),p=Fields(lines[2],'|');
    if(a.size()!=13 || a[0]!="run" || p.size()!=10 || p[0]!="plan") throw std::runtime_error("Invalid full-game fields");
    Record r;
    r.id=Number<int64_t>(a[1]);r.mode=Number<int>(a[2]);r.state=Number<int>(a[3]);r.stage=Number<int>(a[4]);
    r.started=Number<int64_t>(a[5]);r.ended=Number<int64_t>(a[6]);r.seconds=Number<double>(a[7]);r.loads=Number<double>(a[8]);
    if((a[9]!="0" && a[9]!="1") || (a[10]!="0" && a[10]!="1")) throw std::runtime_error("Invalid full-game flags");
    r.eligible=a[9]=="1";r.checked=a[10]=="1";r.aids=Number<uint32_t>(a[11]);r.warnings=Number<uint32_t>(a[12]);r.raw=Number<uint32_t>(p[1]);
    for(int i=0;i<8;i++)r.seeds[i]=Number<int>(p[i+2]);
    if(r.id<=0 || r.id>MaxIdentity || r.started!=r.id || r.ended<0 || r.ended>MaxIdentity ||
        (r.ended && r.ended<r.started) || r.mode<1 || r.mode>2 || r.state<1 || r.state>5 || r.stage<0 || r.stage>8 ||
        (r.state==3)!=(r.stage==8) || (r.state==3 && !r.ended) || !Time(r.seconds) || !Time(r.loads) || r.raw>255 ||
        (r.aids&~1023u) || (r.warnings&~32767u) || (r.eligible && ((r.aids&~(r.mode==2?2u:0u)) || (r.warnings&~64u))))
        throw std::runtime_error("Invalid full-game result");
    if(r.mode==1 && (r.raw || std::any_of(r.seeds.begin(),r.seeds.end(),[](int seed){return seed!=0;})))
        throw std::runtime_error("Random full game contains a fixed seed plan");
    double previous=0;
    for(int i=0;i<8;i++)
    {
        auto row=Fields(lines[i+3],'|');
        if(row.size()!=7 || row[0]!="level" || Number<int>(row[1])!=i) throw std::runtime_error("Invalid full-game level order");
        r.sectors[i]=Number<double>(row[2]);r.cumulative[i]=Number<double>(row[3]);r.replays[i]=Number<int64_t>(row[4]);
        r.attempts[i]=Number<double>(row[5]);r.retries[i]=Number<int>(row[6]);
        if(r.retries[i]<0) throw std::runtime_error("Invalid full-game retries");
        if(i<r.stage)
        {
            if(!Time(r.sectors[i]) || !Time(r.cumulative[i]) || !Time(r.attempts[i]) || r.replays[i]<=0 || r.replays[i]>MaxIdentity ||
                std::find(r.replays.begin(),r.replays.begin()+i,r.replays[i])!=r.replays.begin()+i ||
                std::abs(previous+r.sectors[i]-r.cumulative[i])>.000001 || r.cumulative[i]>r.seconds+.000001 ||
                r.attempts[i]>r.sectors[i]+.000001) throw std::runtime_error("Inconsistent full-game level split");
            previous=r.cumulative[i];
        }
        else if(r.sectors[i]!=-1 || r.cumulative[i]!=-1 || r.attempts[i]!=-1 || r.replays[i])
            throw std::runtime_error("Uncompleted full-game level contains a replay");
    }
    if(r.stage==8 && std::abs(previous-r.seconds)>.000001) throw std::runtime_error("Inconsistent full-game finish");
    r.key=PlanKey(r.mode,r.seeds,r.raw);return r;
}
inline const Record *Best(const std::vector<Record> &runs,int mode,const std::array<int,8> &seeds,uint32_t raw)
{
    auto key=PlanKey(mode,seeds,raw);const Record *best=nullptr;
    for(const auto &r:runs)if(r.Ranked() && r.key==key && (!best || r.seconds<best->seconds || (r.seconds==best->seconds && r.id<best->id)))best=&r;
    return best;
}
// Copy numeric arrays out of the packed IPC into aligned storage before any
// standard-library algorithms or spans bind references to them.
struct Live
{
    int64_t id=0;
    int mode=0,state=0,stage=0;
    double seconds=0,sector=0,reference=0;
    bool eligible=false,checked=false,newPb=false;
    std::array<double,8> sectors{},cumulative{},referenceSectors{},attempts{};
    std::array<int,8> retries{};
    std::array<int64_t,8> replays{};
    explicit Live(const GameData &d,double age=0)
    {
        id=d.fullGameRunId;mode=d.fullGameMode;state=d.fullGameState;stage=std::clamp(int(d.fullGameStage),0,8);
        seconds=d.fullGameSeconds+(state==2?std::clamp(age,0.,.15):0.);
        sector=d.fullGameSectorSeconds+(state==2?std::clamp(age,0.,.15):0.);reference=d.fullGameReferenceTotal;
        eligible=d.fullGameEligible!=0;checked=d.fullGameChecked!=0;newPb=d.fullGameNewPb!=0;
        std::memcpy(sectors.data(),d.fullGameSectors,sizeof(sectors));std::memcpy(cumulative.data(),d.fullGameCumulative,sizeof(cumulative));
        std::memcpy(referenceSectors.data(),d.fullGameReferenceSectors,sizeof(referenceSectors));std::memcpy(attempts.data(),d.fullGameAttemptSeconds,sizeof(attempts));
        std::memcpy(retries.data(),d.fullGameRetries,sizeof(retries));std::memcpy(replays.data(),d.fullGameReplayIds,sizeof(replays));
    }
    std::optional<double> Delta(int level) const
    {
        if(level<0 || level>=stage || reference<=0)return {};
        double pb=0;for(int i=0;i<=level;i++){if(!Time(referenceSectors[i]))return {};pb+=referenceSectors[i];}
        return cumulative[level]-pb;
    }
};
}
