#pragma once
#include "SharedProtocol.h"
#include "MapLevels.h"
#include <array>
#include <cstdio>
#include <ctime>
#include <string>

namespace run_stats
{
inline std::string TimeSpent(double seconds, bool compact = false)
{
    auto minutes = static_cast<uint64_t>(std::max(0., seconds) / 60.);
    char text[64];
    if (minutes >= 60)
        snprintf(text, sizeof(text), compact ? "%lluh %02llum" : "%llu h %02llu min",
                 static_cast<unsigned long long>(minutes / 60),
                 static_cast<unsigned long long>(minutes % 60));
    else
        snprintf(text, sizeof(text), "%llu min", static_cast<unsigned long long>(minutes));
    return text;
}

inline std::string Date(int64_t ticks)
{
    if (ticks < 621355968000000000LL)
        return "Not recorded";
    time_t at = static_cast<time_t>((ticks - 621355968000000000LL) / 10000000);
    std::tm value{};
#ifdef _WIN32
    if (localtime_s(&value, &at))
        return "Not recorded";
#else
    if (!localtime_r(&at, &value))
        return "Not recorded";
#endif
    char text[32];
    std::strftime(text, sizeof(text), "%Y-%m-%d", &value);
    return text;
}

struct Totals
{
    int64_t attempts = 0, finished = 0, resets = 0, deaths = 0;
    double finishSeconds = 0, playSeconds = 0;

    void Add(const GameData &data, int row)
    {
        attempts += data.modeAttempts[row];
        finished += data.modeFinished[row];
        resets += data.modeResets[row];
        deaths += data.modeDeaths[row];
        finishSeconds += data.modeFinishSeconds[row];
        playSeconds += data.modePlaySeconds[row];
    }

    double Completion() const
    {
        return attempts ? 100. * finished / attempts : 0;
    }
};

struct Mode
{
    Totals totals;
    double pb = 0;
    std::string attempts, finished, time, timeShort, pbDate;
};

// Eight bounded rows, formatted once per second or immediately on a run event.
// Keep the last snapshot available for browsing after the game disconnects.
struct Dashboard
{
    GameData snapshot{};
    std::array<Mode, 8> modes;
    Totals total;
    std::string time, timeShort, attempts, finished, since, sinceLabel;
    int filter = -1, selected = 0;
    bool available = false;
    double refreshed = -10;
    uint64_t rebuilds = 0;

    void Update(const GameData *data, int wantedFilter, double now)
    {
        bool refresh = data && (!available || data->pid != snapshot.pid ||
                                data->statsCategory != snapshot.statsCategory ||
                                data->modeStatsSinceUtc != snapshot.modeStatsSinceUtc ||
                                data->modeStatsRevision != snapshot.modeStatsRevision ||
                                now - refreshed >= 1 || now < refreshed);
        if (!refresh && wantedFilter == filter)
            return;
        if (refresh)
        {
            std::memcpy(&snapshot, data, sizeof(snapshot));
            refreshed = now;
            available = true;
        }
        filter = std::clamp(wantedFilter, 0, 2); // All, Clean, Practice
        total = {};
        for (int i = 0; i < kMapLevelCount; i++)
        {
            auto &mode = modes[i];
            mode.totals = {};
            for (int category = 0; category < 3; category++)
                if (!filter || category == filter)
                    mode.totals.Add(snapshot, i * 3 + category);
            mode.pb = snapshot.modePersonalBest[i];
            mode.pbDate = Date(snapshot.modePbUtc[i]);
            mode.attempts = std::to_string(mode.totals.attempts);
            mode.finished = std::to_string(mode.totals.finished);
            mode.time = TimeSpent(mode.totals.playSeconds);
            mode.timeShort = TimeSpent(mode.totals.playSeconds, true);
            total.attempts += mode.totals.attempts;
            total.finished += mode.totals.finished;
            total.resets += mode.totals.resets;
            total.deaths += mode.totals.deaths;
            total.finishSeconds += mode.totals.finishSeconds;
            total.playSeconds += mode.totals.playSeconds;
        }
        time = TimeSpent(total.playSeconds);
        timeShort = TimeSpent(total.playSeconds, true);
        attempts = std::to_string(total.attempts);
        finished = std::to_string(total.finished);
        since = Date(snapshot.modeStatsSinceUtc);
        sinceLabel = "Mode data since " + since;
        rebuilds++;
    }
};

inline Dashboard dashboard;
} // namespace run_stats
