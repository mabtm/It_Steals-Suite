#pragma once
#include "ReplayHealth.h"

inline ImVec4 RecordingStatusColor(practice::RecordingStatus status)
{
    switch (status)
    {
    case practice::RecordingStatus::Complete:
        return {.3f, .88f, .63f, 1};
    case practice::RecordingStatus::Partial:
        return {1, .7f, .3f, 1};
    case practice::RecordingStatus::Missing:
        return {1, .36f, .4f, 1};
    case practice::RecordingStatus::NotNeeded:
        return {.48f, .73f, .82f, 1};
    default:
        return {.62f, .67f, .72f, 1};
    }
}

inline bool RenderRecordingCheck(const practice::ReplayMotion &motion, practice::ReviewPlayback &replay)
{
    const auto check = practice::RecordingCheckFor(motion);
    std::string label = "Recording check";
    if (check->partial + check->missing)
        label += " / " + std::to_string(check->partial + check->missing) + " need attention";
    else if (check->unknown)
        label += " / historical data";
    else
        label += " / complete";
    bool open = ImGui::CollapsingHeader(label.c_str());
    Help(
        "Checks movement, camera, inputs, visual/animation observations, colliders and recorded audio. Complete means no detected capture gaps at the recorded resolution. This is independent of VALID / UNVALID and is not proof of replay origin.");
    if (!open)
        return false;
    bool seek = false;
    if (ImGui::BeginTable("##Recording coverage", 2, ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthStretch, 1.15f);
        ImGui::TableSetupColumn("Capture", ImGuiTableColumnFlags_WidthStretch, 1);
        for (const auto &row : check->channels)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextWrapped("%s", row.name);
            ImGui::TableNextColumn();
            ImGui::TextColored(RecordingStatusColor(row.status), "%s",
                               practice::RecordingStatusName(row.status));
            Help(row.detail.c_str());
        }
        ImGui::EndTable();
    }
    if (suite_ui::Button("Copy recording check"))
    {
        auto report = practice::RecordingCheckReport(motion, *check);
        suite_ui::CopyText(report.c_str());
    }
    bool gaps = false;
    for (const auto &row : check->channels)
        gaps |= !row.gaps.empty();
    if (gaps && ImGui::TreeNode("Recorded gaps"))
    {
        for (size_t channel = 0; channel < check->channels.size(); channel++)
        {
            const auto &row = check->channels[channel];
            for (size_t i = 0; i < row.gaps.size(); i++)
            {
                const auto &gap = row.gaps[i];
                ImGui::PushID(int(channel * 32 + i));
                auto text =
                    std::string(row.name) + " / " + FormatRunTime(gap.begin) + " - " + FormatRunTime(gap.end);
                if (ImGui::Selectable(text.c_str(), false))
                {
                    replay.seconds = gap.begin;
                    replay.playing = false;
                    seek = true;
                }
                ImGui::PopID();
            }
        }
        ImGui::TreePop();
    }
    return seek;
}
