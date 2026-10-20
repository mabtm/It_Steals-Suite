#include "ClipModel.h"
#include "PngSequence.h"
#include <cassert>
#include <iostream>
#include <random>
using namespace suite_clip;
static double Reference(Transition curve, double x, double a, double b) {
    double u = x;
    switch (curve) {
    case Transition::Cut:
        u = 0;
        break;
    case Transition::Linear:
        break;
    case Transition::EaseIn:
        u = x * x;
        break;
    case Transition::EaseOut:
        u = 2 * x - x * x;
        break;
    case Transition::EaseInOut:
        u = 3 * x * x - 2 * x * x * x;
        break;
    case Transition::Smooth:
        u = 6 * std::pow(x, 5) - 15 * std::pow(x, 4) + 10 * std::pow(x, 3);
        break;
    }
    return 1 / (a + (b - a) * u);
}
static double Simpson(Transition c, double a, double b) {
    constexpr int n = 131072;
    double sum = Reference(c, 0, a, b) + Reference(c, 1, a, b);
    for (int i = 1; i < n; i++)
        sum += (i % 2 ? 4 : 2) * Reference(c, double(i) / n, a, b);
    return sum / (3 * n);
}
int main(int argc, char **argv) {
    SpeedMap cut(10, 1, {{2, .5, Transition::Cut}, {6, 1, Transition::Cut}});
    assert(cut.Output(2) == 2 && cut.Output(6) == 10 && cut.Output(10) == 14);
    assert(cut.Rate(std::nextafter(2., 0.)) == 1 && cut.Rate(2) == .5 && cut.Rate(6) == 1);
    Options o;
    o.a = 0;
    o.b = 10;
    o.fps = 60;
    o.camera = 3;
    o.timing = std::make_shared<SpeedMap>(cut);
    o.Validate(10);
    assert(o.Frames() == 840 && o.At(120) == 2 && o.At(600) == 6);
    assert(std::abs(o.Advance(5, 1. / 60) - 5 - 1. / 120) < 1e-12);
    auto sub = o;
    sub.a = 3;
    sub.b = 7;
    assert(std::abs(sub.OutputDuration() - 7) < 1e-12 && sub.Frames() == 420);
    std::mt19937 rng(760);
    for (auto curve : {Transition::Cut, Transition::Linear, Transition::Smooth, Transition::EaseIn,
                       Transition::EaseOut, Transition::EaseInOut})
        for (auto range :
             {std::pair{1., .1}, std::pair{.1, 8.}, std::pair{8., .1}, std::pair{.5, 1.}, std::pair{1., 1.}}) {
            SpeedMap ramp(4, 1, {{0, range.first, curve}, {4, range.second, Transition::Cut}});
            double exact = 4 * Simpson(curve, range.first, range.second);
            assert(std::abs(ramp.Output(4) - exact) < 2e-8);
            for (int i = 0; i < 300; i++) {
                double t = double(rng() % 1000000) / 250000;
                double output = ramp.Output(t);
                assert(std::abs(ramp.Source(output) - t) < 1e-8);
            }
            assert(ramp.Rate(4) == range.second && ramp.Source(0) == 0 && ramp.Source(ramp.Output(4)) == 4);
        }
    CameraTrack track;
    CameraKey a, b, c;
    a.seconds = 1;
    a.speedOverride = true;
    a.playbackSpeed = .5;
    b.seconds = 3;
    b.speedOverride = false;
    c.seconds = 4;
    c.speedOverride = true;
    c.playbackSpeed = 1;
    track.keys = {a, b, c};
    track.Validate(10);
    std::shared_ptr<const SpeedMap> cache;
    ConfigureSpeed(cache, track, 10, 1, true);
    auto first = cache;
    track.keys[0].pose.position.x = 10;
    ConfigureSpeed(cache, track, 10, 1, true);
    assert(cache == first);
    track.keys[1].name = "No speed change";
    ConfigureSpeed(cache, track, 10, 1, true);
    assert(cache == first);
    track.keys[0].playbackSpeed = .25;
    ConfigureSpeed(cache, track, 10, 1, true);
    assert(cache != first);
    ConfigureSpeed(cache, track, 10, 1, false);
    assert(!cache);
    for (double invalid :
         {0., -.1, 8.1, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        bool rejected = false;
        try {
            SpeedMap bad(10, 1, {{2, invalid, Transition::Cut}});
        } catch (...) {
            rejected = true;
        }
        assert(rejected);
    }
    if (argc > 1) {
        std::filesystem::path dir = argv[1];
        std::filesystem::create_directories(dir);
        Options png;
        png.width = 320;
        png.height = 180;
        png.fps = 60;
        png.a = 0;
        png.b = .05;
        png.format = 3;
        png.timing = std::make_shared<SpeedMap>(1, 1, std::vector<SpeedPoint>{{0, .5, Transition::Cut}});
        png.Validate(1);
        assert(png.Frames() == 6);
        PngSequence sequence(dir, png);
        std::vector<uint8_t> pixels(320 * 180 * 4, 255);
        for (uint64_t frame = 0; frame < png.Frames(); frame++) {
            assert(std::abs(png.At(frame) - frame / 120.) < 1e-9);
            sequence.Write(pixels);
        }
        sequence.Finish();
    }
    std::cout << "Speed timeline: exact cuts/inheritance/range duration, 30 curve/extreme-rate cases vs "
                 "independent Simpson integration, 9000 inverse seeks, 840 fixed-FPS frames, cache stability "
                 "and invalid values: PASS\n";
}
