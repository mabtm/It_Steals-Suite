#include "gpu_order_fixture.h"
#include <cassert>
#include <iostream>

namespace gpu_lab
{
using uint = uint32_t;
const GpuOrderFixture *input;

double GpuEdge(uint from, uint to, uint side)
{
    return input->matrix[from * side + to];
}

uint GpuNearest(uint, uint from, uint rank)
{
    return input->nearest[from * 52 + rank];
}

#include "gpu_order_kernel.hlsli"
} // namespace gpu_lab

int main()
{
    int cases = 0;
    for (int n : {1, 2, 8, 15, 16, 22, 31, 32, 33, 40, 52})
        for (int profile = 0; profile < 5; profile++)
            for (uint32_t repeat = 0; repeat < 8; repeat++)
            {
                GpuOrderFixture input(n, profile, 0x720000 + repeat);
                gpu_lab::input = &input;
                double best = practice::Unreachable;
                std::array<uint32_t, 52> order{}, winner{};
                for (int first = 0; first < n; first++)
                {
                    double score = gpu_lab::GpuFirstRoute(n, first, 0, order);
                    if (score < best)
                    {
                        best = score;
                        winner = order;
                    }
                }
                gpu_lab::GpuRelocate(n, winner);
                double length = 0;
                int previous = 0;
                for (int i = 0; i < n; i++)
                {
                    assert(winner[i] == uint32_t(input.expected.order[i]));
                    length += input.matrix[previous * (n + 1) + winner[i] + 1];
                    previous = winner[i] + 1;
                }
                assert(length == input.expected.length);
                cases++;
            }
    std::cout
        << cases
        << " shared C++/HLSL kernel comparisons with the production solver; 1-52 goals, 32-bit mask boundary, ties, directed and threshold-adjacent costs: PASS\n";
}
