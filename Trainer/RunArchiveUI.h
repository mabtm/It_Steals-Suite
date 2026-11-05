#pragma once
#include "RunDataService.h"

namespace suite_runs
{
inline int page = 0, range = 0;
inline Query query;
inline char fromDate[11]{}, throughDate[11]{};

inline bool DateTicks(const char *text, int64_t &ticks, bool end = false)
{
    int y = 0, m = 0, d = 0;
    auto digits = [&](int start, int count, int &value) {
        value = 0;
        for (int i = start; i < start + count; i++)
        {
            if (text[i] < '0' || text[i] > '9') return false;
            value = value * 10 + text[i] - '0';
        }
        return true;
    };
    if (strlen(text) != 10 || text[4] != '-' || text[7] != '-' ||
        !digits(0, 4, y) || !digits(5, 2, m) || !digits(8, 2, d) ||
        y < 1970 || y > 9998 || m < 1 || m > 12 || d < 1 || d > 31)
        return false;
    std::tm local{}; local.tm_year = y - 1900; local.tm_mon = m - 1; local.tm_mday = d;
    local.tm_isdst = -1;
    auto time = std::mktime(&local);
    if (time < 0 || local.tm_year != y - 1900 || local.tm_mon != m - 1 || local.tm_mday != d) return false;
    if (end) { local.tm_mday++; local.tm_isdst = -1; time = std::mktime(&local); }
    ticks = 621355968000000000LL + int64_t(time) * 10000000 - (end ? 1 : 0);
    return true;
}

inline void Filters()
{
    const float u = suite_ui::Unit();
    const bool compact = ImGui::GetContentRegionAvail().x < 620 * u || ImGui::GetContentRegionAvail().y < 260 * u;
    auto scope = [&] {
    ImGui::SetNextItemWidth(std::min(225 * u, ImGui::GetContentRegionAvail().x));
    if (ImGui::BeginCombo("##Archive mode", query.level < 0 ? "All modes" : kMapLevelNames[query.level]))
    {
        for (int i = -1; i < kMapLevelCount; i++)
            if (ImGui::Selectable(i < 0 ? "All modes" : kMapLevelNames[i], query.level == i)) query.level = i;
        ImGui::EndCombo();
    }
    SameLineIfFits("Random Seed + Set-Seed");
    ImGui::SetNextItemWidth(std::min(235 * u, ImGui::GetContentRegionAvail().x));
    const char *label = query.category < 0 ? "All categories" : CategoryName(query.category);
    if (ImGui::BeginCombo("##Archive category", label))
    {
        for (int i = -1; i < 3; i++)
            if (ImGui::Selectable(i < 0 ? "All categories" : CategoryName(i), query.category == i)) query.category = i;
        ImGui::EndCombo();
    }
    Help("Browsing records does not change the category of your next run. Practice is unranked; PBs and Top 5 use valid Random Seed and Set-Seed finishes.");
    };
    if (!compact) scope();
    SameLineIfFits("Filters");
    if (suite_ui::Button("Filters")) ImGui::OpenPopup("Run archive filters");
    ImGui::SetNextWindowSize({std::min(420 * u, ImGui::GetIO().DisplaySize.x - 30), 0}, ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({0, 0}, {ImGui::GetIO().DisplaySize.x - 30, ImGui::GetIO().DisplaySize.y - 30});
    if (ImGui::BeginPopup("Run archive filters"))
    {
        if (compact) scope();
        const char *ranges[] = {"All recorded", "Current game session", "Today", "Last 7 days", "Date range"};
        ImGui::SetNextItemWidth(-1); ImGui::Combo("##Archive range", &range, ranges, 5);
        if (range == 4)
        {
            ImGui::SetNextItemWidth(135 * u); ImGui::InputTextWithHint("From", "YYYY-MM-DD", fromDate, sizeof(fromDate));
            ImGui::SetNextItemWidth(135 * u); ImGui::InputTextWithHint("Through", "YYYY-MM-DD", throughDate, sizeof(throughDate));
        }
        suite_ui::Checkbox("Exact seed", &query.seedFilter);
        if (query.seedFilter) { ImGui::SetNextItemWidth(180 * u); ImGui::InputInt("Seed", &query.seed); }
        const char *recipes[] = {"Any recipe", "Practice base seed", "Captured spawn seed"};
        int recipe = query.recipe + 1;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##Archive recipe", &recipe, recipes, 3)) query.recipe = recipe - 1;
        if (page == 3)
        {
            const char *statuses[] = {"All outcomes", "Valid finishes", "Unranked finishes", "Resets / deaths / stopped"};
            ImGui::SetNextItemWidth(-1); ImGui::Combo("##Archive status", &query.status, statuses, 4);
            suite_ui::Checkbox("PB improvements only", &query.pbOnly);
        }
        if (suite_ui::Button("Clear filters")) { query = {}; range = 0; fromDate[0] = throughDate[0] = 0; }
        ImGui::EndPopup();
    }
    SameLineIfFits("Refresh");
    if (suite_ui::Button("Refresh")) { service.Refresh(); practice::g_replayArchive.Refresh(); }
    if (compact) ImGui::TextWrapped("%s / %s", query.level < 0 ? "All modes" : kMapLevelNames[query.level],
        query.category < 0 ? "All categories" : CategoryName(query.category));
}

inline bool ApplyRange(Query &q, const Store &source)
{
    q.from = q.session = 0; q.through = MaxTicks;
    if (range == 1) { q.session = source.session ? source.session : -1; }
    else if (range == 4)
    {
        if (!DateTicks(fromDate, q.from) || !DateTicks(throughDate, q.through, true) || q.from > q.through)
            return false;
    }
    else if (range >= 2)
    {
        auto time = std::time(nullptr); std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &time);
#else
        localtime_r(&time, &local);
#endif
        local.tm_hour = local.tm_min = local.tm_sec = 0;
        if (range == 3) local.tm_mday -= 6;
        local.tm_isdst = -1;
        q.from = 621355968000000000LL + int64_t(std::mktime(&local)) * 10000000;
    }
    return true;
}

inline const practice::ReplayInfo *FindReplay(const practice::ReplayArchiveState &archive, int64_t id)
{
    auto it = archive.byId.find(id);
    return it == archive.byId.end() ? nullptr : &archive.runs[it->second];
}

inline void Records(const View &view)
{
    ImGui::TextDisabled("%zu %s", view.rows.size(), page == 1 ? "personal bests" : page == 2 ? "ranked finishes" : "attempts");
    if (page == 2) Help("Top 5 local valid finishes per mode and category, including tied times. Exact seed and recipe filters create a narrower leaderboard. Only archived run evidence can be ranked.");
    if (view.rows.empty())
    {
        ImGui::TextWrapped("No runs match this view. Finish a run or change the filters.");
        return;
    }
    auto replays = practice::g_replayArchive.Get();
    float u = suite_ui::Unit();
    if (!ImGui::BeginTable("Run records", 8, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable,
            {0, std::max(1.f, ImGui::GetContentRegionAvail().y)}, 1090 * u)) return;
    ImGui::TableSetupColumn(page == 2 ? "Rank" : "PB", ImGuiTableColumnFlags_NoSort, 42 * u);
    ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_DefaultSort, 155 * u, 2);
    ImGui::TableSetupColumn("Category", 0, 120 * u, 3);
    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_PreferSortAscending, 110 * u, 4);
    ImGui::TableSetupColumn("Recorded", ImGuiTableColumnFlags_PreferSortDescending, 155 * u, 1);
    ImGui::TableSetupColumn("Seed", 0, 120 * u, 5);
    ImGui::TableSetupColumn("Outcome", 0, 125 * u, 6);
    ImGui::TableSetupColumn("Replay", ImGuiTableColumnFlags_NoSort, 110 * u);
    ImGui::TableSetupScrollFreeze(0, 1); ImGui::TableHeadersRow();
    if (auto sort = ImGui::TableGetSortSpecs(); sort && sort->SpecsDirty && sort->SpecsCount)
    {
        query.sort = Sort(sort->Specs[0].ColumnUserID - 1);
        query.ascending = sort->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
        sort->SpecsDirty = false;
    }
    ImGuiListClipper clip; clip.Begin(int(view.rows.size()));
    while (clip.Step()) for (int i = clip.DisplayStart; i < clip.DisplayEnd; i++)
    {
        const auto &r = view.rows[i];
        ImGui::PushID(i); ImGui::TableNextRow(); ImGui::TableNextColumn();
        if (page == 2)
            ImGui::TextColored(view.ranks[i] == 1 ? g_headerCol : g_textCol, "#%d", view.ranks[i]);
        else ImGui::TextDisabled("%s", r.pb ? "PB" : "");
        ImGui::TableNextColumn(); ImGui::TextUnformatted(kMapLevelNames[MapLevelIndex(r.scene)]);
        ImGui::TableNextColumn(); ImGui::TextUnformatted(CategoryName(r.category));
        ImGui::TableNextColumn(); ImGui::TextColored(r.Ranked() || !r.id ? g_headerCol : g_textCol, "%s", FormatRunTime(r.seconds).c_str());
        ImGui::TableNextColumn(); ImGui::TextUnformatted(r.started ? practice::ReplayRecordedAt(r.started).c_str() : "Unknown");
        ImGui::TableNextColumn();
        if (r.seedKnown) { ImGui::Text("%d%s", r.category == 1 ? r.selectedSeed : r.seed, r.raw ? " / spawn" : ""); }
        else ImGui::TextDisabled("Unknown");
        ImGui::TableNextColumn();
        const char *outcome = !r.id ? "Legacy PB" : r.quick ? "Quick reset" : r.outcome == 1 ? (r.eligible ? "Valid finish" : "Unranked finish") :
            r.outcome == 2 ? "Reset" : r.outcome == 3 ? "Death" : r.outcome == 4 ? "Interrupted" : "Stopped";
        ImGui::TextUnformatted(outcome);
        ImGui::TableNextColumn();
        if (auto replay = FindReplay(*replays, r.id); replay && r.id)
        {
            ImGui::BeginDisabled(HasPendingRequest());
            if (suite_ui::SmallButton("Open")) OpenSavedReplay(*replay);
            ImGui::EndDisabled();
            Help("Open the recording linked to this exact run identity.");
            if (ImGui::BeginPopupContextItem("Run actions"))
            {
                if (ImGui::MenuItem("Use as split reference")) practice::g_replayArchive.SetReference(r.id);
                ImGui::EndPopup();
            }
        }
        else
        {
            ImGui::TextDisabled("%s", !r.id ? "Legacy PB" : "Unavailable");
            Help(!r.id ? "This older PB has no retained run identity or seed evidence. Its original time and date are preserved." :
                "No committed replay for this run is present in the current archive. It may be saving, outside the retention rule, or unavailable after a storage error.");
        }
        ImGui::PopID();
    }
    ImGui::EndTable();
}

inline void Statistics(const View &view)
{
    float u = suite_ui::Unit();
    ImGui::BeginChild("Statistics scroll", {0, 0});
    if (ImGui::BeginTable("Archived statistics", 10, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
            ImGuiTableFlags_SizingFixedFit, {0, 0}, 1320 * u))
    {
        const char *columns[] = {"Mode / category", "Attempts", "Finished", "Valid", "Unranked", "Resets", "Deaths", "Quick resets", "Mean finish", "Median finish"};
        for (auto c : columns) ImGui::TableSetupColumn(c);
        ImGui::TableHeadersRow();
        for (int i = 0; i < 24; i++)
        {
            if (query.level >= 0 && query.level != i / 3 || query.category >= 0 && query.category != i % 3) continue;
            const auto &t = view.totals[i];
            ImGui::TableNextRow(); ImGui::TableNextColumn();
            ImGui::Text("%s / %s", kMapLevelNames[i / 3], CategoryName(i % 3));
            for (auto count : {t.attempts, t.finished, t.valid, t.invalid, t.resets, t.deaths, t.quick})
            { ImGui::TableNextColumn(); ImGui::Text("%lld", (long long)count); }
            ImGui::TableNextColumn(); ImGui::TextUnformatted(t.finished ? FormatRunTime(t.finishSeconds / t.finished).c_str() : "--");
            ImGui::TableNextColumn(); ImGui::TextUnformatted(t.finished ? FormatRunTime(t.median).c_str() : "--");
            Help(("Attempt duration: " + FormatRunTime(t.attemptSeconds) + "; interrupted/stopped: " + std::to_string(t.interrupted) +
                ". Quick resets are listed separately, outside attempts. Deaths are included in resets. Valid denotes local suite checks.").c_str());
        }
        ImGui::EndTable();
    }
    if (view.source->legacy.available && suite_ui::CollapsingHeader("Original lifetime counters"))
    {
        ImGui::TextWrapped("Preserved counters since %s, independent of the archive date/seed filters. These include normal playtime and runs predating detailed archiving.",
            practice::ReplayRecordedAt(view.source->legacy.since).c_str());
        if (ImGui::BeginTable("Lifetime modes", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
                ImGuiTableFlags_SizingFixedFit, {0, 0}, 1000 * u))
        {
            for (auto c : {"Mode / category", "Attempts", "Finished", "Resets", "Deaths", "Playtime", "Mean finish"}) ImGui::TableSetupColumn(c);
            ImGui::TableHeadersRow();
            for (int mode = 0; mode < 8; mode++) for (int cat = 0; cat < 4; cat++)
            {
                if (query.level >= 0 && query.level != mode || query.category >= 0 && cat != (query.category == 1 ? 3 : query.category == 0 ? 1 : 2)) continue;
                const auto &c = view.source->legacy.modes[cat == 3 ? 24 + mode : mode * 3 + cat];
                ImGui::TableNextRow(); ImGui::TableNextColumn();
                ImGui::Text("%s / %s", kMapLevelNames[mode], cat == 0 ? "Normal" : cat == 1 ? "Random Seed" : cat == 2 ? "Practice" : "Set-Seed");
                for (auto n : {c.attempts, c.finished, c.resets, c.deaths}) { ImGui::TableNextColumn(); ImGui::Text("%lld", (long long)n); }
                ImGui::TableNextColumn(); ImGui::TextUnformatted(FormatRunTime(c.playSeconds).c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(c.finished ? FormatRunTime(c.finishSeconds / c.finished).c_str() : "--");
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

inline void Archive()
{
    // A run's journal entry can arrive before its larger replay file commits.
    // Follow completed recordings while this view is open, using the existing
    // cached IO worker rather than waiting for a manual visit to Replays.
    static double nextReplayRefresh = 0;
    double now = ImGui::GetTime();
    if (now >= nextReplayRefresh || now < nextReplayRefresh - 5)
    { nextReplayRefresh = now + 5; practice::g_replayArchive.Refresh(); }
    Filters();
    auto view = service.Get();
    Query selected = query;
    selected.display = page == 1 ? Display::Bests : page == 2 ? Display::TopFive : page == 3 ? Display::History : Display::Statistics;
    if (page != 3) { selected.pbOnly = false; selected.status = 0; }
    Store empty;
    if (!ApplyRange(selected, view->source ? *view->source : empty))
    { ImGui::TextWrapped("Enter a valid date range as YYYY-MM-DD."); return; }
    auto request = service.Submit(selected);
    if (!view->source || view->request != request || !(view->query == selected))
    { suite_ui::LoadingState("Run archive", "Reading records"); return; }
    if (!view->source->error.empty())
    { ImGui::TextColored(g_accentCol, "Archive warning"); Help(view->source->error.c_str()); }
    if (g_pShared && (g_pShared->statsSaveStatus == 2 || g_pShared->reportSaveStatus == 2))
    {
        ImGui::TextColored(g_accentCol, "Storage warning");
        Help(g_pShared->statsSaveStatus == 2 ? g_pShared->statsError : g_pShared->reportError);
    }
    else if (g_pShared && (g_pShared->statsSaveStatus == 1 || g_pShared->reportSaveStatus == 1))
        ImGui::TextDisabled("Saving run data / recordings...");
    if (view->source->detailedSince)
    { ImGui::TextDisabled("Detailed records since %s", practice::ReplayRecordedAt(view->source->detailedSince).c_str()); }
    if (view->source->imported)
        Help("Earlier retained attempts were imported. Older counters and PBs are preserved; missing historical attempts cannot be reconstructed.");
    if (page == 4) Statistics(*view); else Records(*view);
}

inline void Render()
{
    const char *pages[] = {"Setup", "PBs", "Top 5", "History", "Statistics"};
    // Tiny windows keep the Start control visible; use one compact native selector.
    if (ImGui::GetContentRegionAvail().y < 230 * suite_ui::Unit())
    {
        ImGui::SetNextItemWidth(-1); ImGui::Combo("##Run views", &page, pages, 5);
        if (page == 0) run_stats::Render(); else Archive();
        return;
    }
    if (ImGui::BeginTabBar("Run workspace", ImGuiTabBarFlags_FittingPolicyScroll))
    {
        for (int i = 0; i < 5; i++)
            if (suite_ui::BeginTabItem(pages[i], nullptr, ImGuiTabItemFlags_NoPushId))
            {
                page = i;
                if (!i) run_stats::Render(); else Archive();
                suite_ui::EndTabItem();
            }
        ImGui::EndTabBar();
    }
}
}
