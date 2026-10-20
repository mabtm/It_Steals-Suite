#include "ClipEncoding.h"
#include "InputModel.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace suite_clip;
static bool Near(float a, float b, float eps = .0001f) { return std::abs(a - b) < eps; }
static void Wait(Encoder &e) {
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (e.State() == 1 && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(e.State() != 1);
}
int main(int argc, char **argv) {
    assert(argc == 2);
    std::filesystem::path folder = argv[1];
    std::filesystem::create_directories(folder);
    CameraTrack track;
    CameraKey a, b, c;
    a.seconds = 1;
    a.pose = {{1, 2, 3}, Angles(350, 10), 60};
    a.next = Transition::Linear;
    b.seconds = 3;
    b.pose = {{11, 12, 13}, Angles(10, 20), 80};
    b.next = Transition::Cut;
    c.seconds = 5;
    c.pose = {{20, 0, 0}, Angles(90, 0), 90};
    track.keys = {a, b, c};
    track.Validate(10);
    assert(Near(track.At(0).position.x, 1) && Near(track.At(8).position.x, 20));
    auto half = track.At(2);
    assert(Near(half.position.x, 6) && Near(half.fov, 70));
    auto f = Forward(half.rotation);
    assert(std::abs(f.x) < .03 && f.z > .9);
    assert(Near(track.At(4.999).position.x, 11) && Near(track.At(5).position.x, 20));
    for (auto transition :
         {Transition::Linear, Transition::Smooth, Transition::EaseIn, Transition::EaseOut, Transition::EaseInOut}) {
        track.keys[0].next = transition;
        float previous = -1;
        for (int i = 0; i <= 1000; i++) {
            auto pose = track.At(1 + i * .002);
            assert(pose.position.x >= previous - .00001f);
            previous = pose.position.x;
            pose.Validate();
        }
        assert(Near(track.At(1).position.x, 1) && Near(track.At(3).position.x, 11));
    }
    assert(Curve(Transition::EaseIn, .25) < .25 && Curve(Transition::EaseOut, .25) > .25 &&
           Near(Curve(Transition::EaseInOut, .5), .5));
    track.keys[0].next = Transition::Linear;
    auto expected = track.At(2.234);
    for (double seek : {4., 1., 7., 2.234, 0., 2.234}) {
        auto pose = track.At(seek);
        if (seek == 2.234)
            assert(Near(pose.position.x, expected.position.x) && Near(pose.rotation.w, expected.rotation.w));
    }
    CameraKey replace = a;
    replace.seconds = 1.0005;
    replace.name = "Replacement";
    assert(track.Put(replace) == 0 && track.keys.size() == 3);
    track.Validate(10);
    for (int caseId = 0; caseId < 5; caseId++) {
        auto bad = track;
        if (caseId == 0)
            bad.keys[1].seconds = bad.keys[0].seconds;
        if (caseId == 1)
            bad.keys[0].pose.position.x = std::numeric_limits<float>::infinity();
        if (caseId == 2)
            bad.keys[0].pose.rotation = {0, 0, 0, 0};
        if (caseId == 3)
            bad.keys[1].seconds = 20;
        if (caseId == 4)
            bad.keys[0].next = Transition(6);
        bool rejected = false;
        try {
            bad.Validate(10);
        } catch (...) {
            rejected = true;
        }
        assert(rejected);
    }
    Orbit orbit;
    orbit.yaw = 90;
    orbit.pitch = 0;
    orbit.radius = 8;
    auto pose = orbit.Pose({5, 2, 7});
    assert(Near(pose.position.x, -3) && Near(pose.position.z, 7));
    orbit.Rotate(100, 10000);
    assert(orbit.pitch == 85);
    orbit.Zoom(10000);
    assert(orbit.radius == .5f);
    orbit.Zoom(-10000);
    assert(orbit.radius == 100);
    Options options;
    options.b = .1;
    options.width = 320;
    options.height = 180;
    options.fps = 30;
    options.format = 3;
    options.transparent = true;
    options.Validate(1);
    for (int fps : {30, 60, 120}) {
        auto video = options;
        video.fps = fps;
        video.b = double(fps + 7) / fps;
        video.format = 2;
        video.transparent = false;
        std::ofstream(folder / ("ffv1-args-" + std::to_string(fps) + ".txt")) << LosslessArguments(video);
    }
    auto output = options;
    output.width = 2560;
    output.height = 1440;
    for (float available : {300.f, 800.f, 1200.f, 2600.f}) {
        auto fit = PreviewOptions(output, available, false);
        assert(fit.width <= output.width && fit.width <= 1280 && fit.height % 2 == 0);
        assert(fit.fps == output.fps && fit.rate == output.rate && fit.a == output.a && fit.b == output.b);
        fit.Validate(1);
    }
    assert(PreviewOptions(output, 300, true).width == 2560);
    for (int fps : {30, 60})
        for (double rate : {.5, 1., 2.}) {
            auto clock = output;
            clock.a = 1.125;
            clock.b = 3.125;
            clock.rate = rate;
            clock.fps = fps;
            for (uint64_t i = 0; i < clock.Frames(); i++) {
                assert(std::abs(clock.At(i) - (clock.a + double(i) * rate / fps)) < 1e-12);
                if (i)
                    assert(clock.At(i) > clock.At(i - 1) && clock.Stamp(i) > clock.Stamp(i - 1));
            }
            assert(std::abs(double(clock.Frames()) / fps - (clock.b - clock.a) / rate) < 1e-12);
        }
    Encoder encoder;
    std::vector<uint8_t> raw(size_t(320) * 180 * 4);
    for (size_t i = 0; i < raw.size(); i++)
        raw[i] = uint8_t(i * 29 + i / 7);
    assert(encoder.Start(options, folder / "rgba.frames"));
    for (uint64_t i = 0; i < options.Frames(); i++) {
        while (!encoder.Ready() && encoder.State() == 1)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        assert(encoder.Push(raw));
    }
    encoder.Finish();
    Wait(encoder);
    assert(encoder.State() == 2 && encoder.Written() == 3);
    std::ofstream expectedRaw(folder / "expected.bgra", std::ios::binary);
    expectedRaw.write(reinterpret_cast<char *>(raw.data()), std::streamsize(raw.size()));
    expectedRaw.close();
    assert(encoder.Start(options, folder / "cancel.frames"));
    encoder.Cancel();
    Wait(encoder);
    assert(encoder.State() == 4 && !std::filesystem::exists(folder / "cancel.frames") &&
           !std::filesystem::exists(folder / "cancel.frames.partial.frames"));
    std::filesystem::create_directory(folder / "keep.frames");
    std::ofstream(folder / "keep.frames" / "keep.txt") << "preserve";
    assert(encoder.Start(options, folder / "keep.frames"));
    Wait(encoder);
    assert(encoder.State() == 3 && std::filesystem::exists(folder / "keep.frames" / "keep.txt"));
    std::vector<uint8_t> alpha = {25, 50, 100, 128, 9, 8, 7, 0, 13, 27, 42, 255};
    StraightAlpha(alpha);
    assert(alpha[0] == 50 && alpha[1] == 100 && alpha[2] == 199 && alpha[4] == 0 && alpha[5] == 0 && alpha[6] == 0 &&
           alpha[8] == 13);
    suite_input::Preferences input;
    input.customKeys = true;
    input.ShowKey(255, true);
    input.opacity = 0;
    input.contentOpacity = .3f;
    input.mouseScale = 2;
    input.background = false;
    std::ostringstream encoded;
    input.Encode(encoded);
    suite_input::Preferences decoded;
    std::istringstream lines(encoded.str());
    std::string line;
    while (std::getline(lines, line)) {
        auto p = line.find('=');
        decoded.Decode(line.substr(6, p - 6), std::stod(line.substr(p + 1)));
    }
    decoded.Validate();
    assert(decoded.ShowKey(255) && decoded.ShowKey('W') && decoded.opacity == 0 && decoded.mouseScale == 2 &&
           !decoded.background && decoded.customKeys);
    std::cout << "Camera endpoints/cuts/shortest quaternion path, 5 monotonic curves, frame-independent seek, key "
                 "replacement and bounds; lossless RGBA sequence, cancel/preserve, alpha and input preferences: PASS\n";
}
