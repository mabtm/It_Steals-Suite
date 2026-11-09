#pragma once
#include "FullGameUI.h"

namespace suite_full
{
inline void ArchivePage(int page)
{
    static bool exact=true;
    static int sort=0;
    static bool ascending=false;
    static double nextRefresh=0;double now=ImGui::GetTime();
    if(now>=nextRefresh || now<nextRefresh-5){nextRefresh=now+5;practice::g_replayArchive.Refresh();}
    auto archive=practice::g_replayArchive.Get();
    const auto &p=preferences;
    ImGui::TextColored(g_headerCol,"%s / %s",Name(p.mode),page==1?"PERSONAL BESTS":page==2?"TOP 5":page==3?"HISTORY":"STATISTICS");
    if(p.mode==2)suite_ui::Checkbox("Current seed plan only",&exact);
    if(suite_ui::SmallButton("Refresh full-game archive"))practice::g_replayArchive.Refresh();
    const char *sorts[]={"Recorded","Total time","Retries"};SameLineIfFits("Recorded");
    ImGui::SetNextItemWidth(150*suite_ui::Unit());ImGui::Combo("##FullArchiveSort",&sort,sorts,3);
    SameLineIfFits("Ascending");suite_ui::Checkbox("Ascending",&ascending);
    static std::shared_ptr<const practice::ReplayArchiveState> previous;
    static int previousMode=-1,previousPage=-1,previousSort=-1;
    static bool previousExact=false,previousAscending=false;
    static std::array<int,8> seeds{};static uint32_t raw=0;
    static std::vector<size_t> rows,all;
    static double total=0,loads=0,best=0;
    static int completed=0,valid=0,resets=0,interrupted=0;
    static int64_t retries=0;
    if(previous!=archive || previousMode!=p.mode || previousPage!=page || previousSort!=sort || previousExact!=exact ||
        previousAscending!=ascending || seeds!=p.seeds || raw!=p.raw)
    {
        previous=archive;previousMode=p.mode;previousPage=page;previousSort=sort;previousExact=exact;
        previousAscending=ascending;seeds=p.seeds;raw=p.raw;
        rows.clear();all.clear();total=loads=best=0;completed=valid=resets=interrupted=0;retries=0;
        auto key=PlanKey(p.mode,p.seeds,p.raw);
        std::map<std::string,size_t> bests;
        for(size_t i=0;i<archive->fullGames.size();i++)
        {
            const auto &r=archive->fullGames[i];
            if(r.mode!=p.mode || (p.mode==2 && exact && r.key!=key) || (r.seconds<=0 && r.stage==0))continue;
            all.push_back(i);total+=r.seconds;loads+=r.loads;
            completed+=r.state==3;valid+=r.Ranked();resets+=r.state==4;interrupted+=r.state==5 || (r.state<=2 && (!g_pShared || g_pShared->fullGameRunId!=r.id));
            for(int n:r.retries)retries+=n;
            if(r.Ranked())
            {
                if(best<=0 || r.seconds<best)best=r.seconds;
                auto old=bests.find(r.key);
                if(old==bests.end() || r.seconds<archive->fullGames[old->second].seconds)bests[r.key]=i;
            }
        }
        if(page==1)for(const auto &[key,index]:bests){(void)key;rows.push_back(index);}
        else for(auto i:all)if(page!=2 || archive->fullGames[i].Ranked())rows.push_back(i);
        auto retryCount=[](const auto &r){int64_t count=0;for(int n:r.retries)count+=n;return count;};
        std::sort(rows.begin(),rows.end(),[&](size_t a,size_t b){
            const auto &x=archive->fullGames[a],&y=archive->fullGames[b];int compare=0;
            if(page==2 || sort==1)compare=x.seconds<y.seconds?-1:x.seconds>y.seconds?1:0;
            else if(sort==2){auto cx=retryCount(x),cy=retryCount(y);compare=cx<cy?-1:cx>cy?1:0;}
            if(!compare)compare=x.id<y.id?-1:x.id>y.id?1:0;
            return page==2?compare<0:ascending?compare<0:compare>0;
        });
        if(page==2 && rows.size()>5)rows.resize(5);
    }
    if(page==4)
    {
        ImGui::Separator();
        ImGui::Text("Attempts: %zu",all.size());ImGui::Text("Completed: %d / valid finishes: %d",completed,valid);
        ImGui::Text("Full resets: %d / interrupted: %d",resets,interrupted);ImGui::Text("Level retries: %lld",static_cast<long long>(retries));
        ImGui::Text("Gameplay time: %s",FormatRunTime(total).c_str());ImGui::Text("Excluded loading: %s",FormatRunTime(loads).c_str());
        ImGui::Text("Best completed time: %s",best>0?FormatRunTime(best).c_str():"--:--.---");
        ImGui::TextWrapped("Full-game records are independent of individual-level statistics. Retries count failed level attempts, including deaths; their time remains in the current sector.");
        return;
    }
    if(rows.empty()){ImGui::TextWrapped("No full-game records match this view. Complete a run or change the seed-plan filter.");return;}
    if(ImGui::BeginTable("Full-game records",5,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingStretchProp,{0,0}))
    {
        ImGui::TableSetupColumn("Recorded",ImGuiTableColumnFlags_WidthStretch,1.5f);ImGui::TableSetupColumn("Total time");
        ImGui::TableSetupColumn("Levels");ImGui::TableSetupColumn("Result");ImGui::TableSetupColumn("Replays");ImGui::TableHeadersRow();
        ImGuiListClipper clip;clip.Begin(int(rows.size()));
        while(clip.Step())for(int at=clip.DisplayStart;at<clip.DisplayEnd;at++)
        {
            const auto &r=archive->fullGames[rows[size_t(at)]];ImGui::PushID(at);ImGui::TableNextRow();ImGui::TableNextColumn();
            ImGui::TextUnformatted(practice::ReplayRecordedAt(r.started).c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(FormatRunTime(r.seconds).c_str());
            ImGui::TableNextColumn();ImGui::Text("%d / 8",r.stage);ImGui::TableNextColumn();
            ImGui::TextColored(r.Ranked()?ImVec4(.42f,.92f,.64f,1):g_textCol,"%s",r.state==3?(r.Ranked()?"VALID":"UNVALID"):r.state==4?"Full reset":"Interrupted");
            ImGui::TableNextColumn();ImGui::BeginDisabled(!openReplays || r.stage==0 || (g_pShared && RunInProgress(*g_pShared)));
            if(suite_ui::SmallButton("Open folder") && openReplays){focusReplayGroup=r.id;openReplays();}ImGui::EndDisabled();ImGui::PopID();
        }
        ImGui::EndTable();
    }
}
}
