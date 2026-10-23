#include "ClipEncoding.h"
#include "ExportProfiles.h"
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace suite_clip;

static void AviClockCases(const std::filesystem::path &folder)
{
    std::vector<char> bytes(12);
    std::memcpy(bytes.data(), "RIFF", 4);
    std::memcpy(bytes.data() + 8, "AVI ", 4);
    auto word = [&](size_t at, uint32_t value) { std::memcpy(bytes.data() + at, &value, 4); };
    auto stream = [&](const char *type, uint32_t scale, uint32_t rate, uint32_t count) {
        size_t at = bytes.size();
        bytes.resize(at + 64);
        std::memcpy(bytes.data() + at, "strh", 4);
        word(at + 4, 56);
        std::memcpy(bytes.data() + at + 8, type, 4);
        word(at + 28, scale);
        word(at + 32, rate);
        word(at + 40, count);
    };
    stream("vids", 1, 60, 60);
    stream("auds", 1, 48000, 48000);
    auto path = folder / "clock-header.avi";
    auto save = [&]() {
        word(4, uint32_t(bytes.size() - 8));
        std::ofstream file(path, std::ios::binary);
        file.write(bytes.data(), bytes.size());
    };
    Options clock;
    clock.a = 0;
    clock.b = 1;
    clock.fps = 60;
    clock.format = 4;
    clock.audio = true;
    save();
    VerifyAviTiming(path, clock);
    for (int fault = 0; fault < 4; fault++)
    {
        word(44, fault == 0 ? 30 : 60);        // Video rate.
        word(52, fault == 1 ? 59 : 60);        // Video frames.
        word(116, fault == 2 ? 47000 : 48000); // Audio frames.
        word(112, fault == 3 ? 240 : 0);       // Audio start.
        save();
        bool rejected = false;
        try
        {
            VerifyAviTiming(path, clock);
        }
        catch (const std::runtime_error &)
        {
            rejected = true;
        }
        assert(rejected);
    }
    word(112, 0);
    word(52, 30); // An OpenDML file may carry the full count in dmlh.
    auto at = bytes.size();
    bytes.resize(at + 12);
    std::memcpy(bytes.data() + at, "dmlh", 4);
    word(at + 4, 4);
    word(at + 8, 60);
    save();
    VerifyAviTiming(path, clock);
    bytes.resize(10);
    save();
    bool rejected = false;
    try
    {
        ReadAviTiming(path);
    }
    catch (const std::runtime_error &)
    {
        rejected = true;
    }
    assert(rejected);
}

int main(int argc, char **argv)
{
    Options source;
    source.a = .125;
    source.b = 2.125;
    source.rate = .5;
    source.camera = 3;
    source.fullbright = true;
    source.colliders = true;
    for (int preset = 0; preset < 9; preset++)
    {
        auto o = source;
        ApplyExportPreset(o, preset);
        o.Validate(3);
        assert(o.camera == 3 && o.fullbright && o.colliders && o.a == source.a && o.b == source.b &&
               o.rate == .5);
        if (o.format != 1)
            for (int fps : {30, 60, 120})
            {
                o.fps = fps;
                o.Validate(3);
                assert(o.Frames() == uint64_t(4 * fps));
            }
        if (preset == 1)
            assert(o.format == 4);
        if (preset == 3)
            assert(o.format == 5);
        if (preset == 4)
            assert(o.format == 6 && o.maxFileBytes == 25000000);
    }
    Options normal;
    normal.a = 0;
    normal.b = 10;
    normal.fps = 120;
    normal.camera = 3;
    normal.timing = std::make_shared<SpeedMap>(10, 1, std::vector<SpeedPoint>{{0, .1, Transition::Cut}});
    assert(normal.Frames() == 12000);
    normal.speedKeys = false;
    assert(normal.Frames() == 1200 && normal.At(600) == 5 && normal.OutputDuration() == 10);
    auto args = LosslessArguments(normal);
    assert(args.find("setpts=N") != std::string::npos && args.find("-r:v 120") != std::string::npos &&
           args.find("-enc_time_base:v 1:120") != std::string::npos);
    Options capped;
    capped.a = 0;
    capped.b = 4;
    capped.fps = 60;
    capped.format = 6;
    capped.maxFileBytes = 1000000;
    assert(SizeBitrate(capped) == uint64_t((1000000 - 65536) * 8 / 4));
    assert(SmallerBitrate(1000000, 1100000, 1000000) < 1000000 &&
           SmallerBitrate(1000000, 999999, 1000000) == 1000000);
    for (int pass : {1, 2})
    {
        auto a = BudgetCodecArguments(capped, SizeBitrate(capped), pass);
        assert(a.find("-fs ") == std::string::npos && a.find("-frames:v 240") != std::string::npos);
    }
    for (int fault = 0; fault < 4; fault++)
    {
        auto bad = capped;
        if (fault == 0)
            bad.format = 2;
        if (fault == 1)
            bad.maxFileBytes = 1;
        if (fault == 2)
            bad.fps = 121;
        if (fault == 3)
            bad.quality = 3;
        bool rejected = false;
        try
        {
            bad.Validate(4);
        }
        catch (...)
        {
            rejected = true;
        }
        assert(rejected);
    }
    if (argc > 1)
    {
        std::filesystem::path dir = argv[1];
        std::filesystem::create_directories(dir);
        AviClockCases(dir);
        std::ofstream json(dir / "codec-cases.json");
        json << "[\n";
        bool first = true;
        for (int format : {2, 4, 5, 6})
            for (int fps : {30, 60, 120})
            {
                Options o;
                o.width = 320;
                o.height = 180;
                o.fps = fps;
                o.format = format;
                o.a = 2.125;
                o.b = o.a + double(fps + 7) / fps;
                o.rate = 1;
                o.quality = 0;
                o.Validate(o.b);
                if (!first)
                    json << ",\n";
                first = false;
                json << "{\"format\":" << format << ",\"fps\":" << fps << ",\"frames\":" << o.Frames()
                     << ",\"arguments\":" << std::quoted(LosslessArguments(o)) << "}";
            }
        json << "\n]\n";
        std::ofstream plan(dir / "budget-cases.json");
        plan << "[\n";
        first = true;
        for (uint64_t cap : {256000ull, 1000000ull, 5000000ull})
        {
            Options o;
            o.width = 320;
            o.height = 180;
            o.fps = 60;
            o.a = 0;
            o.b = 4;
            o.format = 6;
            o.maxFileBytes = cap;
            o.quality = 0;
            auto bitrate = SizeBitrate(o);
            if (!first)
                plan << ",\n";
            first = false;
            plan << "{\"cap\":" << cap << ",\"fps\":60,\"frames\":240,\"bitrate\":" << bitrate
                 << ",\"first\":" << std::quoted(BudgetCodecArguments(o, bitrate, 1))
                 << ",\"second\":" << std::quoted(BudgetCodecArguments(o, bitrate, 2)) << "}";
        }
        plan << "\n]\n";
    }
    std::cout << "Export presets: purpose-specific codecs, 30/60/120 FPS, 1x ignores "
                 "stale speed map while retaining camera, fixed frame stamps, "
                 "hard-size budget/retry bounds and unsupported combinations: PASS\n";
}
