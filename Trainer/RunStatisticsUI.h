#pragma once
#include "RunStatistics.h"

namespace run_stats
{
inline int viewFilter = 0;
inline bool setupReady = false, setupPractice = false, setupFixed = false, setupSpawn = false;
inline int setupSeed = 0;
inline std::array<std::string, 8> pbLabels;
inline std::array<double, 8> pbFlash{};

inline bool CanStart()
{
    return g_pShared && !HasPendingRequest() &&
           (!setupPractice || !setupFixed || g_pShared->rngPatchComplete);
}

inline bool StartSelected()
{
    if (!CanStart())
        return false;
    g_selectedSpeedrunLevel = dashboard.selected;
    g_practiceMode = setupPractice;
    g_seededPractice = setupPractice && setupFixed;
    g_practiceSeed = setupSeed;
    g_rawPracticeSpawn = setupSpawn;
    g_commands.practiceSeed = setupSeed;
    g_commands.setSeedCmd = g_seededPractice ? 1 : 0;
    return SetSpeedrunMode(true);
}

inline void Card(int index, float width)
{
    using namespace suite_ui;
    float u = Unit();
    const auto &mode = dashboard.modes[index];
    ImGui::PushID(index);
    auto a = ImGui::GetCursorScreenPos();
    ImVec2 b(a.x + width, a.y + 132 * u);
    bool click = ImGui::InvisibleButton("##Mode card", {width, 132 * u}, ImGuiButtonFlags_EnableNav);
    auto id = ImGui::GetItemID();
    float selected = Animate(id ^ 0x4118u, dashboard.selected == index ? 1.f : 0.f);
    float hover = Animate(id ^ 0x4119u, ImGui::IsItemHovered() ? 1.f : 0.f);
    float entry = Animate(id ^ 0x411au, 1.f, true, .18f);
    a.y += 6 * u * (1 - entry);
    b.y += 6 * u * (1 - entry);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * entry);
    float flash =
        preferences.motion ? float(std::clamp(1. - (ImGui::GetTime() - pbFlash[index]) / .8, 0., 1.)) : 0;
    auto *draw = ImGui::GetWindowDrawList();
    Surface(draw, a, b, dashboard.selected == index);
    draw->AddRect(a, {b.x - 2 * u, b.y - 3 * u},
                  ImGui::GetColorU32(Alpha(heading, .08f + .6f * selected + .16f * hover + .1f * flash)),
                  9 * u, 0, (1 + selected * .35f) * u);
    ImVec4 clip(a.x + 13 * u, a.y, b.x - 12 * u, b.y);
    TextAt(draw, {a.x + 15 * u, a.y + 12 * u}, kMapLevelNames[index], ink, 16 * u, clip);
    TextAt(draw, {a.x + 15 * u, a.y + 37 * u}, "CLEAN PERSONAL BEST", Blend(ink, background, .45f), 10 * u,
           clip);
    draw->AddText(g_timerFont ? g_timerFont : ImGui::GetFont(), 28 * u, {a.x + 14 * u, a.y + 49 * u},
                  ImGui::GetColorU32(mode.pb > 0 ? heading : Blend(ink, background, .55f)),
                  pbLabels[index].c_str(), nullptr, 0, &clip);
    const char *labels[] = {"ATTEMPTS", "FINISHES", "TIME PLAYED"};
    const std::string *values[] = {&mode.attempts, &mode.finished, &mode.timeShort};
    float cell = (width - 30 * u) / 3;
    for (int i = 0; i < 3; i++)
    {
        float x = a.x + 15 * u + i * cell;
        ImVec4 column(x, a.y, x + cell - 4 * u, b.y);
        TextAt(draw, {x, a.y + 92 * u}, labels[i], Blend(ink, background, .48f), 9 * u, column);
        TextAt(draw, {x, a.y + 106 * u}, values[i]->c_str(), ink, 14 * u, column);
    }
    if (ImGui::IsItemFocused())
        draw->AddRect(a, b, ImGui::GetColorU32(heading), 9 * u, 0, 2 * u);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("View %s statistics", kMapLevelNames[index]);
    ImGui::PopStyleVar();
    if (click)
    {
        dashboard.selected = index;
        Request(Tone::Click);
    }
    ImGui::PopID();
    if (click)
        ImGui::OpenPopup("Mode statistics");
}

inline void Details()
{
    float u = suite_ui::Unit();
    auto work = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize({std::min(530 * u, work.x - 30), std::min(430 * u, work.y - 30)},
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({std::min(300 * u, work.x - 30), std::min(200 * u, work.y - 30)},
                                        {work.x - 30, work.y - 30});
    if (!ImGui::BeginPopupModal("Mode statistics", nullptr, ImGuiWindowFlags_NoResize))
        return;
    ImGui::BeginChild("Mode detail scroll", {0, -ImGui::GetFrameHeightWithSpacing() - 5 * u});
    const auto &mode = dashboard.modes[dashboard.selected];
    const auto &value = mode.totals;
    ImGui::TextColored(g_headerCol, "%s", kMapLevelNames[dashboard.selected]);
    ImGui::TextDisabled("%s statistics", viewFilter == 1   ? "Clean"
                                         : viewFilter == 2 ? "Practice"
                                                           : "All-category");
    ImGui::Separator();
    ImGui::Text("Clean PB  %s", pbLabels[dashboard.selected].c_str());
    if (mode.pb > 0)
        ImGui::TextDisabled("PB date: %s", mode.pbDate.c_str());
    if (ImGui::BeginTable("Mode totals", 2, ImGuiTableFlags_SizingStretchSame))
    {
        auto row = [&](const char *label, const std::string &text) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(text.c_str());
        };
        row("Time played", mode.time);
        row("Attempts", mode.attempts);
        row("Completed", mode.finished);
        row("Resets", std::to_string(value.resets));
        row("Deaths", std::to_string(value.deaths));
        char rate[32];
        snprintf(rate, sizeof(rate), "%.1f%%", value.Completion());
        row("Completion rate", rate);
        row("Average finish", value.finished ? FormatRunTime(value.finishSeconds / value.finished) : "--");
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Recent attempts / all categories");
    int recent = 0;
    if (ImGui::BeginTable("Recent mode runs", 3, ImGuiTableFlags_SizingStretchSame))
    {
        for (int i = 0; i < dashboard.snapshot.historyCount && recent < 6; i++)
        {
            auto run = ReadHistory(dashboard.snapshot, i);
            if (run.scene != kMapScenes[dashboard.selected])
                continue;
            const char *outcomes[] = {"Running", "Finished", "Reset", "Death", "Interrupted", "Stopped"};
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(Date(run.startedUtc).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(outcomes[std::min(int(run.outcome), 5)]);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(FormatRunTime(run.seconds).c_str());
            recent++;
        }
        ImGui::EndTable();
    }
    if (!recent)
        ImGui::TextDisabled("No recent attempts retained for this mode.");
    ImGui::Spacing();
    ImGui::TextWrapped("Complete mode statistics recorded since %s.", dashboard.since.c_str());
    ImGui::EndChild();
    if (suite_ui::Button("Close"))
        ImGui::CloseCurrentPopup();
    SameLineIfFits("Start this mode");
    ImGui::BeginDisabled(!CanStart());
    if (suite_ui::Button("Start this mode") && StartSelected())
        ImGui::CloseCurrentPopup();
    ImGui::EndDisabled();
    ImGui::EndPopup();
}

inline void Settings()
{
    suite_ui::Section("Run setup", 0, [&]() {
        suite_ui::Checkbox("Practice Mode", &setupPractice);
        if (setupPractice)
        {
            suite_ui::Checkbox("Use fixed seed", &setupFixed);
            if (setupFixed)
            {
                if (ImGui::InputInt("Seed", &setupSeed))
                {
                    int type = KnownPracticeSeedType(setupSeed, dashboard.selected);
                    if (type)
                        setupSpawn = type == 1;
                }
                suite_ui::Checkbox("Use captured spawn seed", &setupSpawn);
                Help("Captured Seed Map seeds use the spawn seed option. These runs are SET PRACTICE.");
                if (g_pShared && !g_pShared->rngPatchComplete)
                    ImGui::TextWrapped("Fixed seeds require all RNG hooks to be ready before starting.");
            }
        }
        ImGui::TextWrapped(
            "Setup applies when you start a mode. T restarts the running mode while the game has focus.");
        ImGui::BeginDisabled(!g_pShared || !g_pShared->reviewReady || g_pShared->timerState == 1 ||
                             HasPendingRequest());
        if (suite_ui::Button("Analyze previous run"))
            RequestRunReview();
        ImGui::EndDisabled();
    });
    suite_ui::Section("Statistics information", 0, [&]() {
        ImGui::TextWrapped(
            "Mode statistics since %s. Earlier personal bests and lifetime counters are retained.",
            dashboard.since.c_str());
        ImGui::TextWrapped(
            "Attempts count Speedrun and Practice runs. Time played also includes normal play; menus, loading, pauses and unfocused intervals are excluded. Empty resets under 4 seconds stay out of attempt counters.");
        const auto &data = dashboard.snapshot;
        ImGui::TextWrapped("Lifetime: %d attempts, %d finishes, %d deaths, %d resets. Completion: %.1f%%. "
                           "Average finish: %s.",
                           data.totalRuns, data.totalFinishedRuns, data.totalDeaths, data.totalResets,
                           data.totalRuns ? 100. * data.totalFinishedRuns / data.totalRuns : 0.,
                           OverlayValue(OverlayField::AverageRun, data, "", 0).c_str());
        ImGui::BeginDisabled(!g_pShared || g_pShared->timerState == 1 || HasPendingRequest());
        if (suite_ui::Button("Reset statistics"))
            ImGui::OpenPopup("Reset statistics?");
        ImGui::EndDisabled();
        auto work = ImGui::GetMainViewport()->WorkSize;
        ImGui::SetNextWindowSizeConstraints({0, 0}, {work.x - 30, work.y - 30});
        if (ImGui::BeginPopupModal("Reset statistics?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped(
                "Clear global and per-mode counters, playtime, daily/session statistics and averages?");
            ImGui::TextWrapped("Personal bests and saved replays are retained.");
            ImGui::BeginDisabled(!g_pShared || g_pShared->timerState == 1 || HasPendingRequest());
            if (suite_ui::Button("Reset counters"))
            {
                g_commands.runReviewRequest = 3;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            SameLineIfFits("Cancel");
            if (suite_ui::Button("Cancel"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    });
    if (g_pShared && suite_ui::CollapsingHeader("Seed and run checks"))
    {
        ImGui::TextColored(g_headerCol, "%s", SeedTypeLabel(*g_pShared));
        if (g_pShared->seedKnown)
            ImGui::Text("%s: %d", g_pShared->seedKind == 2 ? "Practice base seed" : "Initial RNG seed",
                        g_pShared->runSeed);
        ImGui::TextDisabled("Reseed hooks: %s",
                            g_pShared->rngPatchComplete ? "22 / 22 ready" : "unavailable");
        RenderVerification(*g_pShared);
    }
}

inline void Render()
{
    float u = suite_ui::Unit();
    if (!setupReady)
    {
        setupReady = true;
        setupPractice = g_practiceMode;
        setupFixed = g_seededPractice;
        setupSpawn = g_rawPracticeSpawn;
        setupSeed = g_practiceSeed;
        dashboard.selected = g_selectedSpeedrunLevel;
    }
    bool compactFooter = ImGui::GetContentRegionAvail().y < 240 * u;
    bool shortLabels = ImGui::GetContentRegionAvail().x < 420 * u;
    float footer = ImGui::GetFrameHeightWithSpacing() + 8 * u +
                   (compactFooter ? 0 : ImGui::GetTextLineHeightWithSpacing());
    ImGui::BeginChild("SpeedrunScroll", {0, -footer});
    const char *filters[] = {"All", "Clean", "Practice"};
    for (int i = 0; i < 3; i++)
    {
        if (i)
            ImGui::SameLine();
        bool selected = viewFilter == i;
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, suite_ui::Alpha(g_headerCol, .25f));
        if (suite_ui::Button(filters[i]))
            viewFilter = i;
        if (selected)
            ImGui::PopStyleColor();
    }
    std::array<double, 8> oldPb;
    for (int i = 0; i < 8; i++)
        oldPb[i] = dashboard.modes[i].pb;
    bool hadData = dashboard.available && (!g_pShared || g_pShared->pid == dashboard.snapshot.pid);
    auto before = dashboard.rebuilds;
    dashboard.Update(g_pShared, viewFilter, ImGui::GetTime());
    if (dashboard.rebuilds != before)
        for (int i = 0; i < 8; i++)
        {
            const auto &mode = dashboard.modes[i];
            pbLabels[i] = mode.pb > 0 ? FormatRunTime(mode.pb) : "--:--.---";
            if (hadData && mode.pb > 0 && (oldPb[i] == 0 || mode.pb < oldPb[i]))
                pbFlash[i] = ImGui::GetTime();
        }
    SameLineIfFits(dashboard.sinceLabel.c_str());
    ImGui::TextDisabled("%s", dashboard.sinceLabel.c_str());
    if (!g_pShared)
        ImGui::TextWrapped(dashboard.available ? "Game disconnected / showing the last received statistics."
                                               : "Connect It Steals to load your saved statistics.");
    ImGui::Spacing();
    float available = ImGui::GetContentRegionAvail().x;
    auto a = ImGui::GetCursorScreenPos();
    auto *draw = ImGui::GetWindowDrawList();
    ImGui::Dummy({available, 62 * u});
    suite_ui::Surface(draw, a, {a.x + available, a.y + 62 * u});
    const char *labels[] = {"TIME PLAYED", "ATTEMPTS", "FINISHES"};
    const char *values[] = {available < 420 * u ? dashboard.timeShort.c_str() : dashboard.time.c_str(),
                            dashboard.attempts.c_str(), dashboard.finished.c_str()};
    float cell = available / 3, inset = available < 420 * u ? 8 * u : 14 * u;
    for (int i = 0; i < 3; i++)
    {
        float x = a.x + i * cell + inset;
        ImVec4 clip(x, a.y, a.x + (i + 1) * cell - inset, a.y + 62 * u);
        float width = ImGui::CalcTextSize(values[i]).x;
        float font = std::min(
            23 * u, std::max(12 * u, ImGui::GetFontSize() * (cell - 2 * inset) / std::max(1.f, width)));
        suite_ui::TextAt(draw, {x, a.y + 11 * u}, labels[i],
                         suite_ui::Blend(suite_ui::ink, suite_ui::background, .4f), 10 * u, clip);
        suite_ui::TextAt(draw, {x, a.y + 30 * u}, values[i], suite_ui::ink, font, clip);
    }
    ImGui::Spacing();
    int columns = available >= 750 * u ? 3 : available >= 490 * u ? 2 : 1;
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, {5 * u, 5 * u});
    if (ImGui::BeginTable("Mode cards", columns, ImGuiTableFlags_SizingStretchSame))
    {
        for (int i = 0; i < kMapLevelCount; i++)
        {
            ImGui::TableNextColumn();
            Card(i, ImGui::GetContentRegionAvail().x);
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    Details();
    ImGui::Spacing();
    Settings();
    if (g_pShared && g_pShared->statsSaveStatus == 2)
        ImGui::TextColored(g_accentCol, "Statistics could not be saved: %s", g_pShared->statsError);
    if (g_pShared && g_pShared->statsWarning[0])
        ImGui::TextWrapped("Data recovery: %s", g_pShared->statsWarning);
    ImGui::EndChild();
    ImGui::Separator();
    if (!compactFooter)
        ImGui::Text("Selected: %s  /  %s", kMapLevelNames[dashboard.selected],
                    setupPractice ? "Practice" : "Clean Speedrun");
    ImGui::BeginDisabled(!CanStart());
    if (suite_ui::Button(shortLabels ? "Start mode" : "Start selected mode"))
        StartSelected();
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!g_pShared || HasPendingRequest());
    if (g_speedrunMode)
    {
        const char *disable = shortLabels ? "Disable" : "Disable Speedrun";
        SameLineIfFits(disable);
        if (suite_ui::Button(disable))
            SetSpeedrunMode(false);
    }
    ImGui::EndDisabled();
}
} // namespace run_stats
