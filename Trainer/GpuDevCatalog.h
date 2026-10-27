#pragma once
#include "SeedSearch.h"

namespace gpu_dev
{
enum class ShaderKind { Generic, Fixed, Triangle, Compact, CompactFixed };
inline constexpr std::array<ShaderKind, 5> ShaderKinds{
    ShaderKind::Generic, ShaderKind::Fixed, ShaderKind::Triangle, ShaderKind::Compact, ShaderKind::CompactFixed};
inline const char *ShaderName(ShaderKind kind)
{
    constexpr const char *names[]{"generic", "fixed", "triangle", "compact", "compact-fixed"};
    return names[int(kind)];
}
struct ShaderShape
{
    int capacity = 52, threads = 64, neighborBits = 16;
    bool triangular = false, fixed = false;
    int SharedBytes(bool catalog = true) const
    {
        int side = capacity + 1, perWord = 32 / neighborBits;
        int tile = triangular ? side * (side + 1) / 2 : side * side;
        return tile * 8 + threads * 12 + capacity * 4 +
            (catalog ? 256 + 4 * capacity * ((capacity + perWord - 1) / perWord) : 0);
    }
};
inline ShaderShape ShapeFor(ShaderKind kind, int crystals = 0)
{
    if (int(kind) < 0 || int(kind) >= int(ShaderKinds.size())) throw std::runtime_error("Invalid GPU shader variant");
    ShaderShape shape;
    shape.fixed = kind == ShaderKind::Fixed || kind == ShaderKind::CompactFixed;
    if (shape.fixed)
    {
        if (crystals < 1 || crystals > 52) throw std::runtime_error("Invalid shader crystal count");
        shape.capacity = crystals; shape.threads = crystals <= 32 ? 32 : 64;
    }
    shape.triangular = kind == ShaderKind::Triangle || kind == ShaderKind::Compact || kind == ShaderKind::CompactFixed;
    shape.neighborBits = kind == ShaderKind::Compact || kind == ShaderKind::CompactFixed ? 8 : 16;
    return shape;
}
inline size_t TriangleIndex(size_t a, size_t b)
{
    if (a < b) std::swap(a, b);
    return a * (a + 1) / 2 + b;
}
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
