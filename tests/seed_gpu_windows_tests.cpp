#define NOMINMAX
#include "SeedGpu.h"
#include "seed_route_fixture.h"
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace practice;
static bool TopEqual(const SeedSearchState &a, const SeedSearchState &b)
{
    if (a.top.size() != b.top.size()) return false;
    for (size_t i = 0; i < a.top.size(); i++)
        if (a.top[i].index != b.top[i].index || a.top[i].seed != b.top[i].seed || !SameLayout(a.top[i].selection, b.top[i].selection) ||
            !seed_gpu::SameScore(a.top[i].route, b.top[i].route)) return false;
    return true;
}
static std::shared_ptr<MapAnalysis> Source(const std::filesystem::path &fixtures, int scene, int crystals = 0)
{
    auto d = LoadRouteBenchmarkFixture((fixtures / (scene == 5 ? "hide-seek-boxes.routefixture" : "hide-seek-outside-boxes.routefixture")).string().c_str(),
        crystals ? crystals : scene == 13 ? 52 : scene == 14 ? 40 : 22, 1.35f);
    d.scene = scene;
    auto source = std::make_shared<MapAnalysis>(); source->data = std::make_shared<MapData>(std::move(d));
    source->live = true; source->session = 77; return source;
}
static void Integration(const std::filesystem::path &fixtures, bool fault)
{
    auto source = Source(fixtures, 13);
    seed_gpu::Engine accelerator(true);
    SeedSearch search([&](auto &state, const auto &distances, const auto &cancel, const auto &progress) {
        return accelerator.Run(state, distances, cancel, progress);
    });
    auto request = StreamingRequest(*source, 321, 4); request.backend = SearchBackend::Gpu;
    for (int pass = 0; pass < 2; pass++)
    {
        search.Begin(request); search.Update(source);
        auto result = AwaitStreaming(search);
        assert(result->phase == SearchPhase::Completed && result->evaluated == request.count && result->generated == request.count);
        assert(result->request.key == request.key && result->gpu == !fault && result->top.size() == 10 && result->preview);
        if (fault) assert(result->backendNote.find("center-length mismatch") != std::string::npos);
        NavGraph graph(*source->data); SearchDistances distances(*source->data, graph, [] { return false; }, [](int, int) {});
        auto reference = SeedSearch::RunFrozen(source, distances, request, [] { return false; });
        for (auto &row : reference.top) row.route = distances.Draw(row.selection, std::move(row.route), [] { return false; });
        assert(TopEqual(reference, *result));
        search.Pin(*source, result->top.front()); search.Stop(); assert(search.Pinned());
        assert(SeedResultsText(*result).find(fault ? "verified CPU" : "GPU + CPU pickup") != std::string::npos);
    }
    std::cout << (fault ? "Actual corrupted dispatch: visible GPU bootstrap, CPU restart with same key, complete Top10, preview/export/pin and second-search fallback: PASS\n"
                       : "Regular search coordinator: GPU, live preview, drawn Top10, export, pin, Stop and cached second search: PASS\n");
}
int main(int argc, char **argv)
{
    try
    {
        assert(argc >= 2); std::filesystem::path fixtures(argv[1]);
        if (argc >= 3 && std::string(argv[2]) == "--fault") { Integration(fixtures, true); return 0; }
        auto adapters = seed_gpu::Adapters(); assert(!adapters.empty());
        std::cout << "Actual production GPU probe: " << adapters.front().name << std::endl;
        seed_gpu::Device device(adapters.front().index, true);
        uint64_t checked = 0;
        for (int n : {1, 15, 16, 22, 31, 32, 33, 40, 52})
        {
            std::cout << "Check all shader variants: " << n << " crystals" << std::endl;
            auto source = Source(fixtures, 13, n); const auto &d = *source->data;
            NavGraph graph(d); SearchDistances distances(d, graph, [] { return false; }, [](int, int) {});
            auto catalog = std::make_shared<seed_gpu::Catalog>(int(d.spawns.size()), int(d.points.size()),
                [&](int a, int b) { return distances.CenterDistance(a, b); });
            device.SetCatalog(catalog);
            PortableSelectionGenerator generator(d); RandomSeedOrder order(FixtureSearchKey);
            std::vector<SeedLayout> layouts(65); std::vector<RouteResult> reference;
            RouteWorkspace work;
            for (size_t i = 0; i < layouts.size(); i++)
            {
                layouts[i].seed = order.SeedAt(uint32_t(i)); generator.Generate(layouts[i].seed, 1, layouts[i].selection);
                int count = distances.FillMatrix(layouts[i].selection, work);
                reference.push_back(SolveOrderFlat(work.matrix.data(), count, work, [] { return false; }, false));
            }
            for (auto kind : seed_gpu::ShaderKinds)
            {
                device.Select(kind, n); auto result = device.Execute(layouts, [] { return false; });
                for (size_t i = 0; i < result.size(); i++) { assert(seed_gpu::SameScore(reference[i], result[i])); checked++; }
            }
        }
        // Stress the actual packed catalog at its 1024-point wire limit,
        // including ties and decisions adjacent to the 1e-6 improvement threshold.
        for (int points : {71, 1024}) for (int profile : {0, 1})
        {
            auto edge = [&](int a, int b) { return profile == 0 ? double((a * 71 + b * 17) % 4)
                : 1. + ((a * 71 + b * 17) % 32) * 1e-7; };
            auto catalog = std::make_shared<seed_gpu::Catalog>(3, points, edge); device.SetCatalog(catalog);
            for (int n : {16, 22, 40, 52})
            {
                std::vector<SeedLayout> layouts(65); std::vector<RouteResult> reference;
                for (size_t i = 0; i < layouts.size(); i++)
                {
                    auto &selection = layouts[i].selection; selection.spawn = int(i % 3);
                    std::vector<int> ids(points); std::iota(ids.begin(), ids.end(), 0);
                    uint32_t seed = 0x720000u + uint32_t(i);
                    for (int at = 0; at < points; at++)
                    {
                        seed = seed * 1664525u + 1013904223u;
                        std::swap(ids[at], ids[size_t(at) + seed % (points - at)]);
                    }
                    ids.resize(n); std::sort(ids.begin(), ids.end(), std::greater<int>()); selection.crystals = ids;
                    uint32_t packed[53]{}; catalog->Pack(selection, packed); RouteWorkspace work;
                    for (int a = 0; a <= n; a++) for (int b = 0; b <= n; b++)
                        work.matrix[a * (n + 1) + b] = catalog->edges[size_t(packed[a]) * catalog->side + packed[b]];
                    reference.push_back(SolveOrderFlat(work.matrix.data(), n + 1, work, [] { return false; }, false));
                }
                for (auto kind : {seed_gpu::ShaderKind::Compact, seed_gpu::ShaderKind::CompactFixed})
                {
                    device.Select(kind, n); auto result = device.Execute(layouts, [] { return false; });
                    for (size_t i = 0; i < result.size(); i++) { assert(seed_gpu::SameScore(reference[i], result[i])); checked++; }
                }
            }
        }
        for (int scene : {13, 14})
        {
            auto source = Source(fixtures, scene); const auto &d = *source->data;
            NavGraph graph(d); SearchDistances distances(d, graph, [] { return false; }, [](int, int) {});
            device.SetCatalog(std::make_shared<seed_gpu::Catalog>(int(d.spawns.size()), int(d.points.size()), [&](int a, int b) { return distances.CenterDistance(a, b); }));
            device.Select(scene == 13 ? seed_gpu::ShaderKind::Compact : seed_gpu::ShaderKind::CompactFixed, d.keep);
            PortableSelectionGenerator generator(d); RandomSeedOrder order(FixtureSearchKey ^ uint64_t(scene));
            for (int count : {4096, 4093})
            {
                std::cout << "Check full/partial batch: scene " << scene << ", " << count << " layouts" << std::endl;
                std::vector<SeedLayout> layouts(count);
                for (int i = 0; i < count; i++) { layouts[i].seed = order.SeedAt(uint32_t(i)); generator.Generate(layouts[i].seed, 2, layouts[i].selection); }
                auto result = device.Execute(layouts, [] { return false; }); RouteWorkspace work;
                for (int i = 0; i < count; i++)
                {
                    int side = distances.FillMatrix(layouts[i].selection, work);
                    auto cpu = SolveOrderFlat(work.matrix.data(), side, work, [] { return false; }, false);
                    assert(seed_gpu::SameScore(cpu, result[i])); checked++;
                }
            }
        }
        for (int scene : {13, 14, 5})
        {
            std::cout << "Check integrated engine: scene " << scene << std::endl;
            auto source = Source(fixtures, scene); NavGraph graph(*source->data);
            SearchDistances distances(*source->data, graph, [] { return false; }, [](int, int) {});
            seed_gpu::Engine engine(true);
            auto request = StreamingRequest(*source, 193, 4); request.backend = SearchBackend::Gpu;
            for (int recipe : {1, 2}) for (bool fast : {false, true})
                for (auto goal : {SearchObjective::ShortestRoute, SearchObjective::LongestRoute}) for (int spawn : {-1, 0})
                {
                    request.recipe = recipe; request.fast = fast; request.criteria.objective = goal;
                    request.criteria.spawn = spawn; request.criteria.catalog = CatalogKey(*source->data);
                    SeedSearchState state; state.source = source; state.request = request; state.portable = true;
                    assert(engine.Run(state, distances, [] { return false; }, [](auto) {}));
                    assert(state.phase == SearchPhase::Completed && state.gpu);
                    auto cpu = SeedSearch::RunFrozen(source, distances, request, [] { return false; });
                    assert(TopEqual(cpu, state) && state.evaluated == request.count && state.filtered == cpu.filtered); checked += state.scored;
                }
            if (scene == 5)
            {
                request.backend = SearchBackend::Automatic; request.criteria.spawn = -1;
                request.criteria.objective = SearchObjective::ShortestRoute;
                SeedSearchState state; state.source = source; state.request = request; state.portable = true;
                assert(!engine.Run(state, distances, [] { return false; }, [](auto) {}) && state.request.prune && !state.gpu);
                seed_gpu::Engine missing(true, 999999);
                state.request.backend = SearchBackend::Gpu;
                assert(!missing.Run(state, distances, [] { return false; }, [](auto) {}) && state.request.prune && !state.gpu);
                request.criteria.objective = SearchObjective::LongestRoute; request.count = 16384;
                state = {}; state.source = source; state.request = request; state.portable = true;
                bool handled = engine.Run(state, distances, [] { return false; }, [](auto) {});
                assert(state.error.empty());
                if (handled) assert(state.phase == SearchPhase::Completed && state.gpu);
                else assert(state.backendNote.find("CPU selected") != std::string::npos);
                if (!handled)
                {
                    auto forced = request; forced.backend = SearchBackend::Gpu;
                    state = {}; state.source = source; state.request = forced; state.portable = true;
                    std::atomic<bool> stop{false};
                    assert(engine.Run(state, distances, [&] { return stop.load(); }, [&](auto s) { if (s.gpu && s.evaluated >= 64) stop = true; }));
                    assert(state.phase == SearchPhase::Stopped && state.error.empty());
                    state = {}; state.source = source; state.request = request; state.portable = true;
                    assert(!engine.Run(state, distances, [] { return false; }, [](auto) {}) && state.calibrationMs == 0);
                }
            }
            else
            {
                request.backend = SearchBackend::Automatic; request.count = 4096; request.criteria.spawn = -1;
                SeedSearchState state; state.source = source; state.request = request; state.portable = true;
                bool handled = engine.Run(state, distances, [] { return false; }, [](auto) {});
                assert(state.error.empty());
                if (handled) assert(state.phase == SearchPhase::Completed && state.gpu);
                else assert(state.backendNote.find("CPU selected") != std::string::npos);
                std::cout << "Calibration scene " << scene << ": " << state.backendNote << '\n';
            }
        }
        Integration(fixtures, false);
        std::cout << "Actual production FP64 shaders: " << checked << " comparisons; 5 variants, 1-52 crystals, full/partial 4096 batches, both recipes/goals/accuracy modes, spawn filters and automatic calibration: PASS\n";
        return 0;
    }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
