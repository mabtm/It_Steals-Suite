#pragma once
#include "SuiteExperience.h"
#include "SuiteDesign.h"
#include "RoutePlayback.h"
#include "PracticeMapWorker.h"
#include "SeedSearch.h"
#include "RouteDrawing.h"

inline bool g_selectSeedPreview = false;
inline void RenderSpawnPreview(const practice::MapAnalysis &source);

inline const practice::TimingEstimate &PreviewMovementTiming(
    std::shared_ptr<const practice::MapAnalysis> analysis, bool collected = false)
{
    struct Cache
    {
        std::shared_ptr<const practice::MapAnalysis> map;
        std::shared_ptr<const practice::MovementData> source;
        practice::TimingEstimate estimate;
    };

    static std::array<Cache, kMapLevelCount * 2> cached;
    static const practice::TimingEstimate missing;
    if (!analysis || !analysis->data)
        return missing;
    int level = MapLevelIndex(analysis->data->scene);
    if (level < 0)
        return missing;
    auto source = practice::MovementFor(*analysis->data);
    auto &cache = cached[level * 2 + int(collected)];
    if (cache.map != analysis || cache.source != source)
    {
        cache.map = analysis;
        cache.source = source;
        cache.estimate = {};
        if (source)
        {
            const auto &route = collected ? analysis->collectedRoute : analysis->route;
            cache.estimate = practice::EstimateMovementRoute(*source, *analysis->data, route.line,
                                                             route.complete && !route.estimated);
        }
    }
    return cache.estimate;
}

inline void PlannerRequest(uint32_t action)
{
    if (action == 1 || action == 2)
        g_seedSearch.HidePreview();
    g_plannerOptions.Validate();
    g_commands.plannerRequest = action;
    g_commands.plannerSeed = g_plannerOptions.seed;
    g_commands.plannerLevel = g_plannerOptions.level;
    g_commands.plannerSearchCount = practice::SeedSpace;
    g_commands.plannerBudgetMicros = static_cast<uint32_t>(g_plannerOptions.generationMs * 1000);
}

inline std::shared_ptr<const practice::MapAnalysis> DisplayedPlanner(int level)
{
    auto base = g_mapWorker.Get(level), preview = g_seedSearch.Pinned();
    if (!preview)
        preview = g_seedSearch.Get()->preview;
    if (base && base->data && preview && preview->data && base->data->Verified() &&
        preview->data->scene == base->data->scene && preview->data->epoch == base->data->epoch &&
        preview->data->pid == base->data->pid && preview->session == base->session &&
        preview->data->hash == base->data->hash)
        return preview;
    return base;
}

inline bool RandomPlannerPreview()
{
    int seed;
    if (!GeneratePlannerSeed(seed, g_plannerRandomError))
        return false;
    g_seedSearch.Stop();
    g_seedSearch.HidePreview();
    g_plannerOptions.seed = seed;
    PlannerRequest(1);
    return true;
}

inline std::array<bool, kMapLevelCount> g_searchFilterReset{};

inline void BindPlannerFilters(const practice::MapAnalysis &source)
{
    int level = MapLevelIndex(source.data->scene);
    if (level < 0)
        return;
    static std::array<std::weak_ptr<const practice::MapData>, kMapLevelCount> previous;
    if (previous[level].lock() == source.data)
        return;
    previous[level] = source.data;
    if (g_plannerOptions.filters[level].Bind(practice::CatalogKey(*source.data)))
        g_searchFilterReset[level] = true;
}

inline void RenderSearchFilters(std::shared_ptr<const practice::MapAnalysis> source, bool busy)
{
    ImGui::BeginDisabled(busy || !source || !source->data);
    auto &filter = g_plannerOptions.filters[g_plannerOptions.level];
    std::string current =
        filter.spawn < 0 ? "Any player spawn" : "Spawn #" + std::to_string(filter.spawn + 1);
    if (ImGui::BeginCombo("Spawn filter", current.c_str()))
    {
        if (ImGui::Selectable("Any player spawn", filter.spawn < 0))
        {
            filter.spawn = -1;
            g_searchFilterReset[g_plannerOptions.level] = false;
        }
        if (source && source->data)
            for (size_t i = 0; i < source->data->spawns.size(); i++)
            {
                auto p = source->data->spawns[i];
                char label[80];
                snprintf(label, sizeof(label), "Spawn #%zu (X %.1f, Z %.1f)", i + 1, p.x, p.z);
                if (ImGui::Selectable(label, filter.spawn == static_cast<int>(i)))
                {
                    filter.spawn = static_cast<int>(i);
                    g_searchFilterReset[g_plannerOptions.level] = false;
                }
            }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    if (source && source->data && filter.spawn >= 0)
        RenderSpawnPreview(*source);
    Help(
        "Only layouts with this player spawn are accepted. Seeds are still sampled randomly across the full 32-bit space. The chosen spawn is saved separately per map; it is cleared if the captured catalog changes.");
}

inline bool BeginPlannerSearch(const practice::MapAnalysis &source)
{
    const auto &d = *source.data;
    if (MapLevelIndex(d.scene) != g_plannerOptions.level)
    {
        g_plannerRandomError = "Select the map matching this catalog first.";
        return false;
    }
    BindPlannerFilters(source);
    g_plannerOptions.Validate();
    practice::SeedSearchRequest request;
    g_plannerOptions.calculateRoutes = true;
    g_plannerOptions.searchCount = practice::SeedSpace;
    g_seedSearch.HidePreview();
    request.criteria = g_plannerOptions.Criteria();
    request.layoutOnly = false;
    request.allowPortable = g_plannerOptions.portableGeneration;
    auto filterError = request.criteria.CheckCatalog(d);
    if (!filterError.empty())
    {
        g_plannerRandomError = filterError;
        return false;
    }
    request.pid = d.pid;
    request.scene = d.scene;
    request.epoch = d.epoch;
    request.hash = d.hash;
    request.session = source.session;
    if (!GenerateSearchKey(request.key, g_plannerRandomError))
        return false;
    request.seed = practice::RandomSeedOrder(request.key).SeedAt(0);
    request.recipe = g_plannerOptions.recipe;
    request.count = practice::SeedSpace;
    request.fast = g_plannerOptions.searchFast;
    request.workers = g_plannerOptions.searchWorkers;
    request.serial = g_sentSerial + 1u;
    g_seedSearch.Begin(request);
    PlannerRequest(3);
    g_commands.plannerSearchKey = request.key;
    return true;
}

inline void SetPracticeSelection(int seed, int recipe, int level)
{
    g_plannerOptions.seed = seed;
    g_plannerOptions.recipe = recipe;
    g_plannerOptions.level = level;
    g_practiceSeed = seed;
    g_rawPracticeSpawn = recipe == 1;
    g_selectedSpeedrunLevel = level;
    g_speedrunMode = g_practiceMode = g_seededPractice = true;
    g_commands.practiceSeed = seed;
    g_commands.speedrunLevelIndex = static_cast<byte>(level);
    g_commands.speedrunModeActive = g_commands.practiceModeActive = g_commands.seededPractice = 1;
    g_commands.plannerOptions = (g_commands.plannerOptions & ~2u) | (g_rawPracticeSpawn ? 2u : 0u);
}

inline bool AdoptSearchResult(const practice::SeedSearchState &state, int rank)
{
    if (rank < 0 || rank >= static_cast<int>(state.top.size()) || !state.source || !state.source->data)
        return false;
    int level = MapLevelIndex(state.request.scene);
    if (level < 0)
        return false;
    g_seedSearch.Pin(*state.source, state.top[rank]);
    SetPracticeSelection(state.top[rank].seed, state.request.recipe, level);
    return true;
}

inline int KnownPracticeSeedType(int seed, int level)
{
    auto state = g_seedSearch.Get();
    if (state->source && state->source->live && state->source->data && state->source->data->Verified() &&
        state->request.session == g_session && MapLevelIndex(state->request.scene) == level)
        for (const auto &row : state->top)
            if (row.seed == seed)
                return state->request.recipe;
    auto analysis = DisplayedPlanner(level);
    if (analysis && analysis->live && analysis->session == g_session && analysis->data &&
        analysis->data->Verified() && analysis->data->seed == seed)
        return analysis->data->recipe ? analysis->data->recipe : analysis->data->liveRecipe;
    return 0;
}

inline void RenderSeedSearchControls(std::shared_ptr<const practice::MapAnalysis> source, bool verified,
                                     bool sampling)
{
    auto state = g_seedSearch.Get();
    bool busy = state->Busy();
    RenderSearchFilters(source, busy);
    if (g_searchFilterReset[g_plannerOptions.level])
        ImGui::TextWrapped("The catalog changed; the spawn filter was cleared.");
    if (suite_ui::CollapsingHeader("Search settings"))
    {
        ImGui::BeginDisabled(busy);
        const char *goals[] = {"Shortest complete route", "Longest route (practice)"};
        ImGui::Combo("Search goal", &g_plannerOptions.searchObjective, goals, 2);
        int method = g_plannerOptions.searchFast ? 1 : 0;
        const char *methods[] = {"Exact up to 15; heuristic above", "Fast heuristic"};
        if (ImGui::Combo("Accuracy", &method, methods, 2))
            g_plannerOptions.searchFast = method == 1;
        if (source && source->data && source->data->selected.size() > 15)
            ImGui::TextDisabled("%zu crystals: both modes use the same heuristic.",
                                source->data->selected.size());
        ImGui::SliderInt("CPU workers", &g_plannerOptions.searchWorkers, 1, SeedWorkerLimit());
        Help(
            "Verified CPU mode generates layouts and evaluates routes in these workers, below normal thread priority. Leave CPU capacity for the game. The original Unity fallback can be limited by game-thread generation, regardless of worker count.");
        suite_ui::Checkbox("Verified CPU generation", &g_plannerOptions.portableGeneration);
        Help(
            "Recommended. Runs generation outside the game only after this Unity runtime passes RNG and complete-layout comparisons. Otherwise the original Unity generator is used automatically.");
        bool cpu =
            source && source->data && source->data->rngStatus == 2 && g_plannerOptions.portableGeneration;
        if (!cpu)
        {
            suite_ui::Checkbox("Automatic game-thread load", &g_plannerOptions.automaticLoad);
            if (!g_plannerOptions.automaticLoad)
                ImGui::SliderFloat("Work per game frame", &g_plannerOptions.generationMs, .25f, 8.f,
                                   "%.2f ms");
            Help(
                "Only applies to the Unity fallback. Higher manual values can increase search speed and reduce game FPS; automatic mode limits that work. Verified CPU searches do not generate layouts on the game thread.");
        }
        ImGui::EndDisabled();
    }
    std::string filterError;
    if (source && source->data)
        filterError = g_plannerOptions.Criteria().CheckCatalog(*source->data);
    if (!filterError.empty())
        ImGui::TextColored(g_accentCol, "%s", filterError.c_str());
    ImGui::BeginDisabled(!verified || sampling || busy || !filterError.empty() || HasPendingRequest());
    if (suite_ui::Button("Search seeds"))
    {
        BeginPlannerSearch(*source);
    }
    ImGui::EndDisabled();
    if (busy)
    {
        SameLineIfFits("Stop seed search");
        ImGui::BeginDisabled(HasPendingRequest());
        if (suite_ui::Button("Stop seed search"))
        {
            PlannerRequest(4);
            g_seedSearch.Stop();
        }
        ImGui::EndDisabled();
    }
    if (source && source->data)
    {
        const auto &d = *source->data;
        if (d.rngStatus == 1)
            ImGui::TextDisabled("Verifying CPU generator: %d / 4160", d.rngProbes);
        else if (d.rngStatus == 2 && g_plannerOptions.portableGeneration)
        {
            ImGui::TextDisabled("Generator: verified CPU");
            Help("Layouts and route searches run in trainer workers, outside the game thread.");
        }
        else
            ImGui::TextDisabled("Generator: original Unity fallback");
    }
    if (!g_plannerRandomError.empty())
        ImGui::TextWrapped("%s", g_plannerRandomError.c_str());
    state = g_seedSearch.Get();
    busy = state->Busy();
    if (state->phase == practice::SearchPhase::Idle)
    {
        suite_ui::EmptyState(
            "Search ready", suite_ui::Icon::Path, "Find your next route",
            "Choose a spawn or keep every spawn eligible. Start a continuous search and stop whenever you like.",
            160);
        return;
    }
    const auto &r = state->request;
    ImGui::Separator();
    const char *phase = state->phase == practice::SearchPhase::Completed ? "Completed"
                        : state->phase == practice::SearchPhase::Stopped ? "Stopped"
                        : state->phase == practice::SearchPhase::Failed  ? "Failed"
                                                                         : "Searching";
    if (busy)
        suite_ui::Busy(phase);
    else
        ImGui::TextColored(g_headerCol, "%s", phase);
    float u = suite_ui::Unit(), available = ImGui::GetContentRegionAvail().x;
    auto bar = ImGui::GetCursorScreenPos();
    ImGui::Dummy({available, 4 * u});
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(
        bar, {bar.x + available, bar.y + 3 * u},
        ImGui::GetColorU32(suite_ui::Alpha(suite_ui::Color(AppearancePreferences::Outline), .32f)), 2 * u);
    if (busy)
    {
        float at =
            suite_ui::preferences.motion ? float(std::fmod(ImGui::GetTime() * .38, 1.)) * available : 0;
        float length = suite_ui::preferences.motion ? available * .22f : available;
        draw->AddRectFilled({bar.x + at, bar.y}, {bar.x + std::min(available, at + length), bar.y + 3 * u},
                            ImGui::GetColorU32(g_headerCol), 2 * u);
    }
    char checked[40], rate[40], best[40], elapsed[50];
    snprintf(checked, sizeof(checked), "%llu", static_cast<unsigned long long>(state->evaluated));
    snprintf(rate, sizeof(rate), "%.0f / s", state->recentChecked);
    snprintf(best, sizeof(best), state->top.empty() ? "--" : "%.2f u",
             state->top.empty() ? 0 : state->top.front().route.length);
    snprintf(elapsed, sizeof(elapsed), "%.1f seconds elapsed", state->elapsed);
    int metricColumns = available >= 500 * u ? 3 : available >= 310 * u ? 2 : 1;
    float cell = (available - ImGui::GetStyle().ItemSpacing.x * (metricColumns - 1)) / metricColumns;
    suite_ui::Metric("Checked", "CHECKED SEEDS", checked, elapsed, cell);
    if (metricColumns > 1)
        ImGui::SameLine();
    suite_ui::Metric("Rate", "CURRENT RATE", rate,
                     state->portable ? "Verified CPU workers" : "Original Unity generator", cell);
    if (metricColumns == 3)
        ImGui::SameLine();
    suite_ui::Metric("Best", "BEST ROUTE", best,
                     state->top.empty()               ? "Waiting for a result"
                     : state->top.front().route.exact ? "Exact visit order"
                                                      : "Heuristic visit order",
                     cell);
    if (state->phase == practice::SearchPhase::Preparing || state->phase == practice::SearchPhase::Generating)
    {
        char progress[80];
        snprintf(progress, sizeof(progress), "Preparing navigation: %d / %d", state->prepared,
                 state->prepareTotal);
        suite_ui::Busy(progress);
    }
    if (!state->error.empty())
        ImGui::TextWrapped("%s", state->error.c_str());
    bool sameCatalog = source && state->source && source->session == state->source->session &&
                       source->data->pid == state->source->data->pid &&
                       source->data->scene == state->source->data->scene &&
                       practice::CatalogKey(*source->data) == practice::CatalogKey(*state->source->data);
    if (!state->top.empty())
    {
        const auto &best = state->top.front();
        if (r.layoutOnly)
            ImGui::TextColored(g_headerCol, "Best layout: %d | spawn #%d", best.seed,
                               best.selection.spawn + 1);
        else
            ImGui::TextColored(best.route.estimated ? g_accentCol : g_headerCol,
                               "Best seed: %d | %.2f units | %s", best.seed, best.route.length,
                               best.route.estimated ? "partial capture"
                               : best.route.exact   ? "checked / exact order"
                                                    : "checked / heuristic order");
        ImGui::TextDisabled("%s", r.recipe == 1 ? "Captured spawn seed" : "Practice base seed");
        ImGui::BeginDisabled(!verified || !sameCatalog || HasPendingRequest());
        if (suite_ui::Button("Use best seed"))
        {
            AdoptSearchResult(*state, 0);
        }
        ImGui::EndDisabled();
        Help(
            "Copies the best result's map, seed and seed type into fixed-seed Practice, and displays that same result. This button does not reload the game.");
        SameLineIfFits("Copy best seed");
        if (suite_ui::Button("Copy best seed"))
            suite_ui::CopyText(std::to_string(best.seed).c_str());
        SameLineIfFits("Copy results (TSV)");
        if (suite_ui::Button("Copy results (TSV)"))
            suite_ui::CopyText(practice::SeedResultsText(*state).c_str());
        static suite_ui::ResultHighlights highlights;
        highlights.Observe(r.key, r.serial, r.session, state->top, ImGui::GetTime());
        float resultHeight = std::min(float(state->top.size() + 1) * 35 * u + 14 * u,
                                      std::max(150 * u, ImGui::GetContentRegionAvail().y - 40 * u));
        if (ImGui::BeginTable("BestSeedRoutes", 6,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX |
                                  ImGuiTableFlags_ScrollY,
                              {0, resultHeight}))
        {
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 25 * u);
            ImGui::TableSetupColumn("Seed", ImGuiTableColumnFlags_WidthFixed, 112 * u);
            ImGui::TableSetupColumn("Spawn", ImGuiTableColumnFlags_WidthFixed, 62 * u);
            ImGui::TableSetupColumn("Units", ImGuiTableColumnFlags_WidthFixed, 82 * u);
            ImGui::TableSetupColumn("Method", ImGuiTableColumnFlags_WidthFixed, 92 * u);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 70 * u);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < state->top.size(); i++)
            {
                const auto &row = state->top[i];
                ImGui::PushID(row.seed);
                ImGui::TableNextRow();
                float glow = highlights.Strength(row.seed, ImGui::GetTime(), suite_ui::preferences.motion);
                if (glow > 0)
                    ImGui::TableSetBgColor(
                        ImGuiTableBgTarget_RowBg0,
                        ImGui::GetColorU32(
                            suite_ui::Alpha(suite_ui::Color(AppearancePreferences::Animation), .16f * glow)));
                ImGui::TableNextColumn();
                ImGui::TextColored(i == 0 ? g_headerCol : g_textCol, "%zu", i + 1);
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%d", row.seed);
                ImGui::TableNextColumn();
                ImGui::Text("#%d", row.selection.spawn + 1);
                ImGui::TableNextColumn();
                if (r.layoutOnly)
                    ImGui::TextUnformatted("--");
                else
                    ImGui::Text("%.2f", row.route.length);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(r.layoutOnly          ? "Layout"
                                       : row.route.estimated ? "Partial"
                                       : row.route.exact     ? "Exact"
                                                             : "Checked");
                if (!r.layoutOnly)
                    Help(
                        row.route.estimated
                            ? "Capture is missing some connections. Only checked pieces are drawn."
                        : row.route.exact
                            ? "Checked route; exact order for the modeled distances up to 15 crystals."
                            : "Checked route; all-start nearest neighbor, 2-opt and relocation optimize the visit order. Large routes are heuristic, not a proven global optimum.");
                ImGui::TableNextColumn();
                ImGui::BeginDisabled(!sameCatalog || !verified || HasPendingRequest());
                if (suite_ui::SmallButton("Show"))
                {
                    g_seedSearch.Pin(*state->source, row);
                    g_plannerOptions.seed = row.seed;
                    g_plannerOptions.recipe = r.recipe;
                    g_selectSeedPreview = true;
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    else if (!busy)
        suite_ui::EmptyState("Search no results", suite_ui::Icon::Map, "No matching layouts",
                             "Try every spawn or inspect the navigation status in Diagnostics.");
    if (busy && state->top.empty())
        suite_ui::LoadingState("Search preparing", "Waiting for verified route results", 140);
    if (state->estimatedPairs)
    {
        ImGui::TextColored(g_accentCol, "Unverified center connections");
        Help(
            "Some center paths are missing. Actual pickup contacts can still have a fully verified route. Only a result marked Estimate uses unverified distances; those segments are hidden from the map.");
    }
    if (suite_ui::CollapsingHeader("Performance details"))
    {
        ImGui::TextDisabled("Navigation preparation: %.2f ms | %s", state->preparationMs,
                            state->distanceCacheReused ? "reused" : "new");
        ImGui::Text("Backend: %s | %d workers", state->portable ? "verified CPU" : "Unity game thread",
                    r.workers);
        ImGui::Text("Generated %llu | evaluated %llu | queued %zu",
                    static_cast<unsigned long long>(state->generated),
                    static_cast<unsigned long long>(state->evaluated), state->queued);
        ImGui::Text("Generation %.0f/s | evaluation %.0f/s", state->recentGenerated, state->recentChecked);
        ImGui::Text("Scored %llu | spawn filtered %llu | proven outside Top 10 %llu",
                    static_cast<unsigned long long>(state->scored),
                    static_cast<unsigned long long>(state->filtered),
                    static_cast<unsigned long long>(state->pruned));
        if (!state->portable && g_pShared)
            ImGui::Text("Game-thread work limit: %.2f ms/frame", g_pShared->plannerGenerationMs);
        ImGui::TextWrapped("Navigation: %s; missing center paths: %d", state->navigationModel.c_str(),
                           state->estimatedPairs);
        ImGui::Text("Reused checked connections: %d | local repairs: %d", state->recoveredPairs,
                    state->repairQueries);
        if (state->source)
        {
            ImGui::TextDisabled("Generator verification");
            Help(state->source->data->rngNote.c_str());
        }
        ImGui::TextDisabled("Worker performance");
        Help(
            "Scaling depends on route complexity, memory bandwidth and CPU capacity left for the game. Verified CPU mode removes game-thread generation and packet pacing limits.");
    }
    ImGui::TextDisabled("Random seeds across the full 32-bit space, with no repeats in this search.");
}

inline bool LivePlannerRoute(const practice::MapAnalysis &analysis)
{
    if (!g_pShared || !g_practiceMode || !g_speedrunMode || !g_plannerOptions.minimapRoute ||
        !analysis.live || !analysis.data || !analysis.route.complete)
        return false;
    const auto &d = *analysis.data;
    return (d.flags & 8) && analysis.session == g_session && d.pid == g_pShared->pid &&
           d.scene == g_pShared->currentSceneIndex && d.epoch == static_cast<int>(g_pShared->sceneEpoch) &&
           g_pShared->playerReady && g_pShared->practiceModeActive && g_pShared->speedrunModeActive &&
           SnapshotAge() < .5;
}

struct SeedCanvasPlayback
{
    const practice::PlaybackSlice *played = nullptr;
    const practice::PlaybackSlice *suggested = nullptr;
    bool showOrder = true;
    const std::vector<practice::MonsterPlayback> *monsters = nullptr;
    const practice::RunReview *review = nullptr;
    double seconds = 0;
    bool flashes = false;
    const practice::RouteResult *route = nullptr;
    bool spawnPreview = false;
    const practice::PlaybackSlice *routePulse = nullptr;
};

inline void RenderSeedCanvas(const practice::MapAnalysis &analysis, float requestedHeight = 0,
                             const SeedCanvasPlayback *playback = nullptr)
{
    if (!analysis.data || analysis.data->spawns.empty())
        return;
    const auto &d = *analysis.data;
    const auto &route = playback && playback->route ? *playback->route : analysis.route;

    struct View
    {
        float zoom = 1, x = 0, z = 0;
        int scene = -1;
    };

    static View normalView, reviewView, spawnView;
    auto &view = playback && playback->spawnPreview ? spawnView : playback ? reviewView : normalView;
    if (view.scene != d.scene)
    {
        view = View();
        view.scene = d.scene;
    }
    float minX = d.minX, maxX = d.maxX, minZ = d.minZ, maxZ = d.maxZ;
    float width = std::max(80.f, ImGui::GetContentRegionAvail().x);
    float height = requestedHeight > 0
                       ? requestedHeight
                       : std::min(width * .64f, std::clamp(ImGui::GetContentRegionAvail().y - 90 * g_dpiScale,
                                                           120.f * g_dpiScale, 380.f * g_dpiScale));
    if ((!playback || !playback->spawnPreview) && suite_ui::Button("Fit full map"))
    {
        view.zoom = 1;
        view.x = view.z = 0;
    }
    Help("Mouse wheel zooms. Drag the map with the left mouse button to pan. North is always up.");
    if (!playback)
    {
        SameLineIfFits("Candidate points");
        suite_ui::Checkbox("Candidate points", &g_plannerOptions.candidates);
        Help("Show all possible crystal locations, including those not selected for this seed.");
    }
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("SeedMapCanvas", ImVec2(width, height), ImGuiButtonFlags_MouseButtonLeft);
    bool hovered = ImGui::IsItemHovered();
    auto &io = ImGui::GetIO();
    float baseScale =
        std::min((width - 32) / std::max(1.f, maxX - minX), (height - 32) / std::max(1.f, maxZ - minZ));
    if (hovered && io.MouseWheel != 0)
        view.zoom = std::clamp(view.zoom * std::pow(1.2f, io.MouseWheel), .5f, 12.f);
    float scale = baseScale * view.zoom;
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        view.x -= io.MouseDelta.x / scale;
        view.z += io.MouseDelta.y / scale;
    }
    ImVec2 center(origin.x + width * .5f, origin.y + height * .5f);
    auto project = [&](practice::Vec3 p) {
        return ImVec2(center.x + (p.x - (minX + maxX) * .5f - view.x) * scale,
                      center.y - (p.z - (minZ + maxZ) * .5f - view.z) * scale);
    };
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(12, 19, 27, 255), 6);
    auto &filter = g_plannerOptions.filters[g_plannerOptions.level];
    draw->PushClipRect(origin, ImVec2(origin.x + width, origin.y + height), true);
    if ((d.physicalCaptured || IsHideSeekScene(d.scene)) && analysis.floor)
    {
        auto flags = draw->Flags;
        draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        for (const auto &t : *analysis.floor)
            draw->AddTriangleFilled(project(t[0]), project(t[1]), project(t[2]), IM_COL32(27, 43, 54, 255));
        draw->Flags = flags;
    }
    if (!d.physicalCaptured && analysis.outline)
        for (const auto &edge : *analysis.outline)
            draw->AddLine(project(edge.first), project(edge.second), IM_COL32(65, 86, 105, 200), 1.f);
    for (auto w : d.walls)
        if (!d.physicalCaptured && !IsHideSeekScene(d.scene) && d.wallOutlines.empty())
        {
            auto a = project({w.minX, 0, w.maxZ}), b = project({w.maxX, 0, w.minZ});
            draw->AddRect(a, b, IM_COL32(79, 97, 114, 165), 0, 0, 1);
        }
    const auto &outlines = IsHideSeekScene(d.scene) && !d.obstacles.empty() ? d.obstacles : d.wallOutlines;
    for (const auto &outline : outlines)
        if (!d.physicalCaptured && (!IsHideSeekScene(d.scene) || !outline.approximate))
        {
            ImVec2 p[4];
            for (int i = 0; i < 4; i++)
                p[i] = project(outline.corners[i]);
            draw->AddPolyline(p, 4,
                              outline.approximate ? IM_COL32(168, 134, 83, 150) : IM_COL32(92, 117, 139, 220),
                              ImDrawFlags_Closed, 1.2f);
        }
    if (d.physicalCaptured)
    {
        static std::vector<ImVec2> polygon;
        for (const auto &part : d.physicalParts)
        {
            polygon.clear();
            for (auto p : part.points)
                polygon.push_back(project(p));
            bool wall = (part.flags & 1) != 0;
            float area = 0;
            for (size_t i = 0; i < polygon.size(); i++)
            {
                auto a = polygon[i], b = polygon[(i + 1) % polygon.size()];
                area += a.x * b.y - b.x * a.y;
            }
            if (area < 0)
                std::reverse(polygon.begin(), polygon.end());
            if (polygon.size() >= 3)
                draw->AddConvexPolyFilled(polygon.data(), int(polygon.size()),
                                          wall ? IM_COL32(54, 73, 90, 255) : IM_COL32(39, 56, 68, 255));
            draw->AddPolyline(polygon.data(), int(polygon.size()),
                              wall ? IM_COL32(133, 166, 187, 240) : IM_COL32(91, 121, 139, 215),
                              ImDrawFlags_Closed, (wall ? 1.4f : 1.f) * g_dpiScale);
        }
    }
    auto animated = [&](const practice::PlaybackSlice *slice, ImU32 color) {
        if (!slice || !slice->visible)
            return;
        for (size_t i = 1; i < slice->trail.size(); i++)
            if (i - 1 >= slice->hidden.size() || !slice->hidden[i - 1])
                draw->AddLine(project(slice->trail[i - 1]), project(slice->trail[i]), color,
                              2.8f * g_dpiScale);
        if (!slice->markerVisible)
            return;
        auto p = project(slice->marker);
        draw->AddCircleFilled(p, 6 * g_dpiScale, IM_COL32(12, 19, 27, 255));
        draw->AddCircleFilled(p, 4 * g_dpiScale, color);
        draw->AddCircle(p, 7 * g_dpiScale, color, 0, 1.6f * g_dpiScale);
    };
    if (playback)
    {
        animated(playback->suggested, IM_COL32(106, 214, 196, 255));
        animated(playback->routePulse, IM_COL32(255, 255, 255, 240));
        animated(playback->played, IM_COL32(233, 119, 220, 255));
    }
    else
        for (size_t i = 1; i < route.line.size(); i++)
        {
            bool estimated = i - 1 < route.estimatedLine.size() && route.estimatedLine[i - 1];
            if (estimated)
                continue; // Distance estimates are never drawn through walls.
            DrawPracticeRouteLeg(draw, project(route.line[i - 1]), project(route.line[i]),
                                 IM_COL32(106, 214, 196, 255), 2.2f * g_dpiScale, false);
        }
    if (playback && playback->monsters)
        for (const auto &monster : *playback->monsters)
        {
            if (monster.path)
                for (size_t i = 1; i < monster.path->size(); i++)
                {
                    auto a = project(i == 1 && monster.pose.visible ? monster.pose.marker
                                                                    : (*monster.path)[i - 1]),
                         b = project((*monster.path)[i]);
                    float dx = b.x - a.x, dy = b.y - a.y, length = std::hypot(dx, dy),
                          stride = std::max(9.f * g_dpiScale, length / 128.f);
                    for (float at = float(std::fmod(playback->seconds * 12., stride)) - stride; at < length;
                         at += stride)
                    {
                        float lo = std::max(0.f, at), hi = std::min(length, at + stride * .5f);
                        if (hi > lo && length > .001f)
                            draw->AddLine(ImVec2(a.x + dx * lo / length, a.y + dy * lo / length),
                                          ImVec2(a.x + dx * hi / length, a.y + dy * hi / length),
                                          IM_COL32(248, 122, 100, 180), 1.f * g_dpiScale);
                    }
                }
            if (monster.pose.visible)
            {
                auto p = project(monster.pose.marker);
                draw->AddCircleFilled(p, 4 * g_dpiScale, IM_COL32(250, 104, 91, 255));
            }
        }
    if (playback && playback->flashes && playback->review)
    {
        const auto &events = playback->review->flashes;
        auto first =
            std::lower_bound(events.begin(), events.end(), playback->seconds - .75,
                             [](const practice::ReplayFlash &f, double at) { return f.seconds < at; });
        for (auto it = first; it != events.end() && it->seconds <= playback->seconds; ++it)
        {
            float age = float(playback->seconds - it->seconds);
            if (age < 0 || age > .75f)
                continue;
            auto p = project(playback->played && playback->played->visible ? playback->played->marker
                                                                           : it->player);
            float radius = (12 + age * 30) * g_dpiScale;
            int alpha = int(255 * std::pow(1 - age / .75f, 2));
            draw->AddCircle(p, radius, IM_COL32(255, 255, 255, alpha), 0, 2.5f * g_dpiScale);
        }
    }
    for (size_t i = 0; i < d.spawns.size(); i++)
    {
        ImVec2 p = project(d.spawns[i]);
        bool active = static_cast<int>(i) == d.spawn && (!playback || !playback->spawnPreview);
        draw->AddCircle(p, active ? 7.f : 4.f,
                        active ? IM_COL32(97, 211, 236, 255) : IM_COL32(106, 130, 150, 220), 0,
                        active ? 2.f : 1.f);
        if (active)
        {
            draw->AddCircleFilled(p, 2.5f, IM_COL32(97, 211, 236, 255));
            draw->AddText(ImVec2(p.x + 9, p.y - 14), IM_COL32(142, 223, 239, 255), "SPAWN");
        }
        if (playback && playback->spawnPreview && filter.spawn == static_cast<int>(i))
        {
            float pulse =
                suite_ui::preferences.motion ? .5f + .5f * std::sin(float(ImGui::GetTime()) * 4) : .5f;
            auto color = IM_COL32(90, 245, 145, int(160 + 95 * pulse));
            float radius = (12 + 2 * pulse) * g_dpiScale;
            draw->AddCircleFilled(p, (5 + 2 * pulse) * g_dpiScale, color);
            for (int dash = 0; dash < 16; dash++)
            {
                float a = dash * 6.2831853f / 16;
                draw->PathArcTo(p, radius, a, a + .22f, 3);
                draw->PathStroke(color, 0, 1.6f * g_dpiScale);
            }
        }
    }
    std::vector<int> rank(d.points.size(), 0);
    for (size_t i = 0; (!playback || playback->showOrder) && i < route.order.size(); i++)
        rank[route.order[i]] = static_cast<int>(i + 1);
    std::vector<bool> selected(d.points.size(), false);
    for (int id : d.selected)
        selected[id] = true;
    for (size_t i = 0; i < d.points.size(); i++)
    {
        if (!selected[i] && (playback || !g_plannerOptions.candidates))
            continue;
        auto p = project(d.points[i]);
        ImU32 color = selected[i] ? IM_COL32(248, 191, 105, 255) : IM_COL32(99, 118, 133, 155);
        float radius = selected[i] ? 4.f : 2.f;
        draw->AddCircleFilled(p, radius * g_dpiScale, color);
        if (rank[i])
        {
            char text[12];
            snprintf(text, sizeof(text), "%d", rank[i]);
            draw->AddText(ImVec2(p.x + 6, p.y - 6), IM_COL32(248, 220, 168, 255), text);
        }
    }
    draw->AddText(ImVec2(origin.x + 10, origin.y + 8), IM_COL32(184, 202, 218, 255),
                  playback          ? "N ^"
                  : route.estimated ? "N ^  |  unverified route legs hidden"
                                    : "N ^  |  spawn  |  crystals  |  route");
    if (IsHideSeekScene(d.scene))
        draw->AddText(ImVec2(origin.x + width - 43 * g_dpiScale, origin.y + 8), IM_COL32(255, 177, 76, 255),
                      "WIP");
    draw->PopClipRect();
    draw->AddRect(origin, ImVec2(origin.x + width, origin.y + height), IM_COL32(57, 78, 96, 255), 6);
    if (hovered && !ImGui::IsMouseDragging(ImGuiMouseButton_Left))
    {
        int closest = -1;
        float distance = 12 * g_dpiScale;
        for (size_t i = 0; i < d.points.size(); i++)
        {
            if (!selected[i] && (playback || !g_plannerOptions.candidates))
                continue;
            auto p = project(d.points[i]);
            float next = std::hypot(p.x - io.MousePos.x, p.y - io.MousePos.y);
            if (next < distance)
            {
                distance = next;
                closest = static_cast<int>(i);
            }
        }
        if (closest >= 0)
        {
            auto p = d.points[closest];
            ImGui::BeginTooltip();
            ImGui::Text("Crystal candidate #%d", closest + 1);
            ImGui::Text("X %.2f  Y %.2f  Z %.2f", p.x, p.y, p.z);
            ImGui::Text("This layout: %s", selected[closest] ? "selected" : "not selected");
            if (rank[closest])
                ImGui::Text("Suggested pickup #%d", rank[closest]);
            if (d.review)
            {
                auto found = std::find(d.review->collected.begin(), d.review->collected.end(), closest);
                if (found != d.review->collected.end())
                    ImGui::Text("Played pickup #%d", int(found - d.review->collected.begin() + 1));
            }
            ImGui::EndTooltip();
        }
    }
}

inline void RenderSpawnPreview(const practice::MapAnalysis &source)
{
    ImGui::PushID("Spawn preview");
    SeedCanvasPlayback view;
    view.spawnPreview = true;
    view.showOrder = false;
    RenderSeedCanvas(source, 210 * g_dpiScale, &view);
    ImGui::PopID();
}

inline practice::SeedRoutePlayback g_seedPreviewPlayback;

inline void BindSeedRoutePreview(const practice::MapAnalysis &analysis)
{
    static std::shared_ptr<const practice::MapData> bound;
    static uint32_t session = 0;
    if (!bound || !analysis.data || session != analysis.session ||
        !practice::SameSeedPreviewLayout(*bound, *analysis.data))
        g_seedPreviewPlayback = {};
    bound = analysis.data;
    session = analysis.session;
}

inline void RenderSeedRouteControls(practice::SeedRoutePlayback &replay)
{
    suite_ui::Checkbox("Optimal route", &replay.suggested);
    if (replay.suggested)
    {
        SameLineIfFits("Show full route");
        if (suite_ui::Button("Show full route"))
            replay.ShowAll();
        Help(
            "Show the whole verified route immediately. Otherwise it draws from spawn to finish in five seconds and stays visible. The white pulse follows the route direction; animation speed is not the estimated run time.");
        SameLineIfFits("Restart drawing");
        if (suite_ui::Button("Restart drawing"))
            replay.Restart();
        if (suite_ui::preferences.motion)
        {
            SameLineIfFits(replay.playing ? "Pause animation" : "Resume animation");
            if (suite_ui::Button(replay.playing ? "Pause animation" : "Resume animation"))
                replay.playing = !replay.playing;
        }
    }
    replay.Advance(ImGui::GetIO().DeltaTime, suite_ui::preferences.motion);
}

inline void RenderSeedMapTab(bool restricted)
{
    if (restricted)
    {
        ImGui::TextColored(g_accentCol, "PRACTICE ONLY");
        ImGui::TextWrapped("Switch to Practice or Normal mode to open seed maps.");
        return;
    }
    g_plannerTabActive = true;
    ImGui::Combo("Map", &g_plannerOptions.level, kMapLevelNames, kMapLevelCount);
    if (IsHideSeekScene(kMapScenes[g_plannerOptions.level]))
    {
        ImGui::TextColored(ImVec4(1, .69f, .30f, 1), "WIP | map geometry and seed search");
        Help(kHideSeekWipHelp);
    }
    static int selected = -1;
    if (selected != g_plannerOptions.level)
    {
        g_seedSearch.Stop();
        g_seedSearch.HidePreview();
        selected = g_plannerOptions.level;
        g_plannerAutoLevel = -1;
        if (g_pShared && !HasPendingRequest())
            PlannerRequest(2);
        else if (!g_mapWorker.Get(selected))
            g_mapWorker.LoadCache(selected);
    }
    auto base = g_mapWorker.Get(g_plannerOptions.level);
    if (base && base->needsAnalysis)
        g_mapWorker.AnalyzeSaved(g_plannerOptions.level);
    if (!base && g_pShared && !HasPendingRequest() && g_plannerAutoLevel != g_plannerOptions.level)
    {
        g_plannerAutoLevel = g_plannerOptions.level;
        PlannerRequest(2);
    }
    if (base && base->data)
        BindPlannerFilters(*base);
    bool live = base && base->live && base->data && g_pShared && base->session == g_session &&
                base->data->pid == g_pShared->pid;
    bool verified = live && base->data->Verified() && g_pShared->rngPatchComplete;
    ImGui::BeginDisabled(!g_pShared || g_seedSearch.Busy() || HasPendingRequest());
    if (suite_ui::Button("Load level in Practice"))
    {
        g_speedrunMode = g_practiceMode = true;
        g_seededPractice = false;
        g_rawPracticeSpawn = false;
        g_selectedSpeedrunLevel = g_plannerOptions.level;
        g_commands.speedrunLoadLevelCmd = 1;
        g_plannerAutoLevel = -1;
    }
    ImGui::EndDisabled();
    if (!verified)
        ImGui::TextWrapped(live ? "Prediction is not verified. Reload the level to capture a fresh catalog."
                                : "Load this level once to verify its spawn catalog.");
    if (!g_mapWorker.Error().empty())
        ImGui::TextWrapped("%s", g_mapWorker.Error().c_str());
    if (!ImGui::BeginTabBar("SeedMapSections"))
        return;
    if (suite_ui::BeginTabItem("Preview & replay", nullptr,
                               g_selectSeedPreview ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None))
    {
        g_selectSeedPreview = false;
        ImGui::BeginDisabled(g_seedSearch.Busy() || HasPendingRequest());
        ImGui::InputInt("Seed##preview-number", &g_plannerOptions.seed);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!verified || g_seedSearch.Busy() || HasPendingRequest());
        if (suite_ui::Button("Preview seed"))
        {
            PlannerRequest(1);
        }
        SameLineIfFits("Random Seed");
        if (suite_ui::Button("Random Seed"))
            RandomPlannerPreview();
        SameLineIfFits("Captured layout");
        if (suite_ui::Button("Captured layout"))
        {
            PlannerRequest(2);
            g_plannerOptions.seed = base->data->liveSeed;
            g_plannerOptions.recipe = std::max(1, base->data->liveRecipe);
        }
        ImGui::EndDisabled();
        auto analysis = DisplayedPlanner(g_plannerOptions.level);
        if (analysis && analysis->data)
        {
            const auto &d = *analysis->data;
            int recipe = d.recipe ? d.recipe : d.liveRecipe;
            ImGui::TextColored(g_headerCol, "Displayed seed: %d | %s | spawn #%d", d.seed,
                               recipe == 1   ? "captured spawn seed"
                               : recipe == 2 ? "Practice base seed"
                                             : "unknown type",
                               d.spawn + 1);
            if (analysis->route.complete)
                ImGui::Text("Route: %.2f units | %zu crystals | %s", analysis->route.length,
                            analysis->route.order.size(),
                            analysis->route.estimated         ? "partial capture"
                            : analysis->route.pickupOptimized ? "checked pickup contacts"
                            : analysis->route.exact           ? "checked / exact order"
                                                              : "checked / heuristic order");
            if (analysis->route.pickupOptimized)
                Help(
                    "Uses the actual crystal sphere and the enabled pickup trigger on Main Camera, separately from the movement controller. Conservative standing/level-camera contacts work under every heading; camera pitch, crouching and bobbing can change the exact edge. One continuous contact per crystal, 8 boundary samples plus safe anchors. The visit order remains a heuristic.");
            if (d.physicalCaptured && d.physicalOmitted > 0)
            {
                ImGui::TextColored(g_accentCol, "Geometry incomplete: %d unsupported parts",
                                   d.physicalOmitted);
                Help(
                    "Some solid colliders could not be captured exactly. Pickup-contact optimization is disabled; rebuild the geometry after the scene has loaded. Unverified route legs stay hidden.");
            }
            auto gameTravel =
                practice::EstimateGameTravel(analysis->route.length, d.movementTick, d.movementSprintStep,
                                             analysis->route.complete && !analysis->route.estimated);
            auto travel = gameTravel.valid ? gameTravel
                                           : practice::EstimateTravel(analysis->route.length,
                                                                      analysis->route.complete &&
                                                                          !analysis->route.estimated);
            const auto &measured = PreviewMovementTiming(analysis);
            if (measured.valid)
            {
                ImGui::TextDisabled("Learned movement: %.2f s (%.1f - %.1f)", measured.seconds, measured.fast,
                                    measured.slow);
                Help(
                    ("Continuously learned physics movement in this mode and game build. Measured route coverage: " +
                     std::to_string(int(measured.coverage * 100)) +
                     "%. Straight movement, bends, sharp turns, slopes and starts are learned separately. Enemy waits are not predicted.")
                        .c_str());
            }
            else if (travel.valid)
            {
                ImGui::TextDisabled("%s: %.1f - %.1f s", gameTravel.valid ? "Game movement" : "Fallback",
                                    travel.fast, travel.slow);
                Help(
                    gameTravel.valid
                        ? "Verified sprint step and captured physics interval, with unnormalized diagonal inputs. Fatigue, aiming and enemy delays are not predicted."
                        : "Complete speedruns to collect reliable movement observations. Without a captured game model, use the observed 8.1-10 u/s range.");
            }
            else if (analysis->route.complete && analysis->route.estimated)
                ImGui::TextDisabled("Time unavailable: capture has missing route connections.");
            bool firstTimer = d.flags & 16;
            ImGui::BeginDisabled(!verified || firstTimer || !recipe || HasPendingRequest());
            if (suite_ui::Button("Use displayed seed"))
                SetPracticeSelection(d.seed, recipe, g_plannerOptions.level);
            ImGui::EndDisabled();
            if (firstTimer)
            {
                ImGui::TextDisabled("First Timer start: forced spawn");
                Help(
                    "The first-ever Timer start forces spawn #5. Preview a subsequent start to replay its seed.");
            }
            BindSeedRoutePreview(*analysis);
            auto &replay = g_seedPreviewPlayback;
            RenderSeedRouteControls(replay);
            auto slice = replay.suggested ? practice::SuggestedSlice(analysis->route, replay.Fraction(), 1.)
                                          : practice::PlaybackSlice{};
            auto pulse = replay.suggested && suite_ui::preferences.motion
                             ? practice::SuggestedSlice(analysis->route, replay.Pulse(), .035)
                             : practice::PlaybackSlice{};
            SeedCanvasPlayback canvas;
            canvas.suggested = &slice;
            canvas.routePulse = &pulse;
            canvas.showOrder = replay.suggested;
            canvas.route = &analysis->route;
            if (!d.spawns.empty())
                RenderSeedCanvas(*analysis, 0, &canvas);
            else if (analysis->working)
                suite_ui::LoadingState("Map loading", "Capturing layout and navigation", 180);
            if (analysis->working)
                suite_ui::Busy("Calculating route...");
            else if (analysis->route.estimated)
            {
                ImGui::TextColored(g_accentCol, "Partial route: missing connections hidden");
                Help(analysis->route.message.c_str());
            }
            if (!analysis->working && !analysis->route.complete && !analysis->route.message.empty())
                ImGui::TextWrapped("%s", analysis->route.message.c_str());
            if (!d.Verified())
                ImGui::TextWrapped("%s", d.message.c_str());
            if (!analysis->error.empty())
                ImGui::TextColored(g_accentCol, "%s", analysis->error.c_str());
            if (d.navigationStatus == 1)
                ImGui::TextDisabled("Preparing navigation: %d / %d", d.navigationQueries, d.navigationTotal);
            if (suite_ui::CollapsingHeader("Seed type & prediction details"))
            {
                ImGui::BeginDisabled(g_seedSearch.Busy() || HasPendingRequest());
                int type = g_plannerOptions.recipe - 1;
                const char *types[] = {"Captured spawn seed", "Practice base seed"};
                if (ImGui::Combo("Input seed type", &type, types, 2))
                {
                    g_plannerOptions.recipe = type + 1;
                }
                ImGui::EndDisabled();
                Help(
                    "Use the matching seed type for numbers entered manually. Use best / Use displayed transfers map, seed and type together. The spawn seed fixes player/crystal positions; monster events may differ.");
                ImGui::TextDisabled("Capture verification");
                Help(d.message.c_str());
                ImGui::TextDisabled("Navigation model");
                Help(d.navigationNote.c_str());
                ImGui::TextDisabled("Route model");
                Help(analysis->route.message.c_str());
                if (d.physicalCaptured)
                    ImGui::TextDisabled("Physical contours: %zu | pickup regions: %zu / %zu",
                                        d.physicalParts.size(),
                                        size_t(std::count_if(d.pickupRegions.begin(), d.pickupRegions.end(),
                                                             [](const auto &p) { return p.verified; })),
                                        d.points.size());
                ImGui::TextDisabled("Generator verification");
                Help(d.rngNote.c_str());
                if (!analysis->cacheWarning.empty())
                    ImGui::TextWrapped("%s", analysis->cacheWarning.c_str());
                if (!analysis->route.message.empty())
                {
                    ImGui::TextDisabled("Route details");
                    Help(analysis->route.message.c_str());
                }
            }
        }
        else
            suite_ui::EmptyState(
                "Map empty", suite_ui::Icon::Map, "Waiting for a map",
                "Enter this level to capture its geometry and layout. Preview and seed search become available when capture is ready.",
                208);
        if (!g_plannerRandomError.empty())
            ImGui::TextWrapped("%s", g_plannerRandomError.c_str());
        suite_ui::EndTabItem();
    }
    if (suite_ui::BeginTabItem("Seed search"))
    {
        RenderSeedSearchControls(base, verified, g_seedSearch.Busy());
        suite_ui::EndTabItem();
    }
    ImGui::EndTabBar();
}
