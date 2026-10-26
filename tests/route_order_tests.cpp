#include "PracticeRouting.h"
#include <cassert>
#include <iostream>

// Frozen pre-7.17 fingerprints: tied, non-metric and grid
// distances. Power-of-two fractions keep every comparison platform-independent.
static uint64_t Fingerprint(int n, int profile)
{
    practice::RouteWorkspace work;
    uint32_t random = 0x715000u + n * 31 + profile;
    auto next = [&] { return random = random * 1664525u + 1013904223u; };
    uint64_t hash = 14695981039346656037ULL;
    auto mix = [&](uint64_t value) { hash = (hash ^ value) * 1099511628211ULL; };
    for (int repeat = 0; repeat < 12; repeat++)
    {
        int size = n + 1;
        std::array<std::array<int, 2>, 65> points;
        for (auto &p : points)
            p = {int(next() % 128), int(next() % 128)};
        for (int a = 0; a < size; a++)
        {
            work.matrix[a * size + a] = 0;
            for (int b = a + 1; b < size; b++)
            {
                double cost = profile == 0   ? (1 + next() % 8192) / 8.
                              : profile == 1 ? double(next() % 5)
                              : profile == 2 ? double(std::abs(points[a][0] - points[b][0]) +
                                                      std::abs(points[a][1] - points[b][1]))
                                             : (1 + next() % 8192) / 8.;
                work.matrix[a * size + b] = work.matrix[b * size + a] = cost;
                if (profile == 3)
                    work.matrix[b * size + a] = (1 + next() % 8192) / 8.;
            }
        }
        for (int exact = 0; exact <= int(n <= 15); exact++)
        {
            auto route =
                practice::SolveOrderFlat(work.matrix.data(), size, work, [] { return false; }, exact);
            assert(route.complete && route.order.size() == size_t(n) && route.exact == bool(exact));
            uint64_t visited = 0;
            int previous = 0;
            double length = 0;
            for (int id : route.order)
            {
                assert(id >= 0 && id < n && !(visited & (uint64_t{1} << id)));
                visited |= uint64_t{1} << id;
                length += work.matrix[previous * size + id + 1];
                previous = id + 1;
                mix(id + 1);
            }
            assert(length == route.length);
            mix(std::bit_cast<uint64_t>(length));
        }
    }
    return hash;
}

int main()
{
    constexpr uint64_t expected[][3] = {
        {12117875592393511173ULL, 9086150399684890885ULL, 14805158976491999493ULL},
        {3407634581858100641ULL, 5005128921852511295ULL, 6439056557780797567ULL},
        {7188228460639288765ULL, 17610730119036792733ULL, 2280193273757023277ULL},
        {17520852117408414615ULL, 18237447901036617867ULL, 5111113660948551275ULL},
        {571619580999766981ULL, 14721048749964028103ULL, 3934606641263893541ULL},
        {15214400589602694419ULL, 1776531299721853609ULL, 8685236504025859735ULL},
        {11498932256997930337ULL, 17813931701174841751ULL, 10418111275456613175ULL},
        {7978581916984128793ULL, 16263134512004231675ULL, 16429632061446302729ULL},
        {7919396285002477737ULL, 8280354242353619261ULL, 9181873428648029671ULL},
    };
    size_t row = 0;
    for (int n : {1, 8, 14, 15, 16, 22, 40, 52, 64})
    {
        for (int profile = 0; profile < 3; profile++)
            assert(Fingerprint(n, profile) == expected[row][profile]);
        row++;
    }
    // Directed input retains the general solver, without symmetric shortcuts.
    assert(Fingerprint(22, 3) == 6127160003320515981ULL);
    std::cout
        << "Frozen route-order/length parity, 1-64 crystals, exact/heuristic, tied, non-metric and directed distances: PASS\n";
}
