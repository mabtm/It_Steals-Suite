#pragma once
#include "InspectorPresentation.h"

namespace inspector_ui {
inline bool Motion(){return suite_ui::preferences.motion&&inspector_fx::preferences.effects;}
inline void Sound(suite_ui::Tone tone){inspector_fx::feedback.Emit(tone,suite_ui::LifecycleNow(),inspector_fx::preferences,suite_ui::preferences,g_pShared&&g_pShared->timerState==1);}
inline ImVec4 Accent(){return g_headerCol;}
inline ImVec4 PinnedAccent(){return suite_ui::Mix(g_headerCol,g_accentCol,.6f);}
inline ImU32 Ink(ImVec4 c,float alpha=1){c.w*=alpha;return ImGui::GetColorU32(c);}
inline void Text(ImDrawList* d,ImVec2 p,const char* value,float size,ImVec4 color,ImVec4 clip){
    d->AddText(ImGui::GetFont(),size,{std::floor(p.x),std::floor(p.y)},Ink(color),value,nullptr,0,&clip);
}
inline void Panel(ImDrawList* d,ImVec2 a,ImVec2 b,bool strong=false){
    float u=suite_ui::Unit();auto edge=suite_ui::Color(AppearancePreferences::Outline),accent=Accent();
    auto bg=suite_ui::Mix(g_mainBgCol,suite_ui::Color(AppearancePreferences::Button),strong?.34f:.22f);
    for(int i=3;i>0;--i)d->AddRect({a.x-i*u,a.y-i*u},{b.x+i*u,b.y+i*u},Ink(g_mainBgCol,.12f),12*u,0,2*u);
    d->AddRectFilled(a,b,Ink(bg,inspector_fx::preferences.opacity),11*u);
    d->AddRectFilledMultiColor({a.x+10*u,a.y+2*u},{b.x-10*u,std::min(b.y-2*u,a.y+56*u)},Ink(accent,.045f),Ink(accent,.015f),Ink(accent,0),Ink(accent,0));
    d->AddRect(a,b,Ink(edge,.72f),11*u,0,u);
    d->AddLine({a.x+14*u,a.y+u},{std::min(b.x-14*u,a.x+82*u),a.y+u},Ink(accent,.8f),2*u);
}
inline void Brackets(ImDrawList* d,ImVec2 a,ImVec2 b,ImVec4 color,float progress=1,float strength=1){
    if(b.x<=a.x||b.y<=a.y)return;
    float u=suite_ui::Unit(),length=std::min({20*u,(b.x-a.x)*.22f,(b.y-a.y)*.22f});
    float grow=(1-progress)*8*u;a.x-=grow;a.y-=grow;b.x+=grow;b.y+=grow;
    float weight=inspector_fx::preferences.outline*u;
    d->AddRect(a,b,Ink(color,.17f*strength),3*u,0,u);
    for(int x:{0,1})for(int y:{0,1}){ImVec2 p(x?b.x:a.x,y?b.y:a.y);d->AddLine(p,{p.x+(x?-length:length),p.y},Ink(color,strength),weight);d->AddLine(p,{p.x,p.y+(y?-length:length)},Ink(color,strength),weight);}
}
inline void BorderImpulse(ImDrawList* d,ImVec2 a,ImVec2 b,double age,ImVec4 color){
    if(!Motion()||age<0||age>.48||b.x<=a.x||b.y<=a.y)return;
    float perimeter=2*((b.x-a.x)+(b.y-a.y)),head=float(age/.48)*perimeter,tail=std::max(0.f,head-perimeter*.18f),walk=0;
    const ImVec2 p[]={a,{b.x,a.y},b,{a.x,b.y},a};
    for(int i=1;i<5;i++){auto x=p[i-1],y=p[i];float length=std::abs(y.x-x.x)+std::abs(y.y-x.y);if(length<=0)continue;float from=std::clamp((tail-walk)/length,0.f,1.f),to=std::clamp((head-walk)/length,0.f,1.f);if(to>from)d->AddLine({x.x+(y.x-x.x)*from,x.y+(y.y-x.y)*from},{x.x+(y.x-x.x)*to,x.y+(y.y-x.y)*to},Ink(suite_ui::Mix(color,g_textCol,.6f),1-float(age/.48)*.45f),2*suite_ui::Unit());walk+=length;}
}
inline bool Action(const char* label,suite_ui::Icon icon,bool active=false,ImVec2 size={0,0}){
    float u=suite_ui::Unit();auto* window=ImGui::GetCurrentWindow();auto id=window->GetID(label);auto text=ImGui::CalcTextSize(label,nullptr,true);
    if(size.x<=0)size.x=text.x+48*u;
    if(size.y<=0)size.y=36*u;
    auto a=ImGui::GetCursorScreenPos();bool clicked=ImGui::InvisibleButton(label,size,ImGuiButtonFlags_EnableNav);bool hovered=ImGui::IsItemHovered();
    float over=suite_ui::Animate(id^0x9ab101u,hovered?1.f:0.f,false,.14f),press=suite_ui::Animate(id^0x9ab102u,ImGui::IsItemActive()?1.f:0.f,false,.08f);
    auto* d=ImGui::GetWindowDrawList();ImVec2 b(a.x+size.x,a.y+size.y);auto base=suite_ui::Color(AppearancePreferences::Button),edge=suite_ui::Color(AppearancePreferences::Outline),color=suite_ui::Mix(base,Accent(),(active?.17f:0)+over*.14f+press*.08f);
    d->AddRectFilled(a,b,Ink(color),7*u);d->AddRect(a,b,Ink(suite_ui::Mix(edge,Accent(),active?.6f:over*.6f),.7f),7*u,0,u);
    suite_ui::DrawIcon(d,icon,{a.x+10*u,a.y+(size.y-18*u)*.5f+press*u},18*u,Ink(active?Accent():g_textCol));
    ImVec4 clip(a.x+36*u,a.y,b.x-7*u,b.y);Text(d,{a.x+36*u,a.y+(size.y-text.y)*.5f+press*u},label,ImGui::GetFontSize(),g_textCol,clip);
    if(ImGui::IsItemFocused())d->AddRect({a.x-2*u,a.y-2*u},{b.x+2*u,b.y+2*u},Ink(Accent()),8*u,0,u);
    if(clicked)Sound(suite_ui::Tone::InspectorSelect);
    return clicked;
}
inline void InlineAction(const char* label){float right=ImGui::GetCursorScreenPos().x+ImGui::GetContentRegionAvail().x;float needed=ImGui::CalcTextSize(label).x+48*suite_ui::Unit()+ImGui::GetStyle().ItemSpacing.x;if(ImGui::GetItemRectMax().x+needed<right)ImGui::SameLine();}
inline void Badge(ImDrawList* d,ImVec2 p,const char* label,ImVec4 color,float width=0){
    float u=suite_ui::Unit(),h=25*u;if(width<=0)width=ImGui::CalcTextSize(label).x+26*u;
    d->AddRectFilled(p,{p.x+width,p.y+h},Ink(color,.11f),h*.5f);d->AddCircleFilled({p.x+10*u,p.y+h*.5f},2.5f*u,Ink(color),12);
    Text(d,{p.x+18*u,p.y+4*u},label,14*u,color,{p.x,p.y,p.x+width-5*u,p.y+h});
}
inline float ValuePulse(const inspector::Value& value){return Motion()?inspector_fx::changes.Pulse(value.key,ImGui::GetTime()):0;}
inline void PropertyValue(const inspector::Value& value){
    float pulse=ValuePulse(value);ImGui::PushStyleColor(ImGuiCol_Text,suite_ui::Mix(g_textCol,Accent(),pulse*.8f));ImGui::TextWrapped("%s",value.value.c_str());ImGui::PopStyleColor();
    if(pulse>0){auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();ImGui::GetWindowDrawList()->AddLine({a.x-4*suite_ui::Unit(),a.y},{a.x-4*suite_ui::Unit(),b.y},Ink(Accent(),pulse*.65f),suite_ui::Unit());}
}
inline void StyleControls(){
    auto& p=inspector_fx::preferences;
    if(Action("Style",suite_ui::Icon::Settings))ImGui::OpenPopup("Inspector style");
    auto screen=ImGui::GetIO().DisplaySize;float maxWidth=std::max(120.f,screen.x-24),maxHeight=std::max(120.f,screen.y-24);
    ImGui::SetNextWindowSizeConstraints({std::min(310*suite_ui::Unit(),maxWidth),0},{maxWidth,maxHeight});
    if(ImGui::BeginPopup("Inspector style")){
        ImGui::TextColored(g_headerCol,"INSPECTOR / STYLE");ImGui::Spacing();
        suite_ui::Checkbox("Selection effects",&p.effects);Help("Short selection impulses and property-change highlights. Also follows the global interface motion setting.");
        suite_ui::Checkbox("Inspector sounds",&p.sounds);Help("Selection, pin and panel cues. Also follows the global interface sound switch and volume.");
        ImGui::TextDisabled("Sound level");ImGui::SetNextItemWidth(-1);float volume=p.volume*100;if(ImGui::SliderFloat("##Inspector sound level",&volume,0,100,"%.0f%%"))p.volume=volume/100;
        ImGui::TextDisabled("Panel opacity");ImGui::SetNextItemWidth(-1);float opacity=p.opacity*100;if(ImGui::SliderFloat("##Inspector panel opacity",&opacity,65,100,"%.0f%%"))p.opacity=opacity/100;
        ImGui::TextDisabled("Outline width");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Inspector outline width",&p.outline,1,4,"%.1f");
        if(Action("Preview sound",suite_ui::Icon::Scan))Sound(suite_ui::Tone::InspectorPin);
        ImGui::TextWrapped("Colors and fonts follow your interface theme.");
        ImGui::EndPopup();
    }
}
}
