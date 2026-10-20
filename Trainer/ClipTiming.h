#pragma once
#include "ClipCamera.h"
#include <array>
#include <memory>
namespace suite_clip {
struct SpeedPoint {
    double seconds = 0, rate = 1;
    Transition curve = Transition::Cut;
    bool operator==(const SpeedPoint &) const = default;
};
// Speeds are defined in recorded/source time. Integrate dt/speed once, then
// invert the monotone table for each output frame. No wall-clock frame drops.
class SpeedMap {
    struct Span {
        double begin = 0, end = 0, first = 1, last = 1, output = 0;
        Transition curve = Transition::Cut;
        std::array<double, 129> integral{};
    };
    std::vector<Span> spans;
    double duration = 0, base = 1;
    std::vector<SpeedPoint> points;
    static double Speed(const Span &s, double t) {
        double u = s.end > s.begin ? std::clamp((t - s.begin) / (s.end - s.begin), 0., 1.) : 0;
        // Double precision curves keep inversion accurate near slow endpoints.
        if (s.curve == Transition::Cut)
            u = 0;
        else if (s.curve == Transition::EaseIn)
            u = u * u;
        else if (s.curve == Transition::EaseOut)
            u = 1 - (1 - u) * (1 - u);
        else if (s.curve == Transition::EaseInOut)
            u = u * u * (3 - 2 * u);
        else if (s.curve == Transition::Smooth)
            u = u * u * u * (u * (u * 6 - 15) + 10);
        return s.first + (s.last - s.first) * u;
    }
    static double Integrate(const Span &s, double a, double b) {
        if (b <= a)
            return 0;
        if (s.curve == Transition::Cut || std::abs(s.first - s.last) < 1e-14)
            return (b - a) / s.first;
        static constexpr double x[]{.1834346424956498, .5255324099163290, .7966664774136267, .9602898564975363};
        static constexpr double w[]{.3626837833783620, .3137066458778873, .2223810344533745, .1012285362903763};
        double middle = (a + b) * .5, half = (b - a) * .5, total = 0;
        for (int i = 0; i < 4; i++)
            total += w[i] * (1 / Speed(s, middle - half * x[i]) + 1 / Speed(s, middle + half * x[i]));
        return total * half;
    }
    const Span &At(double t) const {
        auto p = std::upper_bound(spans.begin(), spans.end(), t,
                                  [](double time, const Span &s) { return time < s.begin; });
        return p == spans.begin() ? spans.front() : *std::prev(p);
    }

  public:
    SpeedMap(double length, double rate, std::vector<SpeedPoint> keys)
        : duration(length), base(rate), points(std::move(keys)) {
        if (!std::isfinite(length) || length <= 0 || !std::isfinite(rate) || rate < .1 || rate > 8)
            throw std::runtime_error("Invalid speed timeline");
        double previous = -1;
        for (const auto &p : points) {
            if (!std::isfinite(p.seconds) || p.seconds < 0 || p.seconds > length || p.seconds <= previous ||
                !std::isfinite(p.rate) || p.rate < .1 || p.rate > 8 || int(p.curve) < 0 || int(p.curve) > 5)
                throw std::runtime_error("Invalid speed keypoint");
            previous = p.seconds;
        }
        double begin = 0, first = rate, output = 0;
        Transition curve = Transition::Cut;
        auto add = [&](double end, double next) {
            if (end <= begin)
                return;
            Span s;
            s.begin = begin;
            s.end = end;
            s.first = first;
            s.last = next;
            s.curve = curve;
            s.output = output;
            for (int i = 1; i <= 128; i++)
                s.integral[i] = s.integral[i - 1] + Integrate(s, begin + (end - begin) * (i - 1) / 128,
                                                              begin + (end - begin) * i / 128);
            output += s.integral.back();
            spans.push_back(s);
        };
        for (const auto &p : points) {
            add(p.seconds, p.rate);
            begin = p.seconds;
            first = p.rate;
            curve = p.curve;
        }
        add(length, first);
        if (spans.empty()) {
            Span s;
            s.begin = 0;
            s.end = length;
            s.first = s.last = first;
            s.curve = Transition::Cut;
            for (int i = 1; i <= 128; i++)
                s.integral[i] = length * i / (128 * first);
            spans.push_back(s);
        }
    }
    bool Matches(double length, double rate, const std::vector<SpeedPoint> &keys) const {
        return duration == length && base == rate && points == keys;
    }
    const std::vector<SpeedPoint> &Points() const { return points; }
    double Rate(double t) const {
        if (!std::isfinite(t))
            return base;
        if (t >= duration && !points.empty() && points.back().seconds == duration)
            return points.back().rate;
        return Speed(At(std::clamp(t, 0., duration)), std::clamp(t, 0., duration));
    }
    double Output(double source) const {
        if (!std::isfinite(source))
            return 0;
        source = std::clamp(source, 0., duration);
        const auto &s = At(source);
        int cell = std::clamp(int((source - s.begin) / (s.end - s.begin) * 128), 0, 127);
        double left = s.begin + (s.end - s.begin) * cell / 128;
        return s.output + s.integral[cell] + Integrate(s, left, source);
    }
    double Source(double output) const {
        if (!std::isfinite(output) || output <= 0)
            return 0;
        if (output >= Output(duration))
            return duration;
        auto p = std::upper_bound(spans.begin(), spans.end(), output,
                                  [](double time, const Span &s) { return time < s.output; });
        const auto &s = p == spans.begin() ? spans.front() : *std::prev(p);
        double local = output - s.output;
        if (local <= 0)
            return s.begin;
        auto c = std::upper_bound(s.integral.begin(), s.integral.end(), local);
        int cell = std::clamp(int(c - s.integral.begin()) - 1, 0, 127);
        double a = s.begin + (s.end - s.begin) * cell / 128, b = s.begin + (s.end - s.begin) * (cell + 1) / 128;
        // Fixed bisection is deterministic even at cuts or extreme speed ratios.
        double left = a;
        for (int i = 0; i < 34; i++) {
            double mid = (a + b) * .5;
            double value = s.integral[cell] + Integrate(s, left, mid);
            if (value < local)
                a = mid;
            else
                b = mid;
        }
        return (a + b) * .5;
    }
};
inline std::vector<SpeedPoint> SpeedPoints(const CameraTrack &track) {
    std::vector<SpeedPoint> points;
    for (const auto &key : track.keys)
        if (key.speedOverride)
            points.push_back({key.seconds, key.playbackSpeed, key.speedCurve});
    return points;
}
inline void ConfigureSpeed(std::shared_ptr<const SpeedMap> &map, const CameraTrack &track, double duration,
                           double base, bool enabled) {
    auto points = enabled ? SpeedPoints(track) : std::vector<SpeedPoint>{};
    if (points.empty()) {
        map.reset();
        return;
    }
    if (!map || !map->Matches(duration, base, points))
        map = std::make_shared<SpeedMap>(duration, base, std::move(points));
}
} // namespace suite_clip
