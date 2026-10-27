#define SUITE_GPU_DEV 1
#include "GpuDevPlan.h"
#include "seed_route_fixture.h"
#include <cassert>
#include <set>
#include <iostream>

int main(int argc, char **argv)
{
    using namespace gpu_dev; using namespace practice;
    assert(argc == 3);
    auto source = std::make_shared<MapAnalysis>();
    source->data = std::make_shared<MapData>(LoadRouteBenchmarkFixture(
        (std::filesystem::path(argv[1]) / "hide-seek-boxes.routefixture").string().c_str(), 22, 1.35f));
    NavGraph graph(*source->data);
    SearchDistances distances(*source->data, graph, [] { return false; }, [](int,int) {});
    SeedSearchRequest r; r.count = 384; r.key = FixtureSearchKey; r.recipe = 1; r.exhaustive = true;
    std::vector<RouteResult> scores(size_t(r.count));
    auto reference = SeedSearch::RunFrozen(source, distances, r, [] { return false; }, {},
        [&](uint64_t i, const Selection &, const RouteResult &route) { scores[i] = route; });
    assert(reference.phase == SearchPhase::Completed && reference.evaluated == r.count && !reference.pruned);
    auto score = [&](std::span<const SeedLayout> layouts) {
        std::vector<RouteResult> result; RouteWorkspace workspace;
        for (const auto &layout : layouts) {
            int count = distances.FillMatrix(layout.selection, workspace);
            result.push_back(SolveOrderFlat(workspace.matrix.data(), count, workspace, [] { return false; }, false));
        }
        return result;
    };
    std::atomic<uint64_t> audited{0};
    auto audit = [&](uint64_t i, const Selection &, const RouteResult &route) { assert(SameScore(scores[i], route)); audited++; };
    r.exhaustive = false;
    for (int workers = 1; workers <= 6; workers++)
    {
        r.workers = workers;
        auto cpu = SeedSearch::RunFrozen(source, distances, r, [] { return false; }, {}, audit);
        assert(cpu.phase == SearchPhase::Completed && SameTop(reference.top, cpu.top));
        assert(cpu.evaluated == r.count && cpu.evaluated == cpu.scored + cpu.pruned + cpu.filtered);
        auto gpu = SearchGpu(source, distances, r, 32, score, [] { return false; }, {}, audit);
        assert(gpu.phase == SearchPhase::Completed && SameTop(reference.top, gpu.top));
        assert(gpu.evaluated == r.count && gpu.generated == r.count && gpu.evaluated == gpu.scored + gpu.pruned + gpu.filtered);
    }
    r.workers = 2;
    for (auto mode : {BoundsMode::Enabled, BoundsMode::Disabled, BoundsMode::Adaptive})
    {
        BoundsStats measured;
        auto gpu = SearchGpu(source, distances, r, 32, score, [] { return false; }, {}, audit, mode, &measured);
        assert(gpu.phase == SearchPhase::Completed && SameTop(reference.top, gpu.top));
        assert(gpu.scored + gpu.pruned == r.count && measured.checked + measured.skipped <= r.count - 64);
        if (mode == BoundsMode::Disabled) assert(!measured.checked && measured.skipped && !gpu.pruned && gpu.scored == r.count);
        if (mode == BoundsMode::Enabled) assert(measured.checked && !measured.skipped);
    }
    BoundsBudget adaptive{BoundsMode::Adaptive};
    for (int i = 0; i < 64; i++) { assert(adaptive.Use()); adaptive.Result(false); }
    for (int i = 0; i < 512; i++) assert(!adaptive.Use());
    assert(adaptive.Use());
    BoundsBudget useful{BoundsMode::Adaptive};
    for (int i = 0; i < 128; i++) { assert(useful.Use()); useful.Result(i % 16 == 0); }
    r.criteria.spawn = 0; r.criteria.catalog = CatalogKey(*source->data); r.exhaustive = true; r.workers = 1;
    auto filtered = SeedSearch::RunFrozen(source, distances, r, [] { return false; });
    r.exhaustive = false; r.workers = 6;
    auto gpuFilter = SearchGpu(source, distances, r, 32, score, [] { return false; }, {}, audit);
    assert(gpuFilter.phase == SearchPhase::Completed && gpuFilter.filtered > 0 && SameTop(filtered.top, gpuFilter.top));
    r.criteria.spawn = -1; r.criteria.objective = SearchObjective::LongestRoute;
    auto longest = SeedSearch::RunFrozen(source, distances, r, [] { return false; });
    auto gpuLongest = SearchGpu(source, distances, r, 32, score, [] { return false; }, {}, audit);
    assert(gpuLongest.phase == SearchPhase::Completed && SameTop(longest.top, gpuLongest.top));
    r.criteria.objective = SearchObjective::ShortestRoute;
    auto invalid = SearchGpu(source, distances, r, 32,
        [](std::span<const SeedLayout>) { return std::vector<RouteResult>{}; }, [] { return false; });
    assert(invalid.phase == SearchPhase::Failed && !invalid.error.empty());
    auto wrong = SearchGpu(source, distances, r, 32, [&](auto layouts) {
        auto routes = score(layouts); routes[0].length += 1.; return routes;
    }, [] { return false; });
    assert(wrong.phase == SearchPhase::Failed && wrong.error.find("center-length") != std::string::npos);
    std::atomic<bool> stopped{false}; int packets = 0;
    auto canceled = SearchGpu(source, distances, r, 32, [&](auto layouts) {
        if (++packets == 3) stopped = true;
        return score(layouts);
    }, [&] { return stopped.load(); });
    assert(canceled.phase == SearchPhase::Stopped && canceled.evaluated < r.count);
    for (int batch : {1024, 4096, 8192})
    {
        auto large = SearchGpu(source, distances, r, batch, score, [] { return false; }, {}, audit, BoundsMode::Disabled);
        assert(large.phase == SearchPhase::Completed && SameTop(reference.top, large.top));
    }
    SuiteOptions options;
    assert(SuiteTestCount(options) == 1116 && options.levels.size() == 3);
    auto screen = Experiments(options, 7, Stage::Screen);
    auto stress = Experiments(options, 7, Stage::Stress);
    assert(screen.size() == 84 && stress.size() == 288);
    std::set<std::string> names;
    for (auto e : screen) { names.insert(e.Name()); assert(e.keyIndex < options.keys && e.count == 16384); }
    for (auto e : stress) { names.insert(e.Name()); assert(e.keyIndex >= options.keys); }
    assert(names.size() == screen.size() + stress.size());
    assert(stress.front().reference && stress.back().gpu && stress.back().spawn == 0);
    std::set<uint64_t> keys;
    for (int i = 0; i < 2 * options.keys; i++) keys.insert(SearchKey(options, i));
    assert(keys.size() == 6 && SearchKey(options, 0) == options.key);
    auto quick = options; quick.keys = quick.rounds = 1;
    quick.screenCount = quick.count = quick.hideSeekCount = 2048; quick.filterCount = 1024;
    assert(SuiteTestCount(quick) == 138);
    auto bad = options; bad.levels = {7, 5, 7};
    bool rejected = false; try { SuiteTestCount(bad); } catch (const std::runtime_error &) { rejected = true; }
    assert(rejected && std::string(ModeName(5)) == "Hide & Seek");
    SuiteState calibration; calibration.options = quick;
    for (auto e : Experiments(quick, 2, Stage::Screen)) if (e.gpu)
    {
        SearchRun test; test.experiment = e; test.topMatch = true;
        test.state.phase = SearchPhase::Completed; test.state.request.scene = 5; test.state.request.count = e.count;
        test.gpu.timestampBatches = 2;
        test.gpu.gpuKernelMs = 5 - int(e.shader);
        test.state.elapsed = e.shader == ShaderKind::Triangle ? .001 : .005;
        if (e.bounds != BoundsMode::Disabled) test.state.elapsed = .02;
        calibration.runs.push_back(test);
    }
    auto selected = Calibrate(calibration, 5);
    assert(selected.gpuTime && selected.shader == ShaderKind::CompactFixed && selected.bounds == BoundsMode::Disabled);
    calibration.runs.front().gpu.timestampInvalid = 1;
    selected = Calibrate(calibration, 5);
    assert(!selected.gpuTime && selected.shader == ShaderKind::Triangle);
    SuiteState suite; suite.options = options; suite.status = "Completed";
    SearchRun run; run.state = gpuFilter; run.topMatch = true; run.audited = gpuFilter.scored;
    suite.runs.push_back(run);
    auto text = SuiteJson(suite);
    assert(text.find("practice-suite.gpu-suite.v6") != std::string::npos && text.find("\"top10Match\":true") != std::string::npos);
    SaveJson(std::filesystem::path(argv[2]) / "Dev6-Summary.json", text);
    std::cout << "Full production CPU workers 1-6 / bounded GPU pipeline: exhaustive score audits, identical Top10, pruning, spawn filters, longest objective, errors, cancellation and automatic plan/JSON: PASS\n";
}
