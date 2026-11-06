#pragma once
#include "SplitDisplay.h"
#include "ReplayPreferences.h"
#include "HudEditorModel.h"
#include "SuiteExperience.h"

namespace suite_splits
{
inline void ResetPresentation(ReplayPreferences &p)
{
    ReplayPreferences defaults;
    p.liveSplits = defaults.liveSplits; p.splitPlacement = defaults.splitPlacement;
    p.splitRows = defaults.splitRows; p.splitX = defaults.splitX; p.splitY = defaults.splitY;
    p.splitScale = defaults.splitScale; p.splitWidth = defaults.splitWidth; p.splitOpacity = defaults.splitOpacity;
    p.splitPace = defaults.splitPace; p.splitTheory = defaults.splitTheory;
    p.splitStyle = defaults.splitStyle; p.splitMetric = defaults.splitMetric; p.splitAnimation = defaults.splitAnimation;
    p.splitTextSize = defaults.splitTextSize; p.splitDuration = defaults.splitDuration;
    p.splitBackgroundOpacity = defaults.splitBackgroundOpacity;
    p.splitAhead = defaults.splitAhead; p.splitBehind = defaults.splitBehind; p.splitEqual = defaults.splitEqual;
}
inline void Settings(ReplayPreferences &p)
{
    suite_ui::Checkbox("Live Set-Seed splits", &p.liveSplits);
    const char *places[] = {"Inside timer HUD", "Independent panel", "Both"};
    ImGui::TextDisabled("Display");
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##SplitDisplay", &p.splitPlacement, places, 3);
    const char *styles[] = {"Delta text / transparent", "Compact panel"};
    ImGui::TextDisabled("Style"); ImGui::SetNextItemWidth(-1); ImGui::Combo("##SplitStyle", &p.splitStyle, styles, 2);
    if (p.splitStyle == 0)
    {
        const char *metrics[] = {"Crystal delta vs reference", "Sector delta vs reference", "Sector delta vs best"};
        ImGui::TextDisabled("Compare"); ImGui::SetNextItemWidth(-1); ImGui::Combo("##SplitMetric", &p.splitMetric, metrics, 3);
        ImGui::TextDisabled("Latest crystal only / no background");
    }
    suite_ui::Checkbox("Projected finish / pace", &p.splitPace);
    suite_ui::Checkbox("Theoretical best", &p.splitTheory);
    ImGui::TextWrapped("Sector +/- compares the last sector with the matching PB route. Total +/- compares arrival at the same crystal. Gold marks a new local sector best.");
    if (suite_ui::CollapsingHeader("Pace and sector information"))
    {
        ImGui::TextWrapped("Pace is a forecast: reference finish plus your observed split delta. It rises if a sector takes longer than expected. Different pickup order suspends the forecast until a compatible route is used.");
        ImGui::TextWrapped("Theoretical best adds the best observed sectors on this exact seed, recipe, spawn and route, including the finish sector. Clean reset and death attempts also contribute, independently of replay retention. Live improvements are provisional until final run checks pass. Older unrecorded resets cannot be reconstructed.");
    }
    if (p.splitStyle == 0)
    {
        ImGui::SliderFloat("Text size", &p.splitTextSize, 24, 96, "%.0f");
        const char *animations[] = {"None", "Fade", "Soft pop", "Slide"};
        ImGui::SetNextItemWidth(-1); ImGui::Combo("##SplitAnimation", &p.splitAnimation, animations, 4);
        bool keep = p.splitDuration == 0;
        if (suite_ui::Checkbox("Keep until next crystal", &keep)) p.splitDuration = keep ? 0 : 3;
        if (!keep) ImGui::SliderFloat("Show seconds", &p.splitDuration, .5f, 10, "%.1f");
        if (suite_ui::CollapsingHeader("Delta colors"))
        {
            ImGui::ColorEdit4("Ahead", p.splitAhead.data(), ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Behind", p.splitBehind.data(), ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Equal", p.splitEqual.data(), ImGuiColorEditFlags_NoInputs);
        }
    }
    else ImGui::SliderFloat("Panel background", &p.splitBackgroundOpacity, 0, 1, "%.2f");
    if (p.splitPlacement != 0)
    {
        if (suite_ui::Button("Bottom center")) { p.splitX = .5f; p.splitY = 1; }
        if (suite_ui::CollapsingHeader("Position / size / opacity"))
        {
            ImGui::SliderFloat("Horizontal", &p.splitX, 0, 1, "%.2f");
            ImGui::SliderFloat("Vertical", &p.splitY, 0, 1, "%.2f");
            ImGui::SliderFloat("Scale", &p.splitScale, .6f, 1.8f, "%.2f");
            ImGui::SliderFloat("Width", &p.splitWidth, 320, 720, "%.0f");
            ImGui::SliderFloat("Opacity", &p.splitOpacity, .1f, 1, "%.2f");
        }
    }
    p.Validate();
}
inline suite_hud::Rect PanelBounds(const ReplayPreferences &p)
{
    auto viewport = suite_hud::canvas.active ? ImVec2(suite_hud::canvas.w, suite_hud::canvas.h) : ImGui::GetIO().DisplaySize;
    float preview = suite_hud::canvas.active ? suite_hud::canvas.scale : 1;
    float margin = 12 * preview;
    float nominal = (p.splitStyle == 0 ? p.splitTextSize * 1.35f : 80.f) + (p.splitPace || p.splitTheory ? 52.f : 10.f);
    float width = std::min(p.splitWidth * p.splitScale * g_dpiScale * preview, std::max(24.f, viewport.x - 2 * margin));
    float height = width / 480 * nominal;
    if (height > viewport.y - 2 * margin)
    { width *= std::max(24.f, viewport.y - 2 * margin) / height; height = std::max(24.f, viewport.y - 2 * margin); }
    return {(suite_hud::canvas.active ? suite_hud::canvas.x : 0) + margin + std::max(0.f, viewport.x - width - 2 * margin) * p.splitX,
            (suite_hud::canvas.active ? suite_hud::canvas.y : 0) + margin + std::max(0.f, viewport.y - height - 2 * margin) * p.splitY,
            width, height};
}
inline void DrawPanel(ImDrawList *draw, ImFont *font, suite_hud::Rect box, const GameData &data,
                      const ReplayPreferences &p, bool motion, bool demo = false)
{
    const auto &rows = Live(data);
    float u = box.w / 480;
    auto color = [&](int r, int g, int b, float opacity = 1.f) { return IM_COL32(r,g,b,int(255 * std::clamp(opacity * p.splitOpacity,0.f,1.f))); };
    auto text = [&](float x, float y, float size, ImU32 ink, const std::string &value) {
        draw->AddText(font,size*u,{box.x+x*u,box.y+y*u},ink,value.c_str());
    };
    auto delta = [&](double value, bool known) {
        char buffer[40];
        if (!known || !std::isfinite(value)) return std::string("--");
        if (std::abs(value) < .0005) value = 0;
        std::snprintf(buffer,sizeof(buffer),"%+.3f",value); return std::string(buffer);
    };
    auto sign = [&](double value,bool known,float alpha) {
        return !known || std::abs(value)<.0005 ? color(192,207,219,alpha) :
            value<0 ? color(107,235,162,alpha) : color(255,112,124,alpha);
    };
    auto right = [&](float edge,float y,float size,ImU32 ink,const std::string &value) {
        float extent=font->CalcTextSizeA(size*u,FLT_MAX,0,value.c_str()).x/u;
        text(edge-extent,y,size,ink,value);
    };
    draw->PushClipRect({box.x,box.y},{box.x+box.w,box.y+box.h},true);
    draw->AddRectFilled({box.x,box.y},{box.x+box.w,box.y+box.h},color(12,20,28,p.splitBackgroundOpacity),6*u);
    draw->AddLine({box.x+12*u,box.y+u},{box.x+box.w-12*u,box.y+u},color(111,209,255,.65f),u);
    text(13,10,14,color(131,211,249),demo ? "SET-SEED SPLITS / PREVIEW" : "SET-SEED SPLITS");
    text(13,32,11,color(150,170,187),"CRYSTAL");
    right(314,32,11,color(150,170,187),"SECTOR +/-"); right(466,32,11,color(150,170,187),"TOTAL +/-");
    size_t end=rows.size(),begin=end ? end-1 : 0;
    for (size_t i=begin;i<end;i++)
    {
        const auto &row=rows[i];
        float t=motion ? float(std::clamp((data.currentRunTimeSeconds-row.at)/.22,0.,1.)) : 1;
        float ease=1-(1-t)*(1-t)*(1-t),y=52+float(i-begin)*26+(1-ease)*5;
        if (row.gold)
        {
            float x=box.x+7*u,at=box.y+(y+7)*u;
            draw->AddQuadFilled({x,at-3*u},{x+3*u,at},{x,at+3*u},{x-3*u,at},color(255,205,106,ease));
        }
        text(17,y,15,color(225,235,244,ease),"Crystal "+std::to_string(i+1));
        right(314,y,16,sign(row.sectorDelta,row.sectorComparable,ease),delta(row.sectorDelta,row.sectorComparable));
        right(466,y,16,sign(row.delta,row.comparable,ease),delta(row.delta,row.comparable));
    }
    if (rows.empty()) text(17,56,14,color(164,184,199),data.splitReferenceState==1 ? "Finding matching route..." : "Ready for the first crystal");
    if (p.splitPace || p.splitTheory)
    {
        float y=80;
        draw->AddLine({box.x+13*u,box.y+(y-3)*u},{box.x+box.w-13*u,box.y+(y-3)*u},color(100,131,153,.25f),u);
        auto pace=Pace(data,rows,data.currentRunTimeSeconds),theory=Theoretical(data,rows);
        if (p.splitPace)
        {
            text(13,y+3,11,color(150,170,187),Completed(data) ? "FINISH" : data.splitReferenceValid ? "PB PACE / ESTIMATE" : "REPLAY PACE / ESTIMATE");
            text(13,y+19,23,color(226,238,247),pace ? FormatRunTime(*pace) : "--:--.---");
        }
        if (p.splitTheory)
        {
            float x=p.splitPace ? 250.f : 13.f;
            bool provisional=theory && data.timerState==1 && (!data.splitHistoryReady || data.splitTheoreticalBest<0 || *theory<data.splitTheoreticalBest-.0005);
            text(x,y+3,11,color(150,170,187),provisional ? "THEORETICAL BEST / LIVE" : "THEORETICAL BEST");
            text(x,y+19,23,color(255,211,131),theory ? FormatRunTime(*theory) : "--:--.---");
        }
    }
    draw->PopClipRect();
}
inline std::optional<double> Delta(const practice::PickupSplit &row, const ReplayPreferences &p)
{
    if (p.splitMetric == 1) return row.sectorComparable ? std::optional<double>(row.sectorDelta) : std::nullopt;
    if (p.splitMetric == 2) return row.bestSegment >= 0 ? std::optional<double>(row.segment-row.bestSegment) : std::nullopt;
    return row.comparable ? std::optional<double>(row.delta) : std::nullopt;
}
// No solid primitives in the text style: only glyph alpha reaches the game.
inline void DrawDeltaText(ImDrawList *draw, ImFont *font, suite_hud::Rect box, const GameData &data,
                          const ReplayPreferences &p, bool motion, double displayAge)
{
    const auto &rows=Live(data);
    if (rows.empty()) return;
    auto value=Delta(rows.back(),p);
    if (!value || !std::isfinite(*value)) return;
    double number=std::abs(*value)<.0005 ? 0 : *value;
    char delta[48]; std::snprintf(delta,sizeof(delta),"%+.3f",number);
    float enter=motion && p.splitAnimation ? float(std::clamp(displayAge/.2,0.,1.)) : 1;
    float ease=1-(1-enter)*(1-enter)*(1-enter),alpha=ease;
    if (p.splitDuration>0)
    {
        if (displayAge>=p.splitDuration) return;
        float remaining=float(p.splitDuration-displayAge);
        alpha*=motion ? std::clamp(remaining/std::min(.3f,p.splitDuration),0.f,1.f) : 1;
    }
    float u=box.w/480, size=p.splitTextSize*u;
    if (motion && p.splitAnimation==2) size*=1+.12f*(1-ease);
    const auto &tint=number<0 ? p.splitAhead : number>0 ? p.splitBehind : p.splitEqual;
    auto color=ImGui::ColorConvertFloat4ToU32({tint[0],tint[1],tint[2],tint[3]*p.splitOpacity*alpha});
    auto extent=font->CalcTextSizeA(size,FLT_MAX,0,delta);
    float mainHeight=p.splitTextSize*1.35f*u;
    ImVec2 at{box.x+(box.w-extent.x)*.5f,box.y+(mainHeight-extent.y)*.5f};
    if (motion && p.splitAnimation==3) at.y+=10*u*(1-ease);
    draw->PushClipRect({box.x,box.y},{box.x+box.w,box.y+box.h},true);
    draw->AddText(font,size,at,color,delta);
    float x=box.x+12*u,y=box.y+mainHeight+8*u;
    auto summary=[&](const char *name,std::optional<double> time) {
        std::string text=std::string(name)+" "+(time ? FormatRunTime(*time) : "--:--.---");
        draw->AddText(font,16*u,{x,y},ImGui::GetColorU32(ImVec4(.83f,.89f,.94f,p.splitOpacity*alpha)),text.c_str());
        x+=box.w*.5f;
    };
    if(p.splitPace) summary("PACE",Pace(data,rows,data.currentRunTimeSeconds));
    if(p.splitTheory) summary("BEST",Theoretical(data,rows));
    draw->PopClipRect();
}
inline double DeltaAge(const GameData &data, const ReplayPreferences &p)
{
    struct State { int pid=0,count=-1,metric=-1;int64_t run=0,reference=0;double started=0; };
    static State state;
    if(state.pid!=data.pid || state.run!=data.splitRunId || state.count!=data.splitPickupCount ||
        state.reference!=data.splitReferenceId || state.metric!=p.splitMetric)
    {
        state={data.pid,data.splitPickupCount,p.splitMetric,data.splitRunId,data.splitReferenceId,ImGui::GetTime()};
    }
    return std::max(0.,ImGui::GetTime()-state.started);
}
inline GameData PreviewData(const GameData &current)
{
    if (current.runCategory == 1 && current.splitRunId > 0) return current;
    GameData d{};
    d.pid = -725; d.timerState = d.runCategory = d.speedrunModeActive = d.runValidity = 1;
    d.splitRunId = d.reviewRunId = 925000725; d.splitReferenceId = 925000724;
    d.splitReferenceValid = d.splitReferenceComplete = d.splitHistoryReady = 1;
    d.splitPickupCount = 3; d.splitReferenceCount = d.splitExpectedPickups = 5;
    d.currentRunTimeSeconds = 31; d.splitReferenceFinish = 45; d.splitTheoreticalBest = 40.5; d.splitBestExit = 2;
    double reference[] = {10.5,19.8,31,38,42}, best[] = {9.8,9.5,10.2};
    for (int i = 0; i < 5; i++) { d.splitReferenceIds[i] = i; d.splitReferenceSeconds[i] = reference[i]; }
    for (int i = 0; i < 3; i++) { d.splitPickupIds[i] = i; d.splitPickupSeconds[i] = 10 * (i + 1); d.splitSectorBest[i] = best[i]; }
    return d;
}
inline void RenderPanel(const GameData &data, const ReplayPreferences &p, double age, bool preview = false)
{
    if (!preview && (!p.liveSplits || p.splitPlacement==0 || data.runCategory!=1 || !data.speedrunModeActive ||
        (data.timerState!=1 && data.timerState!=2) || age>=.5)) return;
    auto box=PanelBounds(p);
    if (preview) suite_hud::bounds[3]=box;
    ImGui::SetNextWindowPos({box.x,box.y},ImGuiCond_Always); ImGui::SetNextWindowSize({box.w,box.h},ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,{0,0,0,0}); ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,0);
    ImGui::Begin(preview ? "Editor / Splits" : "Set-Seed splits",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoInputs|
        ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNav|ImGuiWindowFlags_NoFocusOnAppearing);
    auto *draw=preview ? ImGui::GetForegroundDrawList() : ImGui::GetWindowDrawList();
    if (p.splitStyle == 0)
        DrawDeltaText(draw,g_timerFont ? g_timerFont : ImGui::GetFont(),box,data,p,suite_ui::preferences.motion,preview ? .4 : DeltaAge(data,p));
    else DrawPanel(draw,suite_ui::hudFont ? suite_ui::hudFont : ImGui::GetFont(),box,data,p,suite_ui::preferences.motion,preview);
    ImGui::Dummy({box.w,box.h}); ImGui::End(); ImGui::PopStyleVar(2); ImGui::PopStyleColor();
}
}
