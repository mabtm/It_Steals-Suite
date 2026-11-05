#ifdef _WIN32
#include <windows.h>
#endif
#include "ReplayLibraryModel.h"
#include <cassert>
#include <iostream>

int main()
{
    using namespace practice;
    std::string profile = "{\"version\":1,\"unit\":\"ms\",\"p95_window\":128,\"stages\":{";
    for (auto name : CaptureStages)
        profile += std::string(profile.back() == '{' ? "" : ",") + '"' + name + "\":{\"count\":2,\"mean\":1.5,\"p95\":2,\"max\":3}";
    profile += "}}";
    auto cost = ParseCaptureProfile(profile);
    assert(cost[6].count == 2 && cost[6].mean == 1.5 && cost[6].p95 == 2 && cost[6].maximum == 3);
    auto badProfile = profile; badProfile.replace(badProfile.find("128"), 3, "127");
    bool refused = false; try { ParseCaptureProfile(badProfile); } catch (...) { refused = true; } assert(refused);
    auto state = std::make_shared<ReplayArchiveState>();
    constexpr int64_t day = 864000000000ll, epoch = 621355968000000000ll;
    for (int i = 0; i < 120; i++)
    {
        ReplayInfo row;
        row.id = epoch + (121 - i) * day;
        row.scene = i % 2 ? 3 : 13;
        row.seed = i - 60;
        row.seconds = (i % 5) * 10 + 1;
        row.outcome = i % 3 ? 1 : 2;
        row.valid = i % 2;
        row.favorite = i % 4 == 0;
        row.name = "Run " + std::to_string(i);
        row.category = i % 2 ? "Training" : "PB";
        row.runCategory = i % 3;
        row.selectedSeed = row.seed;
        row.notes = i % 7 ? "" : "Important turn";
        state->runs.push_back(row);
    }
    ReplayLibraryView view;
    ReplayLibraryFilter filter;
    assert(view.Update(state, filter) && view.shown.size() == 120);
    assert(!view.Update(state, filter));
    auto *indices = view.shown.data();
    for (int i = 0; i < 1000; i++)
        assert(!view.Update(state, filter));
    assert(indices == view.shown.data() && view.shown.front() == 0);
    filter.query = "IMPORTANT TURN";
    view.Update(state, filter);
    assert(view.shown.size() == 18);
    filter.query = "RUN 0 PB";
    view.Update(state, filter);
    assert(view.shown.size() == 1 && view.shown.front() == 0);
    filter.query.clear();
    filter.favorites = true;
    filter.level = MapLevelIndex(13);
    filter.status = 4;
    view.Update(state, filter);
    assert(view.shown.size() == 10);
    for (auto i : view.shown)
        assert(state->runs[i].favorite && state->runs[i].scene == 13 && state->runs[i].outcome == 2);
    filter = {};
    filter.category = 1;
    view.Update(state, filter);
    assert(view.shown.size() == 40);
    for (auto i : view.shown) assert(state->runs[i].runCategory == 1);
    filter.category = -1; filter.sort = ReplaySort::Category; filter.ascending = true;
    view.Update(state, filter);
    assert(state->runs[view.shown.front()].runCategory == 0 && state->runs[view.shown.back()].runCategory == 2);
    filter.sort = ReplaySort::Time;
    filter.ascending = true;
    view.Update(state, filter);
    for (size_t i = 1; i < view.shown.size(); i++)
    {
        const auto &a = state->runs[view.shown[i - 1]], &b = state->runs[view.shown[i]];
        assert(a.seconds < b.seconds || (a.seconds == b.seconds && a.id > b.id));
    }
    filter.sort = ReplaySort::Date;
    view.Update(state, filter);
    assert(view.shown.front() == 119 && view.shown.back() == 0);
    filter.ascending = false;
    view.Update(state, filter);
    assert(view.shown.front() == 0);
    filter.ascending = true;
    filter.sort = ReplaySort::Name;
    view.Update(state, filter);
    assert(view.shown[0] == 0 && view.shown[1] == 1 && view.shown[2] == 10);
    filter.sort = ReplaySort::Mode;
    view.Update(state, filter);
    assert(state->runs[view.shown.front()].scene == 3 && state->runs[view.shown.back()].scene == 13);
    filter.sort = ReplaySort::Seed;
    view.Update(state, filter);
    assert(state->runs[view.shown.front()].seed == -60 && state->runs[view.shown.back()].seed == 59);
    filter.sort = ReplaySort::Status;
    view.Update(state, filter);
    assert(state->runs[view.shown.front()].outcome == 1 && state->runs[view.shown.front()].valid);
    assert(state->runs[view.shown.back()].outcome == 2);
    auto updated = std::make_shared<ReplayArchiveState>(*state);
    updated->runs[0].name = "Changed name";
    filter.query = "changed";
    assert(view.Update(updated, filter) && view.shown.size() == 1 && view.shown.front() == 0);
    assert(state->runs[0].name == "Run 0");
    filter.query = "not present";
    view.Update(updated, filter);
    assert(view.shown.empty());
    assert(ReplayRecordedAt(0).empty() && !ReplayRecordedAt(epoch + day).empty());
    std::cout
        << "Replay snapshot reuse, case-insensitive notes/search, combined filters, favorites, stable sorting, changed/empty snapshots and recording dates: PASS\n";
}
