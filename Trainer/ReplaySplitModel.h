#pragma once
#include "PracticeMapData.h"
namespace practice
{
inline bool MatchingSplitLayout(const MapData &a, const MapData &b)
{
    if (!a.review || !b.review || a.scene != b.scene || a.seed != b.seed || a.hash != b.hash ||
        a.spawn != b.spawn || a.keep != b.keep || a.selected != b.selected || a.points.size() != b.points.size() ||
        a.review->category != b.review->category || a.review->rawSpawnSeed != b.review->rawSpawnSeed ||
        (a.review->category == 1 && a.review->selectedSeed != b.review->selectedSeed)) return false;
    for (auto id : a.selected)
        if (Distance(a.points[id], b.points[id]) > .001f) return false;
    return true;
}
struct PickupSplit
{
    int crystal = 0;
    double at = 0, segment = 0, reference = 0, delta = 0;
    bool comparable = false;
};
inline std::vector<PickupSplit> PickupSplits(const MapData &data, const MapData *reference)
{
    std::vector<PickupSplit> result;
    if (!data.review) return result;
    const auto &r = *data.review;
    bool prefix = reference && MatchingSplitLayout(data, *reference);
    for (size_t i = 0; i < r.collected.size() && i < r.pickupSeconds.size(); i++)
    {
        PickupSplit split;
        split.crystal = r.collected[i]; split.at = r.pickupSeconds[i];
        split.segment = split.at - (i ? r.pickupSeconds[i - 1] : 0);
        prefix = prefix && i < reference->review->collected.size() && i < reference->review->pickupSeconds.size() &&
            r.collected[i] == reference->review->collected[i];
        split.comparable = prefix;
        if (prefix) { split.reference = reference->review->pickupSeconds[i]; split.delta = split.at - split.reference; }
        result.push_back(split);
    }
    return result;
}
}
