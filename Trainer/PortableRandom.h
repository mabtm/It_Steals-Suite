#pragma once
#include "PracticeMapData.h"
#include <array>

namespace practice
{
inline int DerivedPracticeSeed(int seed, int scene, int site)
{
    uint32_t x = static_cast<uint32_t>(seed) ^ (static_cast<uint32_t>(scene) * 0x9e3779b9u) ^
                 (static_cast<uint32_t>(site) * 0x85ebca6bu);
    x = (x ^ (x >> 16)) * 0x7feb352du;
    x = (x ^ (x >> 15)) * 0x846ca68bu;
    x ^= x >> 16;
    int32_t value;
    std::memcpy(&value, &x, 4);
    return value;
}

class PortableRandom
{
    uint32_t x = 0, y = 0, z = 0, w = 0;
    int initMode, rangeMode, advanceOne;

    uint32_t Expand(uint32_t v, uint32_t index) const
    {
        return 1812433253u * ((initMode == 1 || initMode == 3) ? v ^ (v >> 30) : v) +
               ((initMode == 1 || initMode == 2) ? index : 1u);
    }

  public:
    PortableRandom(int init, int range, int one) : initMode(init), rangeMode(range), advanceOne(one)
    {
        if (init < 0 || init > 3 || range < 0 || range > 4 || one < 0 || one > 1)
            throw std::runtime_error("Invalid CPU RNG descriptor");
    }

    void Init(int seed)
    {
        x = static_cast<uint32_t>(seed);
        y = Expand(x, 1);
        z = Expand(y, 2);
        w = Expand(z, 3);
    }

    uint32_t Next()
    {
        uint32_t t = x ^ (x << 11);
        x = y;
        y = z;
        z = w;
        w = w ^ (w >> 19) ^ t ^ (t >> 8);
        return w;
    }

    int Range(int min, int max)
    {
        uint32_t count = static_cast<uint32_t>(max - min);
        if (count == 1 && !advanceOne)
            return min;
        uint32_t v = Next(), result;
        switch (rangeMode)
        {
        case 0:
            result = v % count;
            break;
        case 1:
            result = (v & 0x7fffffffu) % count;
            break;
        case 2:
            result = static_cast<uint32_t>((uint64_t(v) * count) >> 32);
            break;
        case 3:
            result = static_cast<uint32_t>((uint64_t(v >> 1) * count) >> 31);
            break;
        default:
            result = ((v & 0x80000000u) ? 0u - v : v) % count;
            break;
        }
        return min + static_cast<int>(result);
    }
};

class PortableSelectionGenerator
{
    PortableRandom rng;
    int spawns, points, keep, scene, site, forced;
    std::array<int, 1024> remaining{};

  public:
    explicit PortableSelectionGenerator(const MapData &d)
        : rng(d.rngInit, d.rngRange, d.rngAdvance), spawns(static_cast<int>(d.spawns.size())),
          points(static_cast<int>(d.points.size())), keep(std::min(d.keep, points)), scene(d.scene),
          site(d.rngSite), forced((d.flags & 16) ? 4 : -1)
    {
        if (d.rngStatus != 2 || d.rngProbes != 4160 || spawns < 1 || spawns > 128 || points < 1 ||
            points > 1024 || keep < 1 || keep > 64)
            throw std::runtime_error("CPU generator needs a verified captured catalog");
    }

    void Generate(int seed, int recipe, Selection &output)
    {
        rng.Init(recipe == 2 ? DerivedPracticeSeed(seed, scene, site) : seed);
        rng.Range(1, 30);
        output.spawn = rng.Range(0, spawns);
        if (forced >= 0 && forced < spawns)
            output.spawn = forced;
        for (int i = 0; i < points; i++)
            remaining[i] = points - 1 - i;
        for (int count = points; count > keep; count--)
        {
            int id = rng.Range(0, count);
            std::memmove(remaining.data() + id, remaining.data() + id + 1,
                         static_cast<size_t>(count - id - 1) * sizeof(int));
        }
        output.crystals.assign(remaining.begin(), remaining.begin() + keep);
    }
};
} // namespace practice
