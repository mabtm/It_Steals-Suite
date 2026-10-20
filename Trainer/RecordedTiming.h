#pragma once
#include "PracticeMapData.h"
#include <array>
#include <atomic>
#include <numeric>
#include <map>
#include <tuple>

namespace practice
{
struct TimingProfile
{
    std::array<double, 5> rate{}, low{}, high{}, distance{};
    double recorded = 0, reconstructed = 0, moving = 0, stopped = 0, missing = 0, coverage = 0,
           pickupPause = 0;
    bool valid = false;
    size_t samples = 0;
};

struct TimingEstimate
{
    bool valid = false;
    double seconds = 0, fast = 0, slow = 0, coverage = 0, length = 0;
    std::vector<double> times, distances;

    double Fraction(double at) const
    {
        if (!valid || length <= 0 || times.empty())
            return 0;
        if (!std::isfinite(at))
            at = 0;
        at = std::clamp(at, 0., seconds);
        auto next = std::upper_bound(times.begin(), times.end(), at);
        if (next == times.begin())
            return 0;
        if (next == times.end())
            return 1;
        size_t b = size_t(next - times.begin()), a = b - 1;
        double dt = times[b] - times[a];
        return std::clamp(
            (distances[a] + (dt > 0 ? (distances[b] - distances[a]) * (at - times[a]) / dt : 0)) / length, 0.,
            1.);
    }
};

inline double TimingAngle(Vec3 a, Vec3 b)
{
    double x = std::hypot(a.x, a.z), y = std::hypot(b.x, b.z);
    if (x < 1e-6 || y < 1e-6)
        return 0;
    return std::acos(std::clamp((double(a.x) * b.x + double(a.z) * b.z) / (x * y), -1., 1.));
}

inline int TimingClass(Vec3 previous, Vec3 direction, bool accelerating = false)
{
    if (accelerating)
        return 4;
    if (std::abs(direction.y) > std::hypot(direction.x, direction.z) * .15)
        return 3;
    double angle = TimingAngle(previous, direction);
    return angle > .7 ? 2 : angle > .2 ? 1 : 0;
}

inline double TimingQuantile(std::vector<double> values, double fraction)
{
    if (values.empty())
        return 0;
    std::sort(values.begin(), values.end());
    return values[std::min(values.size() - 1, size_t(fraction * (values.size() - 1)))];
}

// Reconstruct the recorded clock for Replay Details only. This diagnostic
// does not publish a training profile or estimate a counterfactual route.
// Gaps/teleports stay unknown.
inline TimingProfile BuildTimingProfile(const RunReview &r)
{
    TimingProfile p;
    p.recorded = r.seconds;
    std::array<std::vector<double>, 5> rates;
    std::array<double, 5> classSeconds{};
    std::vector<double> all;
    if (r.trace.size() < 2 || r.seconds <= 0)
        return p;
    Vec3 previous{};
    double priorSpeed = 0, known = 0;
    size_t start = 0;
    for (size_t i = 1; i < r.trace.size(); i++)
    {
        const auto &a = r.trace[i - 1];
        const auto &b = r.trace[i];
        double dt = b.seconds - a.seconds;
        if (dt > .25 || dt < 0 || (dt > 1e-6 && Distance(a.point, b.point) / dt > 25))
        {
            p.missing += std::max(0., b.seconds - r.trace[start].seconds);
            previous = {};
            priorSpeed = 0;
            start = i;
            continue;
        }
        if (b.seconds - r.trace[start].seconds < .12 && i + 1 < r.trace.size())
            continue;
        const auto &first = r.trace[start];
        dt = b.seconds - first.seconds;
        if (dt <= 1e-6)
        {
            start = i;
            continue;
        }
        // Integrate the sampled polyline, not the chord across a smoothing
        // window. A turn must not shorten the measured movement distance.
        double length = 0, turn = 0;
        Vec3 last = previous, direction{};
        for (size_t j = start + 1; j <= i; j++)
        {
            auto x = r.trace[j - 1].point, y = r.trace[j].point;
            double leg = Distance(x, y);
            if (leg < 1e-6)
                continue;
            Vec3 d{y.x - x.x, y.y - x.y, y.z - x.z};
            turn += TimingAngle(last, d);
            last = d;
            if (Distance({}, direction) < 1e-6)
                direction = d;
            length += leg;
        }
        double speed = length / dt;
        known += dt;
        if (speed < .5)
            p.stopped += dt;
        else
        {
            int kind = TimingClass(previous, direction, priorSpeed < .5);
            if (kind <= 2)
                kind = std::max(kind, turn > .7 ? 2 : turn > .2 ? 1 : 0);
            double rate = dt / length;
            p.distance[kind] += length;
            classSeconds[kind] += dt;
            rates[kind].push_back(rate);
            all.push_back(rate);
            p.moving += dt;
            p.samples++;
        }
        previous = last;
        priorSpeed = speed;
        start = i;
    }
    p.missing += std::max(0., r.seconds - known - p.missing);
    p.coverage = std::clamp(known / r.seconds, 0., 1.);
    double total = std::accumulate(p.distance.begin(), p.distance.end(), 0.);
    double center = total > 0 ? p.moving / total : 0, fast = TimingQuantile(all, .1),
           slow = TimingQuantile(all, .9);
    for (size_t k = 0; k < 5; k++)
    {
        p.rate[k] = p.distance[k] > 0 ? classSeconds[k] / p.distance[k] : center;
        bool enough = rates[k].size() >= 5;
        p.low[k] = std::min(p.rate[k], enough ? TimingQuantile(rates[k], .1) : fast * .8);
        p.high[k] = std::max(p.rate[k], enough ? TimingQuantile(rates[k], .9) : slow * 1.25);
        p.reconstructed += p.distance[k] * p.rate[k];
    }
    // No fitted scale factor: the integrated class times account for the
    // recording directly. OnTriggerEnter imposes no crystal dwell time.
    p.reconstructed += p.stopped + p.missing;
    p.pickupPause = 0;
    p.valid = p.samples >= 12 && p.moving >= 2 && total >= 5 && p.coverage >= .95 &&
              std::isfinite(p.reconstructed) && std::abs(p.reconstructed - r.seconds) < .001;
    return p;
}

// Only live cumulative physics observations publish into this registry.
// Opening a replay never replaces it with one run's movement.
inline std::array<std::shared_ptr<const MovementData>, kMapLevelCount> learnedMovement{};

inline bool MovementMatches(const MovementData &m, const MapData &d)
{
    return ValidMovement(m) && m.Ready() && m.scene == d.scene && m.hash != "UNKNOWN" && !m.hash.empty() &&
           m.hash == d.hash && d.movementTick > 0 && std::abs(m.tick - d.movementTick) < .000001f &&
           std::abs(m.radius - d.agentRadius) < .0001f && std::abs(m.height - d.agentHeight) < .0001f;
}

inline void PublishMovement(std::shared_ptr<const MovementData> m)
{
    if (!m || !ValidMovement(*m) || m->hash.empty() || m->hash == "UNKNOWN")
        return;
    int level = MapLevelIndex(m->scene);
    std::atomic_store(&learnedMovement[level], std::move(m));
}

inline std::shared_ptr<const MovementData> MovementFor(const MapData &d)
{
    int level = MapLevelIndex(d.scene);
    if (level < 0)
        return {};
    auto m = std::atomic_load(&learnedMovement[level]);
    if (m && MovementMatches(*m, d))
        return m;
    return d.movement && MovementMatches(*d.movement, d) ? d.movement : nullptr;
}

inline TimingEstimate EstimateMovementRoute(const MovementData &m, const MapData &data,
                                            const std::vector<Vec3> &line, bool verified)
{
    TimingEstimate out;
    if (!verified || !MovementMatches(m, data) || line.size() < 2 || line.size() > 30000 ||
        data.movementSprintStep <= 0)
        return out;
    std::vector<Vec3> points;
    points.reserve(line.size());
    for (auto p : line)
    {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            return {};
        if (points.empty() || Distance(points.back(), p) > 1e-8)
            points.push_back(p);
    }
    if (points.size() < 2)
        return out;
    const double axial = data.movementSprintStep / data.movementTick, diagonal = axial * std::sqrt(2.);
    if (axial <= 0 || diagonal > 25.6)
        return out;
    auto known = [&](int k) { return m.counts[k] >= 8; };
    auto span = [&](int k) { return known(k) ? double(m.lengths[k]) : .12 * (axial + diagonal) * .5; };
    out.times = {0};
    out.distances = {0};
    double measured = 0, starting = span(4);
    auto emit = [&](double length, int kind) {
        if (length <= 1e-8)
            return;
        bool observed = known(kind);
        double low = observed ? m.speeds[kind * 3] : axial,
               center = observed ? m.speeds[kind * 3 + 1] : (axial + diagonal) * .5,
               high = observed ? m.speeds[kind * 3 + 2] : diagonal;
        out.seconds += length / center;
        out.fast += length / high;
        out.slow += length / low;
        out.length += length;
        if (observed)
            measured += length;
        out.times.push_back(out.seconds);
        out.distances.push_back(out.length);
    };
    auto piece = [&](double length, int kind) {
        double start = std::min(length, starting);
        emit(start, 4);
        starting -= start;
        emit(length - start, kind);
    };
    auto direction = [&](size_t i) {
        return Vec3{points[i + 1].x - points[i].x, points[i + 1].y - points[i].y,
                    points[i + 1].z - points[i].z};
    };
    auto corner = [&](size_t i) {
        double angle = TimingAngle(direction(i - 1), direction(i));
        return angle > .7 ? 2 : angle > .18 ? 1 : 0;
    };
    for (size_t i = 0; i + 1 < points.size(); i++)
    {
        Vec3 d = direction(i);
        double length = Distance(points[i], points[i + 1]);
        int base = std::abs(d.y) > length * .15 ? 3 : 0;
        int left = i ? corner(i) : 0, right = i + 2 < points.size() ? corner(i + 1) : 0;
        double before = left ? std::min(length * .5, span(left) * .5) : 0,
               after = right ? std::min(length * .5, span(right) * .5) : 0;
        piece(before, left);
        piece(length - before - after, base);
        piece(after, right);
    }
    out.coverage = out.length > 0 ? measured / out.length : 0;
    out.valid = out.length > 0 && std::isfinite(out.seconds) && out.seconds > 0;
    return out;
}
} // namespace practice
