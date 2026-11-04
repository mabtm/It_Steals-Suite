#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace suite_ui
{
enum class Page
{
    Cheats,
    SeedMap,
    Speedrun,
    Settings,
    Replay,
    Diagnostics,
    Inspector,
    Updates,
    Count
};
inline constexpr int PageCount = int(Page::Count);

// The enum remains stable for saved selection and UI IDs. Present related
// tasks together instead of mixing maintenance pages into the play workflow.
struct NavigationItem
{
    Page page;
    const char *group;
    const char *compact;
};
inline constexpr std::array<NavigationItem, PageCount> NavigationItems = {{
    {Page::Speedrun, "Practice", "Runs"},
    {Page::Cheats, nullptr, "Tools"},
    {Page::SeedMap, nullptr, "Seeds"},
    {Page::Inspector, nullptr, "Inspect"},
    {Page::Replay, "Review", "Clips"},
    {Page::Settings, "Suite", "Setup"},
    {Page::Updates, nullptr, "Update"},
    {Page::Diagnostics, nullptr, "Status"},
}};

struct Navigation
{
    Page page = Page::Cheats;
    double changed = -1;

    bool Select(Page next, double now)
    {
        if (int(next) < 0 || int(next) >= PageCount || next == page)
            return false;
        page = next;
        changed = now;
        return true;
    }

    bool Is(Page wanted) const
    {
        return page == wanted;
    }
};

// Only the ten visible results are tracked. A moved row keeps its identity;
// a new search resets the short highlight without growing an event history.
struct ResultHighlights
{
    struct Entry
    {
        int seed = 0;
        double at = -10;
        bool used = false;
    };

    std::array<Entry, 10> rows{};
    uint64_t key = 0;
    uint32_t serial = 0, session = 0;
    bool bound = false;

    template <class Rows>
    void Observe(uint64_t nextKey, uint32_t nextSerial, uint32_t nextSession, const Rows &top, double now)
    {
        if (!bound || key != nextKey || serial != nextSerial || session != nextSession)
        {
            rows = {};
            key = nextKey;
            serial = nextSerial;
            session = nextSession;
            bound = true;
        }
        auto old = rows;
        rows = {};
        size_t n = std::min(rows.size(), top.size());
        for (size_t i = 0; i < n; i++)
        {
            rows[i] = {top[i].seed, now, true};
            for (const auto &e : old)
                if (e.used && e.seed == top[i].seed)
                {
                    rows[i].at = e.at;
                    break;
                }
        }
    }

    float Strength(int seed, double now, bool motion) const
    {
        if (!motion)
            return 0;
        for (const auto &e : rows)
            if (e.used && e.seed == seed)
            {
                double age = now - e.at;
                return age >= 0 && age < 1.2 ? float((1.2 - age) / 1.2) : 0;
            }
        return 0;
    }
};

inline double TimelineAt(double x, double left, double width, double duration)
{
    if (!std::isfinite(x) || !std::isfinite(left) || !std::isfinite(width) || !std::isfinite(duration) ||
        width <= 0 || duration <= 0)
        return 0;
    return std::clamp((x - left) / width, 0., 1.) * duration;
}

inline double SafeDuration(double duration)
{
    return std::isfinite(duration) && duration > 0 ? duration : 0;
}

// One entry per visible pixel: both drawing and picking stay bounded, even
// when a replay contains thousands of light observations.
struct TimelineMarkers
{
    struct Bin
    {
        double pickup = -1, flash = -1;
        uint32_t pickups = 0, flashes = 0;
    };

    std::array<Bin, 2048> bins{};
    int width = 1;
    double span = 0;

    void Reset(int pixels, double duration)
    {
        width = std::clamp(pixels, 1, int(bins.size()));
        span = SafeDuration(duration);
        bins = {};
    }

    void Add(double at, bool flash)
    {
        if (!std::isfinite(at) || at < 0 || at > span || span <= 0)
            return;
        int i = std::clamp(int(std::round(at / span * (width - 1))), 0, width - 1);
        auto &b = bins[i];
        if (flash)
        {
            if (!b.flashes)
                b.flash = at;
            ++b.flashes;
        }
        else
        {
            if (!b.pickups)
                b.pickup = at;
            ++b.pickups;
        }
    }

    int Nearest(double fraction, int radius = 7) const
    {
        if (!std::isfinite(fraction))
            return -1;
        int x = std::clamp(int(std::round(fraction * (width - 1))), 0, width - 1), found = -1,
            distance = radius + 1;
        for (int i = std::max(0, x - radius); i <= std::min(width - 1, x + radius); i++)
            if ((bins[i].pickups || bins[i].flashes) && std::abs(i - x) < distance)
            {
                distance = std::abs(i - x);
                found = i;
            }
        return found;
    }
};
} // namespace suite_ui
