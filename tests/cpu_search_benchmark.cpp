#include "seed_batch_fixture.h"
#include <cassert>
#include <fstream>
#include <iostream>

int main(int argc, char **argv)
{
    const uint64_t count = argc > 1 ? std::stoull(argv[1]) : 1000000;
    const int crystals = argc > 4 ? std::stoi(argv[4]) : 18;
    assert(crystals >= 1 && crystals <= 64);
    auto fixture = SeedFixture();
    fixture.vertices.clear();
    fixture.indices.clear();
    fixture.walls.clear();
    fixture.spawns.clear();
    fixture.points.clear();
    fixture.samples.clear();
    fixture.requested = fixture.completed = 0;
    for (int i = 0; i < 9; i++)
        fixture.spawns.push_back({float(i * 3 + .17), 1, float(i * i % 19 + .31)});
    for (int i = 0; i < std::max(34, crystals + 16); i++)
        fixture.points.push_back({float(i * 1.73 + .28), 1, float(i * i % 31 * 1.31 + .47)});
    fixture.selected.clear();
    for (int i = 0; i < crystals; i++)
        fixture.selected.push_back(i);
    fixture.liveSelected = fixture.selected;
    fixture.keep = crystals;
    fixture.flags = 1;
    fixture.rngStatus = 2;
    fixture.rngProbes = 4160;
    fixture.rngInit = 0;
    fixture.rngRange = 0;
    fixture.rngAdvance = 1;
    fixture.rngSite = static_cast<int>(0xbe00001f);
    fixture.searchBackend = 1;
    fixture.recipe = 2;
    fixture.seed = practice::RandomSeedOrder(FixtureSearchKey).SeedAt(0);
    fixture.requestSerial = 100;
    fixture.stage = 2;
    auto source = std::make_shared<practice::MapAnalysis>();
    std::string error;
    source->data = practice::DecodeMap(EncodeFixture(fixture, 4), error);
    source->session = 77;
    source->live = true;
    assert(source->data);
    std::cout
        << "Synthetic 9-spawn/" << fixture.points.size() << "-point/" << crystals
        << "-crystal fixture; includes CPU generation, route distances, scoring, ranking and drawing. No live Unity or game FPS measurement.\n";
    if (argc < 3 || std::string_view(argv[2]) != "layout")
        for (int workers : {1, 2, 6})
            for (int repeat = 0; repeat < 3; repeat++)
            {
                if (argc > 3 && workers != std::stoi(argv[3]))
                    continue;
                practice::SeedSearch search;
                auto request = StreamingRequest(*source, count, workers, true);
                auto start = std::chrono::steady_clock::now();
                search.Begin(request);
                search.Update(source);
                auto result = AwaitStreaming(search);
                assert(result->phase == practice::SearchPhase::Completed && result->evaluated == count &&
                       result->generated == count && result->top.size() == 10);
                double seconds =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                std::cout << "route," << workers << ',' << repeat << ',' << seconds << ',' << count / seconds
                          << ',' << result->top.front().seed << ',' << result->top.front().route.length << ','
                          << result->scored << ',' << result->pruned << '\n';
            }
    for (int repeat = 0; repeat < (argc > 2 && std::string_view(argv[2]) == "route" ? 0 : 3); repeat++)
    {
        practice::SeedSearch million;
        auto request = StreamingRequest(*source, count * 10, 6, true);
        request.layoutOnly = true;
        auto start = std::chrono::steady_clock::now();
        million.Begin(request);
        million.Update(source);
        auto result = AwaitStreaming(million);
        assert(result->phase == practice::SearchPhase::Completed && result->evaluated == count * 10 &&
               result->generated == count * 10 && result->top.size() == 10 && result->filtered == 0);
        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        std::cout << "layout,6," << repeat << "," << seconds << ',' << count * 10 / seconds << ','
                  << result->top.front().seed << ",0\n";
    }
    std::ifstream memory("/proc/self/status");
    std::string line;
    while (std::getline(memory, line))
        if (line.rfind("VmHWM:", 0) == 0)
            std::cout << line << '\n';
}
