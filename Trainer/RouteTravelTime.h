#pragma once
#include <algorithm>
#include <cmath>

namespace practice
{
struct RouteTravelTime
{
    double fast = 0, slow = 0;
    bool valid = false;
};

inline RouteTravelTime EstimateTravel(double distance, bool verified = true)
{
    if (!verified || !std::isfinite(distance) || distance < 0)
        return {};
    // User's observed optimal movement range. These are travel times only;
    // no pickup, turn, acceleration or enemy timing is inferred.
    return {distance / 10., distance / 8.1, true};
}

inline RouteTravelTime EstimateGameTravel(double distance, float tick, float step, bool verified = true)
{
    if (!verified || !std::isfinite(distance) || distance < 0 || !std::isfinite(tick) ||
        !std::isfinite(step) || tick <= 0 || step <= 0)
        return {};
    double axial = step / tick;
    return {distance / (axial * std::sqrt(2.)), distance / axial, true};
}

inline float RouteSpeed(float value)
{
    return std::isfinite(value) ? std::clamp(value, 8.1f, 10.f) : 9.f;
}

inline double TravelDuration(double distance, float speed)
{
    return std::isfinite(distance) && distance >= 0 ? distance / RouteSpeed(speed) : 0;
}
} // namespace practice
