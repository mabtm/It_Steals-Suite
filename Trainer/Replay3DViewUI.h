#pragma once
#include "ReplayTimelineUI.h"
#include "ReplayExperienceUI.h"

inline bool RenderReplay3DSettings(const practice::ReplayMotion *motion)
{
    using namespace suite_replay;
    bool changed = false;
    changed |= suite_ui::Checkbox("Simplified graphics", &simplified);
    Help(
        "Off uses original game materials, textures, lighting, shadows and camera effects. On uses the previous analysis style. Switching reloads the replay scene. New visual recordings include model poses and sprite/light states; older replays have limited animation data.");
    changed |= suite_ui::Checkbox("Fullbright", &fullbright);
    Help(
        "Unlit texture view: evenly visible surfaces without flashlight glare or baked darkness. Original triplanar wall/floor projection is preserved where available. Transparent effects keep their original shader if no matching unlit variant exists. Replay view only; run validity is unchanged.");
    if (motion && motion->visualsRecorded)
    {
        ImGui::TextDisabled("Visual tracks: %d / samples: %d", motion->visualTracks, motion->visualSamples);
        if (motion->visualDropped)
            ImGui::TextDisabled("Visual capture reduced / %d observations omitted", motion->visualDropped);
    }
    else
        ImGui::TextDisabled("Legacy replay / model animation not recorded");
    bool hasAudio = motion && motion->experience.audio.recorded && !motion->audioFile.empty();
    ImGui::BeginDisabled(!hasAudio);
    changed |= suite_ui::Checkbox("Recorded game audio", &audio);
    if (hasAudio && audio)
        changed |= ImGui::SliderFloat("Audio volume", &audioVolume, 0, 1, "%.2f");
    ImGui::EndDisabled();
    Help(
        "Recorded game mix from the original listening perspective; free camera position does not change it. Playback speed changes audio pitch. Older replays have no audio channel.");
    bool hasProbes = motion && !motion->experience.probes.empty();
    ImGui::BeginDisabled(!hasProbes);
    changed |= suite_ui::Checkbox("Show recorded game queries", &perception);
    ImGui::EndDisabled();
    Help(
        "Actual recorded sight/light queries. Red: known target hit; blue: other hit; green: no hit. Infinite no-hit rays are capped at 64 units for display. Geometry samples are limited to 10 Hz; state changes remain separate.");
    const char *cameras[] = {"Recorded camera", "Following camera", "Free camera"};
    changed |= ImGui::Combo("Camera", &camera, cameras, 3);
    g_replayObserver = camera == 2;
    Help(
        "Recorded reproduces camera pose and FOV. Following: close controls with Tab, hold right mouse to rotate horizontally or vertically. Scroll up to zoom in, down to zoom out. Backspace aligns once with the recorded view. Orbit and zoom survive seeking and loops. Free: WASD / Q / E, Shift and right mouse drag.");
    if (camera == 1)
        ImGui::TextDisabled("RMB orbit / Mouse wheel zoom");
    changed |= suite_ui::Checkbox("Hide ceiling", &hideCeiling);
    Help(
        "Hide ceiling material sections in this replay view. Original mesh/batch ranges and colliders remain intact; leaving 3D restores the scene.");
    ImGui::BeginDisabled(camera != 1);
    changed |= suite_ui::Checkbox("Fade blocking walls", &fadeWalls);
    ImGui::EndDisabled();
    Help(
        "Following camera: actual solid collider hits between camera and player fade linked wall/door renderers. Checked at up to 20 Hz with a short fade. Works while paused. This affects only the replay view.");
    changed |= suite_ui::Checkbox("Speed trail", &trail);
    Help(
        "Wide movement ribbon with a 0-10 u/s gradient: dark red, red, amber, turquoise. Speeds above 10 stay turquoise. Uses measured positions and timestamps; gaps and teleports are not connected.");
    if (trail)
        changed |= ImGui::SliderFloat("Trail length", &tail, .25f, 10, "%.2f s");
    ImGui::Separator();
    ImGui::TextColored(g_headerCol, "ACTUAL COLLIDERS");
    bool recorded = motion && motion->colliders.recorded;
    int counts[3]{};
    if (recorded)
        for (const auto &track : motion->colliders.tracks)
            if (track.group >= 1 && track.group <= 3)
                counts[track.group - 1]++;
    const char *labels[] = {"Player collider", "Monster colliders", "Crystal colliders"};
    bool *toggles[] = {&player, &monster, &crystal};
    for (int group = 0; group < 3; group++)
    {
        ImGui::BeginDisabled(!recorded || counts[group] == 0);
        changed |= suite_ui::Checkbox(labels[group], toggles[group]);
        ImGui::EndDisabled();
        if (recorded && counts[group] == 0)
            Help(
                "No actual collider history for this group in this recording. A new run records supported colliders; missing historical shapes are never substituted with approximate boxes.");
    }
    if (recorded)
    {
        ImGui::TextDisabled("Player %d / Monster %d / Crystal %d", counts[0], counts[1], counts[2]);
        Help(
            (std::to_string(motion->colliders.count) +
             " samples. Actual primitive shapes, dimensions, transforms and enabled/trigger states captured during this run. Observations are held until the next sample. Player: physics ticks; other sources: up to 10 Hz, with bounded work. Unsupported mesh/terrain shapes are omitted.")
                .c_str());
        if (motion->colliders.flags)
        {
            ImGui::TextColored(g_accentCol, "Collider capture has omissions");
            Help(("Flags: " + std::to_string(motion->colliders.flags) +
                  " / omitted: " + std::to_string(motion->colliders.dropped))
                     .c_str());
        }
    }
    else
    {
        ImGui::TextDisabled("Legacy recording: no moving colliders");
        Help(
            "Input and camera history still work. Moving collider history cannot be reconstructed from an older recording.");
    }
    changed |= suite_ui::Checkbox("Loaded level colliders", &level);
    Help(
        "Actual supported solid primitive colliders from the loaded level, up to 256 nearby shapes per frame. This is a static scene inspection, not a recorded history of doors or other world changes. Mesh/terrain shapes are omitted; no approximate boxes are substituted.");
    changed |= suite_ui::Checkbox("Colliders through walls", &xray);
    Help(
        "Enabled by default so replay markers or walls cannot hide smaller actual collider shapes. Disable for normal depth occlusion.");
    return changed;
}

inline float ReplayMeasuredSpeed(const practice::ReplayMotion &motion, double seconds)
{
    const auto *frame = practice::ReplayViewAt(motion, seconds);
    if (!frame || frame == motion.frames.data())
        return -1;
    size_t index = size_t(frame - motion.frames.data());
    double distance = 0, elapsed = 0;
    int count = 0;
    while (index > 0 && elapsed < .12 && count++ < 64)
    {
        const auto &b = motion.frames[index];
        const auto &a = motion.frames[--index];
        double dt = b.seconds - a.seconds, d = practice::Distance(a.body, b.body);
        if (dt <= 0)
            continue;
        if (dt > .25 || d > std::max(4., dt * 40))
            return -1;
        distance += d;
        elapsed += dt;
    }
    return elapsed >= .025 ? float(distance / elapsed) : -1;
}

inline void RenderReplayInputHud(const practice::ReplayMotion *motion, const GameData &data)
{
    float u = std::min(suite_ui::Unit(), std::max(.75f, ImGui::GetContentRegionAvail().x / 700));
    float width = ImGui::GetContentRegionAvail().x;
    auto a = ImGui::GetCursorScreenPos();
    auto *d = ImGui::GetWindowDrawList();
    bool compact = width < 620 * u;
    float h = (compact ? 200 : 170) * u;
    auto color = [](ImVec4 c) { return ImGui::GetColorU32(c); };
    suite_ui::Surface(d, a, {a.x + width, a.y + h}, true);
    suite_ui::Motif(d, a, {a.x + width, a.y + h}, .035f);
    ImVec4 clip{a.x + 12 * u, a.y, a.x + width - 12 * u, a.y + h};
    auto text = [&](float x, float y, const std::string &label, float size, ImVec4 c) {
        auto *font = size >= 23 * u && suite_ui::introTitleFont ? suite_ui::introTitleFont : ImGui::GetFont();
        d->AddText(font, size, {a.x + x, a.y + y}, ImGui::GetColorU32(c), label.c_str(), nullptr, 0, &clip);
    };
    auto muted = suite_ui::Blend(suite_ui::ink, suite_ui::background, .38f);
    std::string state = data.replay3dState == 1 ? "LOADING" : data.replay3dState == 2 ? "PLAYING" : "PAUSED";
    text(16 * u, 10 * u, "REPLAY / 3D", 18 * u, suite_ui::heading);
    text(16 * u, 38 * u, FormatRunTime(data.replay3dSeconds), 27 * u, suite_ui::ink);
    text(width - 173 * u, 12 * u, state, 16 * u, suite_ui::heading);
    text(width - 173 * u, 38 * u, suite_brand::Credit, 15 * u, muted);
    const auto *input = motion ? practice::ReplayInputAt(*motion, data.replay3dSeconds) : nullptr;
    const auto *frame = motion ? practice::ReplayViewAt(*motion, data.replay3dSeconds) : nullptr;
    uint32_t keys = input ? input->keys : 0;
    float cell = 47 * u, gap = 6 * u, total = cell * 5 + gap * 4, origin = a.x + (width - total) * .5f,
          y = a.y + 62 * u;
    auto key = [&](const char *label, float x, float row, uint32_t mask) {
        bool down = (keys & mask) != 0;
        float glow = suite_ui::Animate(ImGui::GetID(label) ^ 0x730101u, down ? 1.f : 0.f, false, .06f);
        ImVec2 p{origin + x * (cell + gap), y + row * (cell + gap)}, q{p.x + cell, p.y + cell};
        d->AddRectFilled(
            p, q, color(suite_ui::Blend(suite_ui::background, suite_ui::heading, .08f + .24f * glow)), 7 * u);
        d->AddRect(p, q, color(suite_ui::Alpha(suite_ui::heading, down ? 1.f : .18f + .4f * glow)), 7 * u, 0,
                   (down ? 2.f : 1.f) * u);
        float size = (mask == 48 ? 15 : 24) * u;
        auto *font = mask != 48 && suite_ui::introTitleFont ? suite_ui::introTitleFont : ImGui::GetFont();
        auto measure = font->CalcTextSizeA(size, 10000, 0, label);
        d->AddText(font, size, {p.x + (cell - measure.x) * .5f, p.y + (cell - measure.y) * .5f},
                   color(down ? suite_ui::ink : muted), label);
    };
    key("W", 2, 0, 1);
    key("A", 1, 1, 2);
    key("S", 2, 1, 4);
    key("D", 3, 1, 8);
    key("Shift", 0, 1, 48);
    key("R", 4, 1, 64);
    if (width > 620 * u)
    {
        float speed = motion ? ReplayMeasuredSpeed(*motion, data.replay3dSeconds) : -1;
        char label[80];
        snprintf(label, sizeof(label), speed >= 0 ? "%.2f u/s" : "-- u/s", speed);
        text(16 * u, 83 * u, label, 20 * u, suite_ui::ink);
        text(16 * u, 113 * u, input ? (input->state & 4 ? "SPRINT / ON" : "SPRINT / OFF") : "INPUT GAP",
             15 * u, input ? suite_ui::heading : g_accentCol);
        ImVec2 center{a.x + width - 82 * u, a.y + 105 * u};
        d->AddCircle(center, 23 * u, color(suite_ui::Alpha(suite_ui::heading, .4f)), 32, u);
        d->AddLine({center.x - 28 * u, center.y}, {center.x + 28 * u, center.y},
                   color(suite_ui::Alpha(suite_ui::heading, .18f)));
        d->AddLine({center.x, center.y - 28 * u}, {center.x, center.y + 28 * u},
                   color(suite_ui::Alpha(suite_ui::heading, .18f)));
        if (frame)
        {
            float mx = std::tanh(frame->mouseX + frame->joystickX) * 20 * u,
                  my = std::tanh(frame->mouseY + frame->joystickY) * 20 * u;
            d->AddLine(center, {center.x + mx, center.y - my}, color(suite_ui::heading), 2 * u);
            d->AddCircleFilled({center.x + mx, center.y - my}, 4 * u, color(suite_ui::ink), 16);
        }
        text(width - 105 * u, 139 * u, "LOOK", 14 * u, muted);
    }
    text(16 * u, (compact ? 178 : 146) * u,
         input ? (input->state & 8 ? "RADAR / ACTIVE" : "RADAR / OFF") : "NO INPUT SAMPLE", 14 * u,
         input ? suite_ui::heading : g_accentCol);
    ImGui::Dummy({width, h});
    Help(
        "Large keys show the recorded game-local key state. SPRINT and RADAR show the game's accepted state, not just key presses. LOOK shows the effective recorded mouse/controller axes. Missing samples stay labelled.");
}

inline void RenderReplayPassiveTrack(const practice::RunReview &review, double seconds)
{
    auto a = ImGui::GetCursorScreenPos();
    float w = ImGui::GetContentRegionAvail().x, u = suite_ui::Unit(), y = a.y + 8 * u;
    auto *d = ImGui::GetWindowDrawList();
    auto x = [&](double t) {
        return a.x + w * float(std::clamp(t / std::max(.000001, review.seconds), 0., 1.));
    };
    d->AddLine({a.x, y}, {a.x + w, y}, ImGui::GetColorU32(suite_ui::Alpha(suite_ui::heading, .2f)), 3 * u);
    d->AddLine({a.x, y}, {x(seconds), y}, ImGui::GetColorU32(suite_ui::heading), 3 * u);

    // Bin recorded events to avoid thousands of overlapping draw calls.
    struct Cache
    {
        const practice::RunReview *source = nullptr;
        int64_t id = 0;
        size_t pickups = 0, flashes = 0;
        std::array<uint8_t, 512> bins{};
    };

    static Cache cache;
    if (cache.source != &review || cache.id != review.id || cache.pickups != review.pickupSeconds.size() ||
        cache.flashes != review.flashes.size())
    {
        cache.source = &review;
        cache.id = review.id;
        cache.pickups = review.pickupSeconds.size();
        cache.flashes = review.flashes.size();
        cache.bins.fill(0);
        for (double t : review.pickupSeconds)
            cache.bins[std::clamp(int(t / std::max(.000001, review.seconds) * 511), 0, 511)] |= 1;
        for (const auto &f : review.flashes)
            cache.bins[std::clamp(int(f.seconds / std::max(.000001, review.seconds) * 511), 0, 511)] |= 2;
    }
    const auto &bins = cache.bins;
    for (size_t i = 0; i < bins.size(); i++)
    {
        float px = a.x + w * float(i) / 511;
        if (bins[i] & 1)
            d->AddCircleFilled({px, y + 5 * u}, 2 * u, ImGui::GetColorU32(suite_ui::heading), 6);
        if (bins[i] & 2)
            d->AddLine({px, y - 5 * u}, {px, y - 2 * u}, ImGui::GetColorU32(suite_ui::ink), u);
    }
    if (suite_replay::marks)
        for (const auto &marker : suite_replay::marks->markers)
            d->AddCircleFilled({x(marker.seconds), y - 7 * u}, 2.5f * u, ImGui::GetColorU32(g_accentCol), 6);
    d->AddCircleFilled({x(seconds), y}, 4 * u, ImGui::GetColorU32(suite_ui::ink), 12);
    ImGui::Dummy({w, 20 * u});
}

inline void RenderReplay3DDock()
{
    if (!g_pShared || (g_overlayVisible && !g_compactMode))
        return;
    using namespace suite_replay;
    const auto &data = *g_pShared;
    float u = suite_ui::Unit();
    auto display = ImGui::GetIO().DisplaySize;
    bool interactive = ControlsRequested(&data);
    float width = std::min((interactive ? 1060.f : 800.f) * u, std::max(180.f, display.x - 32));
    ImGui::SetNextWindowPos({display.x * .5f, display.y - 16}, ImGuiCond_Always, {.5f, 1});
    ImGui::SetNextWindowSize({width, 0});
    ImGui::SetNextWindowSizeConstraints({width, 40}, {width, std::max(100.f, display.y - 32)});
    ImGui::SetNextWindowBgAlpha(.95f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, suite_ui::Color(AppearancePreferences::HudBackground));
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize;
    if (!interactive)
        flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoScrollbar;
    ImGui::Begin("##Replay3D status", nullptr, flags);
    auto motion = practice::g_replayArchive.Motion();
    bool matching = motion && motion->id == data.replay3dRunId;
    if (source && source->review->id == data.replay3dRunId)
        Bind(source);
    else
        matching = false;
    if (!suite_input::preferences.enabled)
        RenderReplayInputHud(matching ? motion.get() : nullptr, data);
    if (matching && data.replay3dState >= 2 && !practice::ReplayViewAt(*motion, data.replay3dSeconds))
        ImGui::TextColored(g_accentCol, "Camera gap / seek to a recorded frame");
    if (source && source->review->id == data.replay3dRunId)
    {
        const auto &r = *source->review;
        playback.Bind(r.id);
        playback.seconds = std::clamp(double(data.replay3dSeconds), 0., r.seconds);
        playback.playing = data.replay3dState == 2;
        if (!HasPendingRequest() && !g_commands.replay3dCmd)
            playback.speed =
                std::isfinite(data.replay3dSpeed) && data.replay3dSpeed >= .1f ? data.replay3dSpeed : 1;
        double before = playback.seconds;
        bool wasPlaying = playback.playing;
        float rate = playback.speed;
        bool changed = false;
        if (interactive)
        {
            RenderReplayTimeline(r, playback, r.seconds, marks.get(), matching ? motion.get() : nullptr);
            if (matching)
                changed |= RenderReplayExperience(*motion, playback);
            ImGui::BeginDisabled(!matching);
            if (suite_ui::Button("< Frame"))
            {
                playback.seconds = practice::ReplayFrameStep(*motion, playback.seconds, false);
                playback.playing = false;
            }
            SameLineIfFits("Frame >");
            if (suite_ui::Button("Frame >"))
            {
                playback.seconds = practice::ReplayFrameStep(*motion, playback.seconds, true);
                playback.playing = false;
            }
            ImGui::EndDisabled();
            SameLineIfFits("Speed");
            ImGui::SetNextItemWidth(std::min(200.f * u, ImGui::GetContentRegionAvail().x));
            ImGui::SliderFloat("Speed", &playback.speed, .1f, 8.f, "%.2fx",
                               ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
            static double seek = 0;
            static bool editing = false, dirty = false;
            static int64_t seekRun = 0;
            if (seekRun != r.id)
            {
                seekRun = r.id;
                dirty = editing = false;
            }
            if (!editing && !dirty)
                seek = playback.seconds;
            SameLineIfFits("Time (s)");
            ImGui::SetNextItemWidth(125 * u);
            if (ImGui::InputDouble("Time (s)", &seek, 0, 0, "%.3f"))
                dirty = true;
            bool commit =
                (ImGui::IsItemActive() || ImGui::IsItemDeactivated()) && ImGui::IsKeyPressed(ImGuiKey_Enter);
            editing = ImGui::IsItemActive();
            SameLineIfFits("Go");
            commit |= suite_ui::Button("Go");
            if (commit && std::isfinite(seek))
            {
                playback.seconds = std::clamp(seek, 0., r.seconds);
                playback.playing = false;
                dirty = false;
            }

            RenderReplayMarkers(playback);
            SameLineIfFits("Set A");
            changed |= RenderReplayLoop(playback);
            if (suite_ui::Button("Export clip / GIF"))
            {
                RequestClipFrom3D();
                g_overlayVisible = true;
                g_compactMode = false;
                g_selectRunReview = true;
                SetOverlayInteractive(true);
            }
            if (suite_ui::Button("3D options"))
                ImGui::OpenPopup("3D view options");
            ImGui::SetNextWindowSize({std::min(440.f * u, display.x - 32), 0}, ImGuiCond_Appearing);
            if (ImGui::BeginPopup("3D view options"))
            {
                changed |= RenderReplay3DSettings(matching ? motion.get() : nullptr);
                ImGui::EndPopup();
            }
            SameLineIfFits("Hide controls [Tab]");
            if (suite_ui::Button("Hide controls [Tab]"))
            {
                controls = false;
                SetOverlayInteractive(false);
            }
            SameLineIfFits("Exit 3D");
            if (suite_ui::Button("Exit 3D"))
            {
                StopReplay3D();
                g_overlayVisible = true;
                g_compactMode = false;
                SetOverlayInteractive(true);
            }
            if (changed || before != playback.seconds || wasPlaying != playback.playing ||
                rate != playback.speed)
                Replay3DControl(r, playback, camera == 2,
                                before != playback.seconds || wasPlaying != playback.playing);
        }
        else
        {
            RenderReplayPassiveTrack(r, data.replay3dSeconds);
            ImGui::TextDisabled(
                "Tab controls   /   Space play/pause   /   Arrows frame   /   M marker   /   Esc exit");
        }
    }
    else
    {
        ImGui::TextDisabled("%s", data.replay3dState == 1 ? "Loading recorded level..."
                                                          : "F3 opens replay controls");
    }
    ImGui::End();
    ImGui::PopStyleColor();
}
