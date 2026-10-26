#include "seed_route_fixture.h"
#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace practice;
using Clock = std::chrono::steady_clock;

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "tests/fixtures/hide-seek-outside-boxes.routefixture";
    int count = argc > 2 ? std::stoi(argv[2]) : 1000;
    int crystals = argc > 3 ? std::stoi(argv[3]) : 52;
    int workers = argc > 4 ? std::stoi(argv[4]) : 1;
    float contactRadius = argc > 6 ? std::stof(argv[6]) : 0;
    auto source = std::make_shared<MapAnalysis>();
    source->data = std::make_shared<MapData>(LoadRouteBenchmarkFixture(path, crystals, contactRadius));
    source->live = true;
    source->session = 77;
    std::cout
        << "Asset-derived geometry/catalog; synthetic deterministic RNG descriptor and selected layouts; no live Unity/FPS measurement.\n";
    if (argc > 5 && std::string_view(argv[5]) == "search")
    {
        SeedSearch search;
        auto warmup = StreamingRequest(*source, 128, workers, true);
        search.Begin(warmup);
        search.Update(source);
        if (AwaitStreaming(search)->phase != SearchPhase::Completed)
            throw std::runtime_error("Warmup search failed");
        auto request = StreamingRequest(*source, count, workers, true);
        auto started = Clock::now();
        search.Begin(request);
        search.Update(source);
        auto result = AwaitStreaming(search);
        if (result->phase != SearchPhase::Completed || result->evaluated != uint64_t(count))
            throw std::runtime_error(result->error);
        auto seconds = std::chrono::duration<double>(Clock::now() - started).count();
        std::cout << "search," << crystals << ',' << result->request.workers << ',' << count << ','
                  << std::setprecision(17) << seconds << ',' << count / seconds
                  << ",prepare_ms=" << result->preparationMs << ",checked=" << result->evaluated
                  << ",generated=" << result->generated << ",scored=" << result->scored
                  << ",pruned=" << result->pruned;
        for (auto &row : result->top)
        {
            uint64_t orderHash = 0;
            for (int id : row.route.order)
                orderHash = orderHash * 31 + id + 1;
            std::cout << ',' << row.index << ':' << row.route.length << ':' << orderHash;
        }
        std::cout << '\n';
        return 0;
    }
    auto started = Clock::now();
    NavGraph graph(*source->data);
    SearchDistances distances(*source->data, graph, [] { return false; }, [](int, int) {});
    std::cout << "preparation," << std::chrono::duration<double>(Clock::now() - started).count() << '\n';
    PortableSelectionGenerator generator(*source->data);
    RandomSeedOrder order(FixtureSearchKey);
    RouteWorkspace workspace;
    uint64_t fingerprint = 14695981039346656037ULL;
    double total = 0;
    double generation = 0, bounds = 0, scoring = 0;
    for (int i = 0; i < count; i++)
    {
        Selection selection;
        auto at = Clock::now();
        generator.Generate(order.SeedAt(i), 1, selection);
        generation += std::chrono::duration<double>(Clock::now() - at).count();
        at = Clock::now();
        int size = distances.FillBoundMatrix(selection, workspace);
        MstRouteLowerBound(workspace.matrix.data(), size, [] { return false; });
        bounds += std::chrono::duration<double>(Clock::now() - at).count();
        at = Clock::now();
        auto result = distances.Score(selection, true, [] { return false; }, workspace, true);
        scoring += std::chrono::duration<double>(Clock::now() - at).count();
        if (!result.complete)
            throw std::runtime_error("Incomplete route");
        if (argc > 7 && std::string_view(argv[7]) == "details")
        {
            uint64_t hash = 0;
            for (int id : result.order)
                hash = hash * 31 + id + 1;
            std::cout << "layout," << i << ',' << std::setprecision(17) << result.length << ',' << hash
                      << '\n';
        }
        total += result.length;
        fingerprint = (fingerprint ^ std::bit_cast<uint64_t>(result.length)) * 1099511628211ULL;
        for (int id : result.order)
            fingerprint = (fingerprint ^ uint64_t(id + 1)) * 1099511628211ULL;
    }
    std::cout << std::setprecision(17) << "score," << crystals << ',' << count << ",generation=" << generation
              << ",bounds=" << bounds << ",scoring=" << scoring << ",layouts/s=" << count / scoring
              << ",lengths=" << total << ",fingerprint=" << fingerprint << '\n';
}
