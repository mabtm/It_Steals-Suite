#include "PracticeMapWorker.h"
#include "route_order_reference.h"
#include "seed_map_fixture.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
#include <set>

static std::shared_ptr<const practice::MapAnalysis> Await(practice::MapWorker &worker, int level)
{
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < end)
    {
        auto result = worker.Get(level);
        if (result && !result->working)
            return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("Map worker timed out");
}

static void CheckPath(const practice::NavPath &path)
{
    assert(std::isfinite(path.length) && path.points.size() >= 2);
    // The fixture's two solid strips must never be crossed by any path edge.
    for (size_t i = 1; i < path.points.size(); i++)
        for (int s = 0; s <= 100; s++)
        {
            double t = s / 100.;
            auto a = path.points[i - 1], b = path.points[i];
            double x = a.x + (b.x - a.x) * t, z = a.z + (b.z - a.z) * t;
            assert(!((x > 10.001 && x < 14.999) || (x > 20.001 && x < 24.999)) || z <= 5.001 || z >= 24.999);
        }
}

int main(int argc, char **argv)
{
    assert(argc >= 2);
    auto folder = std::filesystem::path(argv[1]);
    std::filesystem::create_directories(folder);
    auto fixture = SeedFixture();
    std::string error;
    auto bytes = EncodeFixture(fixture);
    auto data = practice::DecodeMap(bytes, error);
    assert(data && error.empty() && data->samples.size() == 20 && data->selected == fixture.selected);
    auto legacy = practice::DecodeMap(EncodeFixture(fixture, 1), error);
    assert(legacy && legacy->requestSerial == 0 && legacy->selected == data->selected);
    practice::NavGraph graph(*data);
    auto search = graph.Search(graph.Project({2, 1, 17}));
    auto path = graph.Path(search, graph.Project({32, 1, 17}));
    CheckPath(path);
    assert(path.length > 30 && path.length < 100);
    auto duplicated = *data;
    auto copyIndices = duplicated.indices;
    duplicated.indices.insert(duplicated.indices.end(), copyIndices.begin(), copyIndices.end());
    practice::NavGraph welded(duplicated);
    auto duplicatePath = welded.Path(welded.Search(welded.Project({2, 1, 17})), welded.Project({32, 1, 17}));
    assert(std::abs(duplicatePath.length - path.length) < 1e-8 &&
           welded.Boundary().size() == graph.Boundary().size());
    assert(MapLevelIndex(12) == 6 && MapLevelIndex(13) == 7 && kMapLevelCount == 8);
    auto straight = graph.Path(graph.Search(graph.Project({2, 1, 2})), graph.Project({32, 1, 2}));
    assert(std::abs(straight.length - 30) < .01);
    auto analysis = practice::AnalyzeMap(data, graph);
    assert(analysis.route.complete && analysis.route.exact && analysis.route.order.size() == 4);
    practice::NavPath routePath;
    routePath.points = analysis.route.line;
    routePath.length = analysis.route.length;
    CheckPath(routePath);
    // Shared duplicate vertices weld correctly, but separate floors stay apart.
    practice::MapData disconnected;
    FixtureTile(disconnected, 0, 0, 10, 10);
    FixtureTile(disconnected, 30, 0, 10, 10);
    FixtureTile(disconnected, 0, 0, 10, 10, 10);
    practice::NavGraph split(disconnected);
    auto start = split.Search(split.Project({5, 1, 5}));
    assert(!std::isfinite(split.Path(start, split.Project({35, 1, 5})).length));
    assert(!std::isfinite(split.Path(start, split.Project({5, 11, 5})).length));
    assert(split.Project({1000, 0, 1000}).triangle < 0);
    // Native corridor packets connect real returned paths even when exported
    // triangles cannot reconstruct the same adjacency. Complete corridors stay preferred.
    practice::MapData native;
    native.pid = 1234;
    native.scene = 7;
    native.epoch = 1;
    native.flags = 1;
    native.stage = 1;
    native.keep = 2;
    native.spawn = native.liveSpawn = 0;
    native.spawns = {{2, 1, 2}};
    native.points = {{8, 1, 2}, {18, 1, 2}};
    native.selected = native.liveSelected = {0, 1};
    native.navigationStatus = 2;
    native.navigationNodes = {{2, 1, 2}, {8, 1, 2}, {18, 1, 2}};
    native.navigationEdges = {0, 1, 1, 2};
    native.navigationAnchors = {0, 1, 2};
    native.navigationQueries = native.navigationTotal = 3;
    auto unity = practice::DecodeMap(EncodeFixture(native, 3), error);
    assert(unity && error.empty());
    practice::NavGraph nativeGraph(*unity);
    assert(nativeGraph.UsesUnityCorridors() && !nativeGraph.Empty());
    auto nativeRoute = practice::AnalyzeMap(unity, nativeGraph);
    assert(nativeRoute.route.complete && std::abs(nativeRoute.route.length - 16) < 1e-8 &&
           nativeRoute.route.line.size() == 3);
    auto broken = native;
    broken.navigationEdges = {0, 99};
    assert(!practice::DecodeMap(EncodeFixture(broken, 3), error));
    broken = native;
    broken.navigationAnchors[1] = 99;
    assert(!practice::DecodeMap(EncodeFixture(broken, 3), error));
    broken = native;
    broken.navigationEdges = {0};
    assert(!practice::DecodeMap(EncodeFixture(broken, 3), error));
    broken = native;
    broken.navigationStatus = 1;
    broken.flags |= 128;
    assert(!practice::DecodeMap(EncodeFixture(broken, 3), error));
    broken = native;
    broken.navigationEdges.clear();
    auto islands = practice::DecodeMap(EncodeFixture(broken, 3), error);
    assert(islands);
    practice::NavGraph disconnectedNative(*islands);
    auto estimatedRoute = practice::AnalyzeMap(islands, disconnectedNative);
    assert(estimatedRoute.route.complete && estimatedRoute.route.estimated &&
           estimatedRoute.route.line.size() == 3 &&
           estimatedRoute.route.estimatedLine == std::vector<uint8_t>({1, 1}));
    assert(estimatedRoute.route.message.find("Estimated distances") != std::string::npos);
    // Independently enumerate every open tour and compare the DP solution.
    std::mt19937 random(123);
    for (int n = 1; n <= 8; n++)
    {
        std::vector<practice::Vec3> p;
        for (int i = 0; i <= n; i++)
            p.push_back({float(random() % 100), 0, float(random() % 100)});
        std::vector<std::vector<double>> distances(n + 1, std::vector<double>(n + 1));
        for (int i = 0; i <= n; i++)
            for (int j = 0; j <= n; j++)
                distances[i][j] = practice::Distance(p[i], p[j]);
        auto result = practice::SolveOrder(distances);
        std::vector<int> order(n);
        std::iota(order.begin(), order.end(), 0);
        double best = practice::Unreachable;
        do
        {
            int previous = 0;
            double length = 0;
            for (int i : order)
            {
                length += distances[previous][i + 1];
                previous = i + 1;
            }
            best = std::min(best, length);
        } while (std::next_permutation(order.begin(), order.end()));
        assert(result.complete && result.exact && std::abs(result.length - best) < .00001);
        std::vector<double> flat;
        for (auto &row : distances)
            flat.insert(flat.end(), row.begin(), row.end());
        assert(practice::MstRouteLowerBound(flat.data(), n + 1, [] { return false; }) <= best + 1e-8);
    }
    int n = 20;
    std::vector<std::vector<double>> distances(n + 1, std::vector<double>(n + 1));
    for (int i = 0; i <= n; i++)
        for (int j = 0; j <= n; j++)
            distances[i][j] = std::abs(i - j);
    practice::RouteWorkspace work;
    for (int n : {14, 1, 15, 8, 22, 40, 64})
        for (bool exact : {false, true})
        {
            std::vector<std::vector<double>> matrix(n + 1, std::vector<double>(n + 1, 0));
            for (int a = 0; a <= n; a++)
                for (int b = a + 1; b <= n; b++)
                    matrix[a][b] = matrix[b][a] = 1 + double(random() % 10000) / 100;
            for (int a = 0; a <= n; a++)
                std::copy(matrix[a].begin(), matrix[a].end(), work.matrix.begin() + a * (n + 1));
            auto previous = route_reference::SolvePreviousOrder(matrix, [] { return false; }, exact);
            auto current =
                practice::SolveOrderFlat(work.matrix.data(), n + 1, work, [] { return false; }, exact);
            assert(current.complete && current.exact == previous.exact);
            if (current.exact)
            {
                assert(current.order == previous.order && current.length == previous.length);
            }
            else
            {
                assert(current.length <= previous.length + .00001);
                auto ids = current.order;
                std::sort(ids.begin(), ids.end());
                for (int i = 0; i < n; i++)
                    assert(ids[i] == i);
                double length = 0;
                int from = 0;
                for (int id : current.order)
                {
                    length += matrix[from][id + 1];
                    from = id + 1;
                }
                assert(std::abs(length - current.length) < .00001);
            }
        }
    auto heuristic = practice::SolveOrder(distances);
    assert(heuristic.complete && !heuristic.exact && heuristic.length == 20 &&
           std::set<int>(heuristic.order.begin(), heuristic.order.end()).size() == 20);
    distances[0][1] = practice::Unreachable;
    assert(!practice::SolveOrder(distances).complete);
    bool canceled = false;
    try
    {
        practice::NavGraph stop(*data, [] { return true; });
    }
    catch (const std::runtime_error &)
    {
        canceled = true;
    }
    assert(canceled);
    canceled = false;
    try
    {
        practice::SolveOrder(std::vector<std::vector<double>>(21, std::vector<double>(21, 1)),
                             [] { return true; });
    }
    catch (const std::runtime_error &)
    {
        canceled = true;
    }
    assert(canceled);
    for (size_t size = 0; size < bytes.size(); size++)
    {
        auto truncated = bytes;
        truncated.resize(size);
        assert(!practice::DecodeMap(std::move(truncated), error));
    }
    for (int i = 0; i < 10000; i++)
    {
        auto bad = bytes;
        int mutations = 1 + int(random() % 5);
        for (int j = 0; j < mutations; j++)
            bad[random() % bad.size()] = static_cast<uint8_t>(random());
        auto result = practice::DecodeMap(std::move(bad), error);
        if (result)
            assert(result->spawns.size() <= 128 && result->samples.size() <= 2000);
    }
    auto invalid = fixture;
    invalid.indices[0] = 999999;
    assert(!practice::DecodeMap(EncodeFixture(invalid), error));
    invalid = fixture;
    invalid.points[0].x = std::numeric_limits<float>::quiet_NaN();
    assert(!practice::DecodeMap(EncodeFixture(invalid), error));
    invalid = fixture;
    invalid.selected[1] = invalid.selected[0];
    assert(!practice::DecodeMap(EncodeFixture(invalid), error));
    // Actual background worker persistence, then offline reload and corruption.
    auto file = folder / "SeedMaps/scene_3.ismap";
    std::filesystem::remove(file);
    {
        practice::MapWorker worker;
        worker.SetDirectory(folder);
        worker.Submit(bytes);
        auto result = Await(worker, 0);
        assert(result->live && result->route.complete && result->cacheWarning.empty() &&
               std::filesystem::exists(file));
        practice::MapData unavailable;
        unavailable.pid = 1234;
        unavailable.scene = 3;
        unavailable.stage = 3;
        unavailable.message = "Load this level first";
        worker.Submit(EncodeFixture(unavailable));
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline)
        {
            auto updated = worker.Get(0);
            if (updated && !updated->error.empty())
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(worker.Get(0)->route.complete && !worker.Get(0)->live &&
               worker.Get(0)->error == unavailable.message);
        worker.InvalidateLive();
        assert(!worker.Get(0)->live);
    }
    {
        practice::MapWorker worker;
        worker.SetDirectory(folder);
        worker.LoadCache(0);
        auto result = Await(worker, 0);
        assert(!result->live && result->route.complete);
    }
    {
        practice::MapWorker worker;
        worker.SetDirectory(folder);
        worker.LoadCaches();
        auto result = Await(worker, 0);
        assert(result->needsAnalysis && !result->route.complete);
        worker.AnalyzeSaved(0);
        result = Await(worker, 0);
        assert(!result->live && result->route.complete);
    }
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        out << "corrupt";
    }
    {
        practice::MapWorker worker;
        worker.SetDirectory(folder);
        worker.LoadCache(0);
        auto result = Await(worker, 0);
        assert(!result->error.empty() && !result->live);
    }
    if (argc == 3)
    {
        std::ifstream in(argv[2], std::ios::binary);
        std::vector<uint8_t> managed{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        auto packet = practice::DecodeMap(std::move(managed), error);
        assert(packet && packet->scene == 3 && packet->pid == 6789 && packet->seed == -123 &&
               packet->points[0].z == 3.75f && packet->samples.size() == 1 && packet->requestSerial == 99);
    }
    if (argc == 3)
    {
        auto file = std::filesystem::path(argv[2]).parent_path() / "native-navigation-wire.bin";
        std::ifstream in(file, std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        auto fromMod = practice::DecodeMap(bytes, error);
        assert(fromMod && fromMod->navigationStatus == 2);
        practice::NavGraph paths(*fromMod);
        assert(practice::AnalyzeMap(fromMod, paths).route.length == 16);
        for (int i = 0; i < 3000; i++)
        {
            auto bad = bytes;
            bad[random() % bad.size()] = uint8_t(random());
            auto decoded = practice::DecodeMap(bad, error);
            if (decoded)
                assert(decoded->navigationNodes.size() <= 10000 && decoded->navigationEdges.size() <= 60000);
        }
    }
    if (argc == 3)
    {
        auto reviewPath = std::filesystem::path(argv[2]).parent_path() / "review-wire.bin";
        std::ifstream in(reviewPath, std::ios::binary);
        std::vector<uint8_t> recorded{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        auto d = practice::DecodeMap(recorded, error);
        assert(d && d->review && d->seed == -674714359 && d->review->trace.size() == 6 &&
               d->review->collected == std::vector<int>({1, 0}) &&
               d->review->pickupSeconds == std::vector<double>({.3, .4}));
        assert(d->wallOutlines.size() == 1 && d->wallOutlines[0].corners[0].x == 7.5f &&
               d->obstacles.size() == 1);
        practice::NavGraph safe(*d);
        auto analysis = practice::AnalyzeMap(d, safe);
        assert(!analysis.route.estimated && analysis.collectedRoute.complete &&
               !analysis.collectedRoute.estimated && analysis.collectedRoute.order.size() == 2);
        assert(std::abs(analysis.playedDistance - 18) < 1e-8 &&
               std::abs(analysis.playedToLastPickup - 14) < 1e-8 &&
               std::abs(analysis.collectedRoute.length - 6) < 1e-8);
        for (size_t length = 0; length < recorded.size(); length++)
        {
            auto truncated = recorded;
            truncated.resize(length);
            assert(!practice::DecodeMap(std::move(truncated), error));
        }
        auto invalid = *d;
        auto bad = std::make_shared<practice::RunReview>(*d->review);
        bad->collected = {2, 2};
        invalid.review = bad;
        assert(!practice::DecodeMap(EncodeFixture(invalid, 7), error));
        bad = std::make_shared<practice::RunReview>(*d->review);
        bad->trace[0].seconds = -.1;
        invalid.review = bad;
        assert(!practice::DecodeMap(EncodeFixture(invalid, 7), error));
        bad = std::make_shared<practice::RunReview>(*d->review);
        bad->pickupSeconds = {.3, .2};
        invalid.review = bad;
        assert(!practice::DecodeMap(EncodeFixture(invalid, 7), error));
        auto root = folder / "review-cache";
        practice::MapWorker worker;
        worker.SetDirectory(root);
        worker.Submit(recorded, 77);
        auto result = Await(worker, 2);
        assert(result->data->review && !result->live && result->playedDistance == 18 &&
               !std::filesystem::exists(root / "SeedMaps/scene_5.ismap"));
        std::cout
            << "C# run-review wire, exact timed pickup subset, recorded distance, malformed traces and review/live-cache isolation: PASS\n";
    }
    std::cout
        << "Seed map decoder, 10,000 malformed packets, navigation holes/floors, exact/heuristic tours, cancellation and atomic map cache: PASS\n";
}
