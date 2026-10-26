#include "seed_route_fixture.h"
#include <iomanip>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <chrono>

using namespace practice;

int main(int argc, char **argv)
{
    if (argc != 4)
        return 2;
    std::string operation = argv[1];
    std::filesystem::path folder = argv[2];
    int count = std::stoi(argv[3]);
    std::filesystem::create_directories(folder);
    auto data = LoadRouteBenchmarkFixture("tests/fixtures/hide-seek-outside-boxes.routefixture", 52, 1.35f);
    NavGraph graph(data);
    SearchDistances distances(data, graph, [] { return false; }, [](int, int) {});
    std::vector<int> all(data.spawns.size() + data.points.size());
    std::iota(all.begin(), all.end(), 0);
    PickupRouting contacts(data, graph, all);
    auto table = graph.Routes([] { return false; }, [](int, int) {});
    if (operation != "export")
        for (int a = 0; a < int(all.size()); a++)
            for (int b = std::max(a + 1, int(data.spawns.size())); b < int(all.size()); b++)
                contacts.SetLeg(a, b, table->Between(a, b), table->estimates[a][b] != 0);
    PortableSelectionGenerator generator(data);
    RandomSeedOrder order(FixtureSearchKey);
    RouteWorkspace work;
    for (int i = 0; i < count; i++)
    {
        Selection selection;
        generator.Generate(order.SeedAt(i), 1, selection);
        auto started = std::chrono::steady_clock::now();
        auto baseline = distances.Score(selection, true, [] { return false; }, work);
        double oldMicros =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count();
        int size = int(selection.crystals.size()) + 1;
        auto stem = folder / std::to_string(i);
        if (operation == "export")
        {
            std::ofstream out(stem.string() + ".atsp");
            out << "NAME : seed" << i << "\nTYPE : ATSP\nDIMENSION : " << size
                << "\nEDGE_WEIGHT_TYPE : EXPLICIT\nEDGE_WEIGHT_FORMAT : FULL_MATRIX\nEDGE_WEIGHT_SECTION\n";
            // Free return to spawn turns a fixed-start open path into an ATSP cycle.
            for (int a = 0; a < size; a++)
            {
                for (int b = 0; b < size; b++)
                    out << (b ? int64_t(std::llround(work.matrix[a * size + b] * 100000)) : 0) << ' ';
                out << '\n';
            }
            out << "EOF\n";
            std::cout << i << ',' << std::setprecision(17) << baseline.length << ',' << oldMicros << '\n';
            continue;
        }
        std::ifstream input(stem.string() + ".tour");
        std::string token;
        while (input >> token && token != "TOUR_SECTION")
        {
        }
        std::vector<int> tour;
        int vertex;
        while (input >> vertex && vertex != -1)
            tour.push_back(vertex - 1);
        if (tour.size() != size_t(size))
            throw std::runtime_error("Incomplete LKH tour");
        auto spawn = std::find(tour.begin(), tour.end(), 0);
        if (spawn == tour.end())
            throw std::runtime_error("Missing fixed spawn");
        std::rotate(tour.begin(), spawn, tour.end());
        RouteResult result;
        result.complete = true;
        int previous = 0;
        uint64_t seen = 0;
        for (size_t k = 1; k < tour.size(); k++)
        {
            int goal = tour[k];
            if (goal < 1 || goal >= size || (seen & (uint64_t{1} << (goal - 1))))
                throw std::runtime_error("Invalid goal permutation");
            seen |= uint64_t{1} << (goal - 1);
            result.order.push_back(goal - 1);
            result.length += work.matrix[previous * size + goal];
            previous = goal;
        }
        std::array<int, 65> nodes;
        nodes[0] = selection.spawn;
        for (int k = 1; k < size; k++)
            nodes[k] = int(data.spawns.size()) + selection.crystals[k - 1];
        contacts.Optimize(result, std::span<const int>(nodes.data(), size_t(size)), [] { return false; });
        if (!result.pickupOptimized || result.estimated)
            throw std::runtime_error("Unverified LKH contacts");
        std::cout << i << ',' << std::setprecision(17) << baseline.length << ',' << result.length << ','
                  << oldMicros << '\n';
    }
}
