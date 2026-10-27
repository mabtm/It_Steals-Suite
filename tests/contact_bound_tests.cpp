#include "contact_bound_lab.h"
#include <cassert>
#include <iostream>

template <class Cost>
double ExactContactRoute(const std::vector<size_t> &contacts, const Cost &read, const std::vector<int> &nodes)
{
    if (nodes.size() < 2)
        return 0;
    std::vector<int> order(nodes.begin() + 1, nodes.end());
    std::sort(order.begin(), order.end());
    double optimum = std::numeric_limits<double>::infinity();
    do
    {
        std::vector<double> previous(contacts[nodes[0]], std::numeric_limits<double>::infinity());
        previous[0] = 0;
        int from = nodes[0];
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
    return optimum;
}

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
                        costs[i * total + j] = costs[j * total + i] = profile == 0 ? (next() % 1024) / 8.
                                                                      : profile == 1
                                                                          ? double(next() % 4)
                                                                          : std::abs(double(i) - double(j));
                auto read = [&](size_t a, size_t x, size_t b, size_t y) {
                    return costs[(offset[a] + x) * total + offset[b] + y];
                };
                practice::ContactBoundLab bound(contacts, read, [] { return false; });
                std::vector<int> nodes(n + 1);
                std::iota(nodes.begin(), nodes.end(), 0);
                for (int subset = 0; subset < 3; subset++)
                {
                    double optimum = ExactContactRoute(contacts, read, nodes);
                    double lower = bound.Lower(nodes, [] { return false; });
                    assert(std::isfinite(lower) && lower >= 0 && lower <= optimum);
                    for (double cutoff : {optimum - 1, optimum, optimum + 1})
                        assert(bound.Tighten(nodes, lower, cutoff, [] { return false; }) <= optimum);
                    cases++;
                    if (nodes.size() > 2)
                        nodes.erase(nodes.begin() + 1);
                    std::reverse(nodes.begin(), nodes.end());
                }
            }
    for (int n : {16, 22, 52, 64, 127, 255})
        for (double scale : {1e-100, 1e-9, 1., 1e100})
        {
            std::vector<size_t> contacts(n + 1, 2);
            std::vector<int> nodes(n + 1);
            std::iota(nodes.begin(), nodes.end(), 0);
            auto star = [&](size_t a, size_t, size_t b, size_t) {
                return (a <= 1 && b <= 1 ? 0 : a <= 1 || b <= 1 ? 1 : 2) * scale;
            };
            practice::ContactBoundLab bound(contacts, star, [] { return false; });
            double optimum = (2 * n - 3) * scale;
            double lower = bound.Lower(nodes, [] { return false; });
            assert(lower <= optimum);
            assert(bound.Tighten(nodes, lower, optimum, [] { return false; }) <= optimum);
        }
    std::vector<size_t> contacts{1, 2, 1};
    std::vector<std::vector<double>> points{{0}, {1, 9}, {10}};
    auto read = [&](size_t a, size_t x, size_t b, size_t y) { return std::abs(points[a][x] - points[b][y]); };
    practice::ContactBoundLab example(contacts, read, [] { return false; });
    std::array<int, 3> nodes{0, 1, 2};
    assert(example.Lower(nodes, [] { return false; }) > 5.999999 &&
           example.Lower(nodes, [] { return false; }) <= 6);
    bool canceled = false;
    try
    {
        example.Lower(nodes, [] { return true; });
    }
    catch (const std::runtime_error &)
    {
        canceled = true;
    }
    assert(canceled);
    std::cout
        << cases
        << " exhaustive joint visit/contact optima, shuffled subsets, 256-node catalog masks, scales, tied/zero costs and cancellation: PASS\n";
}
