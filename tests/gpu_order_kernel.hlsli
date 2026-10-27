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
        GPU_LOOP for (int a = 0; a < (int)n - 1; a++)
        {
            uint previous = 0, firstNode = order[a] + 1;
            if (a > 0) previous = order[a - 1] + 1;
            GPU_PRECISE double before = GpuEdge(previous, firstNode, n + 1);
            GPU_LOOP for (int b = a + 1; b < (int)n; b++)
            {
                uint last = order[b] + 1;
                GPU_PRECISE double delta = GpuEdge(previous, last, n + 1) - before;
                if (b + 1 < (int)n)
                    delta += GpuEdge(firstNode, order[b + 1] + 1, n + 1) -
                             GpuEdge(last, order[b + 1] + 1, n + 1);
                if (delta < -GPU_EPS)
                {
                    int left = a, right = b;
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
                int node = (int)order[a] + 1, previous = 0, next = -1, left = 0, right = -1;
                if (a > 0) previous = (int)order[a - 1] + 1;
                if (a + 1 < (int)n) next = (int)order[a + 1] + 1;
                if (after >= 0) left = (int)order[after] + 1;
                if (after + 1 < (int)n) right = (int)order[after + 1] + 1;
                GPU_PRECISE double delta = -GpuEdge(previous, node, n + 1) +
                    (next >= 0 ? GpuEdge(previous, next, n + 1) - GpuEdge(node, next, n + 1) : GPU_ZERO) +
                    GpuEdge(left, node, n + 1) +
                    (right >= 0 ? GpuEdge(node, right, n + 1) - GpuEdge(left, right, n + 1) : GPU_ZERO);
                if (delta < -GPU_EPS)
                {
                    uint value = order[a];
                    GPU_LOOP for (int shiftLeft = a; shiftLeft + 1 < (int)n; shiftLeft++) order[shiftLeft] = order[shiftLeft + 1];
                    int at = after < a ? after + 1 : after;
                    GPU_LOOP for (int shiftRight = (int)n - 1; shiftRight > at; shiftRight--) order[shiftRight] = order[shiftRight - 1];
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
