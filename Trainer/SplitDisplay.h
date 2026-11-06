#pragma once
#include "ReplaySplitModel.h"
#include "SharedProtocol.h"
#include "imgui.h"
#include <cstdio>
#include <array>
#include <cstring>

namespace suite_splits
{
inline const std::vector<practice::PickupSplit> &Live(const GameData &data)
{
    struct Cache
    {
        int64_t run = 0, reference = 0;
        int count = -1, referenceCount = -1, pid = 0;
        bool history = false, eligible = false;
        std::vector<practice::PickupSplit> rows;
    };
    static Cache cache;
    int count = std::clamp(int(data.splitPickupCount), 0, 64), referenceCount = std::clamp(int(data.splitReferenceCount), 0, 64);
    if (cache.run == data.splitRunId && cache.reference == data.splitReferenceId && cache.count == count &&
        cache.referenceCount == referenceCount && cache.pid == data.pid &&
        cache.history == bool(data.splitHistoryReady) && cache.eligible == (data.runValidity == 1)) return cache.rows;
    cache.run = data.splitRunId; cache.reference = data.splitReferenceId; cache.count = count;
    cache.referenceCount = referenceCount; cache.pid = data.pid; cache.history = data.splitHistoryReady != 0;
    cache.eligible = data.runValidity == 1; cache.rows.clear();
    bool prefix = data.splitReferenceId > 0;
    for (int i = 0; i < count; i++)
    {
        practice::PickupSplit row;
        row.crystal = data.splitPickupIds[i]; row.at = data.splitPickupSeconds[i];
        if (!std::isfinite(row.at) || row.at < 0 || (!cache.rows.empty() && row.at < cache.rows.back().at)) break;
        row.segment = row.at - (i ? data.splitPickupSeconds[i - 1] : 0);
        for (int j = 0; j < referenceCount; j++)
            if (data.splitReferenceIds[j] == row.crystal && std::isfinite(data.splitReferenceSeconds[j]) && data.splitReferenceSeconds[j] >= 0)
            {
                row.comparable = true; row.reference = data.splitReferenceSeconds[j]; row.delta = row.at - row.reference; break;
            }
        prefix = prefix && i < referenceCount && data.splitReferenceIds[i] == row.crystal;
        if (prefix)
        {
            row.sectorComparable = true;
            row.referenceSegment = data.splitReferenceSeconds[i] - (i ? data.splitReferenceSeconds[i - 1] : 0);
            row.sectorDelta = row.segment - row.referenceSegment;
        }
        if (cache.history)
        {
            double best = data.splitSectorBest[i];
            if (std::isfinite(best) && best >= 0) row.bestSegment = best;
            row.gold = cache.eligible && row.segment >= 0 && (row.bestSegment < 0 || row.segment < row.bestSegment - .0005);
        }
        cache.rows.push_back(row);
    }
    return cache.rows;
}

inline bool Completed(const GameData &data) { return data.timerState == 2 && data.runOutcome == 1; }
inline std::optional<double> Pace(const GameData &data, const std::vector<practice::PickupSplit> &rows, double now)
{
    if (Completed(data)) return std::isfinite(now) && now >= 0 ? std::optional<double>(now) : std::nullopt;
    int count = std::clamp(int(data.splitReferenceCount), 0, 64);
    if (!data.splitReferenceComplete || data.splitReferenceId <= 0) return {};
    // IPC is packed byte-for-byte. Copy into aligned storage before giving
    // standard-library spans/references to its numeric checkpoint arrays.
    std::array<int, 64> order{};
    std::array<double, 64> times{};
    std::memcpy(order.data(), data.splitReferenceIds, size_t(count) * sizeof(int));
    std::memcpy(times.data(), data.splitReferenceSeconds, size_t(count) * sizeof(double));
    return practice::ProjectedFinish(rows, std::span<const int>(order.data(), size_t(count)),
        std::span<const double>(times.data(), size_t(count)), data.splitReferenceFinish, now);
}
inline std::optional<double> Theoretical(const GameData &data, const std::vector<practice::PickupSplit> &rows)
{
    bool known = data.splitHistoryReady && std::isfinite(data.splitTheoreticalBest) && data.splitTheoreticalBest >= 0;
    bool eligible = data.runValidity == 1;
    if (data.splitHistoryReady && eligible && Completed(data) && !rows.empty() && int(rows.size()) == data.splitExpectedPickups)
    {
        double total = 0;
        for (const auto &row : rows) total += row.bestSegment >= 0 ? std::min(row.segment, row.bestSegment) : row.segment;
        double exit = std::max(0., data.currentRunTimeSeconds - rows.back().at);
        total += data.splitHistoryReady && data.splitBestExit >= 0 ? std::min(exit, double(data.splitBestExit)) : exit;
        return std::isfinite(total) && total >= 0 ? std::optional<double>(total) : std::nullopt;
    }
    if (!known) return {};
    double total = data.splitTheoreticalBest;
    int count = std::clamp(int(data.splitReferenceCount), 0, 64);
    for (size_t i = 0; i < rows.size(); i++)
    {
        if (i >= size_t(count) || rows[i].crystal != data.splitReferenceIds[i]) return {};
        if (eligible && rows[i].bestSegment >= 0) total += std::min(0., rows[i].segment - rows[i].bestSegment);
    }
    return std::isfinite(total) && total >= 0 ? std::optional<double>(total) : std::nullopt;
}

// Uses replay time, never wall-clock animation state: seeks, slow motion and
// exported frames at the same timestamp produce the same overlay.
inline void Draw(ImDrawList *draw, ImFont *font, ImVec2 at, float width, float height,
                 const std::vector<practice::PickupSplit> &splits, double seconds,
                 bool verifiedReference, bool motion, float opacity = 1)
{
    float u = width / 320.f;
    auto color = [&](int r, int g, int b, float alpha = 1.f) {
        return IM_COL32(r, g, b, int(255 * std::clamp(alpha * opacity, 0.f, 1.f)));
    };
    auto text = [&](float x, float y, float size, ImU32 ink, const char *value) {
        ImVec4 clip(at.x + 8 * u, at.y, at.x + width - 8 * u, at.y + height);
        draw->AddText(font, size * u, {at.x + x * u, at.y + y * u}, ink, value, nullptr, 0, &clip);
    };
    size_t end = size_t(std::upper_bound(splits.begin(), splits.end(), seconds,
        [](double time, const auto &split) { return time < split.at; }) - splits.begin());
    size_t rows = std::min<size_t>(4, size_t(std::max(0.f, (height / u - 36) / 26)));
    size_t begin = end > rows ? end - rows : 0;
    draw->AddRectFilled(at, {at.x + width, at.y + height}, color(13, 22, 30, .91f), 5 * u);
    draw->AddRectFilled(at, {at.x + 3 * u, at.y + height}, color(111, 209, 255, .8f), 2 * u);
    text(13, 9, 13, color(131, 211, 249), verifiedReference ? "SET-SEED SPLITS / PB" : "SET-SEED SPLITS / REPLAY");
    if (!end) text(13, 35, 14, color(172, 187, 198), "Ready for the first crystal");
    for (size_t i = begin; i < end; i++)
    {
        const auto &split = splits[i];
        float progress = motion ? float(std::clamp((seconds - split.at) / .22, 0., 1.)) : 1;
        float ease = 1 - (1 - progress) * (1 - progress) * (1 - progress);
        float y = 35 + float(i - begin) * 26 + (1 - ease) * 7;
        char label[48], delta[48];
        std::snprintf(label, sizeof(label), "Crystal %zu", i + 1);
        std::snprintf(delta, sizeof(delta), split.comparable ? "%+.3f s" : "--", split.delta);
        ImU32 ink = !split.comparable ? color(172, 187, 198, ease)
                     : split.delta < -.0005 ? color(107, 235, 162, ease)
                     : split.delta > .0005 ? color(255, 112, 124, ease) : color(209, 221, 230, ease);
        text(13, y, 15, color(223, 233, 242, ease), label);
        float extent = font->CalcTextSizeA(16 * u, FLT_MAX, 0, delta).x / u;
        text(307 - extent, y - 1, 16, ink, delta);
    }
}
}
