#include "RunStatistics.h"
#include <cassert>
#include <iostream>
#include <limits>

int main()
{
    GameData data{};
    data.pid = 123;
    data.modeStatsSinceUtc = 639285120000000000LL;
    data.modeStatsRevision = 7;
    for (int mode = 0; mode < 8; mode++)
    {
        data.modePlaySeconds[mode * 3] = 120;
        for (int category : {1, 2})
        {
            int row = mode * 3 + category;
            data.modeAttempts[row] = 10 * category;
            data.modeFinished[row] = 2 * category;
            data.modeResets[row] = 3 * category;
            data.modeDeaths[row] = category;
            data.modeFinishSeconds[row] = 20 * category;
            data.modePlaySeconds[row] = 3600 * category;
        }
        data.modePersonalBest[mode] = 10 + mode;
        data.modePbUtc[mode] = data.modeStatsSinceUtc;
    }
    run_stats::Dashboard view;
    view.Update(&data, 0, 10);
    assert(view.available && view.total.attempts == 240 && view.total.finished == 48);
    for (int i = 0; i < 8; i++)
        assert(view.modes[i].totals.attempts == 30 && view.modes[i].pb == 10 + i &&
               view.modes[i].time == "3 h 02 min" && view.modes[i].totals.Completion() == 20);
    assert(view.time == "24 h 16 min" && view.modes[0].pbDate != "Not recorded");
    auto rebuilds = view.rebuilds;
    for (int i = 0; i < 120; i++)
        view.Update(&data, 0, 10 + i / 121.);
    assert(view.rebuilds == rebuilds);
    data.modeStatsRevision++;
    data.modeAttempts[1]++;
    view.Update(&data, 0, 10.999);
    assert(view.rebuilds == rebuilds + 1 && view.modes[0].totals.attempts == 31);
    view.Update(&data, 1, 11);
    assert(view.modes[1].totals.attempts == 10 && view.modes[1].time == "1 h 00 min" &&
           view.modes[1].pb == 11);
    view.Update(nullptr, 2, 12);
    assert(view.available && view.modes[1].totals.attempts == 20 && view.modes[1].time == "2 h 00 min");
    data.pid++;
    data.modePersonalBest[1] = 9;
    view.Update(&data, 2, 12);
    assert(view.modes[1].pb == 9);
    data.modeStatsSinceUtc = 1;
    view.Update(&data, 2, 12);
    assert(view.since == "Not recorded");
    assert(run_stats::TimeSpent(59) == "0 min" && run_stats::TimeSpent(60) == "1 min" &&
           run_stats::TimeSpent(3599) == "59 min" && run_stats::TimeSpent(3600) == "1 h 00 min");
    data.modeAttempts[0] = -1;
    data.modeFinished[0] = 99;
    data.modeResets[0] = 99;
    data.modeDeaths[0] = 99;
    data.modePlaySeconds[0] = std::numeric_limits<double>::infinity();
    data.modePersonalBest[0] = std::numeric_limits<double>::quiet_NaN();
    data.modePbUtc[0] = -1;
    SanitizeSnapshot(data);
    assert(data.modeAttempts[0] == 0 && data.modeFinished[0] == 0 && data.modeResets[0] == 0 &&
           data.modeDeaths[0] == 0 && data.modePlaySeconds[0] == 0 && data.modePersonalBest[0] == 0 &&
           data.modePbUtc[0] == 0);
    std::cout
        << "Eight-mode totals, Clean/Practice filters, lifetime time formatting, snapshot caching, offline browsing and packed-data validation: PASS\n";
}
