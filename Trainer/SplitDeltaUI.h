#pragma once
#include "SplitDisplay.h"
#include "ReplayPreferences.h"
#include "HudEditorModel.h"

namespace suite_splits
{
inline std::optional<double> Delta(const practice::PickupSplit &row, const ReplayPreferences &p)
{
    if (p.splitMetric == 1) return row.sectorComparable ? std::optional<double>(row.sectorDelta) : std::nullopt;
    if (p.splitMetric == 2) return row.bestSegment >= 0 ? std::optional<double>(row.segment-row.bestSegment) : std::nullopt;
    return row.comparable ? std::optional<double>(row.delta) : std::nullopt;
}
// No solid primitives in the text style: only glyph alpha reaches the game.
inline void DrawDeltaGlyphs(ImDrawList *draw, ImFont *font, suite_hud::Rect box, const practice::PickupSplit &row,
                          const ReplayPreferences &p, bool motion, double displayAge,
                          std::optional<double> pace = {}, std::optional<double> theory = {})
{
    auto value=Delta(row,p);
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
    if(p.splitPace) summary("PACE",pace);
    if(p.splitTheory) summary("BEST",theory);
    draw->PopClipRect();
}
}
