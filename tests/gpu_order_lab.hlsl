// Optional FP64 research kernel. Not used by the released trainer.
StructuredBuffer<double> matrices : register(t0);
StructuredBuffer<uint> nearest : register(t1);
StructuredBuffer<uint> sizes : register(t2);
RWStructuredBuffer<uint> outputOrders : register(u0);
RWStructuredBuffer<double> outputLengths : register(u1);
groupshared double tile[53 * 53];
groupshared double starts[64];
groupshared uint indices[64];
groupshared uint winner[52];

double GpuEdge(uint from, uint to, uint side) { return tile[from * side + to]; }
uint GpuNearest(uint testId, uint from, uint rank) { return nearest[testId * 52 * 52 + from * 52 + rank]; }
#include "gpu_order_kernel.hlsli"

[numthreads(64, 1, 1)]
void Main(uint3 group : SV_GroupID, uint lane : SV_GroupIndex)
{
    uint testId = group.x, n = sizes[testId], side = n + 1;
    for (uint i = lane; i < side * side; i += 64)
        tile[i] = matrices[testId * 53 * 53 + i];
    GroupMemoryBarrierWithGroupSync();
    uint order[52];
    precise double value = asdouble(0u, 0x7ff00000u);
    if (lane < n) value = GpuFirstRoute(n, lane, testId, order);
    starts[lane] = value;
    indices[lane] = lane;
    GroupMemoryBarrierWithGroupSync();
    for (uint step = 32; step; step >>= 1)
    {
        if (lane < step && (starts[lane + step] < starts[lane] ||
            (starts[lane + step] == starts[lane] && indices[lane + step] < indices[lane])))
        {
            starts[lane] = starts[lane + step];
            indices[lane] = indices[lane + step];
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (lane == indices[0])
        for (uint i = 0; i < n; i++) winner[i] = order[i];
    GroupMemoryBarrierWithGroupSync();
    if (lane == 0)
    {
        for (uint i = 0; i < n; i++) order[i] = winner[i];
        GpuRelocate(n, order);
        precise double length = 0.0L;
        uint previous = 0;
        for (uint i = 0; i < n; i++)
        {
            outputOrders[testId * 52 + i] = order[i];
            length += GpuEdge(previous, order[i] + 1, side);
            previous = order[i] + 1;
        }
        for (uint i = n; i < 52; i++) outputOrders[testId * 52 + i] = 0xffffffffu;
        outputLengths[testId] = length;
    }
}
