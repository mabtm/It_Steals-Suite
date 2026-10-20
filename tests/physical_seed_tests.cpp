#include "seed_batch_fixture.h"
#include <cassert>
#include <iostream>
using namespace practice;

static double Length(const std::vector<Vec3> &p)
{
    double n = 0;
    for (size_t i = 1; i < p.size(); i++)
        n += Distance(p[i - 1], p[i]);
    return n;
}

int main()
{
    MapData d;
    d.pid = 1234;
    d.scene = 5;
    d.epoch = 1;
    d.flags = 1;
    d.stage = 2;
    d.spawn = d.liveSpawn = 0;
    d.keep = 22;
    d.recipe = 2;
    d.rngStatus = 2;
    d.rngProbes = 4160;
    d.rngAdvance = 1;
    d.rngSite = 123;
    d.searchBackend = 1;
    d.agentRadius = .4f;
    d.agentHeight = .8f;
    d.agentSlope = 45;
    d.physicalCaptured = true;
    FixtureTile(d, -10, -10, 450, 450);
    d.spawns = {{0, 1, 0}, {24, 1, 0}};
    d.physicalParts = {{3, {{4, 0, -3}, {6, 0, -3}, {6, 0, 10}, {4, 0, 10}}}};
    for (int i = 0; i < 30; i++)
    {
        Vec3 p{float(i % 6) * 3.7f + 1.3f, 1, float(i / 6) * 4.1f + 1.7f};
        if (p.x > 3 && p.x < 7 && p.z < 11)
            p.x = 2;
        d.points.push_back(p);
        d.pickupRegions.push_back({true, p, 1.35f});
        if (i < 22)
            d.selected.push_back(i);
    }
    d.points.push_back({400, 1, 400});
    d.pickupRegions.push_back({true, d.points.back(), 1.35f});
    d.liveSelected = d.selected;
    auto source = StreamingSource(d);
    NavGraph graph(*source->data);
    auto start = std::chrono::steady_clock::now();
    SearchDistances distances(*source->data, graph, [] { return false; }, [](int, int) {});
    assert(!distances.EstimatedPairs());
    double prep = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << "Synthetic physical/contact preparation: " << prep << " s\n";
    std::shared_ptr<const SeedSearchState> reference;
    for (int workers : {1, 6})
    {
        SeedSearch search;
        auto request = StreamingRequest(*source, 4096, workers, true);
        search.Begin(request);
        search.Update(source);
        auto result = AwaitStreaming(search);
        assert(result->phase == SearchPhase::Completed && result->generated == 4096 &&
               result->evaluated == 4096 && result->estimated == 0 && result->top.size() == 10 &&
               result->scored + result->pruned == 4096 && result->pruned > 100);
        for (size_t i = 0; i < result->top.size(); i++)
        {
            const auto &row = result->top[i];
            assert(row.route.pickupOptimized);
            auto score = distances.Score(row.selection, true, [] { return false; });
            auto line = distances.Draw(row.selection, score, [] { return false; });
            assert(std::abs(score.length - row.route.length) < 1e-6 &&
                   std::abs(Length(line.line) - score.length) < 1e-4);
            for (size_t k = 1; k < line.line.size(); k++)
                assert(graph.Visible(line.line[k - 1], line.line[k]));
            auto selected = std::make_shared<MapData>(*source->data);
            selected->selected = row.selection.crystals;
            selected->spawn = row.selection.spawn;
            auto independently = AnalyzeMap(selected, graph).route;
            // Search's fast heuristic and the independent preview use the same
            // >15-goal ordering and canonical checked pair paths.
            assert(independently.pickupOptimized && std::abs(independently.length - score.length) < 1e-4);
            if (reference)
                assert(row.seed == reference->top[i].seed &&
                       SameLayout(row.selection, reference->top[i].selection) &&
                       std::abs(row.route.length - reference->top[i].route.length) < 1e-7);
        }
        std::cout << "Workers " << workers << ": checked " << result->evaluated << ", scored "
                  << result->scored << ", safely pruned " << result->pruned << ", elapsed " << result->elapsed
                  << " s, best " << result->top[0].route.length << " u\n";
        reference = result;
    }
    // Exhaustive scoring, independent of the worker's MST/cutoff path.
    PortableSelectionGenerator generator(*source->data);
    RandomSeedOrder order(FixtureSearchKey);
    RouteWorkspace work;
    std::vector<SeedResult> exhaustive;
    for (uint32_t i = 0; i < 4096; i++)
    {
        Selection selection;
        int seed = order.SeedAt(i);
        generator.Generate(seed, source->data->recipe, selection);
        SeedResult row;
        row.seed = seed;
        row.index = i;
        row.selection = selection;
        row.route = distances.Score(selection, true, [] { return false; }, work);
        RankSeed(exhaustive, std::move(row));
    }
    assert(exhaustive.size() == reference->top.size());
    for (size_t i = 0; i < exhaustive.size(); i++)
        assert(exhaustive[i].seed == reference->top[i].seed &&
               exhaustive[i].route.length == reference->top[i].route.length);
    std::cout
        << "Physical contacts in actual seed coordinator: 1/6-worker parity, exact accounting, continuous lines and exhaustive Top 10 comparison: PASS\n";
}
