#pragma once
#include "Replay3DControls.h"

inline void RenderReplayMarkers(practice::ReviewPlayback &replay)
{
    using namespace suite_replay;
    static uint32_t editing = 0;
    static int64_t editRun = 0;
    static char name[129]{};
    static double at = 0;
    bool requested = requestMarker && marks;
    if (requested)
        requestMarker = false;
    ImGui::BeginDisabled(!marks);
    if (suite_ui::Button("+ Marker") || requested)
    {
        editing = 0;
        editRun = source->review->id;
        name[0] = 0;
        at = requested && markerAt >= 0 ? markerAt : replay.seconds;
        markerAt = -1;
        replay.playing = false;
        ImGui::OpenPopup("Named marker");
    }
    ImGui::EndDisabled();
    Help(
        "Personal annotations are saved separately and exported with the replay. They never change recorded game events or validation.");
    SameLineIfFits("Markers");
    if (suite_ui::Button("Markers"))
        ImGui::OpenPopup("Saved markers");
    ImGui::SetNextWindowSize({std::min(520.f * suite_ui::Unit(), ImGui::GetIO().DisplaySize.x - 30), 0},
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopup("Saved markers"))
    {
        if (!marks)
            ImGui::TextDisabled("Markers loading...");
        else if (marks->markers.empty())
            ImGui::TextDisabled("No named markers yet");
        bool close = false;
        if (marks)
            for (const auto &m : marks->markers)
            {
                ImGui::PushID(int(m.id));
                if (suite_ui::Button((FormatRunTime(m.seconds) + "  " + m.name).c_str()))
                {
                    replay.seconds = m.seconds;
                    replay.playing = false;
                    ImGui::CloseCurrentPopup();
                }
                Help("Seek to this annotation.");
                SameLineIfFits("Edit");
                if (suite_ui::Button("Edit"))
                {
                    editing = m.id;
                    editRun = marks->run;
                    snprintf(name, sizeof(name), "%s", m.name.c_str());
                    at = m.seconds;
                    close = true;
                }
                SameLineIfFits("Clip start");
                if (suite_ui::Button("Clip start"))
                {
                    auto next = std::make_shared<practice::ReplayAnnotations>(*marks);
                    next->clipA = m.seconds;
                    next->hasClipA = true;
                    Save(next);
                }
                SameLineIfFits("Clip end");
                if (suite_ui::Button("Clip end"))
                {
                    auto next = std::make_shared<practice::ReplayAnnotations>(*marks);
                    next->clipB = m.seconds;
                    next->hasClipB = true;
                    Save(next);
                }
                ImGui::PopID();
                if (close)
                    break;
            }
        if (close)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        if (close)
            ImGui::OpenPopup("Named marker");
    }
    ImGui::SetNextWindowSize({std::min(440.f * suite_ui::Unit(), ImGui::GetIO().DisplaySize.x - 30), 0},
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopup("Named marker"))
    {
        ImGui::TextColored(g_headerCol, "%s", editing ? "EDIT ANNOTATION" : "NEW ANNOTATION");
        ImGui::SetNextItemWidth(-1);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##Marker name", "e.g. missed turn / radar timing", name, sizeof(name));
        ImGui::SetNextItemWidth(150 * suite_ui::Unit());
        ImGui::InputDouble("Time (s)", &at, .01, 1, "%.3f");
        if (!std::isfinite(at))
            at = 0;
        if (source)
            at = std::clamp(at, 0., source->review->seconds);
        bool valid = marks && marks->run == editRun && name[0] && practice::ReplayUtf8(name, 128) &&
                     (editing || marks->markers.size() < 256);
        ImGui::BeginDisabled(!valid);
        if (suite_ui::Button("Save marker"))
        {
            auto next = std::make_shared<practice::ReplayAnnotations>(*marks);
            if (editing)
            {
                for (auto &m : next->markers)
                    if (m.id == editing)
                    {
                        m.seconds = at;
                        m.name = name;
                    }
            }
            else
            {
                uint32_t id = 1;
                while (std::any_of(next->markers.begin(), next->markers.end(),
                                   [&](const auto &m) { return m.id == id; }))
                    id++;
                next->markers.push_back({id, at, name});
            }
            std::sort(next->markers.begin(), next->markers.end(), [](const auto &a, const auto &b) {
                return a.seconds == b.seconds ? a.id < b.id : a.seconds < b.seconds;
            });
            Save(next);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        if (editing && marks && marks->run == editRun)
        {
            SameLineIfFits("Remove marker");
            if (suite_ui::Button("Remove marker"))
            {
                auto next = std::make_shared<practice::ReplayAnnotations>(*marks);
                next->markers.erase(std::remove_if(next->markers.begin(), next->markers.end(),
                                                   [&](const auto &m) { return m.id == editing; }),
                                    next->markers.end());
                Save(next);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
    auto state = practice::g_replayArchive.Get();
    if (source && state->annotationRun == source->review->id && !state->annotationError.empty())
    {
        ImGui::TextColored(g_accentCol, "Marker storage warning");
        Help(state->annotationError.c_str());
        if (marks && suite_ui::Button("Retry marker save"))
            Save(marks);
        if (!marks && suite_ui::Button("Retry marker load"))
            practice::g_replayArchive.LoadAnnotations(source);
    }
}

inline bool RenderReplayLoop(practice::ReviewPlayback &replay)
{
    using namespace suite_replay;
    bool changed = false;
    ImGui::BeginDisabled(!marks);
    if (suite_ui::Button("Set A") && marks)
    {
        auto next = std::make_shared<practice::ReplayAnnotations>(*marks);
        next->a = std::clamp(replay.seconds, 0., next->duration);
        next->hasA = true;
        if (!next->LoopValid())
            next->loop = false;
        Save(next);
        changed = true;
    }
    SameLineIfFits("Set B");
    if (suite_ui::Button("Set B") && marks)
    {
        auto next = std::make_shared<practice::ReplayAnnotations>(*marks);
        next->b = std::clamp(replay.seconds, 0., next->duration);
        next->hasB = true;
        if (!next->LoopValid())
            next->loop = false;
        Save(next);
        changed = true;
    }
    SameLineIfFits("A-B loop");
    bool enabled = marks && marks->loop;
    ImGui::BeginDisabled(!marks || !marks->LoopValid());
    if (suite_ui::Checkbox("A-B loop", &enabled) && marks)
    {
        auto next = std::make_shared<practice::ReplayAnnotations>(*marks);
        next->loop = enabled;
        if (enabled && (replay.seconds < next->a || replay.seconds >= next->b))
            replay.seconds = next->a;
        Save(next);
        changed = true;
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    Help(
        "Choose A before B (at least 0.02 s apart). The replay clock repeats this segment, including when the trainer menu is closed.");
    if (marks && (marks->hasA || marks->hasB))
    {
        ImGui::TextDisabled("A %s  /  B %s", marks->hasA ? FormatRunTime(marks->a).c_str() : "--",
                            marks->hasB ? FormatRunTime(marks->b).c_str() : "--");
    }
    return changed;
}
