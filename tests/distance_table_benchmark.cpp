#include "SeedSearch.h"
#include <iostream>
#if defined(__GLIBC__)
#include <malloc.h>
#endif

int main()
{
    practice::MapData data;
    data.spawns.resize(16);
    for (int i = 0; i < 368; i++)
        data.points.push_back({float(i), 1, float(i % 21)});
    data.keep = 18;
    practice::NavGraph graph(data);
    auto table = graph.Routes();
#if defined(__GLIBC__)
    size_t before = mallinfo2().uordblks;
#endif
    practice::SearchDistances distances(data, graph, [] { return false; }, [](int, int) {});
    std::cout << "384-node prepared catalog; estimated pairs=" << distances.EstimatedPairs();
#if defined(__GLIBC__)
    std::cout << "; added live allocation=" << mallinfo2().uordblks - before << " bytes";
#endif
    std::cout << "\nMeasures retained table allocations, not process RSS or live game FPS.\n";
}
