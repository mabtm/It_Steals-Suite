#pragma once
#include "FullGameModel.h"
#include "FullGamePreferences.h"
#include "SuiteExperience.h"

namespace suite_full
{
inline bool (*launch)(bool reset)=nullptr;
inline bool (*disable)()=nullptr;
inline void (*openReplays)()=nullptr;
inline bool resultOpen=false;
inline int64_t resultSeen=0;
inline std::optional<Live> result;
inline std::string resultStorageError;

inline ImVec4 DeltaColor(double value)
{
    return value<-.0005?ImVec4(.42f,.92f,.64f,1):value>.0005?ImVec4(1,.44f,.49f,1):ImVec4(.83f,.87f,.91f,1);
}
inline void DeltaLabel(std::optional<double> value)
{
    if(value)ImGui::TextColored(DeltaColor(*value),"%+.3f",std::abs(*value)<.0005?0:*value);
    else ImGui::TextDisabled("--");
}
inline void LevelTable(const Live &live,bool compact=false)
{
    if(ImGui::BeginTable("Full-game level splits",compact?3:5,ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Level",ImGuiTableColumnFlags_WidthStretch,1.65f);
        ImGui::TableSetupColumn("PB sector");ImGui::TableSetupColumn("This run");
        if(!compact){ImGui::TableSetupColumn("Total +/-");ImGui::TableSetupColumn("Retries",ImGuiTableColumnFlags_WidthStretch,.65f);}
        ImGui::TableHeadersRow();
        for(int i=0;i<8;i++)
        {
            ImGui::TableNextRow();
            bool current=i==live.stage && (live.state==1 || live.state==2);
            if(current)ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,ImGui::GetColorU32(ImVec4(.16f,.36f,.46f,.35f)));
            ImGui::TableNextColumn();ImGui::TextColored(current?g_headerCol:g_textCol,"%s%s",current?"> ":"",kMapLevelNames[i]);
            ImGui::TableNextColumn();
            if(live.reference>0 && live.referenceSectors[i]>=0)ImGui::TextUnformatted(FormatRunTime(live.referenceSectors[i]).c_str());
            else ImGui::TextDisabled("--");
            ImGui::TableNextColumn();
            if(i<live.stage)ImGui::TextUnformatted(FormatRunTime(live.sectors[i]).c_str());
            else if(current)ImGui::TextColored(g_headerCol,"%s",FormatRunTime(live.sector).c_str());
            else ImGui::TextDisabled("--");
            if(!compact)
            {
                ImGui::TableNextColumn();DeltaLabel(live.Delta(i));
                ImGui::TableNextColumn();ImGui::Text("%d",live.retries[i]);
            }
        }
        ImGui::EndTable();
    }
}
inline void OverlaySettings()
{
    auto &p=preferences;
    if(!suite_ui::CollapsingHeader("Full-game overlay"))return;
    suite_ui::Checkbox("Show level split table",&p.overlay);
    ImGui::SliderFloat("Horizontal",&p.x,0,1,"%.2f");ImGui::SliderFloat("Vertical",&p.y,0,1,"%.2f");
    ImGui::SliderFloat("Width",&p.width,260,600,"%.0f");ImGui::SliderFloat("Scale",&p.scale,.6f,1.8f,"%.2f");
    ImGui::SliderFloat("Opacity",&p.opacity,.1f,1,"%.2f");p.Validate();
}
inline bool TypeSelector()
{
    const char *types[]={"Individual level","RSFG / Random Seed Full Game","SSFG / Set Seed Full Game"};
    ImGui::BeginDisabled(g_pShared && Active(*g_pShared));
    ImGui::TextDisabled("Run type");ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##FullGameType",&preferences.mode,types,3);
    ImGui::EndDisabled();return preferences.mode!=0;
}
inline void Setup()
{
    auto &p=preferences;
    ImGui::BeginChild("Full-game setup scroll",{0,0});
    ImGui::TextColored(g_headerCol,"%s",LongName(p.mode));
    ImGui::TextWrapped("Eight levels, from Classic to Hide & Seek Outside. Level restarts keep the total time; Full reset starts again at Classic. Loading is excluded.");
    bool active=g_pShared && Active(*g_pShared);
    bool connected=g_pShared && !HasPendingRequest() && launch;
    ImGui::BeginDisabled(!connected);
    if(suite_ui::PrimaryButton(active?"Full reset / Classic":"Start full game"))launch(active);
    ImGui::EndDisabled();
    if(active)
    {
        SameLineIfFits("Restart level");ImGui::BeginDisabled(HasPendingRequest());
        if(suite_ui::Button("Restart level"))g_commands.speedrunLoadLevelCmd=1;
        ImGui::EndDisabled();
    }
    if(g_commands.fullGameMode)
    {
        SameLineIfFits("Stop full game");
        if(suite_ui::Button("Stop full game") && disable)disable();
    }
    if(p.mode==2)
    {
        ImGui::Separator();ImGui::TextDisabled("Seed plan / frozen when the full run starts");
        ImGui::BeginDisabled(active);
        if(ImGui::BeginTable("Full-game seed plan",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Level",ImGuiTableColumnFlags_WidthStretch,1.5f);ImGui::TableSetupColumn("Seed");
            ImGui::TableSetupColumn("Spawn recipe",ImGuiTableColumnFlags_WidthStretch,.8f);ImGui::TableHeadersRow();
            for(int i=0;i<8;i++)
            {
                ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(kMapLevelNames[i]);
                ImGui::TableNextColumn();ImGui::SetNextItemWidth(-1);ImGui::InputInt("##Seed",&p.seeds[i],0,0);
                ImGui::TableNextColumn();bool raw=(p.raw&(1u<<i))!=0;
                if(suite_ui::Checkbox("Raw spawn",&raw)){if(raw)p.raw|=1u<<i;else p.raw&=~(1u<<i);}
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndDisabled();
        ImGui::TextWrapped("Each level uses its own saved seed. Raw spawn selects the captured first-spawn seed recipe. PBs compare only the same complete seed plan and recipes.");
    }
    ImGui::Separator();
    auto archive=practice::g_replayArchive.Get();
    static std::shared_ptr<const practice::ReplayArchiveState> previous;
    static std::array<int,8> previousSeeds{};static int previousMode=-1;static uint32_t previousRaw=0;
    static const Record *pb=nullptr;
    if(previous!=archive || previousSeeds!=p.seeds || previousMode!=p.mode || previousRaw!=p.raw)
    {previous=archive;previousSeeds=p.seeds;previousMode=p.mode;previousRaw=p.raw;pb=Best(archive->fullGames,p.mode,p.seeds,p.raw);}
    ImGui::Text("Full-game PB  %s",pb?FormatRunTime(pb->seconds).c_str():"--:--.---");
    bool same=g_pShared && g_pShared->fullGameRunId>0 && g_pShared->fullGameMode==p.mode;
    if(same && p.mode==2)
    {
        same=uint32_t(g_pShared->fullGameRawMask)==p.raw;
        for(int i=0;i<8;i++)same=same && g_pShared->fullGameSeeds[i]==p.seeds[i];
    }
    if(same)LevelTable(Live(*g_pShared,SnapshotAge()),ImGui::GetContentRegionAvail().x<570*suite_ui::Unit());
    else if(pb)
    {
        Live preview(GameData{});preview.reference=pb->seconds;preview.referenceSectors=pb->sectors;
        LevelTable(preview,ImGui::GetContentRegionAvail().x<570*suite_ui::Unit());
    }
    else ImGui::TextDisabled("Finish all eight levels to establish a full-game PB.");
    if(g_pShared && g_pShared->fullGameStorageError[0])ImGui::TextWrapped("%s",g_pShared->fullGameStorageError);
    OverlaySettings();
    ImGui::EndChild();
}

inline void RenderOverlay(const GameData &data,double age)
{
    Live live(data,age);const auto &p=preferences;
    float unit=p.scale*g_dpiScale;
    auto screen=ImGui::GetIO().DisplaySize;
    float width=std::min(p.width*unit,std::max(120.f,screen.x-24));unit=width/p.width;
    float height=(p.overlay?316.f:112.f)*unit;
    if(height>screen.y-24){unit*=std::max(50.f,screen.y-24)/height;height=(p.overlay?316.f:112.f)*unit;width=p.width*unit;}
    ImVec2 position{12+std::max(0.f,screen.x-width-24)*p.x,12+std::max(0.f,screen.y-height-24)*p.y};
    ImGui::SetNextWindowPos(position,ImGuiCond_Always);ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(p.opacity);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::Begin("Full-game timing",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoBringToFrontOnFocus);
    auto *draw=ImGui::GetWindowDrawList();auto *font=suite_ui::hudFont?suite_ui::hudFont:ImGui::GetFont();
    auto text=[&](float x,float y,float size,ImVec4 color,const char *value) {
        draw->AddText(font,size*unit,{position.x+x*unit,position.y+y*unit},ImGui::GetColorU32(color),value);
    };
    auto right=[&](float x,float y,float size,ImVec4 color,const char *value) {
        float extent=font->CalcTextSizeA(size*unit,FLT_MAX,0,value).x/unit;text(x-extent,y,size,color,value);
    };
    ImVec4 muted(.61f,.68f,.74f,1),ink(.92f,.95f,.97f,1),accent(.48f,.83f,1,1);
    text(13,10,14,accent,live.mode==2?"SSFG / FULL GAME":"RSFG / FULL GAME");
    char time[48];auto formatted=FormatRunTime(live.seconds);text(12,31,32,ink,formatted.c_str());
    bool valid=live.eligible && (live.state==3?live.checked:data.runValidity==1 || live.state==1);
    const char *status=!live.eligible?"UNVALID":live.state==3?(live.checked?"VALID":"CHECKING"):(valid?"VALID":"CHECKING");
    ImVec4 statusColor=!live.eligible?ImVec4(1,.44f,.49f,1):valid?ImVec4(.42f,.92f,.64f,1):muted;
    text(14,76,12,statusColor,status);
    right(p.width-13,76,12,muted,live.state==1?"LOADING / CLOCK HELD":live.state==3?"FINISHED":"LOADLESS TIME");
    g_validityBadgeDrawn=true;
    g_validityBadgeBounds={int(position.x+10*unit),int(position.y+72*unit),int(100*unit),int(20*unit)};
    if(p.overlay)
    {
        text(14,103,11,muted,"LEVEL");right(p.width-151,103,11,muted,"PB");right(p.width-76,103,11,muted,"SECTOR");right(p.width-13,103,11,muted,"TOTAL +/-");
        for(int i=0;i<8;i++)
        {
            float y=125+i*22.f;bool current=i==live.stage && (live.state==1 || live.state==2);
            float highlight=suite_ui::Animate(ImGui::GetID(kMapLevelNames[i]),current?1.f:0.f,true,.12f);
            if(highlight>.01f)
            {
                draw->AddRectFilled({position.x+5*unit,position.y+(y-2)*unit},{position.x+width-5*unit,position.y+(y+19)*unit},IM_COL32(63,134,169,int(62*highlight)),2*unit);
                draw->AddRectFilled({position.x+5*unit,position.y+(y-2)*unit},{position.x+7*unit,position.y+(y+19)*unit},ImGui::GetColorU32(accent),unit);
            }
            float nameSize=13*unit,nameWidth=font->CalcTextSizeA(nameSize,FLT_MAX,0,kMapLevelNames[i]).x;
            float room=std::max(30.f,p.width-245)*unit;nameSize*=std::min(1.f,room/std::max(1.f,nameWidth));
            draw->AddText(font,nameSize,{position.x+14*unit,position.y+y*unit},ImGui::GetColorU32(current?accent:ink),kMapLevelNames[i]);
            auto actual=i<live.stage?live.sectors[i]:current?live.sector:-1;
            auto value=actual>=0?FormatRunTime(actual):std::string("--");right(p.width-76,y,13,current?accent:ink,value.c_str());
            auto pbSector=live.reference>0 && live.referenceSectors[i]>=0?FormatRunTime(live.referenceSectors[i]):std::string("--");
            right(p.width-151,y,12,muted,pbSector.c_str());
            auto delta=live.Delta(i);
            if(delta){std::snprintf(time,sizeof(time),"%+.3f",std::abs(*delta)<.0005?0:*delta);right(p.width-13,y,13,DeltaColor(*delta),time);}
            else right(p.width-13,y,13,muted,"--");
        }
        auto pb=live.reference>0?FormatRunTime(live.reference):std::string("--:--.---");
        text(14,301,11,muted,"PB");right(p.width-13,301,11,muted,pb.c_str());
    }
    ImGui::End();ImGui::PopStyleVar();
}
inline void ObserveResult(const GameData *data)
{
    if(data && result && data->fullGameRunId==result->id && resultStorageError!=data->fullGameStorageError)
        resultStorageError=data->fullGameStorageError;
    if(!data || data->fullGameState!=3 || data->fullGameRunId<=0 || data->fullGameRunId==resultSeen)return;
    resultSeen=data->fullGameRunId;result=Live(*data);resultOpen=true;resultStorageError=data->fullGameStorageError;
    practice::g_replayArchive.Refresh();
}
inline void RenderResult()
{
    if(!resultOpen || !result)return;
    auto screen=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({screen.x*.5f,screen.y*.5f},ImGuiCond_Always,{.5f,.5f});
    ImGui::SetNextWindowSize({std::min(760.f*g_dpiScale,screen.x-24),std::min(650.f*g_dpiScale,screen.y-24)},ImGuiCond_Always);
    if(ImGui::Begin("Full-game result",&resultOpen,ImGuiWindowFlags_NoCollapse))
    {
        float available=ImGui::GetContentRegionAvail().x,used=0;int rows=1;
        for(const char *label:{"Full reset / Classic","Full-game replays","Close"})
        {
            float width=ImGui::CalcTextSize(label).x+2*ImGui::GetStyle().FramePadding.x;
            if(used && used+ImGui::GetStyle().ItemSpacing.x+width>available){rows++;used=width;}
            else used+=(used?ImGui::GetStyle().ItemSpacing.x:0)+width;
        }
        ImGui::BeginChild("Full-game result body",{0,-rows*ImGui::GetFrameHeightWithSpacing()-10});
        ImGui::TextColored(g_headerCol,"%s / ALL EIGHT LEVELS",Name(result->mode));
        ImGui::TextColored(result->eligible && result->checked?ImVec4(.42f,.92f,.64f,1):ImVec4(1,.44f,.49f,1),"%s",result->eligible && result->checked?"VALID":"UNVALID");
        ImGui::Text("Final time  %s%s",FormatRunTime(result->seconds).c_str(),result->newPb?" / NEW PB":"");
        if(result->reference>0)ImGui::Text("Previous PB  %s",FormatRunTime(result->reference).c_str());
        if(!resultStorageError.empty())ImGui::TextWrapped("Storage: %s",resultStorageError.c_str());
        LevelTable(*result,ImGui::GetContentRegionAvail().x<550*g_dpiScale);
        ImGui::TextWrapped("Level sectors include retries. Each saved level replay shows only its completed attempt; failed attempts still contribute to the full-game clock and integrity checks.");
        ImGui::EndChild();
        if(suite_ui::Button("Full reset / Classic") && launch && launch(true))resultOpen=false;
        SameLineIfFits("Full-game replays");if(suite_ui::Button("Full-game replays") && openReplays){resultOpen=false;focusReplayGroup=result->id;openReplays();}
        SameLineIfFits("Close");if(suite_ui::Button("Close"))resultOpen=false;
    }
    ImGui::End();
}
}
