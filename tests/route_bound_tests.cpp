#include "PracticeRouting.h"
#include <cassert>
#include <iostream>

using namespace practice;

int main()
{
    uint32_t random = 0x717000;
    auto next = [&] { return random = random * 1664525u + 1013904223u; };
    RouteWorkspace work;
    int cases = 0;
    for (int n : {2, 3, 5, 8, 10, 12})
        for (int profile = 0; profile < 3; profile++)
            for (int repeat = 0; repeat < 8; repeat++)
            {
                int size = n + 1;
                for (int a = 0; a < size; a++)
                {
                    work.matrix[a * size + a] = 0;
                    for (int b = a + 1; b < size; b++)
                    {
                        double cost = profile == 0   ? (next() % 4096) / 8.
                                      : profile == 1 ? double(next() % 5)
                                                     : double(std::abs(a - b));
                        work.matrix[a * size + b] = work.matrix[b * size + a] = cost;
                    }
                }
                auto exact = SolveOrderFlat(work.matrix.data(), size, work, [] { return false; }, true);
                assert(exact.complete && exact.exact);
                double basic = MstRouteLowerBound(work.matrix.data(), size, [] { return false; });
                for (double cutoff : {exact.length - 1, exact.length, exact.length + 1})
                {
                    double bound =
                        TightenRouteLowerBound(work.matrix.data(), size, basic, cutoff, [] { return false; });
                    assert(bound >= basic && bound <= exact.length);
                    cases++;
                }
            }
    for (int n : {16, 22, 40, 52, 64})
        for (double scale : {1e-9, 1., 1e6, 1e100})
        {
            int size = n + 1;
            for (int a = 0; a < size; a++)
                for (int b = 0; b < size; b++)
                    work.matrix[a * size + b] = a == b               ? 0
                                                : (a <= 1 && b <= 1) ? 0
                                                : (a <= 1 || b <= 1) ? scale
                                                                     : 2 * scale;
            // Spawn and one crystal are at the center of a unit star. Visiting
            // n-1 leaves has known optimal open-path length 2*(n-1)-1.
            double optimum = (2 * n - 3) * scale;
            double basic = MstRouteLowerBound(work.matrix.data(), size, [] { return false; });
            double cutoff = .8 * optimum;
            double bound =
                TightenRouteLowerBound(work.matrix.data(), size, basic, cutoff, [] { return false; });
            assert(bound >= basic && bound <= optimum);
            if (scale == 1)
                assert(basic < cutoff && bound > cutoff + 1e-6);
        }
    bool canceled = false;
    try
    {
        TightenRouteLowerBound(work.matrix.data(), 65, 0, 100, [] { return true; });
    }
    catch (const std::runtime_error &)
    {
        canceled = true;
    }
    assert(canceled);
    std::cout << cases
              << " exact-DP bound comparisons, known 16-64-node star optima, scales and cancellation: PASS\n";
}
