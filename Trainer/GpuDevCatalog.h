#pragma once
#include "SeedSearch.h"

namespace gpu_dev
{
// Portable RNG selections use descending catalog IDs. This permits one fixed
// tie order and a GPU bitmask/rank lookup instead of per-layout CPU sorting.
struct Catalog
{
    int spawns, points, side;
    std::vector<double> edges;
    std::vector<uint32_t> nearest;

    template <class Edge> Catalog(int spawnCount, int pointCount, Edge edge)
        : spawns(spawnCount), points(pointCount), side(spawns + points)
    {
        if (spawns < 1 || spawns > 128 || points < 1 || points > 1024)
            throw std::runtime_error("GPU catalog exceeds map wire limits");
        edges.resize(size_t(side) * side);
        nearest.resize(size_t(points) * points);
        for (int a = 0; a < side; a++)
            for (int b = a + 1; b < side; b++)
            {
                // Exactly match FillMatrix's upper local triangle, even if a
                // supplied distance table has directed entries. Spawn first,
                // followed by descending point IDs; unused spawn pairs are zero.
                double value = b < spawns ? 0 : a < spawns ? edge(a, b) : edge(b, a);
                if (!std::isfinite(value)) throw std::runtime_error("Captured catalog has incomplete route distances");
                edges[size_t(a) * side + b] = edges[size_t(b) * side + a] = value;
            }
        for (int from = 0; from < points; from++)
        {
            auto first = nearest.begin() + size_t(from) * points;
            std::iota(first, first + points, uint32_t{0});
            std::sort(first, first + points, [&](uint32_t a, uint32_t b) {
                double x = edges[size_t(from + spawns) * side + a + spawns];
                double y = edges[size_t(from + spawns) * side + b + spawns];
                return x < y || (x == y && a > b);
            });
        }
    }

    void Pack(const practice::Selection &selection, uint32_t *ids) const
    {
        if (selection.spawn < 0 || selection.spawn >= spawns || selection.crystals.empty() || selection.crystals.size() > 52)
            throw std::runtime_error("Invalid GPU catalog selection");
        ids[0] = uint32_t(selection.spawn);
        int previous = points;
        for (size_t i = 0; i < selection.crystals.size(); i++)
        {
            int id = selection.crystals[i];
            if (id < 0 || id >= previous) throw std::runtime_error("GPU catalog selections must use distinct descending point IDs");
            ids[i + 1] = uint32_t(id + spawns);
            previous = id;
        }
    }
};
} // namespace gpu_dev
