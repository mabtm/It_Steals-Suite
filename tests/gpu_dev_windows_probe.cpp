#define SUITE_GPU_DEV 1
#include "GpuDevAutomation.h"
#include "gpu_order_fixture.h"
#include "seed_route_fixture.h"
#include <cassert>
#include <iostream>

int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    using namespace gpu_dev;
    std::filesystem::path fixture = argv[1], output = argv[2];
    auto available = Adapters();
    if (available.empty()) throw std::runtime_error("No D3D11 adapter available for the Windows dev tests");
    Report kernel;
    kernel.options.batch = 8;
    kernel.options.cachedCatalog = false;
    kernel.options.adapter = available.front().index;
    Device device(kernel, true);
    int checks = 0;
    for (int n : {1, 15, 16, 22, 31, 32, 33, 52})
        for (int profile = 0; profile < 5; profile++)
        {
            GpuOrderFixture f(n, profile, 0x719001);
            device.Pack(0, f.matrix.data(), n);
            auto result = device.Execute(1);
            assert(result[0].complete && result[0].order == f.expected.order && result[0].length == f.expected.length);
            checks++;
        }
    std::vector<GpuOrderFixture> fixtures;
    for (int n : {16, 33, 52}) fixtures.emplace_back(n, 1, 0x719002);
    int batchedChecks = 0;
    for (int batch : {16, 64, 128, 256})
    {
        device.Resize(batch, kernel);
        for (int i = 0; i < batch; i++)
        {
            auto &f = fixtures[size_t(i) % fixtures.size()];
            device.Pack(i, f.matrix.data(), int(f.expected.order.size()));
        }
        PairTiming timing; timing.batch = batch;
        device.BeginPass();
        for (int count : {batch, batch - 3})
        {
            auto results = device.Execute(count, &timing);
            for (int i = 0; i < count; i++)
            {
                auto &expected = fixtures[size_t(i) % fixtures.size()].expected;
                assert(results[i].complete && results[i].order == expected.order && results[i].length == expected.length);
                batchedChecks++;
            }
        }
        device.EndPass(timing, [] { return false; });
        if (kernel.timestampsSupported) assert(timing.timestampBatches + timing.timestampInvalid == 2);
        assert(timing.hostUploadMs >= 0 && timing.hostReadbackMs >= 0 && timing.queryCollectMs >= 0);
        kernel.timings.push_back(timing);
    }
    assert(kernel.gpuBufferBytes > 8000000 && batchedChecks == 916);
    kernel.phase = Phase::Completed;
    Save(output / "kernel-batches.json", kernel);
    Report compact;
    compact.options.adapter = available.front().index;
    compact.options.seeds = 4096; compact.options.batch = 256;
    Device cached(compact, true);
    int compactChecks = 0;
    for (int points : {71, 1024})
    {
        auto edge = [&](int a, int b) { return points == 71 ? double((a * 71 + b * 17) % 4)
            : 1. + ((a * 71 + b * 17) % 32) * 1e-7; };
        auto catalog = std::make_shared<Catalog>(3, points, edge);
        cached.SetCatalog(catalog, compact);
        std::vector<practice::Selection> layouts;
        std::vector<practice::RouteResult> expected;
        for (int n : {16, 33, 52})
        {
            practice::Selection selection; selection.spawn = n % 3;
            for (int i = 0; i < n; i++) selection.crystals.push_back((points - 1) * (n - 1 - i) / (n - 1));
            practice::RouteWorkspace work;
            for (int a = 0; a <= n; a++)
                for (int b = a; b <= n; b++)
                {
                    int from = a ? selection.crystals[a - 1] + 3 : selection.spawn;
                    int to = b ? selection.crystals[b - 1] + 3 : selection.spawn;
                    work.matrix[a * (n + 1) + b] = work.matrix[b * (n + 1) + a] = a == b ? 0. : edge(from, to);
                }
            expected.push_back(practice::SolveOrderFlat(work.matrix.data(), n + 1, work, [] { return false; }, false));
            layouts.push_back(std::move(selection));
        }
        for (int batch : points == 71 ? std::vector<int>{256, 1024, 2048} : std::vector<int>{8})
        {
            cached.Resize(batch, compact);
            for (int i = 0; i < batch; i++) cached.PackSelection(i, layouts[size_t(i) % layouts.size()]);
            PairTiming timing; timing.batch = batch;
            cached.BeginPass();
            for (int count : {batch, batch - 3})
            {
                auto results = cached.Execute(count, &timing);
                for (int i = 0; i < count; i++)
                {
                    const auto &r = results[i], &reference = expected[size_t(i) % 3];
                    assert(r.complete && r.order == reference.order && r.length == reference.length);
                    compactChecks++;
                }
            }
            cached.EndPass(timing, [] { return false; });
            if (compact.timestampsSupported) assert(timing.timestampBatches + timing.timestampInvalid == 2);
            compact.timings.push_back(timing);
        }
    }
    assert(compactChecks == 6660 && compact.inputBytesPerLayout == 216 && compact.catalogBytes > 12000000);
    compact.phase = Phase::Completed;
    Save(output / "catalog-kernel.json", compact);
    auto ignore = [](Phase, int, int, int) {};
    for (const auto &[name, crystals] : std::vector<std::pair<std::string,int>>{
             {"hide-seek-outside-boxes", 52}, {"hide-seek-boxes", 22}})
    {
        auto source = std::make_shared<practice::MapAnalysis>();
        source->data = std::make_shared<practice::MapData>(practice::LoadRouteBenchmarkFixture(
            (fixture / (name + ".routefixture")).string().c_str(), crystals, 1.35f));
        Options options;
        options.adapter = available.front().index;
        options.seeds = 8;
        options.batch = 4;
        options.key = FixtureSearchKey;
        options.fixture = true;
        auto r = Run(source, options, [] { return false; }, ignore, true);
        if (r.phase != Phase::Completed) throw std::runtime_error(r.error);
        assert(r.generated == 8 && r.checked == 24 && r.warmupChecked == 4 && r.mismatches == 0);
        assert(r.cases.size() == 7 && r.timings.size() == 3 && r.fp64);
        for (const auto &p : r.timings) assert(p.complete && p.cpuDigest == p.gpuDigest && p.cpuScored == 8 && p.gpuScored == 8);
        assert(r.model == "physical clearance + pickup contacts");
        for (const auto &c : r.cases)
            assert(c.orderMatch && c.scoreMatch && c.centerLengthMatch && c.cpu.pickupOptimized && c.gpu.pickupOptimized);
        Save(output / (name + ".json"), r);
        if (name == "hide-seek-boxes")
        {
            auto sweep = options;
            sweep.seeds = 17; sweep.pairs = 2; sweep.sweep = true;
            int checkpoints = 0;
            auto result = Run(source, sweep, [] { return false; }, ignore, true, [&](const Report &partial) {
                checkpoints++;
                Save(output / "checkpoints" / "latest.json", partial);
            });
            assert(result.phase == Phase::Completed && result.checked == 136 && result.mismatches == 0);
            assert(result.warmupChecked == 68 && result.timings.size() == 8 && checkpoints == 8);
            assert(result.timings[0].batch == 256 && result.timings[4].batch == 512);
            for (const auto &p : result.timings) assert(p.complete && p.cpuDigest == p.gpuDigest);
            Save(output / "batch-sweep.json", result);
            auto cancel = options; cancel.seeds = 17; cancel.batch = 16; cancel.pairs = 1;
            bool stop = false;
            auto partial = Run(source, cancel, [&] { return stop; }, [&](Phase p, int done, int, int) {
                if (p == Phase::Gpu && done >= 33) stop = true;
            }, true);
            assert(partial.phase == Phase::Stopped && partial.checked == 16 && partial.mismatches == 0);
            assert(partial.timings.size() == 1 && !partial.timings[0].complete);
            assert(partial.timings[0].cpuDigest == partial.timings[0].gpuDigest);
            Save(output / "partial-stop.json", partial);
            auto control = options;
            control.seeds = 3; control.pairs = 1; control.gpuTiming = false;
            auto unprofiled = Run(source, control, [] { return false; }, ignore, true);
            assert(unprofiled.phase == Phase::Completed && unprofiled.checked == 3 && unprofiled.mismatches == 0);
            assert(!unprofiled.timestampsSupported && !unprofiled.timings[0].timestampBatches && unprofiled.timings[0].queryCollectMs == 0);
            Save(output / "unprofiled.json", unprofiled);
            auto legacy = options; legacy.cachedCatalog = legacy.overlap = false;
            auto baseline = Run(source, legacy, [] { return false; }, ignore, true);
            assert(baseline.phase == Phase::Completed && baseline.checked == r.checked && baseline.mismatches == 0);
            assert(!baseline.catalogBytes && baseline.inputBytesPerLayout == 33292);
            for (size_t i = 0; i < baseline.timings.size(); i++) assert(baseline.timings[i].cpuDigest == r.timings[i].gpuDigest);
            Save(output / "legacy-input.json", baseline);
            Automation automaticSuite;
            automaticSuite.directory = output / "automatic-suite" / L"\u6e2c\u8a66";
            SuiteOptions automaticOptions;
            automaticOptions.count = 32; automaticOptions.rounds = 1; automaticOptions.workers = 2;
            automaticOptions.key = FixtureSearchKey; automaticOptions.adapter = available.front().index;
            automaticSuite.Start(automaticOptions, 42, 7, true);
            auto autoDeadline = Clock::now() + std::chrono::seconds(120);
            while (automaticSuite.Busy() && Clock::now() < autoDeadline)
            {
                int level = automaticSuite.Wanted();
                if (level >= 0)
                {
                    auto map = std::make_shared<practice::MapAnalysis>(*source);
                    auto data = std::make_shared<practice::MapData>(*source->data);
                    data->pid = 42; data->scene = kMapScenes[level]; data->epoch = 9;
                    map->data = data; map->session = 7;
                    assert(!automaticSuite.Offer(map, 42, data->scene, 8, 7));
                    bool accepted = automaticSuite.Offer(map, 42, data->scene, 9, 7);
                    if (!accepted) assert(automaticSuite.Wanted() != level || !automaticSuite.Busy());
                    automaticSuite.CheckContext(42, data->scene, 9, 7, false);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
            if (automaticSuite.Busy()) { automaticSuite.Stop(); throw std::runtime_error("Automatic suite did not finish"); }
            auto complete = automaticSuite.Get();
            if (complete->status != "Completed") throw std::runtime_error(complete->error);
            assert(complete->completedMaps == 2 && complete->runs.size() == 24 && complete->completedTests == complete->totalTests);
            for (const auto &test : complete->runs) assert(test.topMatch && test.audited == test.state.scored);
            assert(std::filesystem::is_regular_file(std::filesystem::path(complete->directory) / "Dev4-Summary.json"));
            Session session;
            session.directory = output / "automatic";
            auto automatic = options;
            automatic.seeds = automatic.pairs = 1;
            session.Start(source, automatic, {});
            auto deadline = Clock::now() + std::chrono::seconds(60);
            while (session.Busy() && Clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(5));
            assert(!session.Busy());
            auto saved = session.Get();
            assert(saved && !saved->savedPath.empty() && std::filesystem::is_regular_file(saved->savedPath));
        }
        options.adapter = std::numeric_limits<int>::max();
        auto failed = Run(source, options, [] { return false; }, ignore, true);
        assert(failed.phase == Phase::Failed && !failed.error.empty() && failed.checked == 0);
        Save(output / (name + "-unavailable.json"), failed);
        auto stopped = Run(source, options, [] { return true; }, ignore, true);
        assert(stopped.phase == Phase::Stopped && stopped.checked == 0);
        Save(output / (name + "-stopped.json"), stopped);
        if (r.adapter.software)
        {
            options.adapter = available.front().index;
            auto rejected = Run(source, options, [] { return false; }, ignore);
            assert(rejected.phase == Phase::Failed && rejected.adapter.software);
        }
    }
    std::cout << checks << " single + " << batchedChecks << " legacy + " << compactChecks
              << " catalog shader comparisons; live scoring fixtures, legacy/catalog parity, timing/control runs, partial cancellation, replacement checkpoints and automatic export: PASS\n";
}
