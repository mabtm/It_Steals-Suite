#include "SeedSearch.h"
#include "RoutePlayback.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
using namespace practice;

// Frozen pre-7.11.4 multistart/2-opt reference: quality cannot regress on the
// same distance matrix when retaining every start and adding relocation.
static double PriorOrder(const std::vector<std::vector<double>> &m)
{
    int n = int(m.size()) - 1;
    double best = Unreachable;
    for (int first = 0; first < n; first++)
    {
        std::vector<int> order{first};
        std::vector<bool> used(n);
        used[first] = true;
        while (int(order.size()) < n)
        {
            int next = -1;
            for (int i = 0; i < n; i++)
                if (!used[i] && (next < 0 || m[order.back() + 1][i + 1] < m[order.back() + 1][next + 1]))
                    next = i;
            order.push_back(next);
            used[next] = true;
        }
        double length = m[0][order[0] + 1];
        for (int i = 1; i < n; i++)
            length += m[order[i - 1] + 1][order[i] + 1];
        for (int pass = 0; pass < 32; pass++)
        {
            bool improved = false;
            for (int a = 0; a < n - 1; a++)
                for (int b = a + 1; b < n; b++)
                {
                    int previous = a ? order[a - 1] + 1 : 0, next = b + 1 < n ? order[b + 1] + 1 : -1;
                    double delta = m[previous][order[b] + 1] - m[previous][order[a] + 1];
                    if (next >= 0)
                        delta += m[order[a] + 1][next] - m[order[b] + 1][next];
                    if (delta < -.000001)
                    {
                        std::reverse(order.begin() + a, order.begin() + b + 1);
                        length += delta;
                        improved = true;
                    }
                }
            if (!improved)
                break;
        }
        best = std::min(best, length);
    }
    return best;
}

static MapData Load(const std::string &path)
{
    std::ifstream in(path);
    MapData d;
    size_t s, p, v, i, n;
    in >> d.scene >> d.agentRadius >> d.agentHeight >> d.agentStep >> d.agentSlope >> d.agentFloor >> s >>
        p >> v >> i >> n;
    assert(in);
    d.spawns.resize(s);
    d.points.resize(p);
    d.vertices.resize(v);
    d.indices.resize(i);
    d.physicalParts.resize(n);
    for (auto *ar : {&d.spawns, &d.points, &d.vertices})
        for (auto &x : *ar)
            in >> x.x >> x.y >> x.z;
    for (auto &x : d.indices)
        in >> x;
    for (auto &part : d.physicalParts)
    {
        size_t count;
        in >> part.flags >> count;
        part.points.resize(count);
        for (auto &x : part.points)
            in >> x.x >> x.y >> x.z;
    }
    assert(in);
    d.physicalCaptured = true;
    d.flags = 1;
    d.spawn = 0;
    d.movementTick = .02f;
    d.movementSprintStep = .14f;
    return d;
}

static void CheckLine(const RouteResult &route, const NavGraph &graph)
{
    assert(route.complete && !route.estimated && !route.line.empty());
    double length = 0;
    for (size_t i = 1; i < route.line.size(); i++)
    {
        assert(RouteLegVerified(route, i - 1) && graph.Visible(route.line[i - 1], route.line[i]));
        length += Distance(route.line[i - 1], route.line[i]);
    }
    assert(std::abs(length - route.length) < .001);
}

int main(int argc, char **argv)
{
    for (auto objective : {SearchObjective::ShortestRoute, SearchObjective::LongestRoute})
    {
        std::vector<SeedResult> ranked;
        SeedResult unknown, checked;
        unknown.index = 0;
        unknown.selection = {0, {0}};
        unknown.route.complete = unknown.route.estimated = true;
        unknown.route.length = objective == SearchObjective::ShortestRoute ? 1 : 10000;
        checked.index = 1;
        checked.selection = {0, {1}};
        checked.route.complete = true;
        checked.route.length = 100;
        RankSeed(ranked, unknown, objective);
        RankSeed(ranked, checked, objective);
        assert(ranked.size() == 2 && !ranked[0].route.estimated && ranked[1].route.estimated);
    }
    RouteResult partial;
    partial.complete = partial.estimated = true;
    partial.line = {{1, 0, 1}, {3, 0, 1}, {11, 0, 1}, {13, 0, 1}};
    partial.estimatedLine = {0, 1, 0};
    auto slice = SuggestedSlice(partial, 1, 1);
    assert(slice.visible && slice.markerVisible && slice.hidden == std::vector<uint8_t>({0, 1, 0}));
    auto gap = SuggestedSlice(partial, .5, 1);
    assert(gap.visible && !gap.markerVisible && gap.hidden.back());
    auto unknown = partial;
    unknown.estimatedLine.clear();
    assert(!SuggestedSlice(unknown, 1, 1).visible);
    MapData a;
    a.pid = 5;
    a.scene = 13;
    a.epoch = 4;
    a.seed = -25;
    a.spawn = 0;
    a.recipe = 2;
    a.spawns = {{0, 1, 0}};
    a.points = {{1, 1, 0}, {2, 1, 0}};
    a.selected = {0, 1};
    auto b = a;
    b.flags = 128;
    b.navigationStatus = 1;
    b.capturedUtc = 125;
    b.agentFloor = .6f;
    assert(SameSeedPreviewLayout(a, b));
    b.seed++;
    assert(!SameSeedPreviewLayout(a, b));
    b = a;
    b.selected = {1, 0};
    assert(!SameSeedPreviewLayout(a, b));
    b = a;
    b.epoch++;
    assert(!SameSeedPreviewLayout(a, b));
    std::mt19937 random(714);
    double oldSeconds = 0, newSeconds = 0, totalGain = 0;
    int comparisons = 0;
    for (int n : {16, 22, 40, 52, 64})
        for (int caseNo = 0; caseNo < 10; caseNo++)
        {
            std::vector<Vec3> points;
            for (int i = 0; i <= n; i++)
                points.push_back({float(random() % 10000) / 100, 0, float(random() % 10000) / 100});
            std::vector<std::vector<double>> m(n + 1, std::vector<double>(n + 1));
            for (int i = 0; i <= n; i++)
                for (int j = 0; j <= n; j++)
                    m[i][j] = Distance(points[i], points[j]);
            auto started = std::chrono::steady_clock::now();
            double old = PriorOrder(m);
            oldSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            started = std::chrono::steady_clock::now();
            auto improved = SolveOrder(m);
            newSeconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
            assert(improved.complete && !improved.exact && improved.length <= old + 1e-5);
            auto ids = improved.order;
            std::sort(ids.begin(), ids.end());
            for (int i = 0; i < n; i++)
                assert(ids[i] == i);
            double length = 0;
            int last = 0;
            for (int index : improved.order)
            {
                length += m[last][index + 1];
                last = index + 1;
            }
            assert(std::abs(length - improved.length) < 1e-5);
            totalGain += (old - improved.length) / old;
            comparisons++;
        }
    std::cout << "All original starts retained; 16/22/40/52/64 crystal quality vs prior solver: "
              << comparisons << " cases, mean improvement " << 100 * totalGain / comparisons << "%, old "
              << oldSeconds << " s / new " << newSeconds
              << " s. Timing is a host measurement, not an FPS guarantee.\n";
    assert(argc == 3);
    for (int fi = 1; fi < argc; fi++)
    {
        auto d = Load(argv[fi]);
        NavGraph graph(d);
        auto started = std::chrono::steady_clock::now();
        auto table = graph.Routes();
        double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        assert(table->unverifiedPairs == 0 && table->recoveredPairs > 0 && graph.Routes() == table);
        int count = table->count;
        assert(table->checkedPairs + table->recoveredPairs == count * (count - 1) / 2);
        for (int i = 0; i < count; i++)
            for (int j = i + 1; j < count; j++)
            {
                auto path = table->Between(i, j);
                double length = 0;
                for (size_t k = 1; k < path.points.size(); k++)
                {
                    assert(graph.Visible(path.points[k - 1], path.points[k]));
                    length += Distance(path.points[k - 1], path.points[k]);
                }
                assert(std::abs(length - table->lengths[i][j]) < .001);
            }
        for (int keep : fi == 1 ? std::vector<int>{22} : std::vector<int>{40, 52})
        {
            auto full = std::make_shared<MapData>(d);
            full->keep = keep;
            full->selected.resize(keep);
            std::iota(full->selected.begin(), full->selected.end(), 0);
            auto analyzed = AnalyzeMap(full, graph);
            CheckLine(analyzed.route, graph);
            assert(EstimateGameTravel(analyzed.route.length, d.movementTick, d.movementSprintStep,
                                      analyzed.route.complete && !analyzed.route.estimated)
                       .valid);
            assert(SuggestedSlice(analyzed.route, 1, 1).visible);
            SearchDistances distances(d, graph, [] { return false; }, [](int, int) {});
            Selection selection;
            selection.spawn = 0;
            selection.crystals = full->selected;
            auto scored = distances.Score(selection, true, [] { return false; });
            auto draw = distances.Draw(selection, scored, [] { return false; });
            CheckLine(draw, graph);
            assert(std::abs(draw.length - analyzed.route.length) < .001);
            std::cout << "Scene " << d.scene << " / " << keep << " crystals: checked drawn route "
                      << draw.length << " units, time estimate present, preview/search agree.\n";
        }
        std::cout << "Asset-derived BoxCollider standing-band fixture scene " << d.scene << ": " << count
                  << " positions / " << count * (count - 1) / 2 << " checked pairs / "
                  << table->recoveredPairs << " recovered / 0 estimated / " << table->repairAttempts
                  << " repairs / " << seconds
                  << " s preparation. Scope: static fixture, not a live Unity capture.\n";
        auto stale = d;
        stale.navigationStatus = 2;
        stale.navigationNodes = {{0, 0, 0}};
        stale.navigationAnchors.assign(count, -1);
        NavGraph staleGraph(stale);
        assert(!staleGraph.UsesUnityCorridors());
        for (size_t i = 0; i < d.points.size(); i++)
            assert(staleGraph.CatalogProject(int(d.spawns.size() + i), d.points[i]).triangle >= 0);
        // One outlying catalog position must not let an empty old AI capture
        // erase every working floor route.
        stale.points.push_back({10000, 0, 10000});
        stale.navigationAnchors.push_back(-1);
        NavGraph partialGraph(stale);
        assert(!partialGraph.UsesUnityCorridors());
        assert(partialGraph.CatalogProject(0, d.spawns[0]).triangle >= 0);
        stale.selected = {0, 1};
        auto available = AnalyzeMap(std::make_shared<MapData>(stale), partialGraph);
        CheckLine(available.route, partialGraph);
        assert(SuggestedSlice(available.route, 1, 1).visible);
        assert(EstimateGameTravel(available.route.length, stale.movementTick, stale.movementSprintStep,
                                  !available.route.estimated)
                   .valid);
        // Actual Floor BoxCollider coordinates from both supplied scene assets.
        // Keep AI corridors for fast paths, but physical floor must also close a
        // missing AI strip without crossing the captured solid wall footprints.
        auto ground = d;
        Vec3 lo{-32.135997f, .6144456f, -22.084532f}, hi{43.495915f, .6144456f, 34.044336f};
        ground.physicalParts.push_back({8, {lo, {hi.x, lo.y, lo.z}, {hi.x, lo.y, hi.z}}});
        ground.physicalParts.push_back({8, {lo, {hi.x, lo.y, hi.z}, {lo.x, lo.y, hi.z}}});
        auto originalIndices = ground.indices;
        ground.indices.clear();
        for (size_t k = 0; k < originalIndices.size(); k += 3)
            if ((k / 3) % 3 != 0)
                ground.indices.insert(ground.indices.end(), originalIndices.begin() + k,
                                      originalIndices.begin() + k + 3);
        NavGraph groundGraph(ground);
        auto groundTable = groundGraph.Routes();
        assert(groundTable->unverifiedPairs == 0);
        for (int i = 0; i < groundTable->count; i++)
            for (int j = i + 1; j < groundTable->count; j++)
            {
                auto path = groundTable->Between(i, j);
                for (size_t k = 1; k < path.points.size(); k++)
                    assert(groundGraph.Visible(path.points[k - 1], path.points[k]));
            }
        std::cout << "Scene " << d.scene << ": actual physical floor closes fragmented AI floor; "
                  << groundTable->checkedPairs + groundTable->recoveredPairs << " checked pairs.\n";
        NavGraph retry(d);
        int calls = 0;
        bool canceled = false;
        try
        {
            retry.Routes([&] { return ++calls > 20; });
        }
        catch (const std::runtime_error &)
        {
            canceled = true;
        }
        assert(canceled && retry.Routes()->unverifiedPairs == 0);
    }
    std::cout
        << "Partial-route gaps never joined, stable layout identity, checked shortest-path reuse, cancellation retry, 22/40/52 targets, route/time visibility and unchanged seed identities: PASS\n";
}
