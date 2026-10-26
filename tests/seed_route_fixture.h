#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include "seed_batch_fixture.h"
#include <fstream>

namespace practice
{
inline MapData LoadRouteBenchmarkFixture(const char *path, int crystals, float contactRadius)
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

} // namespace practice
