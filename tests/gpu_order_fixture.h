#pragma once
#include "PracticeRouting.h"

struct GpuOrderFixture
{
    int n;
    std::array<double, 53 * 53> matrix{};
    std::array<uint32_t, 52 * 52> nearest{};
    practice::RouteResult expected;

    GpuOrderFixture(int count, int profile, uint32_t seed) : n(count)
    {
        auto next = [&] { return seed = seed * 1664525u + 1013904223u; };
        for (int a = 0; a <= n; a++)
            for (int b = a + 1; b <= n; b++)
            {
                double value = profile == 0   ? (next() % 8192) / 16.
                               : profile == 1 ? double(next() % 4)
                               : profile == 2 ? std::hypot(double(a % 7 - b % 7), double(a / 7 - b / 7))
                               : profile == 3 ? (next() % 1024) / 8.
                                              : 1. + (next() % 32) * 1e-7;
                matrix[a * (n + 1) + b] = matrix[b * (n + 1) + a] = value;
                if (profile == 3)
                    matrix[b * (n + 1) + a] = (next() % 1024) / 8.;
            }
        for (int a = 0; a < n; a++)
        {
            auto row = nearest.begin() + a * 52;
            std::iota(row, row + n, uint32_t{0});
            std::sort(row, row + n, [&](int b, int c) {
                double x = matrix[(a + 1) * (n + 1) + b + 1];
                double y = matrix[(a + 1) * (n + 1) + c + 1];
                return x < y || (x == y && b < c);
            });
        }
        practice::RouteWorkspace work;
        expected = practice::SolveOrderFlat(matrix.data(), n + 1, work, [] { return false; }, false);
    }
};
