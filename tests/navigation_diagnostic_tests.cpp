#include "NavigationCapture.h"
#include "seed_map_fixture.h"
#include <cassert>
#include <iostream>

namespace capture = suite_navigation_capture;

static std::vector<uint8_t> Read(const std::filesystem::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

static capture::Snapshot Await(capture::Service &service)
{
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < until)
    {
        auto result = service.Poll();
        if (result.phase != capture::Phase::Pending)
            return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("Navigation capture worker timed out");
}

int main()
{
    auto folder = std::filesystem::temp_directory_path() /
                  ("suite-navigation-capture-test-" + std::to_string(
                      std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(folder);
    struct Cleanup
    {
        std::filesystem::path folder;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(folder, ignored); }
    } cleanup{folder};

    practice::MapData fixture;
    fixture.pid = 1234;
    fixture.scene = 7;
    fixture.epoch = 42;
    fixture.flags = 1;
    fixture.stage = 2;
    fixture.keep = 2;
    fixture.spawn = fixture.liveSpawn = 0;
    fixture.spawns = {{2, 1, 2}};
    fixture.points = {{8, 1, 2}, {18, 1, 2}};
    fixture.selected = fixture.liveSelected = {0, 1};
    fixture.navigationStatus = 2;
    fixture.navigationNodes = {{2, 1, 2}, {8, 1, 2}, {18, 1, 2}};
    fixture.navigationEdges = {0, 1, 1, 2};
    fixture.navigationAnchors = {0, 1, 2};
    fixture.navigationQueries = fixture.navigationTotal = 3;
    fixture.physicalCaptured = true;
    fixture.agentRadius = .4f;
    fixture.agentHeight = 2;
    fixture.agentSlope = 45;
    fixture.physicalParts = {{3, {{4, 0, -4}, {6, 0, -4}, {6, 0, 4}, {4, 0, 4}}}};
    fixture.hash = std::string(64, 'a');
    fixture.capturedUtc = 638800000000000000LL;
    FixtureTile(fixture, 0, 0, 20, 10);
    std::string error;
    auto exact = practice::DecodeMap(EncodeFixture(fixture), error);
    assert(exact && exact->stage == 2 && error.empty());
    auto cached = std::make_shared<practice::MapData>(*exact);
    cached->stage = 1;
    cached->epoch = 99;
    assert(capture::Source(true, exact, cached) == exact);
    assert(!capture::Source(true, nullptr, cached)); // no stale cache substitution during a search
    assert(capture::Source(false, exact, cached) == cached);

    capture::Metadata metadata{"7.22.0", "7.22.0", "Exact active search input", "Preparing",
                               "navigation paths", 59, 1932};
    capture::Service service;
    service.SetDirectory(folder / "NavigationCaptures");
    assert(service.View().phase == capture::Phase::Idle);
    assert(service.Begin(exact, metadata));
    assert(service.View().phase == capture::Phase::Pending);
    assert(!service.Begin(cached, metadata)); // bounded queue; cannot overwrite an in-flight input
    auto saved = Await(service);
    assert(saved.phase == capture::Phase::Saved && saved.error.empty());
    auto bytes = Read(saved.folder / "map.ismap");
    assert(bytes == exact->bytes); // original wire bytes, not a decoded/re-encoded approximation
    auto decoded = practice::DecodeMap(bytes, error);
    assert(decoded && decoded->stage == 2 && decoded->epoch == 42 && decoded->physicalCaptured);
    assert(decoded->vertices.size() == fixture.vertices.size() &&
           decoded->indices == fixture.indices && decoded->navigationEdges == fixture.navigationEdges &&
           decoded->navigationAnchors == fixture.navigationAnchors &&
           decoded->physicalParts.size() == 1 && decoded->spawns.size() == 1 && decoded->points.size() == 2);
    auto reportBytes = Read(saved.folder / "capture.txt");
    std::string report(reportBytes.begin(), reportBytes.end());
    assert(report.find("Progress: 59 / 1932") != std::string::npos &&
           report.find("Packet stage: 2") != std::string::npos &&
           report.find("Exact active search input") != std::string::npos);
    assert(service.Begin(exact, metadata));
    auto second = Await(service);
    assert(second.phase == capture::Phase::Saved && second.folder != saved.folder &&
           Read(saved.folder / "map.ismap") == bytes);

    // Bad destinations and invalid packet bytes report failure asynchronously;
    // they do not replace the previous successful capture or strand Pending.
    auto invalidRoot = folder / "regular-file";
    { std::ofstream file(invalidRoot); file << "keep"; }
    service.SetDirectory(invalidRoot);
    assert(service.Begin(exact, metadata));
    auto failed = Await(service);
    assert(failed.phase == capture::Phase::Failed && !failed.error.empty() && failed.folder.empty());
    assert(Read(invalidRoot) == std::vector<uint8_t>({'k', 'e', 'e', 'p'}));
    service.SetDirectory(folder / "recovered");
    auto damaged = std::make_shared<practice::MapData>(*exact);
    damaged->bytes.back() ^= 1;
    assert(service.Begin(damaged, metadata));
    failed = Await(service);
    assert(failed.phase == capture::Phase::Failed && failed.error.find("validation") != std::string::npos);
    assert(!std::filesystem::exists(folder / "recovered"));
    assert(service.Begin(exact, metadata));
    service.Shutdown(); // a requested capture is completed before worker teardown
    assert(service.Poll().phase == capture::Phase::Saved);
    assert(!service.Begin(exact, metadata));
    std::cout << "Exact stage-2 navigation capture, async lifecycle, source identity, round-trip and IO failures: PASS\n";
}
