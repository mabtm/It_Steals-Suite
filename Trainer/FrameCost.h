#pragma once
#include <array>
#include <algorithm>
#include <cmath>

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
