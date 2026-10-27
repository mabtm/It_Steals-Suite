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
        auto r = Run(source, options, [] { return false; }, [](Phase, int) {}, true);
        if (r.phase != Phase::Completed) throw std::runtime_error(r.error);
        assert(r.generated == 8 && r.checked == 24 && r.warmupChecked == 1 && r.mismatches == 0);
        assert(r.cases.size() == 25 && r.timings.size() == 3 && r.fp64);
        assert(r.model == "physical clearance + pickup contacts");
        for (const auto &c : r.cases)
            assert(c.orderMatch && c.scoreMatch && c.centerLengthMatch && c.cpu.pickupOptimized && c.gpu.pickupOptimized);
        Save(output / (name + ".json"), r);
        if (name == "hide-seek-boxes")
        {
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
        auto failed = Run(source, options, [] { return false; }, [](Phase, int) {}, true);
        assert(failed.phase == Phase::Failed && !failed.error.empty() && failed.checked == 0);
        Save(output / (name + "-unavailable.json"), failed);
        auto stopped = Run(source, options, [] { return true; }, [](Phase, int) {}, true);
        assert(stopped.phase == Phase::Stopped && stopped.checked == 0);
        Save(output / (name + "-stopped.json"), stopped);
        if (r.adapter.software)
        {
            options.adapter = available.front().index;
            auto rejected = Run(source, options, [] { return false; }, [](Phase, int) {});
            assert(rejected.phase == Phase::Failed && rejected.adapter.software);
        }
    }
    std::cout << checks << " embedded-shader order comparisons; 48 complete CPU/GPU pickup scores, adapter failure, cancellation, software rejection and JSON export: PASS\n";
}
