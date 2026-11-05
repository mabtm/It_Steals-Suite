#include "ReplayArchive.h"
#include "ClipAudio.h"
#include <cassert>
#include <iostream>
using namespace practice;

static std::vector<uint8_t> Read(const std::filesystem::path &p)
{
    std::ifstream f(p, std::ios::binary);
    assert(f);
    return {std::istreambuf_iterator<char>(f), {}};
}

static void Write(const std::filesystem::path &p, const std::vector<uint8_t> &b)
{
    std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char *>(b.data()), std::streamsize(b.size()));
    assert(f);
}

template <class F> static void Reject(F action)
{
    bool bad = false;
    try
    {
        action();
    }
    catch (const std::exception &)
    {
        bad = true;
    }
    assert(bad);
}

template <class F> static void Until(F action)
{
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(15);
    while (!action())
    {
        assert(std::chrono::steady_clock::now() < end);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

int main(int argc, char **argv)
{
    assert(argc == 3 || argc == 4);
    std::filesystem::path folder = argv[1], out = argv[2];
    std::filesystem::create_directories(out);
    std::string error;
    auto bytes = Read(folder / "input-v6.isinput"), mapBytes = Read(folder / "input-map.isrun");
    auto motion = DecodeMotion(bytes, error);
    assert(motion && motion->inputFormat == 6 && motion->experience.recorded &&
           motion->experience.events.size() == 3 && motion->experience.events[0].kind == 4 &&
           motion->experience.animations[0].frames[1].state == 2);
    auto map = DecodeMap(mapBytes, error);
    assert(map && motion->Matches(*map));
    auto prefix = "run-" + std::to_string(motion->id);
    auto audioPath = folder / (prefix + ".isaudio");
    auto info = VerifyRecordedAudio(audioPath, motion->id, motion->scene, motion->seed, motion->hash,
                                    &motion->experience.audio);
    assert(info.recorded && info.flags == 0);
    auto corrupt = Read(audioPath);
    corrupt[300] ^= 1;
    Reject(
        [&] { VerifyRecordedAudio(corrupt, motion->id, motion->scene, motion->seed, motion->hash, &info); });
    Reject(
        [&] { VerifyRecordedAudio(audioPath, motion->id + 1, motion->scene, motion->seed, motion->hash); });
    {
        RecordedAudioReader reader(audioPath, info, motion->id, motion->scene, motion->seed, motion->hash);
        assert(std::abs(reader.Stereo(1234.5)[0] - float(1234.5 / info.rate)) < 1e-6);
        assert(reader.Stereo(-10)[0] == 0);
    }
    for (size_t n = bytes.size() - 160; n < bytes.size(); n++)
    {
        auto shortFile = bytes;
        shortFile.resize(n);
        assert(!DecodeMotion(shortFile, error));
    }
    using namespace suite_clip;
    Options options;
    options.format = 2;
    options.audio = true;
    options.a = .05;
    options.b = motion->seconds;
    options.fps = 60;
    options.Validate(motion->seconds);
    ExportSource source;
    source.run = motion->id;
    source.scene = motion->scene;
    source.seed = motion->seed;
    source.hash = motion->hash;
    source.audioFile = audioPath;
    source.audio = info;
    std::atomic<bool> cancel = false;
    auto wave = [&](const char *name) {
        auto path = out / name;
        RenderRecordedAudio(path, options, source, cancel);
        auto wav = Read(path);
        assert(wav.size() == 44 + AudioFrames(options) * 8);
        for (size_t n = 0; n < AudioFrames(options); n += 31)
        {
            float l = 0, r = 0;
            std::memcpy(&l, wav.data() + 44 + n * 8, 4);
            std::memcpy(&r, wav.data() + 48 + n * 8, 4);
            double time = options.Advance(options.a, double(n) / 48000);
            float expected = time < options.b ? float(time) : 0;
            assert(std::abs(l - expected) < 2e-6 && r == -l);
        }
        return path;
    };
    wave("audio-1x.wav");
    options.rate = .5;
    wave("audio-slow.wav");
    options.rate = 2;
    wave("audio-fast.wav");
    CameraTrack track;
    CameraKey first, last;
    first.seconds = 0;
    first.speedOverride = true;
    first.playbackSpeed = 1;
    first.speedCurve = Transition::Smooth;
    last.seconds = motion->seconds;
    last.speedOverride = true;
    last.playbackSpeed = .25;
    track.keys = {first, last};
    options.rate = 1;
    ConfigureSpeed(options.timing, track, motion->seconds, 1, true);
    wave("audio-speedkeys.wav");
    auto withoutAudio = options;
    withoutAudio.timing.reset();
    withoutAudio.maxFileBytes = 1000000;
    withoutAudio.format = 6;
    auto withAudio = withoutAudio;
    withoutAudio.audio = false;
    assert(SizeBitrate(withAudio) + 128000 <= SizeBitrate(withoutAudio) + 1);
    assert(BudgetCodecArguments(withAudio, SizeBitrate(withAudio), 2).find(" -t ") != std::string::npos);
    cancel = true;
    Reject([&] { RenderRecordedAudio(out / "cancelled.wav", options, source, cancel); });
    cancel = false;
    for (int format : {0, 2, 4, 5, 6})
    {
        auto o = options;
        o.format = format;
        o.timing.reset();
        o.a = .05;
        o.b = .5;
        o.width = 320;
        o.height = 180;
        std::ofstream args(out / ("format-" + std::to_string(format) + ".args"));
        if (format != 0)
            args << LosslessArguments(o);
    }
    {
        std::ofstream plan(out / "clock-arguments.json");
        plan << '[';
        for (int sample : {0, 1, 2})
        {
            int fps = sample == 0 ? 60 : 120;
            Options clock;
            clock.a = fps == 60 ? 16.039872630928851 : 45.712109447483272;
            clock.b = fps == 60 ? 26.909847730928828 : 50.602426142458171;
            if (sample == 2)
            {
                clock.a = 0;
                clock.b = 3601. / 120;
            }
            clock.width = 1920;
            clock.height = 1080;
            clock.fps = fps;
            clock.format = 4;
            clock.audio = true;
            assert(clock.Frames() == (sample == 2 ? 3601 : fps == 60 ? 653 : 587));
            auto codec = AudioCodecArguments(clock.format);
            if (sample)
                plan << ',';
            plan << "{\"width\":" << clock.width << ",\"height\":" << clock.height << ",\"fps\":" << clock.fps
                 << ",\"frames\":" << clock.Frames()
                 << ",\"input\":" << std::quoted(StreamInputArguments(clock, true)) << ",\"output\":"
                 << std::quoted(VideoCodecArguments(clock) + std::string(codec.begin(), codec.end()) +
                                AudioDurationArguments(clock))
                 << '}';
        }
        plan << ']';
    }
    // Full worker path: preserve the companion through Save, ZIP import and staging.
    auto original = out / "original";
    Write(original / (prefix + ".isinput"), bytes);
    Write(original / (prefix + ".isrun"), mapBytes);
    Write(original / (prefix + ".isaudio"), Read(audioPath));
    auto saved = out / "saved", imported = out / "imported", game = out / "game";
    std::filesystem::path package;
    {
        ReplayArchive archive;
        archive.SetDirectory(saved);
        archive.SetSourceDirectory(original);
        archive.LoadMotion(map);
        Until([&] { return archive.Motion() != nullptr || !archive.Get()->motionError.empty(); });
        assert(archive.Motion() && !archive.Motion()->audioFile.empty());
        archive.Save(map, "Audio replay", "QA", "");
        Until([&] { return archive.Get()->savedRevision > 0 || !archive.Get()->error.empty(); });
        if (!archive.Get()->error.empty())
            throw std::runtime_error(archive.Get()->error);
        auto finish = suite_run::HistoricalVerification(*map);
        finish.full = true;
        finish.session = 713;
        std::strcpy(finish.data.externalTool, "captured \"tool\"\nvalue");
        auto encoded = suite_run::EncodeVerification(finish);
        auto restored = suite_run::DecodeVerification(encoded);
        assert(restored.Matches(*map) && suite_run::EncodeVerification(restored) == encoded);
        auto legacySidecar = encoded;
        for (const std::string field : {"runCategory", "statsCategory", "selectedRunSeed"})
        {
            auto start = legacySidecar.find(",\"" + field + "\":");
            assert(start != std::string::npos);
            auto end = legacySidecar.find(',', start + 1);
            legacySidecar.erase(start, end - start);
        }
        assert(suite_run::DecodeVerification(legacySidecar).Matches(*map));
        auto practiceMap = *map;
        auto practiceReview = std::make_shared<RunReview>(*map->review);
        practiceMap.review = practiceReview;
        practiceReview->category = 2;
        practiceReview->aids = 2;
        practiceReview->seedKind = 2;
        practiceReview->valid = false;
        auto practiceSidecar = suite_run::EncodeVerification(suite_run::HistoricalVerification(practiceMap));
        for (const std::string field : {"runCategory", "statsCategory", "selectedRunSeed"})
        {
            auto start = practiceSidecar.find(",\"" + field + "\":");
            auto end = practiceSidecar.find(',', start + 1);
            practiceSidecar.erase(start, end - start);
        }
        auto preservedPractice = suite_run::DecodeVerification(practiceSidecar);
        assert(preservedPractice.data.runCategory == 2 && preservedPractice.Matches(practiceMap));
        auto mismatch = *map;
        mismatch.seed++;
        assert(!restored.Matches(mismatch));
        auto missing = encoded;
        missing.replace(missing.find("\"runValidity\""), 13, "\"missingValue\"");
        Reject([&] { suite_run::DecodeVerification(missing); });
        auto malformed = encoded;
        auto flag = malformed.find("\"reviewReady\":1");
        assert(flag != std::string::npos);
        malformed.replace(flag, 15, "\"reviewReady\":9");
        Reject([&] { suite_run::DecodeVerification(malformed); });
        suite_run::Presentation shown;
        restored.Present(shown, 10);
        assert(shown.completedSession == 713 && shown.scan.elapsed == 0 && shown.hasResult &&
               shown.Technical()[5][1].find("captured \"tool\"") != std::string::npos);
        archive.RecordVerification(finish.data, finish.session);
        Until([&] { return std::filesystem::exists(saved / (prefix + ".verification.json")); });
        auto request = archive.Revalidate(map);
        Until([&] { return archive.Get()->revalidation && archive.Get()->revalidation->request == request; });
        auto checked = archive.Get()->revalidation;
        assert(checked->filesOk && checked->verification && checked->verification->full &&
               checked->verification->session == 713 && !checked->verificationSha256.empty());
        auto inputFile = saved / (prefix + ".isinput");
        auto inputBytes = Read(inputFile), damaged = inputBytes;
        damaged.back() ^= 1;
        Write(inputFile, damaged);
        auto failedRequest = archive.Revalidate(map);
        Until([&] {
            return archive.Get()->revalidation && archive.Get()->revalidation->request == failedRequest;
        });
        auto failed = archive.Get()->revalidation;
        assert(!failed->filesOk && failed->verification && failed->verification->full &&
               !failed->error.empty());
        Write(inputFile, inputBytes);
        auto bytes = Read(saved / (prefix + ".verification.json"));
        archive.RecordVerification(finish.data, 999);
        archive.Refresh();
        Until([&] { return !archive.Get()->busy; });
        assert(Read(saved / (prefix + ".verification.json")) == bytes);
        archive.Export(map);
        Until([&] { return archive.Get()->exportedRun == motion->id || !archive.Get()->error.empty(); });
        if (!archive.Get()->error.empty())
            throw std::runtime_error(archive.Get()->error);
        package = archive.Get()->exportPath;
        archive.Shutdown();
    }
    auto files = DecodeReplayZip(Read(package));
    assert(files.count(prefix + ".isaudio") && files[prefix + ".isaudio"] == Read(audioPath));
    assert(files.count(prefix + ".verification.json"));
    {
        ReplayArchive archive;
        archive.SetDirectory(imported);
        archive.ImportPackage(package);
        Until([&] { return archive.Get()->savedRevision > 0 || !archive.Get()->error.empty(); });
        assert(archive.Get()->error.empty() && Read(imported / (prefix + ".isaudio")) == Read(audioPath));
        auto verification = suite_run::DecodeVerification(
            std::string_view(reinterpret_cast<const char *>(files.at(prefix + ".verification.json").data()),
                             files.at(prefix + ".verification.json").size()));
        assert(verification.Matches(*map) && verification.session == 713);
        auto recheck = archive.Revalidate(map);
        Until([&] { return archive.Get()->revalidation && archive.Get()->revalidation->request == recheck; });
        assert(archive.Get()->revalidation->filesOk && archive.Get()->revalidation->verification->full);
        archive.SetSourceDirectory(game);
        archive.Stage3D(map);
        Until([&] { return archive.Get()->stagedRun == motion->id || !archive.Get()->error.empty(); });
        assert(archive.Get()->error.empty() &&
               Read(game / "ReplayInbox" / (prefix + ".isaudio")) == Read(audioPath));
        archive.Shutdown();
    }
    if (argc == 4)
    {
        ReplayArchive archive;
        auto older = out / "legacy-781";
        archive.SetDirectory(older);
        archive.ImportPackage(argv[3]);
        Until([&] { return archive.Get()->savedRevision > 0 || !archive.Get()->error.empty(); });
        assert(archive.Get()->error.empty());
        auto previous = DecodeMotion(Read(older / (prefix + ".isinput")), error);
        assert(previous && previous->inputFormat == 5 && !previous->coverage.recorded);
        archive.Shutdown();
    }
    std::cout
        << "Experience native: cross-language v6, strict audio identity/corruption, exact WAV sample timing at 0.5/1/2x and smooth speed keys, size budget, cancellation, saved/imported/staged audio package: PASS\n";
}
