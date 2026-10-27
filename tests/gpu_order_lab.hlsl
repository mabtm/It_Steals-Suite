// Optional FP64 research kernel. Not used by the released trainer.
#ifndef GPU_MAX_CRYSTALS
#define GPU_MAX_CRYSTALS 52
#endif
#ifndef GPU_THREADS
#define GPU_THREADS 64
#endif
#ifndef GPU_NEAREST_BITS
#define GPU_NEAREST_BITS 16
#endif
#define GPU_IDS_PER_WORD (32 / GPU_NEAREST_BITS)
#define GPU_NEAREST_WORDS ((GPU_MAX_CRYSTALS + GPU_IDS_PER_WORD - 1) / GPU_IDS_PER_WORD)
#if defined(GPU_TRIANGULAR_TILE) && !defined(GPU_CATALOG_INPUT)
#error Triangular tiles require the symmetric captured catalog
#endif
StructuredBuffer<double> matrices : register(t0);
StructuredBuffer<uint> nearest : register(t1);
StructuredBuffer<uint> sizes : register(t2);
#ifdef GPU_CATALOG_INPUT
StructuredBuffer<uint> selectedIds : register(t3);
cbuffer CatalogInfo : register(b0) { uint pointCount, spawnCount, catalogSide, unusedCatalog; };
groupshared uint selectedMask[32];
groupshared uint aboveWords[32];
groupshared uint localNearest[GPU_MAX_CRYSTALS * GPU_NEAREST_WORDS];
#endif
RWStructuredBuffer<uint> outputOrders : register(u0);
RWStructuredBuffer<double> outputLengths : register(u1);
#ifdef GPU_TRIANGULAR_TILE
groupshared double tile[(GPU_MAX_CRYSTALS + 1) * (GPU_MAX_CRYSTALS + 2) / 2];
#else
groupshared double tile[(GPU_MAX_CRYSTALS + 1) * (GPU_MAX_CRYSTALS + 1)];
#endif
groupshared double starts[GPU_THREADS];
groupshared uint indices[GPU_THREADS];
groupshared uint winner[GPU_MAX_CRYSTALS];

double GpuEdge(uint from, uint to, uint side)
{
#ifdef GPU_TRIANGULAR_TILE
    uint row = max(from, to), column = min(from, to);
    return tile[row * (row + 1) / 2 + column];
#else
    return tile[from * side + to];
#endif
}
#ifdef GPU_CATALOG_INPUT
uint GpuNearest(uint testId, uint from, uint rank)
{
    return (localNearest[from * GPU_NEAREST_WORDS + rank / GPU_IDS_PER_WORD] >>
        ((rank % GPU_IDS_PER_WORD) * GPU_NEAREST_BITS)) & ((1u << GPU_NEAREST_BITS) - 1);
}
#else
uint GpuNearest(uint testId, uint from, uint rank) { return nearest[testId * 52 * 52 + from * 52 + rank]; }
#endif
#include "gpu_order_kernel.hlsli"

[numthreads(GPU_THREADS, 1, 1)]
void Main(uint3 group : SV_GroupID, uint lane : SV_GroupIndex)
{
    uint testId = group.x;
#ifdef GPU_FIXED_CRYSTALS
    uint n = GPU_MAX_CRYSTALS;
#else
    uint n = sizes[testId];
#endif
    uint side = n + 1;
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
    for (uint loadIndex = lane; loadIndex < side * side; loadIndex += GPU_THREADS)
    {
        uint row = loadIndex / side, column = loadIndex % side;
#ifdef GPU_TRIANGULAR_TILE
        if (column > row) continue;
        uint target = row * (row + 1) / 2 + column;
#else
        uint target = loadIndex;
#endif
        uint a = selectedIds[testId * 53 + row], b = selectedIds[testId * 53 + column];
        tile[target] = matrices[a * catalogSide + b];
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
#if GPU_NEAREST_BITS == 16
            if (found & 1) localNearest[lane * GPU_NEAREST_WORDS + found / 2] = packed | (local << 16);
            else packed = local;
            found++;
#else
            packed |= local << ((found % GPU_IDS_PER_WORD) * GPU_NEAREST_BITS);
            found++;
            if (found % GPU_IDS_PER_WORD == 0)
            {
                localNearest[lane * GPU_NEAREST_WORDS + (found - 1) / GPU_IDS_PER_WORD] = packed;
                packed = 0;
            }
#endif
        }
#if GPU_NEAREST_BITS == 16
        if (found & 1) localNearest[lane * GPU_NEAREST_WORDS + found / 2] = packed;
#else
        if (found % GPU_IDS_PER_WORD) localNearest[lane * GPU_NEAREST_WORDS + (found - 1) / GPU_IDS_PER_WORD] = packed;
#endif
    }
#else
    for (uint loadIndex = lane; loadIndex < side * side; loadIndex += GPU_THREADS)
        tile[loadIndex] = matrices[testId * 53 * 53 + loadIndex];
#endif
    GroupMemoryBarrierWithGroupSync();
    uint order[GPU_MAX_CRYSTALS];
    precise double value = asdouble(0u, 0x7ff00000u);
#if defined(GPU_FIXED_CRYSTALS) && GPU_MAX_CRYSTALS == 1
    // One possible order. FXC otherwise diagnoses the unreachable order[1] loop.
    if (lane == 0) { order[0] = 0; value = GpuEdge(0, 1, side); }
#else
    if (lane < n) value = GpuFirstRoute(n, lane, testId, order);
#endif
    starts[lane] = value;
    indices[lane] = lane;
    GroupMemoryBarrierWithGroupSync();
    for (uint step = GPU_THREADS / 2; step; step >>= 1)
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
#if !defined(GPU_FIXED_CRYSTALS) || GPU_MAX_CRYSTALS > 1
        GpuRelocate(n, order);
#endif
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
