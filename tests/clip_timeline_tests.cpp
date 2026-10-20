#include "ClipModel.h"
#include <cassert>
#include <iostream>
#include <random>
using namespace suite_clip;
static bool Near(float a, float b) {
    return std::abs(a - b) < .0001f;
}
int main() {
    CameraTrack growing;
    for (int i = 0; i < 256; i++) {
        CameraKey key;
        key.seconds = 255 - i;
        assert(growing.Put(key) == 0);
        for (size_t n = 0; n < growing.keys.size(); n++)
            assert(growing.keys[n].seconds == 255 - i + n);
    }
    growing.Validate(256);
    auto source = [](double t) -> std::optional<CameraPose> {
        return CameraPose{{float(t * 10), 2, 0}, Angles(0, 0), 65};
    };
    CameraTrack track;
    CameraKey a, b, c, d;
    a.seconds = 1;
    a.mode = 0;
    a.next = Transition::Cut;
    b.seconds = 2;
    b.mode = 2;
    b.pose.position = {100, 5, 6};
    b.next = Transition::Cut;
    c.seconds = 4;
    c.mode = 1;
    c.follow.yaw = 90;
    c.follow.pitch = 0;
    c.follow.radius = 11;
    c.next = Transition::Cut;
    d.seconds = 6;
    d.mode = 0;
    d.next = Transition::Cut;
    b.layerMask = Colliders;
    b.layers = Colliders;
    c.layerMask = Route;
    c.layers = 0;
    d.layerMask = Colliders;
    d.layers = 0;
    track.keys = {a, b, c, d};
    track.Validate(10);
    auto at = [&](double t) { return EvaluateCamera(track, t, source); };
    assert(at(.5).mode == 0 && at(.5).active == NoKey && Near(at(.5).pose->position.x, 5));
    assert(at(1).mode == 0 && at(1).active == 0 && Near(at(1.5).pose->position.x, 15));
    assert(at(std::nextafter(2., 1.)).mode == 0);
    assert(at(2).mode == 2 && Near(at(3).pose->position.x, 100));
    assert(at(4).mode == 1 && Near(at(4).pose->position.x, 29) && Near(at(5).pose->position.x, 39));
    assert(at(6).mode == 0 && Near(at(7).pose->position.x, 70));
    assert(track.LayersAt(std::nextafter(2., 1.), Route) == Route);
    assert(track.LayersAt(2, Route) == (Colliders | Route));
    assert(track.LayersAt(4, Route) == Colliders && track.LayersAt(6, Route) == 0);
    for (double t : {7., 0., 5., 2., 1., 4., 3., 6.}) {
        auto expected = at(t);
        assert(expected.pose);
        expected.pose->Validate();
    }
    // Dynamic endpoints both resolve at the requested source time.
    track.keys[0].next = Transition::Linear;
    auto blended = at(1.5);
    assert(blended.blending && blended.mode == 2 && Near(blended.pose->position.x, 57.5));
    assert(at(1).mode == 0 && at(2).mode == 2);
    track.keys[1].next = Transition::Smooth;
    assert(Near(at(3).pose->position.x, 59.5)); // Free 100 -> Follow 19 at t=3.
    auto absent = [](double) -> std::optional<CameraPose> { return {}; };
    assert(!EvaluateCamera(track, 1.5, absent).pose && EvaluateCamera(track, 2, absent).pose);
    // Independent scalar reference for inherited flags, including reverse seeks.
    std::mt19937 rng(752);
    for (int iteration = 0; iteration < 10000; iteration++) {
        uint32_t defaults = rng() & AllLayers;
        for (auto &key : track.keys) {
            key.layerMask = rng() & AllLayers;
            key.layers = rng() & key.layerMask;
        }
        double t = (rng() % 1000) / 100.;
        uint32_t expected = 0;
        for (uint32_t bit = 1; bit <= Fullbright; bit <<= 1) {
            bool set = defaults & bit;
            for (const auto &key : track.keys)
                if (key.seconds <= t && (key.layerMask & bit))
                    set = key.layers & bit;
            if (set)
                expected |= bit;
        }
        assert(track.LayersAt(t, defaults) == expected);
    }
    for (int id = 0; id < 4; id++) {
        auto bad = track;
        if (id == 0)
            bad.keys[0].mode = 3;
        if (id == 1)
            bad.keys[0].layerMask = 64;
        if (id == 2) {
            bad.keys[0].layerMask = 0;
            bad.keys[0].layers = 1;
        }
        if (id == 3)
            bad.keys[0].follow.radius = 0;
        bool rejected = false;
        try {
            bad.Validate(10);
        } catch (...) {
            rejected = true;
        }
        assert(rejected);
    }
    CameraPose pose{{1, 2, 3}, Angles(0, 0), 65};
    FlyCamera fly;
    fly.Align(pose);
    fly.Look(pose, 100, 50);
    assert(pose.position.x == 1 && pose.position.y == 2 && pose.position.z == 3);
    auto slow = pose, fast = pose, diagonal = pose;
    for (int i = 0; i < 60; i++)
        fly.Move(slow, 0, 0, 1, 1.f / 60, false);
    for (int i = 0; i < 120; i++)
        fly.Move(fast, 0, 0, 1, 1.f / 120, false);
    auto length = [](Point a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); };
    assert(length(slow.position - fast.position) < .0001 &&
           std::abs(length(slow.position - pose.position) - 10) < .0001);
    fly.Move(diagonal, 1, 0, 1, .1f, false);
    assert(std::abs(length(diagonal.position - pose.position) - 1) < .0001);
    auto vertical = pose;
    fly.Move(vertical, 0, 1, 0, .1f, true);
    assert(Near(vertical.position.y, 5));
    fly.Move(vertical, 0, -1, 0, .1f, false);
    assert(Near(vertical.position.y, 4));
    fly.Look(vertical, 0, 10000);
    assert(fly.pitch == 89);
    fly.Travel(vertical, 1);
    fly.Pan(vertical, 3, 4);
    vertical.Validate();
    Options output;
    output.a = 0;
    output.b = 10;
    output.fps = 60;
    output.rate = .5;
    double keyTime = 2.123456;
    uint64_t frame = uint64_t(std::ceil((keyTime - output.a) * output.fps / output.rate));
    assert(output.At(frame - 1) < keyTime && output.At(frame) >= keyTime &&
           output.At(frame) - keyTime < output.rate / output.fps);
    std::cout << "Timeline: exact mode/cut/layer boundaries, dynamic mixed-mode easing, reverse seek, 10000 "
                 "inherited-layer cases, invalid data, true fly controls and frame quantization: PASS\n";
}
