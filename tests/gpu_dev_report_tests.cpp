#include "GpuDevReport.h"
#include <cassert>
#include <iostream>

int main(int argc, char **argv)
{
    using namespace gpu_dev;
    assert(ValidOrder({0, 2, 1}, 3));
    assert(!ValidOrder({0, 2, 2}, 3));
    assert(!ValidOrder({0, -1, 2}, 3));
    assert(!ValidOrder({0, 3, 1}, 3));
    assert(!ValidOrder({0}, 0));
    std::vector<int> boundary(52);
    std::iota(boundary.begin(), boundary.end(), 0);
    assert(ValidOrder(boundary, 52));
    boundary.push_back(52);
    assert(!ValidOrder(boundary, 53));
    practice::RouteResult a;
    a.complete = a.pickupOptimized = true;
    a.order = {0, 1}; a.contacts = {1, 2}; a.length = 3;
    auto b = a;
    assert(SameScore(a, b));
    b.length = std::nextafter(a.length, 4.);
    assert(!SameScore(a, b));
    b = a; b.contacts[0] = 2;
    assert(!SameScore(a, b));
    b = a; b.estimated = true;
    assert(!SameScore(a, b));
    b = a; b.complete = false;
    assert(!SameScore(a, b));
    Report r;
    r.phase = Phase::Stopped;
    r.error = "Quoted \"GPU\"\\driver\n\t" + std::string(1, '\x01');
    r.options.key = std::numeric_limits<uint64_t>::max();
    r.cases.push_back({});
    r.cases.back().cpu = a;
    r.cases.back().gpu.length = std::numeric_limits<double>::infinity();
    auto json = Json(r);
    assert(json.find("\\u0001") != std::string::npos);
    assert(json.find("\"18446744073709551615\"") != std::string::npos);
    assert(json.find("\"length\":null") != std::string::npos);
    auto folder = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::temp_directory_path() / "suite-gpu-report-tests";
    Save(folder / "gpu-report.json", r);
    assert(!std::filesystem::exists(folder / "gpu-report.json.tmp"));
    std::cout << "GPU report: permutation boundaries, exact score/contact validation, JSON escapes, uint64 keys, nonfinite values and atomic export: PASS\n";
}
