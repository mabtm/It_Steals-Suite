#include "ClipEncoding.h"
#include "ExportProfiles.h"
#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>
using namespace suite_clip;

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
    assert(args.find("setpts=N") != std::string::npos && args.find("-r 120") != std::string::npos);
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
