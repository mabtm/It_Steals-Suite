#include "seed_batch_fixture.h"
#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>

using namespace practice;
using Clock = std::chrono::steady_clock;

static MapData Load(const char *path, int crystals, float contactRadius)
{
    std::ifstream in(path);
    MapData d;
    size_t spawns, points, vertices, indices, parts;
    in >> d.scene >> d.agentRadius >> d.agentHeight >> d.agentStep >> d.agentSlope >> d.agentFloor >>
        spawns >> points >> vertices >> indices >> parts;
    if (!in)
        throw std::runtime_error("Cannot read the asset-derived map fixture");
    d.spawns.resize(spawns);
    d.points.resize(points);
    d.vertices.resize(vertices);
    d.indices.resize(indices);
    d.physicalParts.resize(parts);
    for (auto *items : {&d.spawns, &d.points, &d.vertices})
        for (auto &p : *items)
            in >> p.x >> p.y >> p.z;
    for (auto &index : d.indices)
        in >> index;
    for (auto &part : d.physicalParts)
    {
        size_t count;
        in >> part.flags >> count;
        part.points.resize(count);
        for (auto &p : part.points)
            in >> p.x >> p.y >> p.z;
    }
    if (!in || crystals < 1 || crystals > 64 || crystals > int(points))
        throw std::runtime_error("Invalid captured fixture or crystal count");
    d.physicalCaptured = true;
    d.pid = 1234;
    d.epoch = 1;
    d.flags = 1;
    d.stage = 2;
    d.keep = crystals;
    d.rngStatus = 2;
    d.rngProbes = 4160;
    d.rngAdvance = 1;
    d.rngSite = 123;
    d.searchBackend = 1;
    d.selected.resize(crystals);
    std::iota(d.selected.begin(), d.selected.end(), 0);
    d.liveSelected = d.selected;
    if (contactRadius > 0)
        for (auto point : d.points)
            d.pickupRegions.push_back({true, point, contactRadius});
    d.requestSerial = 100;
    d.recipe = 1;
    d.seed = RandomSeedOrder(FixtureSearchKey).SeedAt(0);
    d.CalculateBounds();
    return d;
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "tests/fixtures/hide-seek-outside-boxes.routefixture";
    int count = argc > 2 ? std::stoi(argv[2]) : 1000;
    int crystals = argc > 3 ? std::stoi(argv[3]) : 52;
    int workers = argc > 4 ? std::stoi(argv[4]) : 1;
    float contactRadius = argc > 6 ? std::stof(argv[6]) : 0;
    auto source = std::make_shared<MapAnalysis>();
    source->data = std::make_shared<MapData>(Load(path, crystals, contactRadius));
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
        std::cout << "search," << crystals << ',' << workers << ',' << count << ',' << std::setprecision(17)
                  << seconds << ',' << count / seconds << ",prepare_ms=" << result->preparationMs
                  << ",checked=" << result->evaluated << ",generated=" << result->generated
                  << ",scored=" << result->scored << ",pruned=" << result->pruned;
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
        total += result.length;
        fingerprint = (fingerprint ^ std::bit_cast<uint64_t>(result.length)) * 1099511628211ULL;
        for (int id : result.order)
            fingerprint = (fingerprint ^ uint64_t(id + 1)) * 1099511628211ULL;
    }
    std::cout << std::setprecision(17) << "score," << crystals << ',' << count << ",generation=" << generation
              << ",bounds=" << bounds << ",scoring=" << scoring << ",layouts/s=" << count / scoring
              << ",lengths=" << total << ",fingerprint=" << fingerprint << '\n';
}
