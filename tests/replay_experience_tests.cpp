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
        Options clock;
        clock.a = 16.039872630928851;
        clock.b = 26.909847730928828;
        clock.width = 1920;
        clock.height = 1080;
        clock.fps = 60;
        clock.format = 4;
        clock.audio = true;
        assert(clock.Frames() == 653);
        auto codec = AudioCodecArguments(clock.format);
        std::ofstream plan(out / "clock-arguments.json");
        plan << "{\"width\":" << clock.width << ",\"height\":" << clock.height << ",\"fps\":" << clock.fps
             << ",\"frames\":" << clock.Frames()
             << ",\"input\":" << std::quoted(StreamInputArguments(clock, true)) << ",\"output\":"
             << std::quoted(VideoCodecArguments(clock) + std::string(codec.begin(), codec.end()) +
                            AudioDurationArguments(clock))
             << "}";
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
        archive.Export(map);
        Until([&] { return archive.Get()->exportedRun == motion->id || !archive.Get()->error.empty(); });
        if (!archive.Get()->error.empty())
            throw std::runtime_error(archive.Get()->error);
        package = archive.Get()->exportPath;
        archive.Shutdown();
    }
    auto files = DecodeReplayZip(Read(package));
    assert(files.count(prefix + ".isaudio") && files[prefix + ".isaudio"] == Read(audioPath));
    {
        ReplayArchive archive;
        archive.SetDirectory(imported);
        archive.ImportPackage(package);
        Until([&] { return archive.Get()->savedRevision > 0 || !archive.Get()->error.empty(); });
        assert(archive.Get()->error.empty() && Read(imported / (prefix + ".isaudio")) == Read(audioPath));
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
