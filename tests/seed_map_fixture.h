#pragma once
#include "PracticeMapData.h"

// Synthetic navigation geometry, never presented as captured game data.
inline std::vector<uint8_t> EncodeFixture(const practice::MapData &d, int format = 0)
{
    if (!format)
        format = d.physicalCaptured || !d.pickupRegions.empty()    ? 13
                 : d.review && d.review->motion                    ? 12
                 : d.movement                                      ? 11
                 : d.review || d.movementTick > 0                  ? 10
                 : !d.wallOutlines.empty() || !d.obstacles.empty() ? 7
                 : d.navigationModel                               ? 6
                 : d.rngStatus                                     ? 4
                 : d.navigationStatus                              ? 3
                                                                   : 2;
    std::vector<uint8_t> bytes;
    auto value = [&]<class T>(T v) {
        auto start = bytes.size();
        bytes.resize(start + sizeof(T));
        std::memcpy(bytes.data() + start, &v, sizeof(T));
    };
    auto text = [&](const std::string &s) {
        value(static_cast<int32_t>(s.size()));
        bytes.insert(bytes.end(), s.begin(), s.end());
    };
    auto ints = [&](const std::vector<int> &items) {
        value(static_cast<int32_t>(items.size()));
        for (int i : items)
            value(static_cast<int32_t>(i));
    };
    auto vectors = [&](const std::vector<practice::Vec3> &items) {
        value(static_cast<int32_t>(items.size()));
        for (auto p : items)
        {
            value(p.x);
            value(p.y);
            value(p.z);
        }
    };
    value(0x504D5349u);
    value(int32_t(format));
    for (int i : {d.pid, d.scene, d.epoch, d.seed, d.recipe, d.sourceKind, d.spawn, d.keep, d.flags, d.stage,
                  d.requested, d.completed, d.liveSpawn, d.liveSeed, d.liveRecipe})
        value(static_cast<int32_t>(i));
    value(d.capturedUtc);
    if (format >= 2)
        value(d.requestSerial);
    text(d.hash);
    text(d.message);
    vectors(d.spawns);
    vectors(d.points);
    ints(d.selected);
    ints(d.liveSelected);
    vectors(d.vertices);
    ints(d.indices);
    value(static_cast<int32_t>(d.walls.size()));
    for (auto w : d.walls)
    {
        value(w.minX);
        value(w.minZ);
        value(w.maxX);
        value(w.maxZ);
    }
    value(static_cast<int32_t>(d.samples.size()));
    for (const auto &s : d.samples)
    {
        value(static_cast<int32_t>(s.spawn));
        ints(s.crystals);
    }
    if (format >= 3)
    {
        value(int32_t(d.navigationStatus));
        value(int32_t(d.navigationQueries));
        value(int32_t(d.navigationTotal));
        text(d.navigationNote);
        vectors(d.navigationNodes);
        ints(d.navigationEdges);
        ints(d.navigationAnchors);
    }
    if (format >= 4)
    {
        for (int i :
             {d.rngStatus, d.rngInit, d.rngRange, d.rngAdvance, d.rngSite, d.rngProbes, d.searchBackend})
            value(int32_t(i));
        text(d.rngNote);
    }
    if (format >= 6)
        value(int32_t(d.navigationModel));
    if (format >= 7)
    {
        auto outlines = [&](const std::vector<practice::WallOutline> &items) {
            value(int32_t(items.size()));
            for (const auto &w : items)
            {
                value(uint8_t(w.approximate));
                vectors(std::vector<practice::Vec3>(w.corners.begin(), w.corners.end()));
            }
        };
        outlines(d.wallOutlines);
        for (float v : {d.agentRadius, d.agentHeight, d.agentStep, d.agentSlope, d.agentFloor})
            value(v);
        outlines(d.obstacles);
        value(uint8_t(bool(d.review)));
        if (d.review)
        {
            const auto &r = *d.review;
            value(r.id);
            value(int32_t(r.outcome));
            value(r.seconds);
            value(uint8_t(r.reduced));
            value(int32_t(r.trace.size()));
            for (auto p : r.trace)
            {
                value(p.seconds);
                value(p.point.x);
                value(p.point.y);
                value(p.point.z);
            }
            ints(r.collected);
            value(int32_t(r.pickupSeconds.size()));
            for (double time : r.pickupSeconds)
                value(time);
            if (format >= 8)
            {
                value(uint8_t(r.validationChecked));
                value(uint8_t(r.valid));
                value(uint8_t(r.observedFromStart));
                value(uint8_t(r.seedKnown));
                value(int32_t(r.seedKind));
                value(r.aids);
                value(r.warnings);
                text(r.validityReason);
                value(uint8_t(r.monstersReduced));
                value(int32_t(r.monsters.size()));
                for (const auto &m : r.monsters)
                {
                    value(int32_t(m.id));
                    value(int32_t(m.kind));
                    value(int32_t(m.trace.size()));
                    for (auto p : m.trace)
                    {
                        value(p.seconds);
                        value(p.point.x);
                        value(p.point.y);
                        value(p.point.z);
                        value(uint8_t(p.continuous));
                    }
                    value(int32_t(m.paths.size()));
                    for (const auto &path : m.paths)
                    {
                        value(path.seconds);
                        vectors(path.corners);
                    }
                }
                value(int32_t(r.flashes.size()));
                for (const auto &f : r.flashes)
                {
                    value(f.seconds);
                    value(f.player.x);
                    value(f.player.y);
                    value(f.player.z);
                    value(int32_t(f.kind));
                    value(int32_t(f.actor));
                }
                if (format >= 10)
                {
                    value(r.lightSources);
                    value(int32_t(r.lightRejected));
                }
            }
        }
    }
    if (format >= 10)
    {
        value(d.movementTick);
        value(d.movementSprintStep);
    }
    if (format >= 11)
    {
        value(uint8_t(bool(d.movement)));
        if (d.movement)
        {
            const auto &m = *d.movement;
            value(m.scene);
            value(m.revision);
            value(m.rejected);
            value(m.tick);
            value(m.radius);
            value(m.height);
            for (int k = 0; k < 5; k++)
            {
                value(m.counts[k]);
                for (int q = 0; q < 3; q++)
                    value(m.speeds[k * 3 + q]);
                value(m.lengths[k]);
            }
        }
    }
    if (format >= 12)
    {
        value(uint8_t(bool(d.review && d.review->motion)));
        if (d.review && d.review->motion)
        {
            const auto &m = *d.review->motion;
            value(int32_t(m.frames));
            value(int32_t(m.ticks));
            value(int32_t(m.events));
            value(int32_t(m.dropped));
            value(m.flags);
            text(m.tool);
            text(m.game);
            text(m.unity);
        }
    }
    if (format >= 13)
    {
        value(uint8_t(d.physicalCaptured));
        value(int32_t(d.physicalOmitted));
        value(int32_t(d.physicalParts.size()));
        for (const auto &p : d.physicalParts)
        {
            value(p.flags);
            vectors(p.points);
        }
        value(int32_t(d.pickupRegions.size()));
        for (const auto &p : d.pickupRegions)
        {
            value(uint8_t(p.verified));
            value(p.center.x);
            value(p.center.y);
            value(p.center.z);
            value(p.radius);
        }
    }
    if (format >= 8)
    {
        uint64_t hash = 14695981039346656037ull;
        for (auto b : bytes)
        {
            hash ^= b;
            hash *= 1099511628211ull;
        }
        value(hash);
    }
    return bytes;
}

inline void FixtureTile(practice::MapData &d, float x, float z, float width, float depth, float y = 0)
{
    int i = static_cast<int>(d.vertices.size());
    d.vertices.insert(d.vertices.end(),
                      {{x, y, z}, {x + width, y, z}, {x + width, y, z + depth}, {x, y, z + depth}});
    d.indices.insert(d.indices.end(), {i, i + 1, i + 2, i, i + 2, i + 3});
}

inline practice::MapData SeedFixture()
{
    practice::MapData d;
    d.pid = 1234;
    d.scene = 3;
    d.epoch = 1;
    d.seed = 482915720;
    d.recipe = 0;
    d.sourceKind = 1;
    d.spawn = d.liveSpawn = 0;
    d.keep = 4;
    d.flags = 1 | 8;
    d.stage = 1;
    d.liveSeed = d.seed;
    d.liveRecipe = 1;
    d.hash = std::string(64, 'a');
    d.message = "Synthetic map fixture for UI and routing tests.";
    d.capturedUtc = 638800000000000000LL;
    for (int x = 0; x < 7; x++)
        for (int z = 0; z < 6; z++)
            if (!((x == 2 || x == 4) && z > 0 && z < 5))
                FixtureTile(d, float(x * 5), float(z * 5), 5, 5);
    d.walls = {{10, 5, 15, 25}, {20, 5, 25, 25}};
    d.spawns = {{2, 1, 2}, {32, 1, 27}};
    d.points = {{2, 1, 27}, {17, 1, 22}, {32, 1, 27}, {32, 1, 2}, {17, 1, 2}, {7, 1, 17}};
    d.selected = d.liveSelected = {0, 1, 2, 3};
    for (int i = 0; i < 20; i++)
    {
        practice::Selection s;
        s.spawn = i % 2;
        for (int j = 0; j < 4; j++)
            s.crystals.push_back((i + j) % 6);
        d.samples.push_back(s);
    }
    d.requested = d.completed = 20;
    return d;
}
