#include "FullGameModel.h"
#include "FullGamePreferences.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
using namespace suite_full;
static std::string Checked(std::string body)
{
    body=body.substr(0,body.rfind("checksum|"));
    std::ostringstream out;out<<body<<"checksum|"<<std::hex<<std::setfill('0')<<std::setw(16)<<Hash(body)<<'\n';return out.str();
}
static bool Rejected(const std::string &bytes){try{Decode(bytes);return false;}catch(const std::exception &){return true;}}
int main(int argc,char **argv)
{
    assert(argc==2);std::ifstream file(argv[1],std::ios::binary);
    std::string bytes{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    auto r=Decode(bytes);
    assert(r.Ranked() && r.mode==2 && r.stage==8 && r.seconds==36 && r.loads==16 && r.raw==128);
    assert(r.sectors[0]==1 && r.sectors[7]==8 && r.cumulative[7]==36 && r.replays[7]>r.replays[0]);
    assert(r.key==PlanKey(2,r.seeds,128));
    auto other=r;other.seeds[3]++;other.key=PlanKey(2,other.seeds,other.raw);other.seconds=1;
    std::vector<Record> history{other,r};
    assert(Best(history,2,r.seeds,r.raw)==&history[1]);
    assert(!Best(history,1,r.seeds,r.raw) && !Best(history,2,r.seeds,0));
    auto mismatch=bytes;auto at=mismatch.find("level|0|1|1|");assert(at!=std::string::npos);
    mismatch.replace(at,12,"level|0|9|1|");assert(Rejected(Checked(mismatch)));
    assert(Rejected(bytes.substr(0,bytes.size()-20)));
    assert(Rejected("ELITE_FULL_GAME|2\n"+bytes.substr(bytes.find('\n')+1)));
    GameData d{};d.fullGameMode=2;d.fullGameState=1;d.fullGameRunId=r.id;
    assert(RunInProgress(d));d.fullGameState=2;d.fullGameStage=2;d.fullGameSeconds=12;d.fullGameSectorSeconds=4;
    d.fullGameReferenceTotal=50;d.fullGameCumulative[0]=2;d.fullGameCumulative[1]=6;
    d.fullGameReferenceSectors[0]=3;d.fullGameReferenceSectors[1]=4;
    auto live=Live(d,.1);assert(std::abs(live.seconds-12.1)<1e-8 && live.Delta(0)==-1 && live.Delta(1)==-1 && !live.Delta(2));
    d.fullGameState=1;assert(Live(d,.1).seconds==12);d.fullGameState=3;assert(!RunInProgress(d));
    d.fullGameSeconds=std::numeric_limits<double>::quiet_NaN();d.fullGameStage=255;d.fullGameMode=255;d.fullGameRetries[0]=-1;
    SanitizeSnapshot(d);assert(d.fullGameSeconds==0 && d.fullGameStage==8 && d.fullGameMode==2 && d.fullGameRetries[0]==0);
    Preferences p;p.x=std::numeric_limits<float>::infinity();p.scale=999;p.Decode("seed7",-2147483648.);p.raw=0xffffffff;
    p.Validate();assert(p.x==0 && p.scale==1.8f && p.seeds[7]==std::numeric_limits<int>::min() && p.raw==255);
    std::cout<<"Native full-game metadata: managed golden fixture, seed/recipe/category PB isolation, split consistency, packed live snapshots, loading interpolation and sanitization: PASS\n";
}
