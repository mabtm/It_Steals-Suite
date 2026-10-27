// Research only. Compile the same algorithm as C++ and HLSL, then compare it
// with production SolveOrderFlat. The shader additionally needs FP64 hardware.
#ifdef __cplusplus
#define GPU_PRECISE
#define GPU_ORDER(name) std::array<uint, 52> &name
#define GPU_EPS 0.000001
#define GPU_ZERO 0.0
#define GPU_LOOP
#else
#define GPU_PRECISE precise
#define GPU_ORDER(name) inout uint name[52]
#define GPU_EPS 0.000001L
#define GPU_ZERO 0.0L
#define GPU_LOOP [loop]
#endif

GPU_PRECISE double GpuFirstRoute(uint n, uint first, uint testId, GPU_ORDER(order))
{
    order[0] = first;
    uint low = n < 32 ? (1u << n) - 1 : 0xffffffffu;
    uint high = n <= 32 ? 0 : (1u << (n - 32)) - 1;
    if (first < 32) low &= ~(1u << first);
    else high &= ~(1u << (first - 32));
    GPU_LOOP for (uint count = 1; count < n; count++)
    {
        uint next = 0;
        GPU_LOOP for (uint rank = 0; rank < n; rank++)
        {
            uint candidate = GpuNearest(testId, order[count - 1], rank);
            bool unused = candidate < 32 ? (low & (1u << candidate)) != 0
                                         : (high & (1u << (candidate - 32))) != 0;
            if (unused) { next = candidate; break; }
        }
        order[count] = next;
        if (next < 32) low &= ~(1u << next);
        else high &= ~(1u << (next - 32));
    }
    GPU_PRECISE double current = GPU_ZERO;
    uint from = 0;
    GPU_LOOP for (uint i = 0; i < n; i++)
    {
        current += GpuEdge(from, order[i] + 1, n + 1);
        from = order[i] + 1;
    }
    GPU_LOOP for (uint iteration = 0; iteration < 32; iteration++)
    {
        bool improved = false;
        GPU_LOOP for (uint a = 0; a + 1 < n; a++)
        {
            uint previous = a ? order[a - 1] + 1 : 0, firstNode = order[a] + 1;
            GPU_PRECISE double before = GpuEdge(previous, firstNode, n + 1);
            GPU_LOOP for (uint b = a + 1; b < n; b++)
            {
                uint last = order[b] + 1;
                GPU_PRECISE double delta = GpuEdge(previous, last, n + 1) - before;
                if (b + 1 < n)
                    delta += GpuEdge(firstNode, order[b + 1] + 1, n + 1) -
                             GpuEdge(last, order[b + 1] + 1, n + 1);
                if (delta < -GPU_EPS)
                {
                    uint left = a, right = b;
                    GPU_LOOP while (left < right)
                    {
                        uint swapValue = order[left]; order[left] = order[right]; order[right] = swapValue;
                        left++; right--;
                    }
                    current += delta;
                    improved = true;
                    firstNode = last;
                    before = GpuEdge(previous, last, n + 1);
                }
            }
        }
        if (!improved) break;
    }
    return current;
}

void GpuRelocate(uint n, GPU_ORDER(order))
{
    GPU_LOOP for (uint iteration = 0; iteration < 16; iteration++)
    {
        bool improved = false;
        GPU_LOOP for (int a = 0; a < (int)n; a++)
            GPU_LOOP for (int after = -1; after < (int)n; after++)
            {
                if (after == a || after == a - 1) continue;
                int node = (int)order[a] + 1, previous = a ? (int)order[a - 1] + 1 : 0;
                int next = a + 1 < (int)n ? (int)order[a + 1] + 1 : -1;
                int left = after < 0 ? 0 : (int)order[after] + 1;
                int right = after + 1 < (int)n ? (int)order[after + 1] + 1 : -1;
                GPU_PRECISE double delta = -GpuEdge(previous, node, n + 1) +
                    (next >= 0 ? GpuEdge(previous, next, n + 1) - GpuEdge(node, next, n + 1) : GPU_ZERO) +
                    GpuEdge(left, node, n + 1) +
                    (right >= 0 ? GpuEdge(node, right, n + 1) - GpuEdge(left, right, n + 1) : GPU_ZERO);
                if (delta < -GPU_EPS)
                {
                    uint value = order[a];
                    GPU_LOOP for (uint i = (uint)a; i + 1 < n; i++) order[i] = order[i + 1];
                    int at = after < a ? after + 1 : after;
                    GPU_LOOP for (int i = (int)n - 1; i > at; i--) order[i] = order[i - 1];
                    order[at] = value;
                    improved = true;
                    break;
                }
            }
        if (!improved) break;
    }
}

#undef GPU_PRECISE
#undef GPU_ORDER
#undef GPU_EPS
#undef GPU_ZERO
#undef GPU_LOOP
