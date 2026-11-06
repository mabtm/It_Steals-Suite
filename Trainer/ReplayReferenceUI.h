#pragma once
#include "ReplayArchive.h"
#include "SuiteExperience.h"
#include <cctype>

namespace suite_splits
{
inline std::string ReferenceLower(std::string value)
{
    for(auto &c:value) c=char(std::tolower(static_cast<unsigned char>(c)));
    return value;
}
// Immutable archive rows are filtered and labelled only when their source or
// search changes. Opening the chooser never loads a replay on the UI thread.
struct ReferenceChoices
{
    std::shared_ptr<const practice::ReplayArchiveState> source;
    std::shared_ptr<const practice::MapData> target;
    std::vector<size_t> eligible, visible;
    std::vector<std::string> labels, search;
    char query[96]{};
    void Bind(std::shared_ptr<const practice::ReplayArchiveState> state,std::shared_ptr<const practice::MapData> next)
    {
        if(source==state && target==next) return;
        source=std::move(state);target=std::move(next);const auto &map=*target;
        eligible.clear();labels.clear();search.clear();
        uint64_t key=practice::SplitLayoutKey(map);
        for(size_t i=0;i<source->runs.size();i++)
        {
            const auto &row=source->runs[i];
            if(!key || row.id==map.review->id || row.outcome!=1 || !row.hasSplits || row.splitLayout!=key) continue;
            eligible.push_back(i);
        }
        std::sort(eligible.begin(),eligible.end(),[&](size_t a,size_t b) {
            const auto &x=source->runs[a],&y=source->runs[b];
            if(x.valid!=y.valid)return x.valid;
            return x.seconds!=y.seconds ? x.seconds<y.seconds : x.id>y.id;
        });
        for(auto i:eligible)
        {
            const auto &r=source->runs[i];
            labels.push_back(FormatRunTime(r.seconds)+" | "+r.name+(r.valid?" | VALID":" | unverified"));
            search.push_back(ReferenceLower(labels.back()));
        }
        Filter();
    }
    void Filter()
    {
        visible.clear();auto needle=ReferenceLower(query);
        for(size_t i=0;i<eligible.size();i++)if(needle.empty() || search[i].find(needle)!=std::string::npos)visible.push_back(i);
    }
};
inline bool ReferencePicker(std::shared_ptr<const practice::MapData> target,const char *id)
{
    if(!target || !target->review) return false;
    auto &archive=practice::g_replayArchive;
    archive.EnsureReference(target);
    static ReferenceChoices choices;
    choices.Bind(archive.Get(),target);
    bool changed=false;auto selected=archive.WantedReference();
    auto state=choices.source;auto found=state->byId.find(selected);
    const char *label=!selected ? "Automatic / matching PB" : found==state->byId.end() ? "Selected recording unavailable" : state->runs[found->second].name.c_str();
    ImGui::PushID(id);
    ImGui::TextDisabled("Split reference");ImGui::SetNextItemWidth(-1);
    if(ImGui::BeginCombo("##Reference",label,ImGuiComboFlags_HeightLarge))
    {
        ImGui::SetNextItemWidth(-1);
        if(ImGui::InputTextWithHint("##Search","Search matching recordings",choices.query,sizeof(choices.query)))choices.Filter();
        if(ImGui::Selectable("Automatic / matching PB",!selected))
        {archive.SetReference(0);changed=true;}
        ImGui::Separator();
        ImGuiListClipper clip;clip.Begin(int(choices.visible.size()));
        while(clip.Step())for(int n=clip.DisplayStart;n<clip.DisplayEnd;n++)
        {
            size_t i=choices.visible[size_t(n)];const auto &row=state->runs[choices.eligible[i]];
            ImGui::PushID(int(i));
            if(ImGui::Selectable(choices.labels[i].c_str(),selected==row.id))
            {archive.SetReference(row.id,target);changed=true;}
            ImGui::PopID();
        }
        if(choices.visible.empty())ImGui::TextDisabled("No matching completed recordings");
        ImGui::EndCombo();
    }
    if((selected && archive.ManualReferenceLoading()) || (!selected && archive.ReferenceLoading(target->review->id)))
        ImGui::TextDisabled("Loading split timings...");
    else if(auto reference=archive.ResolvedReference(*target))
        ImGui::TextDisabled("%s / %s",FormatRunTime(reference->review->seconds).c_str(),reference->review->valid?"valid finish":"unverified replay");
    else ImGui::TextWrapped("No compatible reference selected. Choose Automatic or a recording with the same seed, spawn and layout.");
    auto error=archive.ReferenceError();
    if(selected && !error.empty())ImGui::TextWrapped("%s",error.c_str());
    ImGui::PopID();return changed;
}
}
