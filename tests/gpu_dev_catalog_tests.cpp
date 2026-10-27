#include "GpuDevCatalog.h"
#include <cassert>
#include <bit>
#include <iostream>

int main()
{
    using namespace gpu_dev;
    int checked = 0;
    for (int points : {52, 71, 1024})
        for (int profile : {0, 1, 2})
        {
            auto edge = [&](int a, int b) { return profile == 0 ? double((a * 71 + b * 17) % 4)
                : profile == 1 ? std::hypot(double(a % 9 - b % 9), double(a / 9 - b / 9))
                               : 1. + ((a * 71 + b * 17) % 32) * 1e-7; };
            Catalog catalog(3, points, edge);
            for (int n : {1, 15, 16, 22, 40, 52})
            {
                practice::Selection selection; selection.spawn = 2;
                for (int i = 0; i < n; i++) selection.crystals.push_back((points - 1) * (n - 1 - i) / std::max(1, n - 1));
                uint32_t ids[53]{}; catalog.Pack(selection, ids);
                std::vector<double> triangle(size_t(n + 1) * (n + 2) / 2);
                for (int a = 0; a <= n; a++) for (int b = 0; b <= a; b++)
                    triangle[TriangleIndex(a, b)] = catalog.edges[size_t(ids[a]) * catalog.side + ids[b]];
                uint32_t mask[32]{};
                for (int id : selection.crystals) mask[id / 32] |= uint32_t{1} << (id & 31);
                for (int from = 0; from <= n; from++)
                    for (int to = 0; to <= n; to++)
                    {
                        int a = int(ids[std::min(from, to)]), b = int(ids[std::max(from, to)]);
                        double expected = from == to ? 0. : edge(a, b);
                        assert(catalog.edges[size_t(ids[from]) * catalog.side + ids[to]] == expected);
                        assert(std::bit_cast<uint64_t>(triangle[TriangleIndex(from, to)]) == std::bit_cast<uint64_t>(expected));
                    }
                for (int from = 0; from < n; from++)
                {
                    std::vector<int> expected(n), actual;
                    std::iota(expected.begin(), expected.end(), 0);
                    std::sort(expected.begin(), expected.end(), [&](int a, int b) {
                        double x = catalog.edges[size_t(ids[from + 1]) * catalog.side + ids[a + 1]];
                        double y = catalog.edges[size_t(ids[from + 1]) * catalog.side + ids[b + 1]];
                        return x < y || (x == y && a < b);
                    });
                    for (int rank = 0; rank < points; rank++)
                    {
                        uint32_t id = catalog.nearest[size_t(selection.crystals[from]) * points + rank], word = id / 32, bit = id & 31;
                        if (!(mask[word] & (uint32_t{1} << bit))) continue;
                        int above = 0;
                        for (uint32_t w = word + 1; w < 32; w++) above += std::popcount(mask[w]);
                        actual.push_back(above + std::popcount(mask[word] & (0xfffffffeu << bit)));
                    }
                    assert(actual == expected);
                    for (int bits : {8, 16})
                    {
                        std::array<uint32_t, 26> packed{};
                        int perWord = 32 / bits;
                        for (int i = 0; i < n; i++) packed[i / perWord] |= uint32_t(actual[i]) << ((i % perWord) * bits);
                        for (int i = 0; i < n; i++) assert(((packed[i / perWord] >> ((i % perWord) * bits)) & ((1u << bits) - 1)) == uint32_t(expected[i]));
                    }
                    checked++;
                }
                auto rejects = [&](practice::Selection bad) {
                    try { catalog.Pack(bad, ids); } catch (const std::runtime_error &) { return true; }
                    return false;
                };
                auto bad = selection; bad.spawn = 3; assert(rejects(bad));
                bad = selection; bad.crystals[0] = points; assert(rejects(bad));
                if (n > 1) { bad = selection; bad.crystals[1] = bad.crystals[0]; assert(rejects(bad));
                    bad = selection; std::swap(bad.crystals[0], bad.crystals[1]); assert(rejects(bad)); }
            }
        }
    assert(ShapeFor(ShaderKind::Generic).SharedBytes() == 29112);
    assert(ShapeFor(ShaderKind::Triangle).SharedBytes() == 18088);
    assert(ShapeFor(ShaderKind::Compact).SharedBytes() == 15384);
    assert(ShapeFor(ShaderKind::CompactFixed, 22).SharedBytes() == 3464);
    assert(ShapeFor(ShaderKind::CompactFixed, 40).SharedBytes() == 9672);
    std::cout << checked << " catalog neighbor rows: directed table orientation, ties, sparse/1024-point masks, highest bits and invalid selections: PASS\n";
}
