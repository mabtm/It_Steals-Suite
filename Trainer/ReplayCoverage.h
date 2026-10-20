#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <climits>
#include <stdexcept>

namespace practice
{
inline constexpr size_t CoverageChannels = 6;
inline constexpr std::array<double, CoverageChannels> CoverageLimits{.12, .25, .25, .12, .12, .25};

struct CoverageGap
{
    double begin = 0, end = 0;
};

struct CoverageSpan
{
    bool started = false;
    int observations = 0, errors = 0, sources = 0, gapCount = 0;
    double first = 0, through = 0, maxGap = 0;
    std::vector<CoverageGap> gaps;
};

struct ReplayCoverage
{
    bool recorded = false;
    std::array<CoverageSpan, CoverageChannels> spans;
};

inline ReplayCoverage ReadCoverage(MapReader &r, double duration)
{
    ReplayCoverage coverage;
    auto flag = [&] {
        auto b = r.Read<uint8_t>();
        if (b > 1)
            throw std::runtime_error("Invalid recording coverage flag");
        return b != 0;
    };
    auto time = [&] {
        auto t = r.Read<double>();
        if (!std::isfinite(t) || t < 0 || t > duration + .000001)
            throw std::runtime_error("Invalid recording coverage time");
        return t;
    };
    coverage.recorded = flag();
    if (r.Read<int32_t>() != int(CoverageChannels))
        throw std::runtime_error("Unsupported recording coverage channels");
    for (auto &s : coverage.spans)
    {
        s.started = flag();
        s.observations = r.Count(INT_MAX);
        s.errors = r.Count(INT_MAX);
        s.sources = r.Count(8192);
        s.first = time();
        s.through = time();
        s.maxGap = time();
        s.gapCount = r.Count(INT_MAX);
        int n = r.Count(16);
        if (s.first > s.through || n > s.gapCount ||
            (!coverage.recorded && (s.started || s.observations || s.errors || s.sources || s.gapCount)) ||
            (!s.started && s.observations) ||
            (s.observations == 0 && (s.first || s.through || s.maxGap || s.gapCount)))
            throw std::runtime_error("Inconsistent recording coverage");
        double previous = 0;
        for (int i = 0; i < n; i++)
        {
            double begin = time(), end = time();
            if (begin < previous || end <= begin || end - begin > s.maxGap + .000001)
                throw std::runtime_error("Invalid recording coverage interval");
            previous = end;
            s.gaps.push_back({begin, end});
        }
    }
    return coverage;
}
} // namespace practice
