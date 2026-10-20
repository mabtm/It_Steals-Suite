#pragma once
#include "ClipComposition.h"
#include "SuiteBranding.h"

namespace suite_clip
{
inline ImU32 GuideColor(int mode)
{
    return mode == 0   ? IM_COL32(94, 231, 209, 235)
           : mode == 1 ? IM_COL32(255, 193, 104, 235)
                       : IM_COL32(190, 155, 255, 235);
}

struct GuideLine
{
    Point a, b;
    int mode = 2;
    bool cut = false;
};

struct GuideAnchor
{
    CameraPose pose;
    double seconds = 0;
    int mode = 2;
    size_t index = 0;
};

struct CameraGuideCache
{
    uint64_t revision = ~uint64_t(0);
    const void *motion = nullptr, *analysis = nullptr;
    std::vector<GuideLine> lines;
    std::vector<GuideAnchor> anchors;

    void Bind(const Content &content, uint64_t value)
    {
        if (revision == value && motion == content.motion.get() && analysis == content.analysis.get())
            return;
        revision = value;
        motion = content.motion.get();
        analysis = content.analysis.get();
        lines.clear();
        anchors.clear();
        const auto &keys = content.track.keys;
        auto poseAt = [&](double at) {
            return EvaluateCamera(content.track, at, [&](double t) { return RecordedCamera(content, t); })
                .pose;
        };
        auto section = [&](double begin, double end, int mode) {
            if (end <= begin)
                return;
            auto previous = poseAt(begin);
            for (int i = 1; i <= 16; i++)
            {
                auto next = poseAt(begin + (end - begin) * i / 16);
                if (previous && next)
                    lines.push_back({previous->position, next->position, mode, false});
                previous = next;
            }
        };
        if (!keys.empty() && keys.front().seconds > 0)
        {
            double before = std::nextafter(keys.front().seconds, 0.);
            section(0, before, 0);
            auto a = poseAt(before), b = poseAt(keys.front().seconds);
            if (a && b)
                lines.push_back({a->position, b->position, 0, true});
        }
        for (size_t i = 0; i < keys.size(); i++)
        {
            auto pose = poseAt(keys[i].seconds);
            if (pose)
                anchors.push_back({*pose, keys[i].seconds, keys[i].mode, i});
            if (i + 1 == keys.size())
                continue;
            double span = keys[i + 1].seconds - keys[i].seconds;
            int steps = std::clamp(int(512 / std::max(size_t(1), keys.size() - 1)), 2, 16);
            auto previous = pose;
            for (int j = 1; j <= steps; j++)
            {
                double t = keys[i].seconds + span * j / steps;
                if (j == steps && keys[i].next == Transition::Cut)
                    t = std::nextafter(keys[i + 1].seconds, keys[i].seconds);
                auto next = poseAt(t);
                if (previous && next)
                    lines.push_back({previous->position, next->position, keys[i].mode, false});
                previous = next;
            }
            if (keys[i].next == Transition::Cut)
            {
                auto next = poseAt(keys[i + 1].seconds);
                if (previous && next)
                    lines.push_back({previous->position, next->position, keys[i].mode, true});
            }
        }
        if (!keys.empty() && keys.back().mode != 2 && content.analysis && content.analysis->data->review)
            section(keys.back().seconds, content.analysis->data->review->seconds, keys.back().mode);
    }
};

inline std::optional<ImVec2> ProjectGuide(Point p, CameraPose camera, ImVec2 origin, ImVec2 size)
{
    auto right = Right(camera.rotation), forward = Forward(camera.rotation);
    Point up{forward.y * right.z - forward.z * right.y, forward.z * right.x - forward.x * right.z,
             forward.x * right.y - forward.y * right.x};
    auto dot = [](Point a, Point b) { return a.x * b.x + a.y * b.y + a.z * b.z; };
    Point relative = p - camera.position;
    float z = dot(relative, forward);
    if (z <= .05f || size.x <= 0 || size.y <= 0)
        return {};
    float focal = size.y * .5f / std::tan(camera.fov * .00872664626f);
    ImVec2 screen{origin.x + size.x * .5f + dot(relative, right) * focal / z,
                  origin.y + size.y * .5f - dot(relative, up) * focal / z};
    if (!std::isfinite(screen.x) || !std::isfinite(screen.y) || std::abs(screen.x - origin.x) > size.x * 10 ||
        std::abs(screen.y - origin.y) > size.y * 10)
        return {};
    return screen;
}

inline void GuideSegment(ImDrawList *draw, ImVec2 a, ImVec2 b, ImU32 color, bool dashed)
{
    if (!dashed)
    {
        draw->AddLine(a, b, color, 2);
        return;
    }
    float length = std::hypot(b.x - a.x, b.y - a.y);
    int steps = std::clamp(int(length / 12), 1, 128);
    for (int i = 0; i < steps; i++)
    {
        float u = float(i) / steps, v = (i + .5f) / steps;
        draw->AddLine({a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u},
                      {a.x + (b.x - a.x) * v, a.y + (b.y - a.y) * v}, color, 1.4f);
    }
}

inline void WorldCameraGuides(ImDrawList *draw, const CameraGuideCache &cache, CameraPose camera,
                              ImVec2 origin, ImVec2 size)
{
    draw->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
    for (const auto &line : cache.lines)
    {
        auto a = ProjectGuide(line.a, camera, origin, size), b = ProjectGuide(line.b, camera, origin, size);
        if (a && b)
            GuideSegment(draw, *a, *b, GuideColor(line.mode), line.cut);
    }
    for (const auto &anchor : cache.anchors)
    {
        auto p = ProjectGuide(anchor.pose.position, camera, origin, size);
        auto f = ProjectGuide(anchor.pose.position + Forward(anchor.pose.rotation) * 2, camera, origin, size);
        if (!p)
            continue;
        if (f)
            draw->AddLine(*p, *f, GuideColor(anchor.mode), 2);
        draw->AddCircleFilled(*p, 5, GuideColor(anchor.mode));
        draw->AddText({p->x + 8, p->y - 9}, IM_COL32(255, 255, 255, 255), ShotLabel(anchor.index).c_str());
    }
    draw->PopClipRect();
}

inline std::optional<size_t> CameraPathOverview(const CameraGuideCache &cache, const Content &content,
                                                double at, bool &open)
{
    std::optional<size_t> selected;
    auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x * .38f, display.y * .47f}, ImGuiCond_Appearing, {.5f, .5f});
    ImGui::SetNextWindowSize({std::min(760.f, display.x - 24), std::min(600.f, display.y - 24)},
                             ImGuiCond_Appearing);
    std::string title = std::string("Camera path / ") + suite_brand::Version + " / mab";
    if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings))
    {
        ImGui::TextColored({.37f, .91f, .82f, 1}, "Recorded");
        ImGui::SameLine();
        ImGui::TextColored({1, .76f, .41f, 1}, "Follow");
        ImGui::SameLine();
        ImGui::TextColored({.75f, .61f, 1, 1}, "Free");
        ImGui::SameLine();
        ImGui::TextDisabled("/ dashed: cut / arrows: look direction");
        ImVec2 origin = ImGui::GetCursorScreenPos();
        ImVec2 size{std::max(60.f, ImGui::GetContentRegionAvail().x),
                    std::max(60.f, ImGui::GetContentRegionAvail().y - 28)};
        ImGui::InvisibleButton("##CameraPathCanvas", size);
        bool hovered = ImGui::IsItemHovered();
        const auto &map = *content.analysis->data;
        float minX = map.minX, maxX = map.maxX, minZ = map.minZ, maxZ = map.maxZ;
        for (const auto &anchor : cache.anchors)
        {
            minX = std::min(minX, anchor.pose.position.x - 2);
            maxX = std::max(maxX, anchor.pose.position.x + 2);
            minZ = std::min(minZ, anchor.pose.position.z - 2);
            maxZ = std::max(maxZ, anchor.pose.position.z + 2);
        }
        float scale =
            std::min((size.x - 45) / std::max(1.f, maxX - minX), (size.y - 45) / std::max(1.f, maxZ - minZ));
        auto project = [&](Point p) {
            return ImVec2{origin.x + size.x * .5f + (p.x - (minX + maxX) * .5f) * scale,
                          origin.y + size.y * .5f - (p.z - (minZ + maxZ) * .5f) * scale};
        };
        auto *draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(12, 23, 31, 255), 7);
        draw->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
        for (auto wall : map.walls)
            draw->AddRectFilled(project({wall.minX, 0, wall.maxZ}), project({wall.maxX, 0, wall.minZ}),
                                IM_COL32(50, 68, 78, 255), 2);
        for (const auto &line : cache.lines)
            GuideSegment(draw, project(line.a), project(line.b), GuideColor(line.mode), line.cut);
        for (const auto &anchor : cache.anchors)
        {
            auto p = project(anchor.pose.position);
            auto forward = Forward(anchor.pose.rotation);
            float length = std::hypot(forward.x, forward.z);
            if (length > .01f)
            {
                ImVec2 tip{p.x + forward.x / length * 27, p.y - forward.z / length * 27};
                draw->AddLine(p, tip, IM_COL32(246, 250, 255, 255), 1.7f);
                ImVec2 side{forward.z / length * 4, forward.x / length * 4};
                draw->AddTriangleFilled(
                    tip, {tip.x - forward.x / length * 7 + side.x, tip.y + forward.z / length * 7 + side.y},
                    {tip.x - forward.x / length * 7 - side.x, tip.y + forward.z / length * 7 - side.y},
                    IM_COL32(246, 250, 255, 255));
            }
            draw->AddCircleFilled(p, 10, IM_COL32(12, 23, 31, 255));
            draw->AddCircle(p, 9, GuideColor(anchor.mode), 0, 2);
            auto label = ShotLabel(anchor.index);
            auto extent = ImGui::CalcTextSize(label.c_str());
            draw->AddText({p.x - extent.x * .5f, p.y - extent.y * .5f}, GuideColor(anchor.mode),
                          label.c_str());
            if (hovered && std::hypot(ImGui::GetIO().MousePos.x - p.x, ImGui::GetIO().MousePos.y - p.y) < 14)
            {
                float pitch = std::asin(std::clamp(-forward.y, -1.f, 1.f)) * 57.2957795f;
                ImGui::SetTooltip("Shot %s / %s / %.6f s\nHeight %.2f / pitch %.1f", label.c_str(),
                                  ShotModeNames[anchor.mode], anchor.seconds, anchor.pose.position.y, pitch);
                if (ImGui::IsMouseClicked(0))
                    selected = anchor.index;
            }
        }
        auto current =
            EvaluateCamera(content.track, at, [&](double t) { return RecordedCamera(content, t); }).pose;
        if (current)
            draw->AddCircle(project(current->position), 14, IM_COL32(255, 255, 255, 255), 0, 1.6f);
        draw->PopClipRect();
        ImGui::TextDisabled("Top view / click a point to edit / guides are excluded from export");
    }
    ImGui::End();
    return selected;
}
} // namespace suite_clip
