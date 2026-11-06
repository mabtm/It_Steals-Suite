#pragma once
#include "ClipModel.h"
#include "InputUI.h"
#include "PracticeMapWorker.h"
#include "RoutePlayback.h"
#include "SplitDeltaUI.h"

namespace suite_clip
{
struct Content
{
    std::shared_ptr<const practice::MapAnalysis> analysis;
    std::shared_ptr<const practice::ReplayMotion> motion;
    std::shared_ptr<const practice::ReplayRevalidation> revalidation;
    std::shared_ptr<const practice::ReplayAnnotations> markers;
    suite_input::Preferences input;
    suite_input::Palette palette;
    ImFont *font = nullptr;
    Layout layout;
    std::shared_ptr<const practice::MapData> splitReference;
    std::shared_ptr<const std::vector<practice::PickupSplit>> splits;
    bool splitMotion = true;
    ReplayPreferences splitPresentation;
    ImFont *splitFont = nullptr;
    CameraTrack track;
    Orbit orbit;
    CameraPose freePose;
    bool freeReady = false;
};

inline std::optional<CameraPose> RecordedCamera(const Content &c, double at)
{
    if (!c.motion)
    {
        return {};
    }
    auto p = practice::ReplayViewAt(*c.motion, at);
    if (!p)
        return {};
    CameraPose pose{{p->camera.x, p->camera.y, p->camera.z},
                    {p->cameraRotation.x, p->cameraRotation.y, p->cameraRotation.z, p->cameraRotation.w},
                    p->fov};
    if (p + 1 < c.motion->frames.data() + c.motion->frames.size())
    {
        auto n = p + 1;
        double dt = n->seconds - p->seconds;
        float distance = std::sqrt(std::pow(n->body.x - p->body.x, 2) + std::pow(n->body.y - p->body.y, 2) +
                                   std::pow(n->body.z - p->body.z, 2));
        if (dt > 0 && dt <= .25 && distance <= std::max(4., dt * 40))
        {
            float u = float(std::clamp((at - p->seconds) / dt, 0., 1.));
            pose.position =
                pose.position + (Point{n->camera.x, n->camera.y, n->camera.z} - pose.position) * u;
            pose.rotation = Slerp(
                pose.rotation,
                {n->cameraRotation.x, n->cameraRotation.y, n->cameraRotation.z, n->cameraRotation.w}, u);
            pose.fov += (n->fov - pose.fov) * u;
        }
    }
    return pose;
}

inline uint32_t LayerFlags(const Options &o)
{
    return (o.colliders ? Colliders : 0u) | (o.route ? Route : 0u) | (o.monster ? Monster : 0u) |
           (o.hideCeiling ? Ceiling : 0u) | (o.fadeWalls ? FadeWalls : 0u) | (o.fullbright ? Fullbright : 0u);
}

inline void ApplyLayers(Options &o, uint32_t bits)
{
    o.colliders = bits & Colliders;
    o.route = bits & Route;
    o.monster = bits & Monster;
    o.hideCeiling = bits & Ceiling;
    o.fadeWalls = bits & FadeWalls;
    o.fullbright = bits & Fullbright;
}

struct ResolvedFrame
{
    Options options;
    std::optional<CameraPose> pose;
    size_t active = NoKey;
};

inline ResolvedFrame ResolveFrame(const Options &o, const Content &c, double at)
{
    ResolvedFrame frame{o, {}, NoKey};
    if (o.camera == 3)
    {
        auto shot = EvaluateCamera(c.track, at, [&](double t) { return RecordedCamera(c, t); });
        frame.pose = shot.pose;
        frame.active = shot.active;
        frame.options.camera = shot.mode;
        ApplyLayers(frame.options, c.track.LayersAt(at, LayerFlags(o)));
    }
    else if (o.camera == 2)
        frame.pose = c.freeReady ? std::optional<CameraPose>(c.freePose) : std::nullopt;
    else if (o.camera == 1)
    {
        auto recorded = RecordedCamera(c, at);
        if (recorded)
            frame.pose = c.orbit.Pose(recorded->position, c.freePose.fov);
    }
    else
        frame.pose = RecordedCamera(c, at);
    return frame;
}

inline std::optional<CameraPose> CameraAt(const Options &o, const Content &c, double at)
{
    return ResolveFrame(o, c, at).pose;
}

// Pending scene frames retain only their immutable source and overlay settings.
inline Content CompositionSnapshot(const Content &c)
{
    Content snapshot;
    snapshot.analysis = c.analysis;
    snapshot.motion = c.motion;
    snapshot.revalidation = c.revalidation;
    snapshot.markers = c.markers;
    snapshot.input = c.input;
    snapshot.palette = c.palette;
    snapshot.font = c.font;
    snapshot.layout = c.layout;
    snapshot.splitReference = c.splitReference;
    snapshot.splits = c.splits;
    snapshot.splitMotion = c.splitMotion;
    snapshot.splitPresentation = c.splitPresentation;
    snapshot.splitFont = c.splitFont;
    return snapshot;
}

struct Bounds
{
    float x = 0, y = 0, w = 0, h = 0;
};

inline Bounds OverlayBounds(int block, const Options &o, const Content &c)
{
    auto p = c.layout.blocks[block];
    auto nominal = block == RunInfo ? ImVec2(1280, 74) : block == Splits ? (c.splitPresentation.splitStyle == 0 ? ImVec2(480,c.splitPresentation.splitTextSize*1.35f+10) : ImVec2(320,68)) : suite_input::ModuleSize(block - 1, c.input);
    float w = std::min(float(o.width) * p.width * (block == Mouse ? c.input.mouseScale : 1), float(o.width)),
          h = w * nominal.y / nominal.x;
    if (h > o.height)
    {
        w *= o.height / h;
        h = float(o.height);
    }
    return {std::min(p.x * o.width, o.width - w), std::min(p.y * o.height, o.height - h), w, h};
}

inline bool BlockEnabled(int block, const Options &o, const Content &c)
{
    return block == RunInfo ? !o.transparent : block == Splits ?
        o.splits && c.splits && c.splitReference && !c.splits->empty() :
        o.inputs && suite_input::ModuleVisible(block - 1, c.input);
}

// Defined beside the live run-result renderer, after the application UI.
inline void (*verificationRenderer)(const Content &, double) = nullptr;

inline void Compose(ImDrawList *draw, const Options &sourceOptions, const Content &content, double seconds,
                    ImTextureID image = {}, double outro = -1)
{
    const auto options = ResolveFrame(sourceOptions, content, seconds).options;
    const auto &map = *content.analysis->data;
    const auto &run = *map.review;
    float w = float(options.width), h = float(options.height), u = w / 1280.f;
    ImFont *font = content.font ? content.font : ImGui::GetFont();
    auto ink = content.palette.ink, accent = content.palette.accent;
    auto text = [&](float x, float y, float size, ImU32 color, const std::string &value) {
        draw->AddText(font, size * u, {x * u, y * u}, color, value.c_str());
    };
    if (!options.transparent)
        draw->AddRectFilled({0, 0}, {w, h}, IM_COL32(10, 16, 23, 255));
    if (!options.transparent && options.scene3d && image)
        draw->AddImage(image, {0, 0}, {w, h});
    float inputHeight = options.inputs ? h * .32f : 0;
    if (!options.transparent && !options.scene3d)
    {
        ImVec2 lo{18 * u, 94 * u}, hi{w - 18 * u, h - 18 * u - inputHeight};
        float scale = std::min((hi.x - lo.x - 40 * u) / std::max(1.f, map.maxX - map.minX),
                               (hi.y - lo.y - 40 * u) / std::max(1.f, map.maxZ - map.minZ));
        scale = std::max(.01f, scale);
        ImVec2 center{(lo.x + hi.x) * .5f, (lo.y + hi.y) * .5f};
        auto project = [&](practice::Vec3 p) {
            return ImVec2(center.x + (p.x - (map.minX + map.maxX) * .5f) * scale,
                          center.y - (p.z - (map.minZ + map.maxZ) * .5f) * scale);
        };
        draw->AddRectFilled(lo, hi, IM_COL32(15, 25, 32, 255), 10 * u);
        draw->PushClipRect(lo, hi, true);
        if (content.analysis->floor)
            for (const auto &tri : *content.analysis->floor)
                draw->AddTriangleFilled(project(tri[0]), project(tri[1]), project(tri[2]),
                                        IM_COL32(31, 46, 54, 255));
        if (content.analysis->outline)
            for (const auto &e : *content.analysis->outline)
                draw->AddLine(project(e.first), project(e.second), IM_COL32(110, 140, 151, 255), 1.3f * u);
        const auto &walls =
            IsHideSeekScene(map.scene) && !map.obstacles.empty() ? map.obstacles : map.wallOutlines;
        for (const auto &wall : walls)
        {
            if (IsHideSeekScene(map.scene) && wall.approximate)
                continue;
            ImVec2 points[4];
            for (int i = 0; i < 4; i++)
                points[i] = project(wall.corners[i]);
            draw->AddPolyline(points, 4, IM_COL32(125, 155, 164, 230), ImDrawFlags_Closed, 1.5f * u);
        }
        if (walls.empty() && !IsHideSeekScene(map.scene))
            for (const auto &wall : map.walls)
                draw->AddRect(project({wall.minX, 0, wall.maxZ}), project({wall.maxX, 0, wall.minZ}),
                              IM_COL32(125, 155, 164, 230), 0, 0, 1.5f * u);
        for (size_t i = 0; i < map.selected.size(); i++)
        {
            int id = map.selected[i];
            if (id < 0 || id >= int(map.points.size()))
                continue;
            auto picked = std::find(run.collected.begin(), run.collected.end(), id);
            if (picked != run.collected.end())
            {
                auto ix = size_t(picked - run.collected.begin());
                if (ix < run.pickupSeconds.size() && run.pickupSeconds[ix] <= seconds)
                    continue;
            }
            auto p = project(map.points[id]);
            draw->AddQuadFilled({p.x, p.y - 4 * u}, {p.x + 4 * u, p.y}, {p.x, p.y + 4 * u},
                                {p.x - 4 * u, p.y}, IM_COL32(255, 194, 91, 255));
        }
        if (map.spawn >= 0 && map.spawn < int(map.spawns.size()))
            draw->AddCircle(project(map.spawns[map.spawn]), 7 * u, accent, 0, 2 * u);
        auto player = practice::PlayedSlice(run, seconds, 2.5);
        if (run.trace.empty() || seconds < run.trace.front().seconds)
            player.visible = false;
        if (player.visible)
        {
            if (options.route)
                for (size_t i = 1; i < player.trail.size(); i++)
                    draw->AddLine(project(player.trail[i - 1]), project(player.trail[i]),
                                  IM_COL32(231, 124, 221, 255), 3 * u);
            auto p = project(player.marker);
            draw->AddCircleFilled(p, 7 * u, IM_COL32(8, 15, 21, 255));
            draw->AddCircleFilled(p, 4.5f * u, IM_COL32(231, 124, 221, 255));
        }
        if (options.monster)
            for (const auto &enemy : practice::MonstersAt(run, seconds, true))
            {
                if (enemy.path)
                    for (size_t i = 1; i < enemy.path->size(); i++)
                    {
                        auto a = project((*enemy.path)[i - 1]), b = project((*enemy.path)[i]);
                        float length = std::hypot(b.x - a.x, b.y - a.y);
                        if (length <= .01f)
                            continue;
                        float stride = std::max(10 * u, length / 128);
                        for (float at = float(std::fmod(seconds * 14 * u, stride)) - stride; at < length;
                             at += stride)
                        {
                            float first = std::max(0.f, at), last = std::min(length, at + stride * .5f);
                            if (last > first)
                                draw->AddLine(
                                    {a.x + (b.x - a.x) * first / length, a.y + (b.y - a.y) * first / length},
                                    {a.x + (b.x - a.x) * last / length, a.y + (b.y - a.y) * last / length},
                                    IM_COL32(248, 113, 93, 190), u);
                        }
                    }
                if (enemy.pose.visible)
                    draw->AddCircleFilled(project(enemy.pose.marker), 5 * u, IM_COL32(248, 113, 93, 255));
            }
        auto flash = std::lower_bound(run.flashes.begin(), run.flashes.end(), seconds - .75,
                                      [](const auto &f, double t) { return f.seconds < t; });
        for (; flash != run.flashes.end() && flash->seconds <= seconds; ++flash)
        {
            float age = float(seconds - flash->seconds);
            draw->AddCircle(project(flash->player), (10 + age * 32) * u,
                            IM_COL32(255, 255, 255, int(255 * std::pow(1 - age / .75f, 2))), 0, 2 * u);
        }
        draw->PopClipRect();
    }
    if (!options.transparent)
    {
        auto r = OverlayBounds(RunInfo, options, content);
        float ru = r.w / 1280.f;
        float opacity = content.layout.blocks[RunInfo].opacity;
        auto label = [&](float x, float y, float size, ImU32 color, const std::string &value) {
            draw->AddText(font, size * ru, {r.x + x * ru, r.y + y * ru}, suite_input::Fade(color, opacity),
                          value.c_str());
        };
        draw->AddRectFilled({r.x, r.y}, {r.x + r.w, r.y + r.h},
                            suite_input::Fade(content.palette.background, opacity), 8 * ru);
        bool aborted = practice::ReplayAborted(run.outcome);
        std::string status = aborted                  ? "ABORTED"
                             : !run.validationChecked ? "UNKNOWN"
                             : run.valid              ? "VALID"
                                                      : "UNVALID";
        ImU32 color = aborted                  ? IM_COL32(255, 181, 79, 255)
                      : !run.validationChecked ? IM_COL32(179, 193, 202, 255)
                      : run.valid              ? IM_COL32(103, 237, 143, 255)
                                               : IM_COL32(255, 98, 103, 255);
        int mode = MapLevelIndex(map.scene);
        label(20, 12, 20, accent, "REPLAY  /  " + std::string(mode >= 0 ? kMapLevelNames[mode] : "Unknown"));
        label(20, 43, 15, content.palette.muted, std::string("CATEGORY / ") + (run.category == 1 ? "Set-Seed" : run.category == 2 ? "Practice" : "Random Seed") + "  /  Elite-Suite " + suite_brand::Version);
        label(1070, 14, 22, color, status);
        if (options.time)
            label(480, 12, 28, ink, FormatRunTime(seconds));
        if (options.seed)
            label(700, 43, 17, ink, "Seed " + std::to_string(map.seed));
        if (options.speed)
        {
            float speed = -1;
            if (content.motion)
            {
                auto p = practice::ReplayAt(content.motion->ticks, seconds);
                if (p && p != content.motion->ticks.data())
                {
                    auto prior = p - 1;
                    double dt = p->seconds - prior->seconds;
                    if (dt > 0 && dt <= .25)
                    {
                        float dx = p->position.x - prior->position.x, dz = p->position.z - prior->position.z;
                        speed = float(std::hypot(dx, dz) / dt);
                        if (speed > 40)
                            speed = -1;
                    }
                }
            }
            char value[48];
            if (speed >= 0)
                std::snprintf(value, sizeof(value), "%.2f u/s", speed);
            else
                std::snprintf(value, sizeof(value), "Speed --");
            label(700, 12, 24, ink, value);
        }
    }
    if (BlockEnabled(Splits, options, content))
    {
        auto bounds = OverlayBounds(Splits, options, content);
        auto end = std::upper_bound(content.splits->begin(),content.splits->end(),seconds,
            [](double time,const auto &row) { return time < row.at; });
        if (end != content.splits->begin())
        {
            auto style = content.splitPresentation;
            style.splitPace = style.splitTheory = false;
            style.splitOpacity *= content.layout.blocks[Splits].opacity;
            if (style.splitStyle == 0)
                suite_splits::DrawDeltaGlyphs(draw,content.splitFont ? content.splitFont : font,
                    {bounds.x,bounds.y,bounds.w,bounds.h},*std::prev(end),style,content.splitMotion,seconds-std::prev(end)->at);
            else suite_splits::Draw(draw,font,{bounds.x,bounds.y},bounds.w,bounds.h,*content.splits,seconds,
                    content.splitReference->review->validationChecked && content.splitReference->review->valid,
                    content.splitMotion,content.layout.blocks[Splits].opacity,true);
        }
    }
    if (options.inputs)
    {
        auto f = content.motion ? suite_input::ReplayFrame(*content.motion, seconds) : std::nullopt;
        auto changes = content.motion ? suite_input::ReplayChanges(*content.motion, seconds)
                                      : std::vector<suite_input::Change>{};
        suite_input::Glow glow = content.motion && content.input.animations
                                     ? suite_input::ReplayGlow(*content.motion, seconds)
                                     : suite_input::Glow{};
        for (int block = Keyboard; block < Splits; block++)
            if (BlockEnabled(block, options, content))
            {
                auto r = OverlayBounds(block, options, content);
                auto prefs = content.input;
                prefs.contentOpacity *= content.layout.blocks[block].opacity;
                suite_input::Module(draw, block - 1, {r.x, r.y}, {r.x + r.w, r.y + r.h}, f ? &*f : nullptr,
                                    prefs, changes, seconds, font, content.palette, &glow);
            }
    }
    if (!options.transparent && options.events && content.markers)
    {
        const practice::ReplayMarker *latest = nullptr;
        for (const auto &marker : content.markers->markers)
            if (marker.seconds <= seconds && (!latest || marker.seconds > latest->seconds))
                latest = &marker;
        if (latest && seconds - latest->seconds < 3)
            text(24, options.scene3d ? 92 : 102, 18, IM_COL32(255, 190, 105, 255), latest->name);
    }
    (void)outro;
}

// The same 2D composition can be drawn directly into a preview viewport.
// Transform only commands/vertices added here; parent clipping and indices stay
// intact.
inline void ComposeViewport(ImDrawList *draw, const Options &o, const Content &c, double seconds,
                            ImVec2 origin, ImVec2 size, double outro = -1)
{
    ImVec4 parent =
        draw->_ClipRectStack.empty() ? ImVec4(0, 0, FLT_MAX, FLT_MAX) : draw->_ClipRectStack.back();
    float scale = std::min(size.x / o.width, size.y / o.height);
    draw->PushClipRect({0, 0}, {float(o.width), float(o.height)}, false);
    draw->AddDrawCmd(); // Isolate an existing parent command even when its clip rectangle matches.
    int firstCommand = draw->CmdBuffer.Size - 1, firstVertex = draw->VtxBuffer.Size;
    Compose(draw, o, c, seconds, {}, outro);
    for (int i = firstVertex; i < draw->VtxBuffer.Size; i++)
    {
        auto &p = draw->VtxBuffer[i].pos;
        p = {origin.x + p.x * scale, origin.y + p.y * scale};
    }
    for (int i = firstCommand; i < draw->CmdBuffer.Size; i++)
    {
        auto &clip = draw->CmdBuffer[i].ClipRect;
        clip = {std::max(parent.x, origin.x + clip.x * scale), std::max(parent.y, origin.y + clip.y * scale),
                std::min(parent.z, origin.x + clip.z * scale), std::min(parent.w, origin.y + clip.w * scale)};
    }
    draw->PopClipRect();
}
} // namespace suite_clip
