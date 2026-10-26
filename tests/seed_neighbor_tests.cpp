#ifdef _WIN32
#include <windows.h>
#endif
#include "SeedSearch.h"
#include "seed_map_fixture.h"
#include <cassert>
#include <iostream>
#include <random>

using namespace practice;

int main()
{
    MapData data;
    data.spawns = {{1, 1, 1}, {40, 1, 40}, {25, 1, 25}};
    FixtureTile(data, 0, 0, 50, 50);
    // Integer grid: many equal distances exercise the local-index tie rule.
    for (int i = 0; i < 71; i++)
        data.points.push_back({float(2 + i % 9 * 5), 1, float(2 + i / 9 * 5)});
    NavGraph graph(data);
    SearchDistances distances(data, graph, [] { return false; }, [](int, int) {});
    std::mt19937 random(71801);
    RouteWorkspace work, reference;
    int cases = 0;
    for (int n : {1, 8, 15, 16, 22, 40, 52, 64})
        for (int repeat = 0; repeat < 12; repeat++)
        {
            Selection selection;
            selection.spawn = repeat % 3;
            std::vector<int> ids(data.points.size());
            std::iota(ids.begin(), ids.end(), 0);
            std::shuffle(ids.begin(), ids.end(), random);
            selection.crystals.assign(ids.begin(), ids.begin() + n);
            std::sort(selection.crystals.begin(), selection.crystals.end(), std::greater<int>());
            for (bool canonical : {true, false})
            {
                if (!canonical)
                    std::shuffle(selection.crystals.begin(), selection.crystals.end(), random);
                for (bool fast : {true, false})
                {
                    int count = distances.FillMatrix(selection, reference);
                    auto expected = SolveOrderFlat(
                        reference.matrix.data(), count, reference, [] { return false; }, !fast);
                    auto actual = distances.Score(selection, fast, [] { return false; }, work);
                    assert(actual.complete && actual.exact == expected.exact &&
                           actual.order == expected.order && actual.length == expected.length &&
                           !actual.pickupOptimized && !actual.estimated);
                    // Prepared center matrices retain the same path as well.
                    distances.FillBoundMatrix(selection, work);
                    auto prepared = distances.Score(selection, fast, [] { return false; }, work, true);
                    assert(prepared.order == expected.order && prepared.length == expected.length);
                    cases++;
                }
            }
        }
    // Catalogs too sparse for reuse retain per-selection sorting.
    for (int i = 71; i < 257; i++)
        data.points.push_back(data.points[i % 71]);
    NavGraph sparseGraph(data);
    SearchDistances sparse(data, sparseGraph, [] { return false; }, [](int, int) {});
    Selection selection{0, {}};
    for (int i = 63; i >= 0; i--)
        selection.crystals.push_back(i);
    int size = sparse.FillMatrix(selection, reference);
    auto expected = SolveOrderFlat(reference.matrix.data(), size, reference, [] { return false; }, false);
    auto actual = sparse.Score(selection, true, [] { return false; }, work);
    assert(actual.order == expected.order && actual.length == expected.length);
    SeedSearchState report;
    report.portable = true;
    report.request.workers = 4;
    report.generated = report.evaluated = report.scored = 12000;
    report.elapsed = 2;
    auto text = SeedResultsText(report);
    assert(text.find("backend: verified CPU | workers: 4 | generated: 12000 | scored: 12000") !=
               std::string::npos &&
           text.find("average_checked_per_s: 6000") != std::string::npos);
    report.portable = false;
    report.elapsed = 0;
    text = SeedResultsText(report);
    assert(text.find("backend: Unity game thread") != std::string::npos &&
           text.find("average_checked_per_s: 0") != std::string::npos);
    std::cout
        << cases
        << " cached/fresh route comparisons, 1-64 crystals, equal-distance ties, shuffled layouts, exact/heuristic and sparse fallback: PASS\n";
}
