#pragma once
#include "ReplayArchive.h"
#include "FullGameModel.h"

namespace suite_full
{
inline void ReplayGroups(std::shared_ptr<const practice::ReplayArchiveState> archive,
                         const practice::ReplayLibraryFilter &filter)
{
    struct Display{std::string id,label,search;};
    static std::shared_ptr<const practice::ReplayArchiveState> previous;
    static practice::ReplayLibraryFilter oldFilter;
    static std::vector<Display> labels;
    static std::vector<size_t> shown;
    static bool unfinished=false,oldUnfinished=false;
    static int page=0;
    auto lower=[](std::string value){for(auto &c:value)c=char(std::tolower(static_cast<unsigned char>(c)));return value;};
    suite_ui::Checkbox("Include unfinished full-game runs",&unfinished);
    if(previous!=archive || oldFilter!=filter || oldUnfinished!=unfinished)
    {
        if(previous!=archive)
        {
            labels.clear();labels.reserve(archive->fullGames.size());
            for(const auto &r:archive->fullGames)
            {
                auto title=std::string(Name(r.mode))+" / "+FormatRunTime(r.seconds)+" / "+practice::ReplayRecordedAt(r.started);
                auto search=title+" "+LongName(r.mode)+" "+r.key;
                for(int seed:r.seeds)search+=" "+std::to_string(seed);
                labels.push_back({std::to_string(r.id),std::move(title),lower(std::move(search))});
            }
        }
        previous=archive;oldFilter=filter;oldUnfinished=unfinished;shown.clear();page=0;
        auto query=lower(filter.query);
        for(size_t i=0;i<archive->fullGames.size();i++)
        {
            const auto &r=archive->fullGames[i];
            bool status=filter.status==0 || (filter.status==1 && r.state==3) || (filter.status==2 && r.Ranked()) ||
                (filter.status==3 && r.state==3 && !r.Ranked()) || (filter.status==4 && r.state!=3);
            if(r.stage>0 && (unfinished || r.state==3) && status && (filter.category<0 || filter.category==r.mode-1) &&
                labels[i].search.find(query)!=std::string::npos)shown.push_back(i);
        }
        std::sort(shown.begin(),shown.end(),[&](size_t a,size_t b) {
            const auto &x=archive->fullGames[a],&y=archive->fullGames[b];int compare=0;
            if(filter.sort==practice::ReplaySort::Time)compare=x.seconds<y.seconds?-1:x.seconds>y.seconds?1:0;
            else if(filter.sort==practice::ReplaySort::Category)compare=x.mode-y.mode;
            else if(filter.sort==practice::ReplaySort::Name)compare=labels[a].label.compare(labels[b].label);
            if(!compare)compare=x.id<y.id?-1:x.id>y.id?1:0;
            return filter.ascending?compare<0:compare>0;
        });
    }
    ImGui::TextDisabled("%zu full-game folders / completed level replays only",shown.size());
    if(!archive->error.empty())ImGui::TextWrapped("%s",archive->error.c_str());
    if(!archive->fullGameError.empty())ImGui::TextWrapped("%s",archive->fullGameError.c_str());
    if(shown.empty())
    {
        ImGui::TextWrapped("Complete an RSFG or SSFG run to see its eight level replays here. Failed attempts contribute to sector time and checks but are not added as replay entries.");
        return;
    }
    constexpr int perPage=25;int pages=int((shown.size()+perPage-1)/perPage);page=std::clamp(page,0,std::max(0,pages-1));
    if(focusReplayGroup)
        for(size_t i=0;i<shown.size();i++)if(archive->fullGames[shown[i]].id==focusReplayGroup){page=int(i/perPage);break;}
    if(pages>1)
    {
        ImGui::BeginDisabled(page==0);if(suite_ui::SmallButton("Previous folders"))page--;ImGui::EndDisabled();
        SameLineIfFits("Next folders");ImGui::BeginDisabled(page+1>=pages);if(suite_ui::SmallButton("Next folders"))page++;ImGui::EndDisabled();
        ImGui::SameLine();ImGui::TextDisabled("%d / %d",page+1,pages);
    }
    ImGui::BeginChild("Full-game replay folders",{0,0});
    for(size_t at=size_t(page)*perPage;at<std::min(shown.size(),size_t(page+1)*perPage);at++)
    {
        size_t index=shown[at];const auto &r=archive->fullGames[index];const auto &display=labels[index];
        ImGui::PushID(display.id.c_str());
        bool focus=focusReplayGroup==r.id;
        if(focus)ImGui::SetNextItemOpen(true,ImGuiCond_Always);
        bool expanded=ImGui::TreeNodeEx(display.label.c_str(),ImGuiTreeNodeFlags_SpanAvailWidth);
        if(focus){ImGui::SetScrollHereY(.1f);focusReplayGroup=0;}
        if(expanded)
        {
            ImGui::TextColored(r.Ranked()?ImVec4(.42f,.92f,.64f,1):g_accentCol,"%s / %d of 8 levels",r.state==3?(r.Ranked()?"VALID FINISH":"UNVALID FINISH"):"UNFINISHED",r.stage);
            ImGui::TextWrapped("Sector times include retries; each replay contains its successful attempt only.");
            ImGui::BeginDisabled(r.state!=3 || (g_pShared && RunInProgress(*g_pShared)));
            if(suite_ui::SmallButton("Export replay folder"))practice::g_replayArchive.ExportFullGame(r.id);
            ImGui::EndDisabled();
            if(r.mode==2)
            {
                SameLineIfFits("Copy seed plan to setup");ImGui::BeginDisabled(g_pShared && RunInProgress(*g_pShared));
                if(suite_ui::SmallButton("Copy seed plan to setup")){preferences.mode=2;preferences.seeds=r.seeds;preferences.raw=r.raw;}
                ImGui::EndDisabled();
            }
            if(archive->exportedFullGame==r.id)ImGui::TextWrapped("Exported: %s",archive->exportPath.c_str());
            if(ImGui::BeginTable("Completed level recordings",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp))
            {
                ImGui::TableSetupColumn("Level",ImGuiTableColumnFlags_WidthStretch,1.5f);
                ImGui::TableSetupColumn("Sector / retries");ImGui::TableSetupColumn("Replay time");ImGui::TableSetupColumn("Replay");ImGui::TableHeadersRow();
                for(int i=0;i<r.stage;i++)
                {
                    ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(kMapLevelNames[i]);
                    ImGui::TableNextColumn();ImGui::Text("%s / %d",FormatRunTime(r.sectors[i]).c_str(),r.retries[i]);
                    ImGui::TableNextColumn();ImGui::TextUnformatted(FormatRunTime(r.attempts[i]).c_str());
                    ImGui::TableNextColumn();auto found=archive->byId.find(r.replays[i]);
                    const practice::ReplayInfo *row=found==archive->byId.end()?nullptr:&archive->runs[found->second];
                    bool usable=row && row->outcome==1 && row->scene==kMapScenes[i] && row->runCategory==r.mode-1;
                    ImGui::BeginDisabled(!usable || HasPendingRequest() || (g_pShared && RunInProgress(*g_pShared)));
                    if(suite_ui::SmallButton(usable?"Open":"Unavailable") && usable){OpenSavedReplay(*row);ImGui::CloseCurrentPopup();}
                    ImGui::EndDisabled();ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}
}
