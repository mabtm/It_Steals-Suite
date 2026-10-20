#pragma once
#include "ReplayMotion.h"
inline std::string ReplayObservationLabel(const std::string& raw){
    static constexpr std::pair<const char*,const char*> labels[]={
        {"HideAndSeekIt.FoundPlayer","Player discovered"},{"HideAndSeekIt.spottingPlayer","Sight contact"},{"HideAndSeekIt.SeeingIt","Reacting to player light"},{"HideAndSeekIt.SeekingMode","Seeking"},{"HideAndSeekIt.startChase","Chase started"},{"HideAndSeekIt.hiding","Hiding"},{"HideAndSeekIt.running","Running"},
        {"MonsterController.HasBeenSpotted","Spotted / game flag"},{"MonsterController.IsStunned","Stunned"},{"MonsterController.EnteringRageMode","Entering rage"},{"MonsterController.goBerserk","Berserk"},
        {"GhostScript.isHunting","Hunting"},{"GhostScript.isRunning","Running"},{"GhostScript.isChasing","Chasing"},{"GhostScript.isHibernating","Hibernating"},{"GhostScript.monsterHeardSounds","Heard player sounds"},{"GhostScript.preparingToChase","Preparing chase"},
        {"WallAI.isSpotted","Spotted / game flag"},{"WallAI.HasLineOfSight","Line of sight / game flag"},{"WallAI.isWithinLight","Within player light"},{"WallAI.isSleeping","Sleeping"},{"WallAI.angry","Angry"},{"WallAI.attackFreeze","Attack freeze"}};
    for(const auto& [key,label]:labels)if(raw==key)return label;
    return raw;
}
inline bool RenderReplayExperience(const practice::ReplayMotion& motion,practice::ReviewPlayback& playback){
    if(!ImGui::CollapsingHeader("Monster analysis / recorded game data"))return false;
    const auto& data=motion.experience;
    if(!data.recorded){ImGui::TextDisabled("Legacy replay / game observations unavailable");return false;}
    ImGui::TextDisabled("%zu game queries / %zu events / %zu animation tracks",data.probes.size(),data.events.size(),data.animations.size());
    Help("Queries come from the actual game Raycast and Linecast calls, sampled at up to 10 Hz. State writes are observed separately without throttling. A query hit alone is not a detection verdict. Classic HasBeenSpotted can include scripted conditions; Hide and Seek SeeingIt is a player-light response; FoundPlayer can include proximity.");
    if(data.dropped)ImGui::TextColored(g_accentCol,"Incomplete observations / %d omitted",data.dropped);
    if(data.audio.recorded){ImGui::TextDisabled("Game audio / %d Hz / %d ch / %.2f s",data.audio.rate,data.audio.channels,data.audio.Duration());if(data.audio.flags||!motion.audioError.empty()){
        ImGui::TextColored(g_accentCol,"Incomplete audio / %d gaps",data.audio.gaps);
        if(data.audio.flags&1)Help("Some PCM blocks were missing or the bounded audio queue filled.");
        if(data.audio.flags&2)Help("Recording reached the 256 MiB audio file limit.");
        if(data.audio.flags&4)Help("An audio route was unavailable. Centered 2D listener-bypass sounds such as crystal pickups are captured separately from 7.8.1. Bypassed 3D/spatialized sources, disabled source effects or mixer configurations that cannot be observed accurately are not reconstructed.");
        if(data.audio.flags&8)Help("Unsupported channels or invalid sample values occurred.");
        if(data.audio.flags&16)Help("Writing the recorded game audio failed.");
    }if(!motion.audioError.empty())Help(motion.audioError.c_str());}
    else ImGui::TextDisabled("No recorded game audio");
    static int actor=0,kind=0;const char* kinds[]={"All observations","State changes","Sounds","First observed state"};
    ImGui::SetNextItemWidth(180*suite_ui::Unit());ImGui::Combo("Event type",&kind,kinds,4);
    ImGui::SetNextItemWidth(140*suite_ui::Unit());ImGui::InputInt("Actor (0 = all)",&actor);actor=std::clamp(actor,0,128);
    struct Cache {const practice::ReplayMotion* source=nullptr;int64_t id=0;size_t count=0;int actor=-1,kind=-1;std::vector<size_t> rows;};static Cache cache;
    if(cache.source!=&motion||cache.id!=motion.id||cache.count!=data.events.size()||cache.actor!=actor||cache.kind!=kind){cache={&motion,motion.id,data.events.size(),actor,kind,{}};for(size_t i=0;i<data.events.size();i++){const auto& e=data.events[i];if(actor&&e.actor!=actor)continue;if((kind==1&&e.kind!=1)||(kind==2&&e.kind!=2&&e.kind!=3)||(kind==3&&e.kind!=4))continue;cache.rows.push_back(i);}}
    bool seek=false;
    if(ImGui::BeginChild("##Game observations",{0,190*suite_ui::Unit()},ImGuiChildFlags_Borders)){
        ImGuiListClipper clipper;clipper.Begin(int(cache.rows.size()));while(clipper.Step())for(int row=clipper.DisplayStart;row<clipper.DisplayEnd;row++){
            const auto& e=data.events[cache.rows[size_t(row)]];ImGui::PushID(row);
            auto label=FormatRunTime(e.seconds)+"  /  "+ReplayObservationLabel(data.Name(e.name));
            if(e.actor)label+=" ["+std::to_string(e.actor)+"]";
            if(e.kind==1||e.kind==4)label+=e.value!=0?" / ON":" / OFF";
            if(e.kind==4)label+=" / observed";
            if(ImGui::Selectable(label.c_str(),std::abs(playback.seconds-e.seconds)<.001)){playback.seconds=e.seconds;playback.playing=false;seek=true;}
            if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::Text("Source time %.6f s",e.seconds);ImGui::TextUnformatted(data.Name(e.name).c_str());ImGui::Text("World position %.3f / %.3f / %.3f",e.position.x,e.position.y,e.position.z);ImGui::EndTooltip();}ImGui::PopID();
        }
    }ImGui::EndChild();
    return seek;
}
