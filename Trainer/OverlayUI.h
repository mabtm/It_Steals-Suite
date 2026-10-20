#include "SuiteExperience.h"
#include "HudSurfaceBounds.h"
inline HudRegion g_validityBadgeBounds{};
#pragma once
#include "OverlayConfiguration.h"
#include "TelemetryPresentation.h"
#include "SuiteAppearance.h"
#include "HudTelemetry.h"
#include "HudEditorModel.h"
#include <cstdio>
#include <ctime>

inline std::string FormatRunTime(double seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 315360000) return "--";
    auto ms=static_cast<int64_t>(seconds*1000);
    char text[48]; std::snprintf(text,sizeof(text),"%02lld:%02lld.%03lld",
        static_cast<long long>(ms/60000),static_cast<long long>(ms/1000%60),static_cast<long long>(ms%1000));
    return text;
}
inline const char* SeedTypeLabel(const GameData& data) {
    if (!data.seedKnown || data.seedKind==0) return "SEED: UNKNOWN";
    return data.seedKind==2 ? "SET SEED" : "RANDOM SEED";
}
inline const char* OutcomeLabel(byte outcome) {
    const char* labels[]={"Running","Completed","Reset","Death / reset","Interrupted","Process stopped"};
    return outcome<6 ? labels[outcome] : "Unknown";
}
inline const char* RunStatusLabel(const GameData& data) {
    if(data.timerState==1) return "RUNNING";
    if(data.timerState==0) return "READY";
    switch(data.runOutcome) {
    case 1:return "COMPLETED";
    case 2:return "RESET";
    case 3:return "DEATH";
    case 4:case 5:return "ABORTED";
    default:return "STOPPED";
    }
}
#include "VerificationUI.h"
inline std::string OverlayValue(OverlayField field,const GameData& d,const std::string& mode,double age) {
    char value[64]{};
    auto rate=[&](int wins,int attempts){if(attempts>0) std::snprintf(value,sizeof(value),"%.1f%%",std::clamp(wins*100./attempts,0.,100.));else std::snprintf(value,sizeof(value),"--");};
    switch(field) {
    case OverlayField::Fps: if(d.gameFps>0)std::snprintf(value,sizeof(value),"%.0f",d.gameFps);else return "--"; break;
    case OverlayField::Timer: return d.speedrunModeActive ? FormatRunTime(DisplayRunTime(d,age)) : "--";
    case OverlayField::Seed: return d.seedKnown ? std::to_string(d.runSeed) : "UNKNOWN";
    case OverlayField::SeedType: return SeedTypeLabel(d);
    case OverlayField::Attempts: return std::to_string(d.totalRuns);
    case OverlayField::Resets: return std::to_string(d.totalResets);
    case OverlayField::Completed: return std::to_string(d.totalFinishedRuns);
    case OverlayField::CompletionRate: rate(d.totalFinishedRuns,d.totalRuns); break;
    case OverlayField::RunNumber: return d.currentRunNumber ? "#"+std::to_string(d.currentRunNumber) : "--";
    case OverlayField::SessionTime: return FormatRunTime(d.sessionTimeSeconds+std::clamp(age,0.,.15));
    case OverlayField::PersonalBest: return d.personalBestSeconds>0 ? FormatRunTime(d.personalBestSeconds) : "--";
    case OverlayField::AverageRun: return d.totalFinishedRuns>0 ? FormatRunTime(d.averagePace) : "--";
    case OverlayField::AverageReset: return d.averageResetSeconds>0 ? FormatRunTime(d.averageResetSeconds) : "--";
    case OverlayField::Progress:
        if(d.speedrunModeActive)std::snprintf(value,sizeof(value),"%.0f%%",d.runProgress*100);
        else if(d.totalObjectives>0)std::snprintf(value,sizeof(value),"%d / %d",d.totalObjectives-d.objectives,d.totalObjectives);
        else return "--";
        break;
    case OverlayField::Furthest: std::snprintf(value,sizeof(value),"%.0f%%",d.furthestProgress*100); break;
    case OverlayField::AttemptsToday: return std::to_string(d.attemptsToday);
    case OverlayField::CompletedToday: return std::to_string(d.completedToday);
    case OverlayField::ResetsToday: return std::to_string(d.resetsToday);
    case OverlayField::SessionAttempts: return std::to_string(d.sessionAttempts);
    case OverlayField::SessionCompleted: return std::to_string(d.sessionCompleted);
    case OverlayField::SessionResets: return std::to_string(d.sessionResets);
    case OverlayField::SessionRate: rate(d.sessionCompleted,d.sessionAttempts); break;
    case OverlayField::Deaths: return std::to_string(d.totalDeaths);
    case OverlayField::Speed: std::snprintf(value,sizeof(value),"%.1f u/s",d.smoothedSpeed); break;
    case OverlayField::Mode: return mode;
    case OverlayField::Coordinates:if(d.playerReady&&age<.5)std::snprintf(value,sizeof(value),"X %.2f  Y %.2f  Z %.2f",d.playerX,d.playerY,d.playerZ);else std::snprintf(value,sizeof(value),"X --  Y --  Z --");break;
    case OverlayField::Rotation:if(d.playerReady&&age<.5)std::snprintf(value,sizeof(value),"%.1f deg",std::fmod(d.playerRotY+360.f,360.f));else return "--";break;
    case OverlayField::PlayerState:return PlayerStateLabel(d,age);
    case OverlayField::MonsterState:return MonsterStateLabel(d,age);
    case OverlayField::MonsterSpeed:if(age>=.5||(!d.hideAndSeekMode&&!d.ghostMode))return "--";std::snprintf(value,sizeof(value),"%.1f u/s",d.hideAndSeekMode?d.hideSeek_chasingSpeed:d.ghostMode?d.ghostDefaultSpeed:0);break;
    case OverlayField::Noise:return age<.5?std::to_string(d.playerNoise):"--";
    case OverlayField::Health:return d.playerReady&&age<.5?std::to_string(d.health):"--";
    default: return "--";
    }
    return value;
}

inline void OverlayBadge(const char* text,bool warning,float fontScale) {
    ImVec4 color=warning ? ImVec4(1,.63f,.35f,1) : suite_ui::HudAccent();
    ImVec2 p=ImGui::GetCursorScreenPos();
    float height=ImGui::GetTextLineHeight()+8*g_dpiScale*fontScale;
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(p,ImVec2(p.x+ImGui::GetContentRegionAvail().x,p.y+height),
        ImGui::GetColorU32(suite_ui::Alpha(suite_ui::Blend(suite_ui::HudBackground(),color,.20f),.8f)),4*g_dpiScale*fontScale);
    ImGui::SetCursorScreenPos(ImVec2(p.x+6*g_dpiScale*fontScale,p.y+4*g_dpiScale*fontScale));
    ImGui::TextColored(color,"%s",text);
    ImGui::SetCursorScreenPos(ImVec2(p.x,p.y+height+ImGui::GetStyle().ItemSpacing.y));
}

inline bool HudFieldAllowed(OverlayField f,bool speedrunProfile,bool clean){
    if((speedrunProfile||clean)&&f>=OverlayField::PlayerState)return false;
    if(clean&&f==OverlayField::Speed)return false;
    return true;
}
inline void RenderOverlayPanel(const char* name,const GameData& d,const OverlayLayout& layout,
    bool speedrun,const std::string& mode,double age,bool compactConfig=false) {
    if(!layout.enabled&&!speedrun&&!suite_hud::canvas.active)return;
    const bool compact=compactConfig||!speedrun,clean=d.speedrunModeActive&&!d.practiceModeActive;
    const float preview=suite_hud::canvas.active?suite_hud::canvas.scale:1.f;
    const float scale=std::max(.85f,layout.scale),u=scale*g_dpiScale*preview,ratio=scale/suite_ui::loadedHudScale*preview;
    auto viewport=suite_hud::canvas.active?ImVec2(suite_hud::canvas.w,suite_hud::canvas.h):ImGui::GetIO().DisplaySize;float width=std::min(std::max(220.f,layout.width)*u,std::max(60.f,viewport.x-24*preview));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,suite_ui::HudBackground(layout.opacity));ImGui::PushStyleColor(ImGuiCol_Border,suite_ui::Color(AppearancePreferences::Outline,.65f));ImGui::PushStyleColor(ImGuiCol_Text,suite_ui::HudInk());
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{layout.padding*u,layout.padding*u});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{10*u,std::max(4.f,layout.gap)*u});ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,12*u);
    ImGui::SetNextWindowSize({width,0},ImGuiCond_Always);ImGui::SetNextWindowSizeConstraints({width,1},{width,std::max(40.f,viewport.y-24*preview)});
    ImVec2 pivot(0,0);if(layout.anchor==1||layout.anchor==3)pivot.x=1;if(layout.anchor>=2&&layout.anchor<=3)pivot.y=1;if(layout.anchor==4)pivot={layout.x,layout.y};
    ImGui::SetNextWindowPos({(suite_hud::canvas.active?suite_hud::canvas.x:0)+12*preview+(viewport.x-24*preview)*pivot.x,(suite_hud::canvas.active?suite_hud::canvas.y:0)+12*preview+(viewport.y-24*preview)*pivot.y},ImGuiCond_Always,pivot);
    if(suite_ui::hudFont)ImGui::PushFont(suite_ui::hudFont);
    ImGui::Begin(name,nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoFocusOnAppearing|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoNav);
    ImGui::SetWindowFontScale(ratio);auto start=ImGui::GetWindowPos();auto ink=suite_ui::HudInk(),muted=suite_ui::HudMuted(),accent=suite_ui::HudAccent();
    auto label=[&](const char* text,ImVec4 color){if(suite_ui::labelFont)ImGui::PushFont(suite_ui::labelFont);ImGui::TextColored(color,"%s",text);if(suite_ui::labelFont)ImGui::PopFont();};
    auto wanted=[&](OverlayField f){return layout.enabled&&layout.visible[int(f)]&&HudFieldAllowed(f,!compact,clean);};
    label(speedrun?(d.practiceModeActive?"PRACTICE":"SPEEDRUN"):"PRACTICE HUD",accent);
    if(speedrun){bool valid=d.runValidity==1&&age<.5&&d.seedSourcesVerified&&d.rngPatchComplete&&d.observedFromStart&&d.seedKind==1&&d.seedKnown&&!d.practiceModeActive&&!d.runAssisted&&!d.trainerFeatures&&!d.runUsedFeatures&&!(d.runWarnings&~64u);
        const char* text=valid?"VALID":"UNVALID";auto* font=suite_ui::labelFont?suite_ui::labelFont:ImGui::GetFont();float size=std::max(16*g_dpiScale*preview,font->FontSize*ratio);auto extent=font->CalcTextSizeA(size,FLT_MAX,0,text);auto at=ImGui::GetCursorScreenPos();at.x=start.x+width-layout.padding*u-extent.x;at.y=ImGui::GetItemRectMin().y;
        if(!ImGui::GetCurrentWindow()->Hidden){auto* fg=ImGui::GetForegroundDrawList();fg->AddRectFilled({at.x-4*u,at.y-3*u},{at.x+extent.x+4*u,at.y+extent.y+3*u},IM_COL32(14,20,26,250),4*u);fg->AddText(font,size,at,ImGui::ColorConvertFloat4ToU32(valid?ImVec4(.30f,.94f,.40f,1):ImVec4(1,.23f,.23f,1)),text);g_validityBadgeBounds={int(at.x-4*u),int(at.y-3*u),int(std::ceil(at.x+extent.x+4*u)),int(std::ceil(at.y+extent.y+3*u))};g_validityBadgeDrawn=true;}}
    if(wanted(OverlayField::Mode))ImGui::TextColored(ink,"%s",(mode+(speedrun?std::string(" / ")+RunStatusLabel(d):"")).c_str());
    else if(speedrun)label(RunStatusLabel(d),d.runOutcome==4||d.runOutcome==5?ImVec4(1,.69f,.30f,1):muted);
    if(wanted(OverlayField::Timer)){
        auto value=OverlayValue(OverlayField::Timer,d,mode,age);if(g_timerFont)ImGui::PushFont(g_timerFont);float available=ImGui::GetContentRegionAvail().x,text=ImGui::CalcTextSize(value.c_str()).x;
        if(text>available)ImGui::SetWindowFontScale(ratio*available/std::max(1.f,text));
        ImGui::TextColored(d.lastRunPb&&d.runOutcome==1?accent:ink,"%s",value.c_str());ImGui::SetWindowFontScale(ratio);if(g_timerFont)ImGui::PopFont();
        if(layout.progressBar&&d.speedrunModeActive){auto p=ImGui::GetCursorScreenPos();auto* draw=ImGui::GetWindowDrawList();float w=ImGui::GetContentRegionAvail().x;draw->AddRectFilled(p,{p.x+w,p.y+4*u},ImGui::GetColorU32(suite_ui::Alpha(accent,.15f)),2*u);draw->AddRectFilled(p,{p.x+w*std::clamp(d.runProgress,0.f,1.f),p.y+4*u},ImGui::GetColorU32(accent),2*u);ImGui::Dummy({w,5*u});}}
    const bool shortView=viewport.y<480*u;
    if(shortView&&(wanted(OverlayField::Seed)||wanted(OverlayField::SeedType))){std::string seedLine=std::string(SeedTypeLabel(d))+(wanted(OverlayField::Seed)?std::string(" / ")+OverlayValue(OverlayField::Seed,d,mode,age):"");label(seedLine.c_str(),d.seedKind==1?accent:ImVec4(1,.69f,.38f,1));}
    else{if(wanted(OverlayField::SeedType))label(SeedTypeLabel(d),d.seedKind==1?accent:ImVec4(1,.69f,.38f,1));if(wanted(OverlayField::Seed))ImGui::TextColored(ink,"Seed  %s",OverlayValue(OverlayField::Seed,d,mode,age).c_str());}
    if(speedrun&&(!d.seedKnown||d.seedKind==2)&&!wanted(OverlayField::SeedType))label(!d.seedKnown?"SEED UNKNOWN":"SET SEED / PRACTICE",ImVec4(1,.69f,.38f,1));
    bool coordinatesShown=false;
    if(wanted(OverlayField::Coordinates)&&viewport.y-24*preview-(ImGui::GetCursorScreenPos().y-start.y)-layout.padding*u>135*u){label("Position / X Y Z",muted);ImGui::TextWrapped("%s",OverlayValue(OverlayField::Coordinates,d,mode,age).c_str());coordinatesShown=true;}
    std::vector<OverlayField> fields;for(int f:layout.order){auto field=OverlayField(f);if(wanted(field)&&field!=OverlayField::Mode&&field!=OverlayField::Timer&&field!=OverlayField::Seed&&field!=OverlayField::SeedType&&!(field==OverlayField::Coordinates&&coordinatesShown))fields.push_back(field);}
    float footer=(30.f+(!fields.empty()?28.f:0.f))*u;bool warning=d.runWarnings||d.statsSaveStatus==2||d.reportSaveStatus==2;if(warning)footer+=28*u;
    float available=std::max(0.f,viewport.y-24*preview-(ImGui::GetCursorScreenPos().y-start.y)-layout.padding*u-footer);
    int columns=width>460*u?3:2,rows=int(available/(68*u+layout.gap*u)),capacity=std::max(0,rows)*columns;
    int shown=std::min(int(fields.size()),capacity);
    if(shown&&ImGui::BeginTable("Metrics",columns,ImGuiTableFlags_SizingStretchSame)){
        for(int i=0;i<shown;i++){auto field=fields[i];ImGui::TableNextColumn();label(kOverlayFieldNames[int(field)],muted);auto value=OverlayValue(field,d,mode,age);ImGui::TextUnformatted(value.c_str());}ImGui::EndTable();}
    if(shown<int(fields.size())){char message[80];std::snprintf(message,sizeof(message),"+%d fields / adjust HUD layout",int(fields.size())-shown);label(message,muted);}
    if(warning)label(d.statsSaveStatus==2?"STATS NOT SAVED":d.reportSaveStatus==2?"REPORT NOT SAVED":"OBSERVATION WARNINGS",ImVec4(1,.69f,.38f,1));
    label(suite_brand::Credit,muted);auto size=ImGui::GetWindowSize();suite_hud::bounds[compact?0:1]={start.x,start.y,size.x,size.y};suite_ui::DrawCardEdge(ImGui::GetWindowDrawList(),start,{start.x+size.x,start.y+size.y},u);
    ImGui::End();ImGui::PopStyleVar(3);ImGui::PopStyleColor(3);if(suite_ui::hudFont)ImGui::PopFont();
}

inline void RenderOverlaySettings(OverlayLayout& layout,bool speedrun,bool stacked=false) {
    ImGui::PushID(speedrun?"SpeedrunConfig":"HudConfig");
    suite_ui::Checkbox(stacked?"Show metrics":"Enable metrics in this overlay",&layout.enabled);
    auto slider=[&](const char* label,float* value,float lo,float hi,const char* format){if(stacked){ImGui::TextUnformatted(label);ImGui::SetNextItemWidth(-1);ImGui::SliderFloat((std::string("##")+label).c_str(),value,lo,hi,format);}else ImGui::SliderFloat(label,value,lo,hi,format);};
    if(speedrun){ImGui::TextDisabled("Validity badge: always visible");Help("VALID means the suite's local checks passed. Hidden or renamed memory tools cannot be ruled out. The badge remains visible even when metrics are disabled.");}
    slider("Scale",&layout.scale,.85f,1.75f,"%.2fx");
    slider("Panel opacity",&layout.opacity,.25f,1.f,"%.2f");
    suite_ui::Section("Layout details",0,[&](){
        slider("Panel width",&layout.width,220,560,"%.0f");
        slider("Padding",&layout.padding,6,24,"%.0f");
        slider("Spacing",&layout.gap,2,14,"%.0f");
    });
    suite_ui::Checkbox("Timer progress bar",&layout.progressBar);
    const char* positions[]={"Top left","Top right","Bottom left","Bottom right","Custom"};
    if(stacked){ImGui::TextUnformatted("Position");ImGui::SetNextItemWidth(-1);}ImGui::Combo(stacked?"##Position":"Position",&layout.anchor,positions,5);
    if(layout.anchor==4) {slider("Horizontal position",&layout.x,0,1,"%.2f");slider("Vertical position",&layout.y,0,1,"%.2f");}
    suite_ui::Section("Visible values / order",0,[&](){
    ImGui::TextDisabled("Metrics and order");Help("Choose values and use the arrows to change their order. Game FPS measures Unity updates. Seed and validity warnings remain visible.");
    if(ImGui::BeginTable("Elements",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Up",ImGuiTableColumnFlags_WidthFixed,32*g_dpiScale);
        ImGui::TableSetupColumn("Down",ImGuiTableColumnFlags_WidthFixed,40*g_dpiScale);
        for(int i=0;i<kOverlayFields;i++) {
            int field=layout.order[i];if(speedrun&&field>=int(OverlayField::PlayerState))continue;
            ImGui::PushID(i);ImGui::TableNextRow();ImGui::TableNextColumn();
            suite_ui::Checkbox(kOverlayFieldNames[field],&layout.visible[field]);
            if(field==static_cast<int>(OverlayField::PersonalBest)) Help("Local PBs require an observed random seed and no detected trainer aids. This is not leaderboard verification.");
            if(field==static_cast<int>(OverlayField::Seed)) Help("Random: first observed RNG seed. Set: the helper's applied practice base seed. The game reseeds repeatedly.");
            if(field==static_cast<int>(OverlayField::Resets)) Help("Manual restart of an active run and death reloads. Restarting after a completed run is not a reset.");
            if(field==static_cast<int>(OverlayField::MonsterSpeed)) Help("The game's configured chase speed (Hide & Seek) or default speed (Ghost), not a measured instantaneous velocity. Other modes show --.");
            ImGui::TableNextColumn(); ImGui::BeginDisabled(i==0);
            if(ImGui::ArrowButton("Up",ImGuiDir_Up)) std::swap(layout.order[i],layout.order[i-1]);
            ImGui::EndDisabled(); ImGui::TableNextColumn(); ImGui::BeginDisabled(i==kOverlayFields-1);
            if(ImGui::ArrowButton("Down",ImGuiDir_Down)) std::swap(layout.order[i],layout.order[i+1]);
            ImGui::EndDisabled(); ImGui::PopID();
        }
        ImGui::EndTable();
    }
    });
    ImGui::PopID();
}

inline void RenderRunHistory(const GameData& d) {
    if(!suite_ui::CollapsingHeader("Recent runs")) return;
    ImGui::TextDisabled("Latest 16 here; last 64 stored. Dates are UTC.");
    if(ImGui::BeginTable("RunHistory",12,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollX|ImGuiTableFlags_SizingFixedFit)) {
        for(const char* name:{"Run","Started (UTC)","Level","Time","Seed","Type","Result","Progress","PB","Local validity","Aids used","Warnings"}) ImGui::TableSetupColumn(name);
        ImGui::TableHeadersRow();
        for(int i=0;i<d.historyCount && i<16;i++) {
            auto r=ReadHistory(d,i); ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("#%lld",static_cast<long long>(r.id));
            ImGui::TableNextColumn();
            std::time_t stamp=static_cast<std::time_t>((r.startedUtc-621355968000000000LL)/10000000LL);
            char date[32]="--"; if(auto utc=std::gmtime(&stamp)) std::strftime(date,sizeof(date),"%Y-%m-%d %H:%M:%S",utc);
            ImGui::TextUnformatted(date); ImGui::TableNextColumn(); ImGui::Text("%d",r.scene);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(FormatRunTime(r.seconds).c_str()); ImGui::TableNextColumn();
            if(r.seedKnown) ImGui::Text("%d",r.seed); else ImGui::TextUnformatted("UNKNOWN");
            ImGui::TableNextColumn(); ImGui::TextColored(r.seedKind==2?g_accentCol:g_textCol,"%s",r.seedKind==2?"SET":r.seedKind==1?"RANDOM":"UNKNOWN");
            ImGui::TableNextColumn(); ImGui::TextUnformatted(OutcomeLabel(r.outcome));
            ImGui::TableNextColumn(); ImGui::Text("%.0f%%",r.progress*100); ImGui::TableNextColumn(); ImGui::TextUnformatted(r.pb?"Local PB":"--");
            ImGui::TableNextColumn();ImGui::TextColored(r.Valid()?ImVec4(.30f,.94f,.40f,1):ImVec4(1,.23f,.23f,1),"%s",r.Valid()?"VALID":"UNVALID");
            ImGui::TableNextColumn();ImGui::TextUnformatted(FeatureNames(r.aids).c_str());ImGui::TableNextColumn();ImGui::TextUnformatted(WarningNames(r.Warnings()).c_str());
        }
        ImGui::EndTable();
    }
}
