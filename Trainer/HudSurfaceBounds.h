#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

struct HudRegion
{
    int left = 0, top = 0, right = 0, bottom = 0;

    int Width() const
    {
        return std::max(0, right - left);
    }

    int Height() const
    {
        return std::max(0, bottom - top);
    }

    uint64_t Bytes() const
    {
        return uint64_t(Width()) * Height() * 4;
    }

    bool operator==(const HudRegion &) const = default;
};

inline HudRegion ClampHudRegion(HudRegion r, int width, int height)
{
    r.left = std::clamp(r.left, 0, width);
    r.top = std::clamp(r.top, 0, height);
    r.right = std::clamp(r.right, r.left, width);
    r.bottom = std::clamp(r.bottom, r.top, height);
    return r;
}

// Clip actual primitives, not just fullscreen foreground clip rectangles.
inline HudRegion HudListRegion(const ImDrawList &list, ImVec2 origin, int width, int height)
{
    float x0 = float(width), y0 = float(height), x1 = 0, y1 = 0;
    bool found = false;
    for (const auto &cmd : list.CmdBuffer)
    {
        if (cmd.UserCallback)
            return {0, 0, width, height};
        if (cmd.ElemCount == 0)
            continue;
        float a = INFINITY, b = INFINITY, c = -INFINITY, d = -INFINITY;
        for (unsigned j = 0; j < cmd.ElemCount; j++)
        {
            if (cmd.IdxOffset + j >= unsigned(list.IdxBuffer.Size))
                return {0, 0, width, height};
            unsigned i = cmd.VtxOffset + list.IdxBuffer[cmd.IdxOffset + j];
            if (i >= unsigned(list.VtxBuffer.Size))
                return {0, 0, width, height};
            auto p = list.VtxBuffer[i].pos;
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                return {0, 0, width, height};
            a = std::min(a, p.x);
            b = std::min(b, p.y);
            c = std::max(c, p.x);
            d = std::max(d, p.y);
        }
        a = std::max(a, cmd.ClipRect.x) - origin.x;
        b = std::max(b, cmd.ClipRect.y) - origin.y;
        c = std::min(c, cmd.ClipRect.z) - origin.x;
        d = std::min(d, cmd.ClipRect.w) - origin.y;
        if (c <= a || d <= b || c < 0 || d < 0 || a >= width || b >= height)
            continue;
        found = true;
        x0 = std::min(x0, a);
        y0 = std::min(y0, b);
        x1 = std::max(x1, c);
        y1 = std::max(y1, d);
    }
    if (!found)
        return {};
    // AA margin and small tiles avoid resources changing for a one-pixel glyph.
    return ClampHudRegion({int(std::floor((x0 - 2) / 16)) * 16, int(std::floor((y0 - 2) / 16)) * 16,
                           int(std::ceil((x1 + 2) / 16)) * 16, int(std::ceil((y1 + 2) / 16)) * 16},
                          width, height);
}

inline HudRegion HudUnion(HudRegion a, HudRegion b)
{
    return {std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right),
            std::max(a.bottom, b.bottom)};
}

inline bool HudIntersects(HudRegion a, HudRegion b)
{
    return a.left <= b.right && b.left <= a.right && a.top <= b.bottom && b.top <= a.bottom;
}

inline std::vector<HudRegion> HudContentRegions(const ImDrawData &data, int width, int height)
{
    std::vector<HudRegion> regions;
    for (const auto *list : data.CmdLists)
    {
        auto r = HudListRegion(*list, data.DisplayPos, width, height);
        if (r.Width() && r.Height())
            regions.push_back(r);
    }
    for (size_t i = 0; i < regions.size(); i++)
        for (size_t j = i + 1; j < regions.size();)
        {
            if (HudIntersects(regions[i], regions[j]))
            {
                regions[i] = HudUnion(regions[i], regions[j]);
                regions.erase(regions.begin() + j);
                i = size_t(-1);
                break;
            }
            else
                j++;
        }
    if (regions.size() > 4)
    {
        auto all = regions[0];
        for (size_t i = 1; i < regions.size(); i++)
            all = HudUnion(all, regions[i]);
        regions = {all};
    }
    if (regions.empty())
        regions.push_back({0, 0, 1, 1});
    std::sort(regions.begin(), regions.end(),
              [](auto a, auto b) { return a.top == b.top ? a.left < b.left : a.top < b.top; });
    return regions;
}

inline HudRegion HudContentRegion(const ImDrawData &data, int width, int height)
{
    auto regions = HudContentRegions(data, width, height);
    auto all = regions[0];
    for (size_t i = 1; i < regions.size(); i++)
        all = HudUnion(all, regions[i]);
    return all;
}
