#include "ReplayArchive.h"
#include "seed_map_fixture.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <random>
using namespace practice;

static std::vector<uint8_t> Read(const std::filesystem::path &p)
{
    std::ifstream f(p, std::ios::binary);
    assert(f);
    return {std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

static void Write(const std::filesystem::path &p, const std::vector<uint8_t> &b)
{
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char *>(b.data()), std::streamsize(b.size()));
    assert(f);
}

template <class F> static void Until(F f)
{
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!f())
    {
        if (std::chrono::steady_clock::now() > end)
            throw std::runtime_error("archive operation timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

static bool BadZip(const std::vector<uint8_t> &b)
{
    try
    {
        DecodeReplayZip(b);
        return false;
    }
    catch (const std::exception &)
    {
        return true;
    }
}

static void Checksums(ReplayFiles &files)
{
    files.erase("SHA256SUMS.txt");
    std::string text;
    for (const auto &[name, b] : files)
        text += ReplaySha256(b) + "  " + name + "\n";
    files["SHA256SUMS.txt"] = ReplayText(text);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    std::filesystem::path fixtures = argv[1], test = argv[2];
    std::filesystem::remove_all(test);
    std::filesystem::create_directories(test);
    std::string error;
    auto map = DecodeMap(Read(fixtures / "input-map.isrun"), error);
    assert(map && map->review && map->review->motion);
    auto bytes = Read(fixtures / "input-wire.isinput");
    auto motion = DecodeMotion(bytes, error);
    assert(motion && motion->Matches(*map));
    assert(motion->frames.size() == 21 && motion->ticks.size() == 41 && motion->events.size() == 4);
    assert(motion->frames[0].lampIntensity == 2.5f && motion->frames[0].lampAngle == 62 &&
           motion->frames[0].lampPosition.y == 2 && motion->frames[4].mouseX == 1 &&
           motion->frames[0].fov == 75 && motion->frames[0].horizontal == .5f);
    assert(motion->colliders.recorded && motion->colliders.count == 12 &&
           motion->colliders.tracks.size() == 4 &&
           motion->colliders.tracks[3].samples[1].shape.height == 1.1f &&
           motion->colliders.tracks[1].samples[0].shape.matrix[3] == 2 &&
           motion->colliders.tracks[2].samples[2].state == 2);
    assert((motion->inputFormat == 4 || motion->inputFormat == 5 || motion->inputFormat == 6) &&
           motion->visualsRecorded && motion->visualTracks == 4 && motion->visualSamples == 12 &&
           motion->devices.size() == 2 && motion->deviceThrough == .2 &&
           motion->devices[0].pads[0].lx == -32768 && motion->devices[0].pads[0].rt == 240 &&
           motion->devices[0].lookX == .75f && motion->devices[0].moveY == 1);
    auto v3Motion = DecodeMotion(Read(fixtures / "input-v3.isinput"), error);
    assert(v3Motion && v3Motion->inputFormat == 3 && v3Motion->devices.size() == 2 &&
           !v3Motion->visualsRecorded);
    auto v2Motion = DecodeMotion(Read(fixtures / "input-v2.isinput"), error);
    assert(v2Motion && v2Motion->inputFormat == 2 && v2Motion->devices.empty() &&
           v2Motion->colliders.recorded);
    auto oldMotion = DecodeMotion(Read(fixtures / "input-v1.isinput"), error);
    assert(oldMotion && oldMotion->Matches(*map) && !oldMotion->colliders.recorded &&
           oldMotion->colliders.tracks.empty());
    auto annotations = std::make_shared<ReplayAnnotations>();
    annotations->run = map->review->id;
    annotations->duration = map->review->seconds;
    annotations->a = .1;
    annotations->b = .35;
    annotations->hasA = annotations->hasB = annotations->loop = true;
    annotations->markers = {{1, .12, "Radar / check corner"}, {2, .3, "Turn"}};
    auto markBytes = EncodeAnnotations(*annotations);
    auto marks = DecodeAnnotations(markBytes);
    assert(marks->Matches(*map) && marks->markers[0].name == annotations->markers[0].name && marks->loop &&
           marks->LoopValid());
    for (size_t n = 0; n < markBytes.size(); n++)
    {
        auto shortFile = markBytes;
        shortFile.resize(n);
        bool rejected = false;
        try
        {
            DecodeAnnotations(shortFile);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        assert(rejected);
    }
    for (int fault = 0; fault < 6; fault++)
    {
        auto bad = *annotations;
        if (fault == 0)
            bad.markers[1].id = 1;
        if (fault == 1)
            bad.markers[0].name = std::string("bad\xc0\x80", 5);
        if (fault == 2)
            bad.markers[0].seconds = std::numeric_limits<double>::quiet_NaN();
        if (fault == 3)
            bad.b = bad.a;
        if (fault == 4)
            bad.markers[0].name = "bad\nname";
        if (fault == 5)
            bad.markers[0].seconds = bad.duration + 1;
        bool rejected = false;
        try
        {
            EncodeAnnotations(bad);
        }
        catch (const std::exception &)
        {
            rejected = true;
        }
        assert(rejected);
    }
    assert(ReplayInputAt(*motion, 0)->keys == 1 && ReplayInputAt(*motion, .25)->keys == 65 &&
           !(ReplayInputAt(*motion, .25)->state & 8) && ReplayInputAt(*motion, .35)->state & 8 &&
           ReplayInputAt(*motion, .45)->keys == 0);
    assert(!ReplayInputAt(*motion, -1) && !ReplayInputAt(*motion, motion->seconds + 1) &&
           !ReplayAt(motion->events, std::numeric_limits<double>::quiet_NaN()));
    assert(ReplayFrameStep(*motion, .1, true) > .1 && ReplayFrameStep(*motion, .1, false) < .1 &&
           ReplayFrameStep(*motion, 0, false) == 0 &&
           ReplayFrameStep(*motion, motion->seconds, true) == motion->seconds);
    auto altered = *map;
    altered.seed++;
    assert(!motion->Matches(altered));
    altered = *map;
    auto r = std::make_shared<RunReview>(*map->review);
    r->id++;
    altered.review = r;
    assert(!motion->Matches(altered));
    for (size_t n = 0; n < bytes.size(); n += 7)
    {
        auto shortFile = bytes;
        shortFile.resize(n);
        assert(!DecodeMotion(std::move(shortFile), error));
    }
    auto damaged = bytes;
    damaged[100] ^= 1;
    assert(!DecodeMotion(damaged, error));
    std::mt19937 rng(720);
    for (int i = 0; i < 1500; i++)
    {
        auto fuzz = bytes;
        for (int k = 0; k < 1 + i % 4; k++)
            fuzz[rng() % (fuzz.size() - 8)] = uint8_t(rng());
        auto hash = ReplayHash(fuzz, fuzz.size() - 8);
        std::memcpy(fuzz.data() + fuzz.size() - 8, &hash, 8);
        DecodeMotion(std::move(fuzz), error);
    }
    auto legacy = DecodeMap(EncodeFixture(*map, 11), error);
    assert(legacy && !legacy->review->motion);
    auto current = DecodeMap(EncodeFixture(*map, 12), error);
    assert(current && motion->Matches(*current));
    assert(ReplaySha256({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    assert(ReplaySha256(ReplayText("abc")) ==
           "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    assert(ReplaySha256(std::vector<uint8_t>(1000000, 'a')) ==
           "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    assert(ReplayCrc(ReplayText("123456789")) == 0xcbf43926u);
    ReplayFiles simple{{"a.txt", ReplayText("abc")}, {"b.txt", {}}};
    auto zip = EncodeReplayZip(simple);
    assert(DecodeReplayZip(zip) == simple);
    assert(BadZip({}));
    for (size_t n = 0; n < zip.size(); n++)
    {
        auto z = zip;
        z.resize(n);
        assert(BadZip(z));
    }
    auto unsafe = zip;
    unsafe[30] = '/';
    assert(BadZip(unsafe));
    auto badData = zip;
    badData[35] ^= 1;
    assert(BadZip(badData));
    std::cout
        << "Cross-language input format, immutable run matching, separate frame/tick clocks, radar request/acceptance, frame stepping, gaps, 1500 fuzz cases, legacy v11 and SHA256/CRC reference vectors: PASS\n";
    auto source = test / "game", saved = test / "saved";
    auto prefix = "run-" + std::to_string(map->review->id);
    Write(source / (prefix + ".isinput"), bytes);
    Write(source / (prefix + ".validation.json"),
          ReplayText("{\"runId\":" + std::to_string(map->review->id) + ",\"original\":true}\n"));
    Write(source / (prefix + ".isrun"), map->bytes);
    auto wireCopy = *map;
    wireCopy.pid++;
    wireCopy.flags &= ~8;
    wireCopy.searchBackend = 0;
    auto transport = DecodeMap(EncodeFixture(wireCopy, 12), error);
    assert(transport && transport->bytes != map->bytes);
    std::filesystem::path exported;
    {
        ReplayArchive archive;
        archive.SetDirectory(saved);
        archive.SetSourceDirectory(source);
        Until([&] { return archive.Get()->runs.size() == 1; });
        archive.LoadMotion(transport);
        Until([&] { return archive.Motion() || !archive.Get()->motionError.empty(); });
        assert(archive.Motion() && archive.Motion()->Matches(*map));
        archive.Save(transport, "Input run", "Moderation", "Original notes\nSecond line");
        Until([&] { return archive.Get()->savedRevision == 1 && !archive.Get()->busy; });
        assert(Read(saved / (prefix + ".isinput")) == bytes &&
               Read(saved / (prefix + ".validation.json")) == Read(source / (prefix + ".validation.json")));
        archive.LoadAnnotations(transport);
        Until([&] { return archive.Annotations() && archive.Annotations()->Matches(*map); });
        auto revision = archive.Get()->annotationRevision;
        archive.SaveAnnotations(transport, annotations);
        Until([&] {
            return archive.Get()->annotationRevision > revision || !archive.Get()->annotationError.empty();
        });
        assert(archive.Get()->annotationError.empty() && archive.Annotations()->markers.size() == 2);
        archive.Export(transport);
        Until([&] { return archive.Get()->exportedRun == map->review->id || !archive.Get()->error.empty(); });
        assert(archive.Get()->error.empty());
        exported = archive.Get()->exportPath;
        assert(std::filesystem::exists(exported));
        archive.Stage3D(transport);
        Until([&] { return archive.Get()->stagedRun == map->review->id; });
        assert(Read(source / "ReplayInbox" / (prefix + ".isinput")) == bytes &&
               Read(source / "ReplayInbox" / (prefix + ".isrun")) == map->bytes);
        archive.Shutdown();
    }
    auto package = DecodeReplayZip(Read(exported));
    assert(package.size() == 8 && package[prefix + ".isrun"] == map->bytes &&
           package[prefix + ".isinput"] == bytes && package[prefix + ".ismarks"] == markBytes &&
           package["manifest.json"] ==
               ReplayText(ReplayManifest(*map, true, true, suite_brand::Version, true)));
    auto imported = test / "imported";
    {
        ReplayArchive archive;
        archive.SetDirectory(imported);
        archive.ImportPackage(exported);
        Until([&] {
            return (archive.Get()->savedRevision == 1 && !archive.Get()->busy) ||
                   !archive.Get()->error.empty();
        });
        assert(archive.Get()->error.empty() && archive.Get()->runs.size() == 1 &&
               archive.Get()->runs[0].name == "Input run" &&
               archive.Get()->runs[0].notes == "Original notes\nSecond line");
        archive.LoadMotion(map);
        Until([&] { return bool(archive.Motion()); });
        assert(archive.Motion()->Matches(*map));
        archive.LoadAnnotations(map);
        Until([&] { return bool(archive.Annotations()); });
        assert(archive.Annotations()->markers.size() == 2 && archive.Annotations()->LoopValid());
        assert(Read(imported / (prefix + ".isrun")) == map->bytes &&
               Read(imported / (prefix + ".isinput")) == bytes);
        archive.Shutdown();
    }
    auto prior = package;
    prior.erase(prefix + ".ismarks");
    prior["manifest.json"] = ReplayText(ReplayManifest(*map, true, true, "7.2.0"));
    Checksums(prior);
    auto priorPath = test / "version-720.zip";
    Write(priorPath, EncodeReplayZip(prior));
    {
        ReplayArchive archive;
        archive.SetDirectory(test / "prior-version");
        archive.ImportPackage(priorPath);
        Until([&] {
            return (archive.Get()->savedRevision == 1 && !archive.Get()->busy) ||
                   !archive.Get()->error.empty();
        });
        assert(archive.Get()->error.empty() && archive.Get()->runs.size() == 1);
        archive.Shutdown();
    }
    prior["manifest.json"] = ReplayText(ReplayManifest(*map, true, true, "7.2.1"));
    Checksums(prior);
    auto previousPath = test / "version-721.zip";
    Write(previousPath, EncodeReplayZip(prior));
    {
        ReplayArchive archive;
        archive.SetDirectory(test / "prior-721");
        archive.ImportPackage(previousPath);
        Until([&] {
            return (archive.Get()->savedRevision == 1 && !archive.Get()->busy) ||
                   !archive.Get()->error.empty();
        });
        assert(archive.Get()->error.empty());
        archive.Shutdown();
    }
    for (const std::string version :
         {"7.3.0",  "7.3.1",  "7.3.2",  "7.3.3",  "7.3.4",  "7.3.5",  "7.4.0",  "7.5.0",  "7.5.1",  "7.5.2",
          "7.6.0",  "7.7.0",  "7.8.0",  "7.8.1",  "7.9.0",  "7.10.0", "7.10.1", "7.11.0", "7.11.1", "7.11.2",
          "7.11.3", "7.11.4", "7.11.5", "7.12.0", "7.12.1", "7.12.2", "7.12.3", "7.12.4", "7.12.5", "7.12.6"})
    {
        auto previousPackage = package;
        previousPackage["manifest.json"] = ReplayText(ReplayManifest(*map, true, true, version, true));
        Checksums(previousPackage);
        auto previousMarkedPath = test / ("previous-" + version + "-markers.zip");
        Write(previousMarkedPath, EncodeReplayZip(previousPackage));
        {
            ReplayArchive archive;
            auto previousRoot = test / ("previous-" + version);
            archive.SetDirectory(previousRoot);
            archive.ImportPackage(previousMarkedPath);
            Until([&] {
                return (archive.Get()->savedRevision == 1 && !archive.Get()->busy) ||
                       !archive.Get()->error.empty();
            });
            assert(archive.Get()->error.empty() && Read(previousRoot / (prefix + ".ismarks")) == markBytes &&
                   Read(previousRoot / (prefix + ".isinput")) == bytes);
            archive.Shutdown();
        }
    }
    // Recomputed ZIP/checksums must not let unsupported exporters or formats through.
    for (const auto &[version, withMarkers] :
         {std::pair{"7.2.0", true}, {"7.2.1", true}, {"7.3.6", false}, {"7.12.99", true}, {"99.0.0", false}})
    {
        auto rejected = package;
        if (!withMarkers)
            rejected.erase(prefix + ".ismarks");
        rejected["manifest.json"] = ReplayText(ReplayManifest(*map, true, true, version, withMarkers));
        Checksums(rejected);
        auto rejectedPath = test / ("rejected-" + std::string(version) + ".zip");
        auto rejectedRoot = test / ("rejected-" + std::string(version));
        Write(rejectedPath, EncodeReplayZip(rejected));
        ReplayArchive archive;
        archive.SetDirectory(rejectedRoot);
        archive.ImportPackage(rejectedPath);
        Until([&] { return !archive.Get()->error.empty(); });
        assert(archive.Get()->error == "Package manifest identity mismatch" &&
               archive.Get()->savedRevision == 0 &&
               !std::filesystem::exists(rejectedRoot / (prefix + ".isrun")) &&
               !std::filesystem::exists(rejectedRoot / (prefix + ".isinput")));
        archive.Shutdown();
    }
    // Valid CRC and updated SHA list, but a conflicting manifest identity.
    auto fake = package;
    fake["manifest.json"] = ReplayText("{\"runId\":\"wrong\"}");
    Checksums(fake);
    auto fakePath = test / "wrong-identity.zip";
    Write(fakePath, EncodeReplayZip(fake));
    {
        ReplayArchive archive;
        archive.SetDirectory(imported);
        archive.ImportPackage(fakePath);
        Until([&] { return !archive.Get()->error.empty(); });
        assert(Read(imported / (prefix + ".isrun")) == map->bytes &&
               Read(imported / (prefix + ".isinput")) == bytes);
        archive.Shutdown();
    }
    auto missing = test / "missing";
    {
        ReplayArchive archive;
        archive.SetDirectory(missing);
        archive.LoadMotion(map);
        Until([&] { return !archive.Get()->motionError.empty(); });
        assert(!archive.Motion());
        archive.Save(map, "Cannot lose input", "", "");
        Until([&] { return !archive.Get()->error.empty(); });
        assert(!std::filesystem::exists(missing / (prefix + ".isrun")));
        archive.Shutdown();
    }
    auto oldRoot = test / "legacy";
    {
        ReplayArchive archive;
        archive.SetDirectory(oldRoot);
        archive.Save(legacy, "Legacy", "", "");
        Until([&] { return archive.Get()->savedRevision == 1 && !archive.Get()->busy; });
        archive.Export(legacy);
        Until([&] { return archive.Get()->exportedRun == legacy->review->id; });
        auto old = DecodeReplayZip(Read(archive.Get()->exportPath));
        assert(!old.count(prefix + ".isinput") &&
               old["manifest.json"] == ReplayText(ReplayManifest(*legacy, false, false)));
        archive.Shutdown();
    }
    Write(test / "package-for-python.zip", Read(exported));
    std::cout
        << "Replay packages: full background export/import, named UTF-8 markers and A-B loops, format-1 inputs, 7.2.0/7.2.1 and annotated 7.3.0/7.3.1/7.3.2/7.3.3/7.3.4 packages, original validation/notes, immutable companions, independent game inbox, manifest conflicts, missing input rejection and legacy export: PASS\n";
}
