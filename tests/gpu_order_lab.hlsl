// Optional FP64 research kernel. Not used by the released trainer.
StructuredBuffer<double> matrices : register(t0);
StructuredBuffer<uint> nearest : register(t1);
StructuredBuffer<uint> sizes : register(t2);
#ifdef GPU_CATALOG_INPUT
StructuredBuffer<uint> selectedIds : register(t3);
cbuffer CatalogInfo : register(b0) { uint pointCount, spawnCount, catalogSide, unusedCatalog; };
groupshared uint selectedMask[32];
groupshared uint aboveWords[32];
groupshared uint localNearest[52 * 26];
#endif
RWStructuredBuffer<uint> outputOrders : register(u0);
RWStructuredBuffer<double> outputLengths : register(u1);
groupshared double tile[53 * 53];
groupshared double starts[64];
groupshared uint indices[64];
groupshared uint winner[52];

double GpuEdge(uint from, uint to, uint side) { return tile[from * side + to]; }
#ifdef GPU_CATALOG_INPUT
uint GpuNearest(uint testId, uint from, uint rank)
{
    return (localNearest[from * 26 + rank / 2] >> ((rank & 1) * 16)) & 0xffffu;
}
#else
uint GpuNearest(uint testId, uint from, uint rank) { return nearest[testId * 52 * 52 + from * 52 + rank]; }
#endif
#include "gpu_order_kernel.hlsli"

[numthreads(64, 1, 1)]
void Main(uint3 group : SV_GroupID, uint lane : SV_GroupIndex)
{
    uint testId = group.x, n = sizes[testId], side = n + 1;
#ifdef GPU_CATALOG_INPUT
    if (lane < 32) selectedMask[lane] = 0;
    GroupMemoryBarrierWithGroupSync();
    if (lane < n)
    {
        uint id = selectedIds[testId * 53 + lane + 1] - spawnCount;
        InterlockedOr(selectedMask[id / 32], 1u << (id & 31));
    }
    GroupMemoryBarrierWithGroupSync();
    if (lane < 32)
    {
        uint above = 0;
        for (uint word = lane + 1; word < 32; word++) above += countbits(selectedMask[word]);
        aboveWords[lane] = above;
    }
    for (uint loadIndex = lane; loadIndex < side * side; loadIndex += 64)
    {
        uint a = selectedIds[testId * 53 + loadIndex / side];
        uint b = selectedIds[testId * 53 + loadIndex % side];
        tile[loadIndex] = matrices[a * catalogSide + b];
    }
    GroupMemoryBarrierWithGroupSync();
    if (lane < n)
    {
        uint from = selectedIds[testId * 53 + lane + 1] - spawnCount;
        uint found = 0, packed = 0;
        for (uint rank = 0; rank < pointCount && found < n; rank++)
        {
            uint id = nearest[from * pointCount + rank], word = id / 32, bit = id & 31;
            if ((selectedMask[word] & (1u << bit)) == 0) continue;
            uint local = aboveWords[word] + countbits(selectedMask[word] & (0xfffffffeu << bit));
            if (found & 1) localNearest[lane * 26 + found / 2] = packed | (local << 16);
            else packed = local;
            found++;
        }
        if (found & 1) localNearest[lane * 26 + found / 2] = packed;
    }
#else
    for (uint loadIndex = lane; loadIndex < side * side; loadIndex += 64)
        tile[loadIndex] = matrices[testId * 53 * 53 + loadIndex];
#endif
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
        for (uint copyIndex = 0; copyIndex < n; copyIndex++) winner[copyIndex] = order[copyIndex];
    GroupMemoryBarrierWithGroupSync();
    if (lane == 0)
    {
        for (uint recoverIndex = 0; recoverIndex < n; recoverIndex++) order[recoverIndex] = winner[recoverIndex];
        GpuRelocate(n, order);
        precise double length = 0.0L;
        uint previous = 0;
        for (uint outputIndex = 0; outputIndex < n; outputIndex++)
        {
            outputOrders[testId * 52 + outputIndex] = order[outputIndex];
            length += GpuEdge(previous, order[outputIndex] + 1, side);
            previous = order[outputIndex] + 1;
        }
        for (uint padIndex = n; padIndex < 52; padIndex++) outputOrders[testId * 52 + padIndex] = 0xffffffffu;
        outputLengths[testId] = length;
    }
}
