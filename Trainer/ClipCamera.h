#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace suite_clip
{
struct Point
{
    float x = 0, y = 0, z = 0;

    Point operator+(Point b) const
    {
        return {x + b.x, y + b.y, z + b.z};
    }

    Point operator-(Point b) const
    {
        return {x - b.x, y - b.y, z - b.z};
    }

    Point operator*(float s) const
    {
        return {x * s, y * s, z * s};
    }
};

struct Rotation
{
    float x = 0, y = 0, z = 0, w = 1;
};

inline Rotation Normal(Rotation q)
{
    float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (!std::isfinite(n) || n < .0001f)
        throw std::runtime_error("Invalid camera rotation");
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}

inline Rotation Slerp(Rotation a, Rotation b, float t)
{
    a = Normal(a);
    b = Normal(b);
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0)
    {
        b = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }
    float x = 1 - t, y = t;
    if (dot < .9995f)
    {
        float angle = std::acos(std::clamp(dot, -1.f, 1.f));
        x = std::sin((1 - t) * angle) / std::sin(angle);
        y = std::sin(t * angle) / std::sin(angle);
    }
    return Normal({a.x * x + b.x * y, a.y * x + b.y * y, a.z * x + b.z * y, a.w * x + b.w * y});
}

inline Rotation Angles(float yaw, float pitch)
{
    constexpr float k = .00872664626f;
    float cy = std::cos(yaw * k), sy = std::sin(yaw * k), cp = std::cos(pitch * k), sp = std::sin(pitch * k);
    return {cy * sp, sy * cp, -sy * sp, cy * cp};
}

inline Point Forward(Rotation q)
{
    q = Normal(q);
    return {2 * (q.x * q.z + q.w * q.y), 2 * (q.y * q.z - q.w * q.x), 1 - 2 * (q.x * q.x + q.y * q.y)};
}

inline Point Right(Rotation q)
{
    q = Normal(q);
    return {1 - 2 * (q.y * q.y + q.z * q.z), 2 * (q.x * q.y + q.w * q.z), 2 * (q.x * q.z - q.w * q.y)};
}

struct CameraPose
{
    Point position;
    Rotation rotation;
    float fov = 65;

    void Validate() const
    {
        for (float v : {position.x, position.y, position.z})
            if (!std::isfinite(v) || std::abs(v) > 1000000)
                throw std::runtime_error("Camera position is out of range");
        float norm = rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z +
                     rotation.w * rotation.w;
        if (!std::isfinite(norm) || norm < .98f || norm > 1.02f)
            throw std::runtime_error("Invalid camera rotation");
        if (!std::isfinite(fov) || fov < 10 || fov > 150)
            throw std::runtime_error("Camera field of view must be 10-150 degrees");
    }
};
enum class Transition : int
{
    Cut,
    Linear,
    Smooth,
    EaseIn,
    EaseOut,
    EaseInOut
};
inline constexpr const char *TransitionNames[] = {"Cut",     "Linear",   "Smooth",
                                                  "Ease in", "Ease out", "Ease in / out"};

inline float Curve(Transition mode, float t)
{
    t = std::clamp(t, 0.f, 1.f);
    switch (mode)
    {
    case Transition::Cut:
        return 0;
    case Transition::EaseIn:
        return t * t;
    case Transition::EaseOut:
        return 1 - (1 - t) * (1 - t);
    case Transition::Smooth:
        return t * t * t * (t * (t * 6 - 15) + 10);
    case Transition::EaseInOut:
        return t * t * (3 - 2 * t);
    default:
        return t;
    }
}

// Absolute yaw/pitch, independent of playback direction and source render rate.
struct Orbit
{
    float yaw = 0, pitch = 22, radius = 7;
    Point pan;
    bool ready = false;

    void Align(CameraPose pose, Point focus)
    {
        auto forward = Forward(pose.rotation);
        yaw = std::atan2(forward.x, forward.z) * 57.2957795f;
        pitch = std::asin(std::clamp(-forward.y, -1.f, 1.f)) * 57.2957795f;
        float distance =
            std::sqrt(std::pow(pose.position.x - focus.x, 2) + std::pow(pose.position.y - focus.y, 2) +
                      std::pow(pose.position.z - focus.z, 2));
        radius = std::clamp(distance, .5f, 100.f);
        pan = pose.position + Forward(Angles(yaw, pitch)) * radius - focus;
        ready = true;
    }

    CameraPose Pose(Point focus, float fov = 65) const
    {
        auto q = Angles(yaw, pitch);
        return {focus + pan - Forward(q) * radius, q, fov};
    }

    void Rotate(float x, float y)
    {
        yaw = std::remainder(yaw + x * .3f, 360.f);
        pitch = std::clamp(pitch + y * .3f, -85.f, 85.f);
    }

    void Zoom(float wheel)
    {
        radius = std::clamp(radius * std::exp(-wheel * .12f), .5f, 100.f);
    }
};

enum SceneLayer : uint32_t
{
    Colliders = 1,
    Route = 2,
    Monster = 4,
    Ceiling = 8,
    FadeWalls = 16,
    Fullbright = 32,
    AllLayers = 63
};

inline constexpr const char *ShotModeNames[] = {"Recorded", "Follow", "Free"};
inline constexpr size_t NoKey = std::numeric_limits<size_t>::max();

inline std::string ShotLabel(size_t index)
{
    std::string s;
    do
    {
        s.insert(s.begin(), char('A' + index % 26));
        if (index < 26)
            break;
        index = index / 26 - 1;
    } while (true);
    return s;
}

struct CameraKey
{
    double seconds = 0;
    CameraPose pose;
    Transition next = Transition::EaseInOut;
    std::string name = "Camera";
    int mode = 2; // Legacy static poses are Free shots.
    Orbit follow;
    uint32_t layerMask = 0, layers = 0;
    bool speedOverride = false;
    double playbackSpeed = 1;
    Transition speedCurve = Transition::Cut;
};

struct CameraTrack
{
    std::vector<CameraKey> keys;

    void Validate(double duration) const
    {
        if (keys.size() > 256)
            throw std::runtime_error("Camera track supports up to 256 keypoints");
        double previous = -1;
        for (const auto &key : keys)
        {
            if (!std::isfinite(key.seconds) || key.seconds < 0 || key.seconds > duration ||
                key.seconds - previous < .001 || int(key.next) < 0 || int(key.next) > 5 ||
                key.name.size() > 80 || key.name.find_first_of("\r\n") != std::string::npos)
                throw std::runtime_error("Invalid camera keypoint");
            key.pose.Validate();
            if (!std::isfinite(key.playbackSpeed) || key.playbackSpeed < .1 || key.playbackSpeed > 8 ||
                int(key.speedCurve) < 0 || int(key.speedCurve) > 5)
                throw std::runtime_error("Invalid keypoint playback speed");
            if (key.mode < 0 || key.mode > 2 || (key.layerMask & ~AllLayers) || (key.layers & ~key.layerMask))
                throw std::runtime_error("Invalid keypoint mode or scene layers");
            for (float value : {key.follow.yaw, key.follow.pitch, key.follow.radius, key.follow.pan.x,
                                key.follow.pan.y, key.follow.pan.z})
                if (!std::isfinite(value) || std::abs(value) > 1000000)
                    throw std::runtime_error("Invalid keypoint follow camera");
            if (key.follow.radius < .5f || key.follow.radius > 100 || std::abs(key.follow.pitch) > 89)
                throw std::runtime_error("Invalid keypoint follow camera range");
            previous = key.seconds;
        }
    }

    size_t Active(double seconds) const
    {
        auto p = std::upper_bound(keys.begin(), keys.end(), seconds,
                                  [](double t, const auto &key) { return t < key.seconds; });
        return p == keys.begin() ? NoKey : size_t(std::prev(p) - keys.begin());
    }

    uint32_t LayersAt(double seconds, uint32_t defaults) const
    {
        for (const auto &key : keys)
        {
            if (key.seconds > seconds)
                break;
            defaults = (defaults & ~key.layerMask) | key.layers;
        }
        return defaults;
    }

    CameraPose At(double seconds) const
    {
        if (keys.empty() || !std::isfinite(seconds))
            throw std::runtime_error("Add a camera keypoint first");
        auto p = std::upper_bound(keys.begin(), keys.end(), seconds,
                                  [](double t, const auto &key) { return t < key.seconds; });
        if (p == keys.begin())
            return p->pose;
        if (p == keys.end())
            return keys.back().pose;
        const auto &a = *std::prev(p);
        const auto &b = *p;
        float u = Curve(a.next, float((seconds - a.seconds) / (b.seconds - a.seconds)));
        return {a.pose.position + (b.pose.position - a.pose.position) * u,
                Slerp(a.pose.rotation, b.pose.rotation, u), a.pose.fov + (b.pose.fov - a.pose.fov) * u};
    }

    size_t Put(CameraKey key)
    {
        auto p = std::lower_bound(keys.begin(), keys.end(), key.seconds,
                                  [](const auto &k, double t) { return k.seconds < t; });
        if (p != keys.end() && std::abs(p->seconds - key.seconds) < .001)
        {
            *p = std::move(key);
            return size_t(p - keys.begin());
        }
        if (p != keys.begin() && std::abs(std::prev(p)->seconds - key.seconds) < .001)
        {
            *--p = std::move(key);
            return size_t(p - keys.begin());
        }
        if (keys.size() >= 256)
            throw std::runtime_error("Camera keypoint limit reached");
        // Evaluate begin() after insert: vector growth invalidates old iterators.
        auto inserted = keys.insert(p, std::move(key));
        return size_t(inserted - keys.begin());
    }
};

inline CameraPose Blend(CameraPose a, CameraPose b, float u)
{
    return {a.position + (b.position - a.position) * u, Slerp(a.rotation, b.rotation, u),
            a.fov + (b.fov - a.fov) * u};
}

struct CameraEvaluation
{
    std::optional<CameraPose> pose;
    int mode = 0;
    size_t active = NoKey;
    bool blending = false;
};

// One pure evaluator for seeking, preview and every exported frame. Dynamic
// modes sample the original recording at the requested time, never UI state.
template <class Recorded>
CameraEvaluation EvaluateCamera(const CameraTrack &track, double at, Recorded recorded)
{
    if (!std::isfinite(at))
        throw std::runtime_error("Invalid camera time");
    size_t active = track.Active(at);
    if (active == NoKey)
        return {recorded(at), 0, active, false};
    auto resolve = [&](const CameraKey &key) -> std::optional<CameraPose> {
        if (key.mode == 2)
            return key.pose;
        auto source = recorded(at);
        if (!source || key.mode == 0)
            return source;
        return key.follow.Pose(source->position, key.pose.fov);
    };
    const auto &a = track.keys[active];
    auto first = resolve(a);
    if (active + 1 == track.keys.size() || a.next == Transition::Cut || at == a.seconds)
        return {first, a.mode, active, false};
    const auto &b = track.keys[active + 1];
    auto last = resolve(b);
    if (!first || !last)
        return {{}, 2, active, true};
    float u = Curve(a.next, float((at - a.seconds) / (b.seconds - a.seconds)));
    return {Blend(*first, *last, u), 2, active, true};
}

// Free camera rotates in place and flies in world space; there is no orbit pivot.
struct FlyCamera
{
    float yaw = 0, pitch = 0, speed = 10;
    bool ready = false;

    void Align(const CameraPose &pose)
    {
        auto forward = Forward(pose.rotation);
        yaw = std::atan2(forward.x, forward.z) * 57.2957795f;
        pitch = std::asin(std::clamp(-forward.y, -1.f, 1.f)) * 57.2957795f;
        ready = true;
    }

    void Look(CameraPose &pose, float dx, float dy)
    {
        if (!ready)
            Align(pose);
        yaw = std::remainder(yaw + dx * .22f, 360.f);
        pitch = std::clamp(pitch + dy * .22f, -89.f, 89.f);
        pose.rotation = Angles(yaw, pitch);
    }

    void Move(CameraPose &pose, float x, float y, float z, float dt, bool fast) const
    {
        Point direction = Right(pose.rotation) * x + Point{0, y, 0} + Forward(pose.rotation) * z;
        float length =
            std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
        if (length > 0)
            pose.position =
                pose.position + direction * (speed * std::clamp(dt, 0.f, .1f) * (fast ? 3 : 1) / length);
    }

    void Travel(CameraPose &pose, float wheel) const
    {
        pose.position = pose.position + Forward(pose.rotation) * (wheel * speed * .22f);
    }

    void Pan(CameraPose &pose, float dx, float dy) const
    {
        auto right = Right(pose.rotation), forward = Forward(pose.rotation);
        Point up{forward.y * right.z - forward.z * right.y, forward.z * right.x - forward.x * right.z,
                 forward.x * right.y - forward.y * right.x};
        pose.position = pose.position + (right * -dx + up * dy) * (speed * .003f);
    }
};
} // namespace suite_clip
