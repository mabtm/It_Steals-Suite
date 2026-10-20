#include "ReplayArchive.h"
#include "RoutePlayback.h"
#include "seed_map_fixture.h"
#include <cassert>
#include <chrono>
#include <iostream>
using namespace practice;

template <class F> void Await(F predicate)
{
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < end)
    {
        if (predicate())
            return;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    throw std::runtime_error("Replay worker timed out");
}

static auto ReadMap(const char *path)
{
    std::ifstream in(path, std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::string error;
    auto d = DecodeMap(std::move(bytes), error);
    if (!d)
        throw std::runtime_error(error);
    return d;
}

int main(int argc, char **argv)
{
    assert(argc == 5);
    auto wire = ReadMap(argv[1]), outside = ReadMap(argv[3]), miss = ReadMap(argv[4]);
    assert(outside->scene == 13 && MapLevelIndex(13) == 7 && kMapScenes[6] == 12);
    assert(wire->movementTick == .02f && wire->movementSprintStep == .14f &&
           wire->review->lightSources == 255 && wire->review->lightRejected == 2);
    assert(wire->review && wire->review->valid && wire->review->monsters.size() == 1 &&
           wire->review->flashes.size() == 2);
    assert(miss->review && miss->review->monsters.empty() && miss->review->flashes.size() == 2 &&
           miss->review->flashes[0].actor == 0 && miss->review->flashes[1].kind == 2);
    for (int outcome : {2, 3, 4, 5})
        for (bool valid : {false, true})
            assert(std::string(ReplayStatusName(outcome, valid)) == "ABORTED");
    assert(std::string(ReplayStatusName(1, true)) == "VALID" &&
           std::string(ReplayStatusName(1, false)) == "UNVALID");
    std::string eventError;
    assert(!DecodeMap(EncodeFixture(*miss, 8), eventError));
    assert(DecodeMap(EncodeFixture(*wire, 8), eventError));
    auto wrongActor = *miss;
    auto badEvent = std::make_shared<RunReview>(*miss->review);
    badEvent->flashes[0].actor = 90;
    wrongActor.review = badEvent;
    assert(!DecodeMap(EncodeFixture(wrongActor, 9), eventError));
    badEvent->flashes[0].actor = -1;
    assert(!DecodeMap(EncodeFixture(wrongActor, 9), eventError));
    std::cout
        << "Replay v9: emitted flashlight/camera flashes without monsters; valid v8 compatibility, invalid actor rejection, and every aborted outcome takes precedence over VALID: PASS\n";
    auto monsters = MonstersAt(*wire->review, .3, true);
    assert(monsters.size() == 1 && monsters[0].path && monsters[0].path->size() == 2);
    assert(monsters[0].pose.marker.x == 0); // Do not bridge a .5 s observation gap.
    assert(MonstersAt(*wire->review, 1.2, true).empty());
    auto smooth = *wire->review;
    smooth.monsters[0].trace[1].continuous = true;
    auto keyframed = MonstersAt(smooth, .3, true);
    assert(keyframed.size() == 1 && std::abs(keyframed[0].pose.marker.x - 2.4) < .001);
    auto bad = wire->bytes;
    bad[24] ^= 1;
    std::string error;
    assert(!DecodeMap(bad, error) && error.find("checksum") != std::string::npos);
    auto invalid = *wire;
    auto review = std::make_shared<RunReview>(*wire->review);
    review->aids = 4;
    invalid.review = review;
    assert(!DecodeMap(EncodeFixture(invalid, 8), error));
    review->valid = false;
    auto invalidBytes = EncodeFixture(invalid, 8);
    assert(DecodeMap(invalidBytes, error) && !DecodeMap(invalidBytes, error)->review->valid);
    review->monsters[0].paths[0].seconds = 2;
    assert(!DecodeMap(EncodeFixture(invalid, 8), error));
    std::cout
        << "Cross-language v8: validation, Outside, monster paths/events, timestamps, checksum and conservative legacy validity: PASS\n";
    auto root = std::filesystem::path(argv[2]);
    std::filesystem::remove_all(root);
    auto source = root / "game";
    auto library = root / "saved";
    std::filesystem::create_directories(source);
    for (int i = 0; i < 14; i++)
    {
        auto d = *wire;
        auto r = std::make_shared<RunReview>(*wire->review);
        r->id = 1000 + i;
        r->outcome = i % 5 + 1;
        d.review = r;
        if (i % 2)
            d.scene = 13;
        auto b = EncodeFixture(d, i == 13 ? 7 : 8);
        std::ofstream out(source / ("run-" + std::to_string(r->id) + ".isrun"), std::ios::binary);
        out.write(reinterpret_cast<const char *>(b.data()), std::streamsize(b.size()));
    }
    {
        ReplayArchive archive;
        archive.SetDirectory(library);
        archive.SetSourceDirectory(source);
        Await([&] { return archive.Get()->runs.size() == 14; });
        assert(!archive.Get()->runs.front().valid);
        archive.Load(1000);
        Await([&] { return archive.Loaded() && archive.Loaded()->review->id == 1000; });
        auto data = archive.Loaded();
        archive.Save(data, "Early attempt / café", "Routing / practice",
                     "Take the first corner later.\nCamera test / カメラ");
        Await([&] {
            auto s = archive.Get();
            return std::any_of(s->runs.begin(), s->runs.end(), [](const auto &r) {
                return r.id == 1000 && r.saved && r.notes.find("\n") != std::string::npos;
            });
        });
        archive.Save(data, "Umbenannt", "PB", "Neue Notiz");
        Await([&] {
            auto s = archive.Get();
            return std::any_of(s->runs.begin(), s->runs.end(),
                               [](const auto &r) { return r.id == 1000 && r.name == "Umbenannt"; });
        });
        assert(archive.Get()->savedRevision == 2 && archive.Get()->savedRun == 1000);
        archive.Shutdown();
    }
    {
        ReplayArchive archive;
        archive.SetDirectory(library);
        Await([&] { return archive.Get()->runs.size() == 14; });
        auto rows = archive.Get();
        auto row =
            std::find_if(rows->runs.begin(), rows->runs.end(), [](const auto &r) { return r.id == 1000; });
        assert(row != rows->runs.end() && row->valid && row->name == "Umbenannt" && row->category == "PB" &&
               row->notes == "Neue Notiz");
        archive.Load(1000);
        Await([&] { return archive.Loaded() != nullptr; });
        assert(archive.Loaded()->review->monsters[0].paths.size() == 1);
        archive.Save(archive.Loaded(), std::string(161, 'x'), "", "unsafe");
        Await([&] { return !archive.Get()->error.empty(); });
        assert(archive.Get()->runs.size() == 14 && archive.Get()->savedRevision == 0);
        archive.Shutdown();
    }
    auto metadata = library / "run-1000.meta";
    {
        std::fstream out(metadata, std::ios::binary | std::ios::in | std::ios::out);
        out.put('X');
    }
    {
        ReplayArchive archive;
        archive.SetDirectory(library);
        Await([&] { return archive.Get()->runs.size() == 14; });
        auto rows = archive.Get();
        assert(!rows->error.empty());
        auto row =
            std::find_if(rows->runs.begin(), rows->runs.end(), [](const auto &r) { return r.id == 1000; });
        assert(row->name == "Early attempt / café" && row->category == "Routing / practice");
        archive.Shutdown();
    }
    std::cout
        << "Replay library: >8 completed/aborted records, repeated loading, save/rename/category/UTF-8 notes, restart, immutable validity, damaged notes backup and limits: PASS\n";
    RunReview run;
    run.id = 42;
    run.seconds = 12;
    run.pickupSeconds = {4.8, 11.8};
    for (int i = 0; i <= 600; i++)
    {
        double t = i * .02, x = t < 4 ? t * 9 : t < 5 ? 36 : t < 11 ? 36 + (t - 5) * 3 : 54;
        run.trace.push_back({t, {float(x), 0, 0}});
    }
    auto profile = BuildTimingProfile(run);
    assert(profile.valid && std::abs(profile.reconstructed - 12) < 1e-6 && profile.stopped > 1 &&
           profile.coverage > .99);
    auto gap = run;
    gap.trace.erase(gap.trace.begin() + 100, gap.trace.begin() + 201);
    auto lost = BuildTimingProfile(gap);
    assert(!lost.valid && lost.missing > 2 && std::abs(lost.reconstructed - 12) < 1e-6);
    RunReview corners;
    corners.seconds = 6;
    for (int i = 0; i <= 300; i++)
    {
        double t = i * .02;
        float phase = float(i % 4);
        float x = float(i / 4) * .4f + (phase >= 1 ? .2f : 0) + (phase >= 3 ? .2f : 0),
              z = phase == 2 || phase == 3 ? .2f : 0;
        corners.trace.push_back({t, {x, 0, z}});
    }
    auto cornerProfile = BuildTimingProfile(corners);
    double walked = 0;
    for (size_t i = 1; i < corners.trace.size(); i++)
        walked += Distance(corners.trace[i - 1].point, corners.trace[i].point);
    assert(cornerProfile.valid &&
           std::abs(std::accumulate(cornerProfile.distance.begin(), cornerProfile.distance.end(), 0.) -
                    walked) < 1e-5 &&
           std::abs(cornerProfile.reconstructed - corners.seconds) < 1e-6);
    assert(profile.pickupPause == 0);
    auto physics = EstimateGameTravel(70, .02f, .14f);
    assert(physics.valid && std::abs(physics.slow - 10) < 1e-5 &&
           std::abs(physics.fast - 10 / std::sqrt(2.)) < 1e-5);
    assert(!EstimateGameTravel(70, 0, .14f).valid);
    std::cout
        << "Timing regression: integrated tight-turn distance, natural duration reconstruction without fitted scaling, no invented pickup pause, and verified fixed-step diagonal mechanics: PASS\n";
    ReviewPlayback player;
    player.Bind(run.id);
    player.seconds = 11;
    player.Restart();
    assert(player.seconds == 0 && player.playing);
    auto end = PlayedSlice(run, 12);
    assert(end.visible && end.marker.x == 54);
    std::cout
        << "Timing: exact recorded-duration reconstruction, explicit waits, gaps and verified-path gating: PASS\n";
}
