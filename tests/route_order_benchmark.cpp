#include "SeedSearch.h"
#include <chrono>
#include <iostream>
#include <random>
#include <iomanip>

int main()
{
    practice::MapData data;
    data.spawns = {{1, 1, 1}, {49, 1, 49}};
    data.vertices = {{0, 0, 0}, {50, 0, 0}, {50, 0, 50}, {0, 0, 50}};
    data.indices = {0, 1, 2, 0, 2, 3};
    std::mt19937 random(765);
    for (int i = 0; i < 64; i++)
        data.points.push_back({float(1 + random() % 48), 1, float(1 + random() % 48)});
    practice::NavGraph graph(data);
    practice::SearchDistances distances(data, graph, [] { return false; }, [](int, int) {});
    for (int n : {14, 22, 40})
        for (bool fast : {true, false})
        {
            if (!fast && n != 14)
                continue;
            int count = fast ? 3000 : 48;
            std::vector<practice::Selection> choices;
            std::vector<int> points(64);
            std::iota(points.begin(), points.end(), 0);
            for (int i = 0; i < count; i++)
            {
                std::shuffle(points.begin(), points.end(), random);
                practice::Selection s;
                s.spawn = i % 2;
                s.crystals.assign(points.begin(), points.begin() + n);
                std::sort(s.crystals.begin(), s.crystals.end(), std::greater<int>());
                choices.push_back(std::move(s));
            }
#ifdef SEARCHLAB_WORKSPACE
            practice::RouteWorkspace workspace;
#endif
            auto start = std::chrono::steady_clock::now();
            double checksum = 0;
            uint64_t orderHash = 0;
            for (const auto &selection : choices)
            {
#ifdef SEARCHLAB_WORKSPACE
                auto result = distances.Score(selection, fast, [] { return false; }, workspace);
#else
                auto result = distances.Score(selection, fast, [] { return false; });
#endif
                if (!result.complete)
                    return 1;
                checksum += result.length;
                for (int id : result.order)
                    orderHash = orderHash * 31 + id + 1;
            }
            double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            std::cout << n << " crystals / " << (fast ? "heuristic" : "exact") << ": " << count / seconds
                      << " layouts/s / " << seconds << " s / checksum " << std::setprecision(17) << checksum
                      << " / order " << orderHash << '\n';
        }
    std::cout
        << "Unique synthetic layouts, one worker, precomputed NavMesh distances; no Unity generation.\n";
}
