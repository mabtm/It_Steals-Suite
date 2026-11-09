#pragma once
#include "RunStatistics.h"

namespace run_stats
{
inline int viewFilter = 0;
inline bool setupReady = false, setupPractice = false, setupFixed = false, setupSpawn = false;
inline int setupSeed = 0;
struct SetupSelection
{
    bool practice = false, fixed = false, spawn = false;
    int seed = 0, level = 0;

    bool operator==(const SetupSelection &) const = default;
};
inline SetupSelection observedSelection;
inline std::array<std::string, 8> pbLabels;
inline std::array<double, 8> pbFlash{};

inline SetupSelection CurrentSelection()
{
    return {g_practiceMode, g_seededPractice, g_rawPracticeSpawn, g_practiceSeed,
            g_selectedSpeedrunLevel};
}

inline void SyncSetupSelection()
{
    const auto current = CurrentSelection();
    // Keep edits local until Start is pressed. A selection adopted elsewhere
    // (for example a seed search result) replaces the draft on the next visit.
    if (!setupReady || !(current == observedSelection))
    {
        setupReady = true;
        setupPractice = current.practice;
        setupFixed = current.fixed;
        setupSpawn = current.spawn;
        setupSeed = current.seed;
        dashboard.selected = std::clamp(current.level, 0, kMapLevelCount - 1);
    }
    observedSelection = current;
}

inline int SetupCategory() { return setupPractice ? 2 : setupFixed ? 1 : 0; }
inline const char *SetupBestCategory() { return setupPractice ? "Practice" : setupFixed ? "Set-Seed" : "Random Seed"; }
inline double SetupBest(int mode)
{
    const auto &data = dashboard.snapshot;
    double value = data.allModePersonalBest[std::clamp(mode, 0, 7) * 3 + SetupCategory()];
    // Old cached/offline snapshots only know their explicitly identified clean category.
    if (value <= 0 && SetupCategory() == data.statsCategory)
        value = data.modePersonalBest[std::clamp(mode, 0, 7)];
    return value;
}
inline std::string SetupBestLabel(int mode)
{
    double value = SetupBest(mode);
    return value > 0 ? FormatRunTime(value) : "--:--.---";
}

inline bool CanStart()
{
    const bool setSeed = setupFixed && !setupPractice;
    auto version = g_pShared ? suite_update::Version::Parse(g_pShared->version) : std::nullopt;
    const bool supportsCategory = version && *version >= suite_update::Version{{7, 22, 0}};
    return g_pShared && !HasPendingRequest() && !suite_hud::active && !suite_clip::open &&
           (!setupFixed || g_pShared->rngPatchComplete) && (!setSeed || supportsCategory);
}

inline bool StartSelected()
{
    SyncSetupSelection();
    if (!CanStart())
        return false;
    g_selectedSpeedrunLevel = dashboard.selected;
    g_practiceMode = setupPractice;
    g_seededPractice = setupFixed;
    g_practiceSeed = setupSeed;
    g_rawPracticeSpawn = setupSpawn;
    g_commands.practiceSeed = setupSeed;
    g_commands.setSeedCmd = g_seededPractice ? 1 : 0;
    const bool started = SetSpeedrunMode(true);
    observedSelection = CurrentSelection();
    return started;
}

// Native mode rows keep selection separate from loading. Keep the stable row
// ID shared with keyboard/navigation and direct interaction verification.
inline void Card(int index, float width)
{
    using namespace suite_ui;
    float u = Unit(), height = 36 * u;
    ImGui::PushID(index);
    auto a = ImGui::GetCursorScreenPos();
    bool click = ImGui::InvisibleButton("##Mode card", {width, height}, ImGuiButtonFlags_EnableNav);
    auto id = ImGui::GetItemID();
    float selected = Animate(id ^ 0x4118u, dashboard.selected == index ? 1.f : 0.f, true, .12f);
    float hover = Animate(id ^ 0x4119u, ImGui::IsItemHovered() ? 1.f : 0.f, true, .1f);
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(a, {a.x + width, a.y + height},
        ImGui::GetColorU32(Blend(background, heading, .06f * hover + .16f * selected)), 3 * u);
    if (selected > .01f)
        draw->AddRectFilled(a, {a.x + 3 * u, a.y + height},
                            ImGui::GetColorU32(Alpha(heading, selected)), u);
    const auto bestText = SetupBest(index) > 0 ? SetupBestLabel(index) : std::string();
    float bestWidth = bestText.empty() ? 0 : ImGui::CalcTextSize(bestText.c_str()).x + 18 * u;
    float textWidth = ImGui::CalcTextSize(kMapLevelNames[index]).x;
    float font = ImGui::GetFontSize() * std::min(1.f, std::max(1.f, width - 24 * u - bestWidth) /
                                                        std::max(1.f, textWidth));
    TextAt(draw, {a.x + 12 * u, a.y + (height - font) * .5f}, kMapLevelNames[index], ink, font,
           {a.x + 10 * u, a.y, a.x + width - 8 * u - bestWidth, a.y + height});
    if (!bestText.empty())
        TextAt(draw, {a.x + width - bestWidth, a.y + (height - ImGui::GetFontSize()) * .5f}, bestText.c_str(),
               Muted(), ImGui::GetFontSize(), {a.x, a.y, a.x + width - 8 * u, a.y + height});
    if (ImGui::IsItemFocused())
        draw->AddRect(a, {a.x + width, a.y + height}, ImGui::GetColorU32(heading), 3 * u);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Select %s. Use Details to view its statistics, or Start to play.", kMapLevelNames[index]);
    if (click)
    {
        dashboard.selected = index;
        Request(Tone::Click);
    }
    ImGui::PopID();
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
    ImGui::TextDisabled("%s statistics", viewFilter == 1
                                           ? (dashboard.snapshot.statsCategory == 1 ? "Set-Seed" : "Random Seed")
                                           : viewFilter == 2 ? "Practice" : "Overview");
    ImGui::Separator();
    ImGui::Text("%s %s  %s", SetupBestCategory(), setupPractice ? "best (unranked)" : "PB",
                 SetupBestLabel(dashboard.selected).c_str());
    const auto bestDate = dashboard.snapshot.allModePbUtc[dashboard.selected * 3 + SetupCategory()];
    if (bestDate > 0) ImGui::TextDisabled("Best date: %s", Date(bestDate).c_str());
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
    if (ImGui::BeginTable("Recent mode runs", 4, ImGuiTableFlags_SizingStretchSame))
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
            ImGui::TextUnformatted(run.CategoryLabel());
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
    if (suite_ui::PrimaryButton("Start this mode") && StartSelected())
        ImGui::CloseCurrentPopup();
    ImGui::EndDisabled();
    ImGui::EndPopup();
}

inline const char *SetupCategoryName()
{
    return setupPractice ? (setupFixed ? "Set-Seed Practice" : "Practice")
                         : setupFixed ? "Set-Seed" : "Random Seed";
}

inline void CategorySetup()
{
    const char *categories[] = {"Random Seed", "Set-Seed", "Practice"};
    int selected = setupPractice ? 2 : setupFixed ? 1 : 0;
    const char *categoryLabel = ImGui::GetContentRegionAvail().x < 420 * suite_ui::Unit()
                                    ? "Category" : "Run category";
    ImGui::SetNextItemWidth(std::min(300 * suite_ui::Unit(),
        std::max(100 * suite_ui::Unit(), ImGui::GetContentRegionAvail().x -
            ImGui::CalcTextSize(categoryLabel).x - ImGui::GetStyle().ItemInnerSpacing.x)));
    if (ImGui::Combo(categoryLabel, &selected, categories, 3))
    {
        setupPractice = selected == 2;
        setupFixed = selected == 1;
        if (selected == 0)
            setupSpawn = false;
    }
    Help("Random Seed and Set-Seed are separate speedrun categories. Choose before starting. Set-Seed permits the selected seed; training aids remain locked. Practice permits aids and does not produce a clean speedrun.");
    if (setupPractice)
        suite_ui::Checkbox("Use Set-Seed in Practice", &setupFixed);
    if (setupFixed)
    {
        ImGui::SetNextItemWidth(std::min(300 * suite_ui::Unit(),
            std::max(100 * suite_ui::Unit(), ImGui::GetContentRegionAvail().x -
                ImGui::CalcTextSize("Seed").x - ImGui::GetStyle().ItemInnerSpacing.x)));
        if (ImGui::InputInt("Seed", &setupSeed))
        {
            int type = KnownPracticeSeedType(setupSeed, dashboard.selected);
            if (type)
                setupSpawn = type == 1;
        }
        int recipe = setupSpawn ? 1 : 0;
        const char *recipes[] = {"Practice base seed", "Captured spawn seed"};
        ImGui::SetNextItemWidth(std::min(300 * suite_ui::Unit(),
            std::max(100 * suite_ui::Unit(), ImGui::GetContentRegionAvail().x -
                ImGui::CalcTextSize("Recipe").x - ImGui::GetStyle().ItemInnerSpacing.x)));
        if (ImGui::Combo("Recipe", &recipe, recipes, 2))
            setupSpawn = recipe == 1;
        Help("Practice base seed uses the standard derived recipe. Captured spawn seed reuses the exact seed from Seeds & routes. Both recipes are supported in Set-Seed; changing the recipe during a run invalidates that attempt.");
        if (g_pShared && !g_pShared->rngPatchComplete)
            ImGui::TextWrapped("Set-Seed requires all RNG hooks to be ready before starting.");
        if (!setupPractice && g_pShared)
        {
            auto version = suite_update::Version::Parse(g_pShared->version);
            if (!version || *version < suite_update::Version{{7, 22, 0}})
                ImGui::TextWrapped("Set-Seed speedruns require game mod 7.22.0 or newer. Update the mod and restart It Steals.");
        }
    }
}

inline void Settings()
{
    suite_ui::Section("Run controls", 0, [&]() {
        ImGui::TextWrapped(
            "Setup applies when you start a mode. Configure Quick restart / reset in Settings > Shortcuts (default T).");
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
            ImGui::TextWrapped("The permanent run archive and its derived statistics remain available.");
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
            ImGui::Text("%s: %d", g_pShared->seedKind == 2 ? "Selected base seed" : "Initial RNG seed",
                        g_pShared->runSeed);
        ImGui::TextDisabled("Reseed hooks: %s",
                            g_pShared->rngPatchComplete ? "22 / 22 ready" : "unavailable");
        RenderVerification(*g_pShared);
    }
}

inline void RefreshDashboard()
{
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
}

inline void RestartButton();

// Small/high-DPI windows keep mode, category and actions together in a
// scrolling native panel instead of squeezing the two-column workspace.
inline void RenderCompact()
{
    ImGui::BeginChild("SpeedrunCompactScroll", {0, 0});
    float startWidth = ImGui::CalcTextSize("Start").x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::SetNextItemWidth(std::max(80 * suite_ui::Unit(), ImGui::GetContentRegionAvail().x -
        startWidth - ImGui::GetStyle().ItemSpacing.x));
    ImGui::Combo("##SelectedMode", &dashboard.selected, kMapLevelNames, kMapLevelCount);
    Help("Select a game mode. This only changes the selection; Start mode loads it.");
    ImGui::SameLine();
    ImGui::BeginDisabled(!CanStart());
    if (suite_ui::PrimaryButton("Start"))
        StartSelected();
    ImGui::EndDisabled();
    CategorySetup();
    SameLineIfFits("Details");
    if (suite_ui::Button("Details"))
        ImGui::OpenPopup("Mode statistics");
    ImGui::BeginDisabled(!g_pShared || HasPendingRequest());
    if (g_speedrunMode)
    {
        SameLineIfFits("Stop mode");
        if (suite_ui::Button("Stop mode"))
            SetSpeedrunMode(false);
    }
    ImGui::EndDisabled();
    const auto restartLabel = suite_keys::Label("Quick restart", suite_keys::Restart);
    SameLineIfFits(restartLabel.c_str());
    RestartButton();
    const char *filters[] = {"Overview", dashboard.snapshot.statsCategory == 1 ? "Set-Seed" : "Random Seed", "Practice"};
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##StatisticsCategory", &viewFilter, filters, 3);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Overview includes normal play, Practice, and the displayed clean category. Random Seed and Set-Seed records are tracked separately.");
    RefreshDashboard();
    const auto &mode = dashboard.modes[std::clamp(dashboard.selected, 0, kMapLevelCount - 1)];
    ImGui::TextWrapped("%s %s: %s", SetupBestCategory(), setupPractice ? "best (unranked)" : "PB",
                        SetupBestLabel(dashboard.selected).c_str());
    Settings();
    Details();
    ImGui::EndChild();
}

inline bool CanRestart()
{
    auto version = g_pShared ? suite_update::Version::Parse(g_pShared->version) : std::nullopt;
    return version && *version >= suite_update::Version{{7, 21, 0}} && g_speedrunMode &&
           g_pShared->speedrunModeActive && (!g_commands.fullGameMode || suite_full::Active(*g_pShared)) &&
           !(g_pShared->replay3dState >= 1 && g_pShared->replay3dState <= 3) &&
           !HasPendingRequest() && !g_commands.runReviewRequest;
}

inline void RestartButton()
{
    const auto label = suite_keys::Label("Quick restart", suite_keys::Restart);
    ImGui::BeginDisabled(!CanRestart());
    if (suite_ui::Button(label.c_str()) && CanRestart())
        g_commands.speedrunLoadLevelCmd = 1;
    ImGui::EndDisabled();
    Help("Restart the active mode with its current run setup. Available with game mod 7.21.0 or newer after the previous command completes.");
}

inline void StatisticsFilter()
{
    const char *filters[] = {"Overview", dashboard.snapshot.statsCategory == 1 ? "Set-Seed" : "Random Seed", "Practice"};
    for (int i = 0; i < 3; i++)
    {
        if (i)
            SameLineIfFits(filters[i]);
        bool selected = viewFilter == i;
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, suite_ui::Alpha(g_headerCol, .25f));
        if (suite_ui::Button(filters[i]))
            viewFilter = i;
        if (!i && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Overview includes normal play, Practice and the displayed clean category. Random Seed and Set-Seed records remain separate.");
        if (selected)
            ImGui::PopStyleColor();
    }
    RefreshDashboard();
    if (viewFilter == 2)
        ImGui::TextDisabled("Practice statistics");
    else
        ImGui::TextDisabled(viewFilter == 1 ? "%s speedruns" : "%s + normal / Practice",
                           dashboard.snapshot.statsCategory == 1 ? "Set-Seed" : "Random Seed");
}

inline void SectionHeading(const char *label)
{
    ImGui::PushStyleColor(ImGuiCol_Text, g_headerCol);
    ImGui::SeparatorText(label);
    ImGui::PopStyleColor();
}

inline void SelectionOverview()
{
    const auto &mode = dashboard.modes[dashboard.selected];
    const char *cleanCategory = SetupBestCategory();
    int latest = -1;
    for (int i = 0; i < dashboard.snapshot.historyCount; i++)
        if (ReadHistory(dashboard.snapshot, i).scene == kMapScenes[dashboard.selected])
        {
            latest = i;
            break;
        }
    if (ImGui::BeginTable("Selected mode times", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();
        ImGui::TextDisabled(setupPractice ? "%s BEST / UNRANKED" : "%s PERSONAL BEST", cleanCategory);
        const auto drawValue = [&](const std::string &text, bool available) {
            float u = suite_ui::Unit();
            auto at = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            float baseWidth = ImGui::CalcTextSize(text.c_str()).x;
            float font = std::min(28 * u, ImGui::GetFontSize() * width / std::max(1.f, baseWidth));
            auto *draw = ImGui::GetWindowDrawList();
            auto *fontFace = g_timerFont ? g_timerFont : ImGui::GetFont();
            float actualWidth = fontFace->CalcTextSizeA(font, FLT_MAX, 0, text.c_str()).x;
            font *= std::min(1.f, width / std::max(1.f, actualWidth));
            ImVec4 clip(at.x, at.y, at.x + width, at.y + 36 * u);
            draw->AddText(fontFace, font, at,
                ImGui::GetColorU32(available ? g_textCol : suite_ui::Muted()), text.c_str(), nullptr, 0, &clip);
            ImGui::Dummy({width, 36 * u});
        };
        drawValue(SetupBestLabel(dashboard.selected), SetupBest(dashboard.selected) > 0);
        ImGui::TableNextColumn();
        if (latest >= 0)
        {
            const auto run = ReadHistory(dashboard.snapshot, latest);
            ImGui::TextDisabled("LAST ATTEMPT / %s", run.CategoryLabel());
            drawValue(FormatRunTime(run.seconds), true);
        }
        else
        {
            ImGui::TextDisabled("LAST ATTEMPT");
            drawValue("--:--.---", false);
        }
        ImGui::EndTable();
    }
    if (ImGui::BeginTable("Selected mode totals", 3, ImGuiTableFlags_SizingStretchSame))
    {
        const char *labels[] = {"Attempts", "Finishes", "Time played"};
        const std::string *values[] = {&mode.attempts, &mode.finished, &mode.timeShort};
        for (int i = 0; i < 3; i++)
        {
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", labels[i]);
            ImGui::TextUnformatted(values[i]->c_str());
        }
        ImGui::EndTable();
    }
    Help("Personal bests show the clean category in the latest game snapshot, independently of the draft category above. Attempts, finishes and playtime use the selected statistics filter.");
}

inline void RecentAttempts()
{
    int recent = 0;
    if (ImGui::BeginTable("Recent selected attempts", 3,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                              ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableSetupColumn("Category");
        ImGui::TableSetupColumn("Time");
        ImGui::TableSetupColumn("Outcome");
        ImGui::TableHeadersRow();
        for (int i = 0; i < dashboard.snapshot.historyCount && recent < 4; i++)
        {
            const auto run = ReadHistory(dashboard.snapshot, i);
            if (run.scene != kMapScenes[dashboard.selected])
                continue;
            const char *outcomes[] = {"Running", "Finished", "Reset", "Death", "Interrupted", "Stopped"};
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(run.CategoryLabel());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(FormatRunTime(run.seconds).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(outcomes[std::min(int(run.outcome), 5)]);
            recent++;
        }
        ImGui::EndTable();
    }
    if (!recent)
        ImGui::TextDisabled("No recent attempts retained for this mode.");
}

inline void Render()
{
    float u = suite_ui::Unit();
    if(suite_full::preferences.mode){suite_full::Setup();return;}
    SyncSetupSelection();
    if (ImGui::GetContentRegionAvail().y < 300 * u || ImGui::GetContentRegionAvail().x < 650 * u)
    {
        RenderCompact();
        return;
    }
    const char *startLabel = "Start selected mode";
    const char *disableLabel = "Disable Speedrun";
    auto buttonWidth = [](const char *label) {
        return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
    };
    const auto restartLabel = suite_keys::Label("Quick restart", suite_keys::Restart);
    float footerWidth = ImGui::GetContentRegionAvail().x;
    float used = 0;
    int rows = 1;
    for (const char *label : {startLabel, "Details", restartLabel.c_str(), g_speedrunMode ? disableLabel : ""})
    {
        if (!label[0])
            continue;
        float width = buttonWidth(label);
        if (used && used + ImGui::GetStyle().ItemSpacing.x + width > footerWidth)
        {
            rows++;
            used = width;
        }
        else
            used += (used ? ImGui::GetStyle().ItemSpacing.x : 0) + width;
    }
    float footer = ImGui::GetTextLineHeightWithSpacing() + 8 * u +
                   ImGui::GetFrameHeightWithSpacing() * rows;
    ImGui::BeginChild("SpeedrunScroll", {0, -footer});
    RefreshDashboard();
    if (ImGui::BeginTable("Run setup columns", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Mode selection", ImGuiTableColumnFlags_WidthFixed,
                                std::clamp(ImGui::GetContentRegionAvail().x * .25f, 190 * u, 245 * u));
        ImGui::TableSetupColumn("Run configuration");
        ImGui::TableNextColumn();
        SectionHeading("Game mode");
        if (ImGui::BeginTable("Mode cards", 1, ImGuiTableFlags_SizingStretchSame))
        {
            for (int i = 0; i < kMapLevelCount; i++)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                Card(i, ImGui::GetContentRegionAvail().x);
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
        ImGui::TextDisabled("Select, then Start.");
        ImGui::TextWrapped("Browsing a mode does not change the active game.");
        ImGui::TableNextColumn();
        SectionHeading("Run setup");
        CategorySetup();
        ImGui::Spacing();
        SectionHeading("Mode overview");
        StatisticsFilter();
        ImGui::Spacing();
        SelectionOverview();
        ImGui::Spacing();
        SectionHeading("Recent attempts / all categories");
        if (suite_ui::CollapsingHeader("Show recent attempts")) RecentAttempts();
        ImGui::Spacing();
        ImGui::TextDisabled("%s", dashboard.sinceLabel.c_str());
        if (!g_pShared)
            ImGui::TextWrapped(dashboard.available ? "Game disconnected / showing the last received statistics."
                                                   : "Connect It Steals to load your saved statistics.");
        Settings();
        if (g_pShared && g_pShared->statsSaveStatus == 2)
            ImGui::TextColored(g_accentCol, "Statistics could not be saved: %s", g_pShared->statsError);
        if (g_pShared && g_pShared->statsWarning[0])
            ImGui::TextWrapped("Data recovery: %s", g_pShared->statsWarning);
        ImGui::EndTable();
    }
    ImGui::EndChild();
    ImGui::Separator();
    std::string selected = std::string(kMapLevelNames[dashboard.selected]) + "  /  " + SetupCategoryName();
    auto at = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float textWidth = ImGui::CalcTextSize(selected.c_str()).x;
    float fontSize = ImGui::GetFontSize() * std::min(1.f, width / std::max(1.f, textWidth));
    suite_ui::TextAt(ImGui::GetWindowDrawList(), at, selected.c_str(), suite_ui::ink, fontSize,
                     {at.x, at.y, at.x + width, at.y + ImGui::GetTextLineHeight()});
    ImGui::Dummy({width, ImGui::GetTextLineHeight()});
    ImGui::BeginDisabled(!CanStart());
    if (suite_ui::PrimaryButton(startLabel))
        StartSelected();
    ImGui::EndDisabled();
    Help("Start the selected mode with the run setup above. Browsing a mode does not load the game.");
    SameLineIfFits("Details");
    if (suite_ui::Button("Details"))
        ImGui::OpenPopup("Mode statistics");
    SameLineIfFits(restartLabel.c_str());
    RestartButton();
    ImGui::BeginDisabled(!g_pShared || HasPendingRequest());
    if (g_speedrunMode)
    {
        SameLineIfFits(disableLabel);
        if (suite_ui::Button(disableLabel))
            SetSpeedrunMode(false);
    }
    ImGui::EndDisabled();
    Details();
}
} // namespace run_stats
