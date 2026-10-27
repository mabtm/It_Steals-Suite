#include "GpuDevSession.h"
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
            assert(result.warmupChecked == 67 && result.timings.size() == 8 && checkpoints == 8);
            assert(result.timings[0].batch == 16 && result.timings[4].batch == 64);
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
    std::cout << checks << " single + " << batchedChecks << " full/tail embedded shader checks; 184 paired pickup scores, rotating sweep, timing queries, partial cancellation, replacement checkpoints and automatic export: PASS\n";
}
