#pragma once
#include "PracticeMapData.h"
#include <optional>
#include <span>
namespace practice
{
inline bool MatchingSplitLayout(const MapData &a, const MapData &b)
{
    if (!a.review || !b.review || a.scene != b.scene || a.seed != b.seed || a.hash != b.hash ||
        a.spawn != b.spawn || a.spawn < 0 || a.spawn >= int(a.spawns.size()) || a.spawn >= int(b.spawns.size()) ||
        Distance(a.spawns[a.spawn], b.spawns[b.spawn]) > .001f || a.keep != b.keep || a.selected != b.selected || a.points.size() != b.points.size() ||
        a.review->category != b.review->category || a.review->rawSpawnSeed != b.review->rawSpawnSeed ||
        (a.review->category == 1 && a.review->selectedSeed != b.review->selectedSeed)) return false;
    for (auto id : a.selected)
        if (Distance(a.points[id], b.points[id]) > .001f) return false;
    return true;
}
// Index key is only a fast UI filter. Loading a choice still validates the
// complete recorded layout before any comparison is allowed.
inline uint64_t SplitLayoutKey(const MapData &map)
{
    if (!map.review || map.spawn < 0 || map.spawn >= int(map.spawns.size())) return 0;
    uint64_t hash = 14695981039346656037ull;
    auto bytes = [&](const void *data,size_t count) { auto p=static_cast<const uint8_t *>(data);for(size_t i=0;i<count;i++){hash^=p[i];hash*=1099511628211ull;} };
    auto value = [&](auto v) { bytes(&v,sizeof(v)); };
    value(map.scene);value(map.seed);value(map.spawn);value(map.keep);value(map.points.size());
    value(map.review->category);if(map.review->category==1)value(map.review->selectedSeed);value(map.review->rawSpawnSeed);
    bytes(map.hash.data(),map.hash.size());
    // Positions are checked with a tolerance by MatchingSplitLayout, not hashed.
    for(int id:map.selected)
    {
        if(id<0 || id>=int(map.points.size())) return 0;
        value(id);
    }
    return hash;
}
struct PickupSplit
{
    int crystal = 0;
    double at = 0, segment = 0, reference = 0, delta = 0;
    double referenceSegment = 0, sectorDelta = 0, bestSegment = -1;
    bool comparable = false, sectorComparable = false, gold = false;
};
inline std::vector<PickupSplit> PickupSplits(const MapData &data, const MapData *reference)
{
    std::vector<PickupSplit> result;
    if (!data.review) return result;
    const auto &r = *data.review;
    const bool matching = reference && MatchingSplitLayout(data, *reference);
    bool prefix = matching;
    for (size_t i = 0; i < r.collected.size() && i < r.pickupSeconds.size(); i++)
    {
        PickupSplit split;
        split.crystal = r.collected[i]; split.at = r.pickupSeconds[i];
        split.segment = split.at - (i ? r.pickupSeconds[i - 1] : 0);
        if (matching)
        {
            const auto &other = *reference->review;
            auto found = std::find(other.collected.begin(), other.collected.end(), split.crystal);
            if (found != other.collected.end())
            {
                size_t at = size_t(found - other.collected.begin());
                if (at < other.pickupSeconds.size())
                {
                    split.comparable = true;
                    split.reference = other.pickupSeconds[at];
                    split.delta = split.at - split.reference;
                }
            }
        }
        prefix = prefix && i < reference->review->collected.size() && i < reference->review->pickupSeconds.size() &&
                 split.crystal == reference->review->collected[i];
        if (prefix)
        {
            split.sectorComparable = true;
            split.referenceSegment = reference->review->pickupSeconds[i] - (i ? reference->review->pickupSeconds[i - 1] : 0);
            split.sectorDelta = split.segment - split.referenceSegment;
        }
        result.push_back(split);
    }
    return result;
}
// A forecast uses observed checkpoint timing, never distance or a machine-
// dependent rate. Overdue sectors raise the estimate before the next pickup.
inline std::optional<double> ProjectedFinish(const std::vector<PickupSplit> &splits,
    std::span<const int> order, std::span<const double> times, double finish, double now, bool completed = false)
{
    if (order.empty() || order.size() != times.size() || !std::isfinite(now) || now < 0 ||
        !std::isfinite(finish) || finish <= 0) return {};
    double previous = 0;
    for (double time : times)
    {
        if (!std::isfinite(time) || time < previous || time > finish) return {};
        previous = time;
    }
    size_t count = 0;
    double actual = 0;
    for (const auto &split : splits)
    {
        if (split.at > now) break;
        if (!std::isfinite(split.at) || split.at < actual || count >= order.size() || split.crystal != order[count]) return {};
        actual = split.at; count++;
    }
    if (completed) return count == order.size() ? std::optional<double>(now) : std::nullopt;
    double reference = count ? times[count - 1] : 0;
    double next = count < times.size() ? times[count] : finish;
    return std::max(finish + actual - reference, now + finish - next);
}

}
