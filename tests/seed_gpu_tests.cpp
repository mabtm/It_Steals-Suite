#include "SeedGpuSearch.h"
#include "seed_route_fixture.h"
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace practice;
static bool TopEqual(const SeedSearchState &a, const SeedSearchState &b)
{
    if (a.top.size() != b.top.size()) return false;
    for (size_t i = 0; i < a.top.size(); i++)
        if (a.top[i].index != b.top[i].index || a.top[i].seed != b.top[i].seed ||
            !SameLayout(a.top[i].selection, b.top[i].selection) || !seed_gpu::SameScore(a.top[i].route, b.top[i].route)) return false;
    return true;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    for (int scene : {13, 14, 5})
    {
        auto d = LoadRouteBenchmarkFixture((std::filesystem::path(argv[1]) /
            (scene == 5 ? "hide-seek-boxes.routefixture" : "hide-seek-outside-boxes.routefixture")).string().c_str(),
            scene == 13 ? 52 : scene == 14 ? 40 : 22, 1.35f);
        d.scene = scene;
        auto source = std::make_shared<MapAnalysis>(); source->data = std::make_shared<MapData>(d);
        NavGraph graph(d); SearchDistances distances(d, graph, [] { return false; }, [](int, int) {});
        SeedSearchRequest r; r.scene = scene; r.key = FixtureSearchKey; r.count = 193; r.prune = false;
        assert(seed_gpu::Eligible(d, r));
        auto invalid = d; invalid.keep = 15; invalid.selected.resize(15); assert(!seed_gpu::Eligible(invalid, r));
        invalid = d; invalid.rngProbes--; assert(!seed_gpu::Eligible(invalid, r));
        invalid = d; invalid.physicalOmitted = 1; assert(!seed_gpu::Eligible(invalid, r));
        invalid = d; invalid.scene = 7; assert(!seed_gpu::Eligible(invalid, r));
        auto scoring = [&](std::span<const SeedLayout> layouts) {
            std::vector<RouteResult> result; RouteWorkspace workspace;
            for (const auto &layout : layouts)
            {
                int count = distances.FillMatrix(layout.selection, workspace);
                result.push_back(SolveOrderFlat(workspace.matrix.data(), count, workspace, [] { return false; }, false));
            }
            return result;
        };
        for (int recipe : {1, 2}) for (auto goal : {SearchObjective::ShortestRoute, SearchObjective::LongestRoute})
        {
            r.recipe = recipe; r.criteria.objective = goal;
            std::vector<RouteResult> scores(size_t(r.count));
            auto reference = SeedSearch::RunFrozen(source, distances, r, [] { return false; }, {},
                [&](uint64_t i, const Selection &, const RouteResult &route) { scores[i] = route; });
            assert(reference.phase == SearchPhase::Completed && !reference.pruned);
            for (int workers : {1, 2, 4, 6})
            {
                r.workers = workers; uint64_t audited = 0; std::mutex auditGate;
                auto result = seed_gpu::Search(source, distances, r, workers == 1 ? 32 : 4096, scoring, [] { return false; },
                    [&](SeedSearchState p) { assert(p.evaluated <= p.generated && p.evaluated <= r.count); },
                    [&](uint64_t i, const Selection &, const RouteResult &route) {
                        assert(seed_gpu::SameScore(scores[i], route)); std::lock_guard lock(auditGate); audited++;
                    });
                assert(result.phase == SearchPhase::Completed && TopEqual(reference, result) && result.gpu);
                assert(result.evaluated == r.count && result.generated == r.count && result.scored == audited && !result.pruned);
            }
            r.criteria.spawn = 0; r.criteria.catalog = CatalogKey(d); r.workers = 2;
            auto cpu = SeedSearch::RunFrozen(source, distances, r, [] { return false; });
            auto gpu = seed_gpu::Search(source, distances, r, 48, scoring, [] { return false; });
            assert(gpu.phase == SearchPhase::Completed && gpu.filtered > 0 && gpu.evaluated == gpu.filtered + gpu.scored && TopEqual(cpu, gpu));
            r.criteria.spawn = -1;
        }
        auto bad = seed_gpu::Search(source, distances, r, 32, [](auto) { return std::vector<RouteResult>{}; }, [] { return false; });
        assert(bad.phase == SearchPhase::Failed && !bad.error.empty());
        bad = seed_gpu::Search(source, distances, r, 32, [&](auto layouts) { auto result = scoring(layouts); result[0].length += 1.; return result; }, [] { return false; });
        assert(bad.phase == SearchPhase::Failed && bad.error.find("center-length") != std::string::npos);
        std::atomic<bool> stop{false};
        auto stopped = seed_gpu::Search(source, distances, r, 32, scoring, [&] { return stop.load(); }, [&](auto s) { if (s.evaluated >= 64) stop = true; });
        assert(stopped.phase == SearchPhase::Stopped && stopped.evaluated < r.count);
        auto restarted = seed_gpu::Search(source, distances, r, 32, scoring, [] { return false; });
        assert(restarted.phase == SearchPhase::Completed && restarted.evaluated == r.count);
    }
    PlannerPreferences preferences; assert(preferences.searchBackend == 0);
    preferences.Decode("searchBackend", 2); preferences.Validate(); assert(preferences.searchBackend == 2);
    std::ostringstream encoded; preferences.Encode(encoded); assert(encoded.str().find("planner.searchBackend=2") != std::string::npos);
    preferences.Decode("searchBackend", 99); preferences.Validate(); assert(preferences.searchBackend == 2);
    std::cout << "Production GPU pipeline: CPU parity across 3 maps, both recipes/goals, 1/2/4/6 helpers, filters, strict audits, failures, cancellation/restart and preferences: PASS\n";
}
