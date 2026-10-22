#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <string>

class FrameCost
{
    std::array<double, 128> samples{};
    size_t at = 0, count = 0;
    double total = 0;

  public:
    void Observe(double ms)
    {
        if (!std::isfinite(ms) || ms < 0)
            return;
        if (count == samples.size())
            total -= samples[at];
        else
            count++;
        samples[at] = ms;
        total += ms;
        at = (at + 1) % samples.size();
    }

    double Mean() const
    {
        return count ? total / count : 0;
    }

    double P95() const
    {
        if (!count)
            return 0;
        auto copy = samples;
        std::sort(copy.begin(), copy.begin() + count);
        return copy[(count - 1) * 95 / 100];
    }
};

namespace practice
{
class SearchTiming
{
    struct Sample
    {
        double seconds;
        uint64_t done;
    };

    std::deque<Sample> samples;
    double lastProgress = 0;

  public:
    double Remaining(uint64_t done, uint64_t total, double seconds)
    {
        const double unknown = std::numeric_limits<double>::quiet_NaN();
        if (!std::isfinite(seconds) || seconds < 0 || done > total)
            return unknown;
        if (done == total)
            return 0;
        if (!samples.empty() && (seconds < samples.back().seconds || done < samples.back().done))
        {
            samples.clear();
            lastProgress = seconds;
        }
        if (samples.empty() || done != samples.back().done)
            lastProgress = seconds;
        samples.push_back({seconds, done});
        while (samples.size() > 64 || (samples.size() > 2 && samples[1].seconds < seconds - 12))
            samples.pop_front();
        double span = seconds - samples.front().seconds;
        if (span < 2 || seconds - lastProgress > 3 || done <= samples.front().done)
            return unknown;
        double rate = (done - samples.front().done) / span;
        return (total - done) / rate;
    }
};

inline std::string DurationEstimate(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0 || seconds > 3153600000.)
        return "Calculating...";
    uint64_t value = static_cast<uint64_t>(std::ceil(seconds));
    if (value < 60)
        return std::to_string(value) + " s";
    if (value < 3600)
        return std::to_string(value / 60) + " min " + std::to_string(value % 60) + " s";
    if (value < 86400)
        return std::to_string(value / 3600) + " h " + std::to_string(value % 3600 / 60) + " min";
    return std::to_string(value / 86400) + " d " + std::to_string(value % 86400 / 3600) + " h";
}
} // namespace practice
