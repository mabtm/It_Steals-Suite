#include "seed_batch_fixture.h"
#include "PracticeMapWorker.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace practice;

static WallOutline WallQuad(float x, float z, float xx, float zz)
{
    return {{{{x, 0, z}, {xx, 0, z}, {xx, 0, zz}, {x, 0, zz}}}, false};
}

static void Profile(MapData &d)
{
    d.agentRadius = .35f;
    d.agentHeight = 1.8f;
    d.agentStep = .3f;
    d.agentSlope = 45;
    d.agentFloor = 0;
    d.obstacles = d.wallOutlines = {WallQuad(8, 0, 12, 12)};
}

static void FloorWithWall(MapData &d, float top = 12, float radius = .4f)
{
    d.vertices.clear();
    d.indices.clear();
    float left = 8 - radius, right = 12 + radius, high = top + radius;
    FixtureTile(d, 0, 0, left, high);
    FixtureTile(d, 0, high, left, 20 - high);
    FixtureTile(d, left, high, right - left, 20 - high);
    FixtureTile(d, right, 0, 20 - right, high);
    FixtureTile(d, right, high, 20 - right, 20 - high);
}

static void Clear(const NavPath &path, float top = 12, float radius = .35)
{
    assert(std::isfinite(path.length) && !path.points.empty());
    double length = 0;
    // Independent axis slabs, deliberately smaller than the planner margin.
    for (size_t i = 1; i < path.points.size(); i++)
    {
        auto a = path.points[i - 1], b = path.points[i];
        length += Distance(a, b);
        double lo = 0, hi = 1;
        bool intersects = true;
        double starts[] = {a.x, a.z}, ends[] = {b.x, b.z}, mins[] = {8 - radius, -radius},
               maxs[] = {12 + radius, top + radius};
        for (int axis = 0; axis < 2; axis++)
        {
            double delta = ends[axis] - starts[axis];
            if (std::abs(delta) < 1e-12)
            {
                if (starts[axis] < mins[axis] || starts[axis] > maxs[axis])
                {
                    intersects = false;
                    break;
                }
            }
            else
            {
                double p = (mins[axis] - starts[axis]) / delta, q = (maxs[axis] - starts[axis]) / delta;
                lo = std::max(lo, std::min(p, q));
                hi = std::min(hi, std::max(p, q));
                if (lo > hi)
                {
                    intersects = false;
                    break;
                }
            }
        }
        assert(!intersects);
    }
    assert(std::abs(length - path.length) < 1e-5);
}

static std::shared_ptr<const MapAnalysis> WaitMap(MapWorker &worker, int epoch)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto r = worker.Get(2);
        if (r && !r->working && r->data && r->data->epoch == epoch)
            return r;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("Map preparation timeout: " + worker.Error());
}

static MapData Many()
{
    MapData d;
    d.pid = 1234;
    d.scene = 5;
    d.epoch = 1;
    d.flags = 1;
    d.stage = 2;
    d.spawn = d.liveSpawn = 0;
    d.keep = 22;
    d.rngStatus = 2;
    d.rngProbes = 4160;
    d.rngAdvance = 1;
    d.rngSite = 123;
    d.searchBackend = 1;
    d.recipe = 2;
    d.requestSerial = 100;
    d.seed = RandomSeedOrder(FixtureSearchKey).SeedAt(0);
    FloorWithWall(d);
    d.walls = {{8, 0, 12, 12}};
    d.navigationStatus = 0;
    Profile(d);
    auto position = [](float t) -> Vec3 {
        if (t <= 16)
            return {2, 1, 2 + t};
        if (t <= 32)
            return {2 + t - 16, 1, 18};
        return {18, 1, 18 - (t - 32)};
    };
    d.spawns = {position(0), position(48)};
    for (int i = 0; i < 30; i++)
        d.points.push_back(position(1.5f + i * 45.f / 29));
    for (int i = 0; i < 22; i++)
        d.selected.push_back(i);
    d.liveSelected = d.selected;
    d.CalculateBounds();
    return d;
}

static void CapturedAsset(const char *file, int scene = 5)
{
    MapData d;
    d.scene = scene;
    d.agentRadius = .35f;
    d.agentHeight = 1.8f;
    d.agentStep = .3f;
    d.agentSlope = 45;
    d.agentFloor = -1.5031815f;
    d.flags = 1;
    d.spawn = 0;
    std::ifstream input(file);
    size_t vertices, indices;
    input >> vertices >> indices;
    assert(input && vertices > 800 && indices % 3 == 0);
    d.vertices.resize(vertices);
    d.indices.resize(indices);
    for (auto &p : d.vertices)
        input >> p.x >> p.y >> p.z;
    for (auto &i : d.indices)
        input >> i;
    assert(input);
    // Asset files contain no scene collider instances. Add an explicit remote
    // obstacle only to exercise captured floor routing, not claim live walls.
    d.obstacles = {WallQuad(1000, 1000, 1001, 1001)};
    auto stock = d;
    stock.scene = 3;
    NavGraph original(stock);
    std::map<int, std::vector<Vec3>> pieces;
    size_t roof = 0;
    for (size_t i = 0; i < d.indices.size(); i += 3)
    {
        auto a = d.vertices[d.indices[i]], b = d.vertices[d.indices[i + 1]], c = d.vertices[d.indices[i + 2]];
        auto p = Vec3{(a.x + b.x + c.x) / 3, (a.y + b.y + c.y) / 3, (a.z + b.z + c.z) / 3};
        if (p.y > 3)
            roof++;
        if (std::abs(p.y - d.agentFloor) > .4)
            continue;
        auto projection = original.Project(p);
        if (original.Component(projection) >= 0)
            pieces[original.Component(projection)].push_back(p);
    }
    assert(roof > 50);
    auto largest = std::max_element(pieces.begin(), pieces.end(),
                                    [](auto &a, auto &b) { return a.second.size() < b.second.size(); });
    assert(largest != pieces.end() && largest->second.size() > 100);
    auto &points = largest->second;
    d.spawns = {points.front()};
    d.spawns[0].y += 1;
    for (size_t i = 1; i < points.size(); i += std::max(size_t(1), points.size() / 30))
    {
        auto p = points[i];
        p.y += 1;
        d.points.push_back(p);
    }
    NavGraph player(d);
    size_t reached = 0;
    for (const auto &t : player.Floor())
        for (auto p : t)
            assert(p.y < 2);
    for (size_t i = 0; i < d.points.size(); i++)
    {
        auto path = player.Path(player.Search(player.CatalogProject(0, d.spawns[0])),
                                player.CatalogProject(int(i + 1), d.points[i]));
        if (!std::isfinite(path.length))
            continue;
        reached++;
        for (auto p : path.points)
            assert(p.y < 2); // Never route across roof polygons.
        // Independently sample each emitted leg against original floor triangles.
        for (size_t j = 1; j < path.points.size(); j++)
            for (int step = 0; step <= 20; step++)
            {
                auto a = path.points[j - 1], b = path.points[j];
                float f = step / 20.f;
                Vec3 p{a.x + (b.x - a.x) * f, a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f};
                bool supported = false;
                for (size_t k = 0; k < d.indices.size(); k += 3)
                {
                    auto x = d.vertices[d.indices[k]], y = d.vertices[d.indices[k + 1]],
                         z = d.vertices[d.indices[k + 2]];
                    if (std::max({x.y, y.y, z.y}) > 2)
                        continue;
                    double u = Area(x, y, p), v = Area(y, z, p), w = Area(z, x, p);
                    if ((u >= -.001 && v >= -.001 && w >= -.001) || (u <= .001 && v <= .001 && w <= .001))
                    {
                        supported = true;
                        break;
                    }
                }
                assert(supported);
            }
    }
    assert(reached == d.points.size());
    std::cout << "Uploaded Hide & Seek scene " + std::to_string(scene) + " NavMesh: " << vertices
              << " vertices, " << indices / 3 << " triangles, " << roof << " roof triangles, " << reached
              << " ground targets; all emitted legs on captured ground: PASS\n";
    // Actual NavMeshData world transform from these supplied assets. The
    // voxelized ground is at ~0.621, outside the former physical-bottom band.
    // Do not make the profile match the fixture's baked elevation by hand.
    auto world = d;
    auto transform = [](Vec3 &p) {
        p.x -= 6.3200407f;
        p.y += 2.1241815f;
        p.z -= 5.5100975f;
    };
    for (auto &p : world.vertices)
        transform(p);
    for (auto &p : world.spawns)
        transform(p);
    for (auto &p : world.points)
        transform(p);
    world.obstacles.clear();
    world.agentFloor = 0;
    size_t oldMatches = 0;
    for (size_t i = 0; i < world.indices.size(); i += 3)
    {
        bool inside = true;
        for (int k = 0; k < 3; k++)
            inside &= std::abs(world.vertices[world.indices[i + k]].y - world.agentFloor) <= .45f;
        if (inside)
            oldMatches++;
    }
    assert(oldMatches == 0);
    NavGraph transformed(world);
    assert(transformed.Floor().size() == player.Floor().size() && !transformed.Boundary().empty());
    for (float physicalBottom : {-5.f, 0.f, 1.4f, 6.f})
    {
        world.agentFloor = physicalBottom;
        NavGraph supported(world);
        assert(supported.Floor().size() == transformed.Floor().size());
        for (size_t i = 0; i < world.points.size(); i++)
        {
            auto path = supported.Path(supported.Search(supported.CatalogProject(0, world.spawns[0])),
                                       supported.CatalogProject(int(i + 1), world.points[i]));
            assert(std::isfinite(path.length));
            for (auto p : path.points)
                assert(p.y < 2 + 2.1241815f);
        }
    }
    world.agentHeight = world.agentSlope = 0;
    NavGraph legacyFloor(world, [] { return false; }, true);
    assert(!legacyFloor.Floor().empty() && !legacyFloor.Boundary().empty());
    std::cout
        << "World-coordinate asset regression: former height band selects ZERO triangles; catalog-supported floors/routing survive physical-bottom offsets and legacy missing profile: PASS\n";
}

int main(int argc, char **argv)
{
    assert(argc >= 4);
    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::string error;
    auto d = DecodeMap(bytes, error);
    assert(d && error.empty() && d->scene == 5 && !d->obstacles.empty());
    NavGraph player(*d);
    assert(player.UsesUnityCorridors());
    auto path = player.Path(player.Search(player.CatalogProject(0, d->spawns[0])),
                            player.CatalogProject(1, d->points[0]));
    Clear(path);
    assert(path.length > 23);
    auto analysis = AnalyzeMap(d, player);
    assert(analysis.route.complete && !analysis.route.estimated);
    Clear({analysis.route.line, analysis.route.length});
    // No native corridor capture needed, even when covered enemy triangles
    // previously returned an illegal straight path through this wall.
    auto raw = *d;
    FloorWithWall(raw);
    raw.navigationStatus = 0;
    raw.navigationModel = 0;
    raw.navigationQueries = raw.navigationTotal = 0;
    raw.navigationNodes.clear();
    raw.navigationEdges.clear();
    raw.navigationAnchors.clear();
    NavGraph common(raw);
    auto rebuilt = AnalyzeMap(std::make_shared<MapData>(raw), common);
    assert(!rebuilt.route.estimated);
    Clear({rebuilt.route.line, rebuilt.route.length});
    auto noGeometry = raw;
    noGeometry.obstacles.clear();
    assert(!AnalyzeMap(std::make_shared<MapData>(noGeometry), NavGraph(noGeometry)).route.estimated);
    auto meshBounds = noGeometry;
    meshBounds.obstacles = {WallQuad(-1, -1, 21, 21)};
    meshBounds.obstacles[0].approximate = true;
    auto noImaginaryWalls = AnalyzeMap(std::make_shared<MapData>(meshBounds), NavGraph(meshBounds));
    assert(!noImaginaryWalls.route.estimated &&
           std::abs(noImaginaryWalls.route.length -
                    AnalyzeMap(std::make_shared<MapData>(noGeometry), NavGraph(noGeometry)).route.length) <
               1e-6);
    auto outside = *d;
    outside.scene = 13;
    auto outsideRoute = AnalyzeMap(std::make_shared<MapData>(outside), NavGraph(outside));
    assert(!outsideRoute.route.estimated);
    Clear({outsideRoute.route.line, outsideRoute.route.length});
    auto roof = raw;
    FixtureTile(roof, 0, 0, 20, 20, 3.9f);
    auto grounded = AnalyzeMap(std::make_shared<MapData>(roof), NavGraph(roof));
    assert(!grounded.route.estimated && std::abs(grounded.route.length - rebuilt.route.length) < 1e-6);
    Clear({grounded.route.line, grounded.route.length});
    for (int scene : {5, 13})
    {
        auto airborne = roof;
        airborne.scene = scene;
        airborne.agentFloor = 20;
        NavGraph graph(airborne);
        auto ground = AnalyzeMap(std::make_shared<MapData>(airborne), graph);
        assert(ground.floor && !ground.floor->empty() && !graph.Boundary().empty() &&
               !ground.route.estimated);
        assert(std::abs(ground.route.length - rebuilt.route.length) < 1e-6);
        Clear({ground.route.line, ground.route.length});
        for (const auto &t : *ground.floor)
            for (auto p : t)
                assert(p.y < 1);
    }
    MapWorker worker;
    worker.SetDirectory(argv[2]);
    worker.Submit(EncodeFixture(raw), 77);
    assert(!WaitMap(worker, 1)->route.estimated);
    raw.epoch = 2;
    raw.obstacles[0] = WallQuad(8, 0, 12, 15);
    FloorWithWall(raw, 15);
    worker.Submit(EncodeFixture(raw), 77);
    auto changed = WaitMap(worker, 2);
    assert(changed->route.length > rebuilt.route.length + .1);
    Clear({changed->route.line, changed->route.length}, 15);
    raw.epoch = 3;
    raw.agentRadius = .7f;
    FloorWithWall(raw, 15, .75f);
    worker.Submit(EncodeFixture(raw), 77);
    auto wider = WaitMap(worker, 3);
    assert(wider->route.length > changed->route.length);
    Clear({wider->route.line, wider->route.length}, 15, .7);
    raw.epoch = 4;
    raw.agentFloor = 20;
    worker.Submit(EncodeFixture(raw), 77);
    auto displaced = WaitMap(worker, 4);
    assert(displaced->floor && !displaced->floor->empty() && !displaced->outline->empty() &&
           !displaced->route.estimated && std::abs(displaced->route.length - wider->route.length) < 1e-6);
    for (int scene : {3, 4, 7, 8, 14, 12})
    {
        auto other = *d;
        other.scene = scene;
        other.navigationModel = 0;
        NavGraph existing(other);
        assert(!existing.UsesUnityCorridors());
        auto p =
            existing.Path(existing.Search(existing.Project(d->spawns[0])), existing.Project(d->points[0]));
        assert(std::abs(p.length - 16) < 1e-6);
    }
    auto many = Many();
    auto source = std::make_shared<MapAnalysis>();
    source->data = DecodeMap(EncodeFixture(many), error);
    assert(source->data);
    source->session = 77;
    source->live = true;
    NavGraph model(*source->data);
    SearchDistances distances(*source->data, model, [] { return false; }, [](int, int) {});
    assert(distances.EstimatedPairs() == 0);
    std::shared_ptr<const SeedSearchState> reference;
    for (int workers : {1, 6})
    {
        SeedSearch search;
        auto request = StreamingRequest(*source, 2000, workers, true);
        search.Begin(request);
        search.Update(source);
        auto result = AwaitStreaming(search);
        assert(result->phase == SearchPhase::Completed && result->generated == 2000 &&
               result->evaluated == 2000 && result->estimated == 0 && result->filtered == 0 &&
               result->top.size() == 10);
        for (size_t i = 0; i < result->top.size(); i++)
        {
            const auto &row = result->top[i];
            auto score = distances.Score(row.selection, true, [] { return false; });
            auto draw = distances.Draw(row.selection, score, [] { return false; });
            Clear({draw.line, draw.length});
            if (reference)
            {
                assert(row.seed == reference->top[i].seed &&
                       SameLayout(row.selection, reference->top[i].selection));
                assert(std::abs(row.route.length - reference->top[i].route.length) < 1e-7);
            }
        }
        reference = result;
    }
    MapData ramp;
    ramp.scene = 13;
    ramp.agentRadius = .35f;
    ramp.agentHeight = 1.8f;
    ramp.agentSlope = 45;
    ramp.agentStep = .3f;
    ramp.spawn = 0;
    FixtureTile(ramp, 0, 0, 4, 4);
    int offset = int(ramp.vertices.size());
    ramp.vertices.insert(ramp.vertices.end(), {{4, 0, 0}, {8, 1, 0}, {8, 1, 4}, {4, 0, 4}});
    ramp.indices.insert(ramp.indices.end(), {offset, offset + 1, offset + 2, offset, offset + 2, offset + 3});
    FixtureTile(ramp, 8, 0, 4, 4, 1);
    FixtureTile(ramp, 0, 0, 12, 4, 4);
    ramp.spawns = {{1, 1, 2}};
    ramp.points = {{11, 2, 2}};
    NavGraph rampFloor(ramp);
    auto uphill = rampFloor.Path(rampFloor.Search(rampFloor.CatalogProject(0, ramp.spawns[0])),
                                 rampFloor.CatalogProject(1, ramp.points[0]));
    assert(std::isfinite(uphill.length));
    bool upper = false;
    for (const auto &t : rampFloor.Floor())
        for (auto p : t)
        {
            assert(p.y < 2);
            upper |= p.y == 1;
        }
    assert(upper);
    NavGraph floorOnly(ramp, [] { return false; }, true);
    assert(!floorOnly.Floor().empty() && !floorOnly.UsesUnityCorridors());
    auto outsideSearch = Many();
    outsideSearch.scene = 13;
    outsideSearch.keep = 52;
    outsideSearch.points.clear();
    outsideSearch.selected.clear();
    outsideSearch.liveSelected.clear();
    for (int i = 0; i < 70; i++)
    {
        float t = 1.5f + i * 45.f / 69;
        outsideSearch.points.push_back(t <= 16   ? Vec3{2, 1, 2 + t}
                                       : t <= 32 ? Vec3{2 + t - 16, 1, 18}
                                                 : Vec3{18, 1, 18 - (t - 32)});
        if (i < 52)
            outsideSearch.selected.push_back(i);
    }
    outsideSearch.liveSelected = outsideSearch.selected;
    auto outsideSource = std::make_shared<MapAnalysis>();
    outsideSource->data = DecodeMap(EncodeFixture(outsideSearch), error);
    assert(outsideSource->data);
    outsideSource->session = 77;
    outsideSource->live = true;
    reference.reset();
    for (int workers : {1, 6})
    {
        SeedSearch search;
        auto request = StreamingRequest(*outsideSource, 128, workers, true);
        search.Begin(request);
        search.Update(outsideSource);
        auto result = AwaitStreaming(search);
        assert(result->phase == SearchPhase::Completed && result->generated == 128 &&
               result->evaluated == 128 && result->estimated == 0 && result->filtered == 0 &&
               result->top.size() == 10);
        for (size_t i = 0; i < result->top.size(); i++)
        {
            assert(result->top[i].selection.crystals.size() == 52);
            if (reference)
                assert(result->top[i].seed == reference->top[i].seed &&
                       std::abs(result->top[i].route.length - reference->top[i].route.length) < 1e-7);
        }
        reference = result;
    }
    std::cout << "Outside: 52-crystal catalog, complete accounting and identical 1/6-worker Top 10: PASS\n";
    // The same captured surface must use the exact same corridor/funnel
    // algorithm in every mode. H&S keeps only the separate roof selection.
    auto parity = Many();
    std::vector<Vec3> referenceLine;
    double referenceLength = -1;
    for (int scene : {3, 4, 5, 7, 8, 14, 12, 13})
    {
        parity.scene = scene;
        NavGraph graph(parity);
        auto route = AnalyzeMap(std::make_shared<MapData>(parity), graph).route;
        assert(route.complete && !route.estimated);
        Clear({route.line, route.length});
        if (referenceLength < 0)
        {
            referenceLength = route.length;
            referenceLine = route.line;
        }
        else
        {
            assert(std::abs(route.length - referenceLength) < 1e-6 &&
                   route.line.size() == referenceLine.size());
            for (size_t k = 0; k < route.line.size(); k++)
                assert(Same(route.line[k], referenceLine[k]));
        }
    }
    std::cout
        << "Shared routing: byte-equivalent polylines and lengths across all eight modes on the same captured floor; no H&S auxiliary graph: PASS\n";
    bool canceled = false;
    try
    {
        NavGraph stop(*d, [] { return true; });
    }
    catch (const std::runtime_error &)
    {
        canceled = true;
    }
    assert(canceled);
    CapturedAsset(argv[3]);
    if (argc > 4)
        CapturedAsset(argv[4], 13);
    std::cout
        << "Shared captured-surface corridor routing, no auxiliary graph, roof exclusion, geometry cache invalidation, unchanged Classic/Bonus/Camera/Timer, 22-crystal 1/6-worker Top 10 and full accounting: PASS\n";
}
