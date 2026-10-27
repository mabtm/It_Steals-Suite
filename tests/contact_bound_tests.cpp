#include "contact_bound_lab.h"
#include <cassert>
#include <iostream>

int main()
{
    uint32_t random = 0x720000;
    auto next = [&] { return random = random * 1664525u + 1013904223u; };
    int cases = 0;
    for (int n = 1; n <= 7; n++)
        for (int profile = 0; profile < 3; profile++)
            for (int repeat = 0; repeat < 12; repeat++)
            {
                std::vector<size_t> contacts(n + 1, 1), offset(n + 2);
                for (int i = 1; i <= n; i++)
                    contacts[i] = 1 + next() % 3;
                std::partial_sum(contacts.begin(), contacts.end(), offset.begin() + 1);
                size_t total = offset.back();
                std::vector<double> costs(total * total);
                for (size_t i = 0; i < total; i++)
                    for (size_t j = i + 1; j < total; j++)
                        costs[i * total + j] = costs[j * total + i] =
                            profile == 0 ? (next() % 1024) / 8.
                            : profile == 1 ? double(next() % 4)
                                           : std::abs(double(i) - double(j));
                auto read = [&](size_t a, size_t x, size_t b, size_t y) {
                    return costs[(offset[a] + x) * total + offset[b] + y];
                };
                practice::ContactBoundLab bound(contacts, read, [] { return false; });
                std::vector<int> nodes(n + 1), order(n);
                std::iota(nodes.begin(), nodes.end(), 0);
                std::iota(order.begin(), order.end(), 1);
                double optimum = std::numeric_limits<double>::infinity();
                do
                {
                    std::vector<double> previous{0};
                    int from = 0;
                    for (int to : order)
                    {
                        std::vector<double> best(contacts[to], std::numeric_limits<double>::infinity());
                        for (size_t x = 0; x < previous.size(); x++)
                            for (size_t y = 0; y < best.size(); y++)
                                best[y] = std::min(best[y], previous[x] + read(from, x, to, y));
                        previous = std::move(best);
                        from = to;
                    }
                    optimum = std::min(optimum, *std::min_element(previous.begin(), previous.end()));
                } while (std::next_permutation(order.begin(), order.end()));
                assert(bound.Lower(nodes, [] { return false; }) <= optimum);
                for (double cutoff : {optimum - 1, optimum, optimum + 1})
                    assert(bound.Tighten(nodes, bound.Lower(nodes, [] { return false; }), cutoff,
                                         [] { return false; }) <= optimum);
                // Catalog indices and contact offsets stay global for subsets.
                if (n > 1)
                    nodes.erase(nodes.begin() + 1);
                double subset = bound.Lower(nodes, [] { return false; });
                assert(std::isfinite(subset) && subset >= 0);
                cases++;
            }
    std::vector<size_t> contacts{1, 2, 1};
    std::vector<std::vector<double>> points{{0}, {1, 9}, {10}};
    auto read = [&](size_t a, size_t x, size_t b, size_t y) { return std::abs(points[a][x] - points[b][y]); };
    practice::ContactBoundLab example(contacts, read, [] { return false; });
    std::array<int, 3> nodes{0, 1, 2};
    assert(example.Lower(nodes, [] { return false; }) > 5.999999 &&
           example.Lower(nodes, [] { return false; }) <= 6);
    bool canceled = false;
    try { example.Lower(nodes, [] { return true; }); }
    catch (const std::runtime_error &) { canceled = true; }
    assert(canceled);
    std::cout << cases << " exhaustive joint visit/contact optima, catalog subsets, tied/zero costs and cancellation: PASS\n";
}
