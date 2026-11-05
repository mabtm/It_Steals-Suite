#include "seed_batch_fixture.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace practice;

static PhysicalPart WallPart(float x0, float z0, float x1, float z1)
{
    return {3, {{x0,0,z0}, {x1,0,z0}, {x1,0,z1}, {x0,0,z1}}};
}

// Synthetic counterpart of the actual Timer failure: the enemy's baked
// navigation separates the bunker spawn, while the physical door can open.
// Six spawn records (including duplicates) +53 points reproduce59/1932.
static MapData Timer(bool closed)
{
    auto d = SeedFixture();
    d.scene = 7;
    d.flags = 1;
    d.vertices.clear(); d.indices.clear(); d.walls.clear();
    d.spawns = {{0,1.5f,0}, {-12,1.5f,0}, {-12,1.5f,0},
                {0,1.5f,0}, {-12,1.5f,0}, {-12,1.5f,0}};
    d.points.clear(); d.samples.clear(); d.requested = d.completed = 0;
    for (int i = 0; i < 53; i++)
        d.points.push_back({-7.f - float(i % 8), 1.5f, -10.f + float(i / 8) * 3});
    FixtureTile(d, -25,-25,50,50);
    FixtureTile(d, -4,-4,8,8,.08f);
    d.physicalCaptured = true;
    d.agentRadius = .4f; d.agentHeight = 2; d.agentStep = .3f; d.agentSlope = 45;
    d.physicalParts = {WallPart(-4.2f,-4.2f,-4,4.2f), WallPart(-4,-4.2f,4.2f,-4),
                      WallPart(-4,4,4.2f,4.2f), WallPart(4,-4,4.2f,-1), WallPart(4,1,4.2f,4)};
    if (closed) d.physicalParts.push_back(WallPart(4,-1,4.2f,1));
    d.physicalParts.push_back({8, {{-25,0,-25},{25,0,-25},{25,0,25}}});
    d.physicalParts.push_back({8, {{-25,0,-25},{25,0,25},{-25,0,25}}});
    d.keep = 8;
    d.selected.resize(d.keep);
    std::iota(d.selected.begin(),d.selected.end(),0);
    d.liveSelected = d.selected;
    d.pickupRegions.clear();
    for (auto p : d.points) d.pickupRegions.push_back({true,p,.6f});
    d.rngStatus = 2; d.rngProbes = 4160; d.rngAdvance = 1; d.searchBackend = 1;
    d.CalculateBounds();
    return d;
}

static void Checked(const NavGraph &graph, const NavPath &path)
{
    assert(std::isfinite(path.length) && !path.points.empty());
    double length = 0;
    for (size_t i = 1; i < path.points.size(); i++)
    {
        assert(graph.Visible(path.points[i-1],path.points[i]));
        length += Distance(path.points[i-1],path.points[i]);
    }
    assert(std::abs(length-path.length) < 1e-7);
}

int main()
{
    auto d = Timer(false);
    NavGraph graph(d);
    auto started = std::chrono::steady_clock::now();
    int done = 0, total = 0;
    std::vector<std::string> stages;
    SearchDistances distances(d,graph,[]{return false;},[&](int n,int t){
        assert(n >= done && n <= t); done = n; total = t;
    },[&](auto stage){stages.emplace_back(stage);});
    assert(done == total && total == 1932 && stages.size() == 3);
    auto table = graph.Routes();
    assert(table->count == 59 && table->unverifiedPairs == 0);
    assert(table->checkedPairs + table->recoveredPairs == 1711);
    // This is a work regression, not a timing assertion. Previously the two
    // duplicate bunker starts each repaired a corridor to every other node.
    assert(table->repairAttempts == 1);
    for (int a = 0; a < table->count; a++)
        for (int b = a+1; b < table->count; b++)
        {
            assert(!table->estimates[a][b]);
            Checked(graph,table->Between(a,b));
        }
    for (int id = 6; id < 59; id++)
    {
        auto path = table->Between(0,id);
        bool eastExit = false;
        for (size_t i = 1; i < path.points.size(); i++)
        {
            auto a=path.points[i-1],b=path.points[i];
            if (a.x < 4 && b.x >= 4)
            {
                double z = a.z + (b.z-a.z)*(4-a.x)/(b.x-a.x);
                assert(std::abs(z) < .6); eastExit = true;
            }
        }
        assert(eastExit);
    }
    // Preparation must reach real CPU scoring, including pickup contacts.
    d.seed = RandomSeedOrder(FixtureSearchKey).SeedAt(0);
    d.recipe = 1; d.stage = 2; d.requestSerial = 100;
    std::string error;
    auto decoded = DecodeMap(EncodeFixture(d,14),error);
    if (!decoded) std::cerr << error << "\n";
    assert(decoded);
    auto source = std::make_shared<MapAnalysis>();
    source->data = decoded; source->live = true; source->session = 77;
    auto request = StreamingRequest(*source,32,2,true);
    auto result = SeedSearch::RunFrozen(source,distances,request,[]{return false;});
    assert(result.phase == SearchPhase::Completed && result.evaluated == 32 && result.generated == 32);
    assert(result.estimated == 0 && result.top.size() == 10);
    for (auto &row : result.top)
    {
        auto route = distances.Draw(row.selection,row.route,[]{return false;});
        assert(route.complete && !route.estimated);
        for (size_t i = 1; i < route.line.size(); i++)
            assert(graph.Visible(route.line[i-1],route.line[i]));
    }
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count();
    // A canceled call_once build must be retryable; no partial table is cached.
    NavGraph interrupted(d);
    bool cancel = false, stopped = false;
    try { interrupted.Routes([&]{return cancel;},[&](int n,int){if(n==59)cancel=true;}); }
    catch(const std::runtime_error &e){stopped=std::string(e.what())=="Canceled";}
    assert(stopped);
    assert(interrupted.Routes()->unverifiedPairs == 0);
    // With the actual doorway blocked, the bunker must remain disconnected.
    auto shut = Timer(true);
    NavGraph shutGraph(shut);
    auto shutTable = shutGraph.Routes();
    assert(shutTable->unverifiedPairs > 0 && shutTable->estimates[0][6]);
    auto start = shutGraph.CatalogProject(0,shut.spawns[0]);
    auto goal = shutGraph.CatalogProject(6,shut.points[0]);
    assert(!std::isfinite(shutGraph.Path(shutGraph.Search(start),goal).length));
    std::cout << "Timer59-node split capture: full1932-task preparation ->32 scored CPU seeds; all1711 pairs checked, one doorway repair, east-only exit, closed-door rejection, cancel/retry: PASS (" << seconds << "s)\n";
}
