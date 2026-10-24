#pragma once
#include "ClipEncoding.h"
#include "ClipGuides.h"
#include "ClipProject.h"
#include "ExportProfiles.h"

namespace suite_clip
{
inline bool open = false, requestOpen = false, playPreview = false, startRequested = false,
            testRequested = false, fullRunRequested = false, cancelRequested = false, dirty = false,
            backendAvailable = false, actualSize = false;
inline uint64_t fullRunRecheck = 0;
inline bool awaitingFullRunCheck = false;
inline bool focusWindow = false, started3d = false, editLayout = false, cameraEditing = false,
            shownPoseReady = false;
inline int selectedBlock = Keyboard, selectedKey = -1, profileSlot = 0, dragBlock = -1;
inline bool resizeBlock = false;
inline Options options;
inline Content content;
inline Encoder encoder;
inline CameraPose shownPose, editorPose;
inline FlyCamera fly;
inline bool cameraDrag = false, cameraPanDrag = false, showCameraPath = true, pathOverview = false;
inline CameraGuideCache guideCache;
inline double freeDiskGiB = -1;
inline double previewAt = 0, previewClock = 0, previewShown = 0;
inline uint64_t exportFrameCount = 0;
inline uint64_t revision = 0, cameraEpoch = 0;
inline uint64_t previewRevision = 0, previewEpoch = 0;
inline float previewPixelWidth = 960;
inline ImTextureID previewImage{};
inline int64_t previewRun = 0;
inline std::string error, status;
inline char profileName[81] = "My export";
inline void (*openDirectory)() = nullptr;
inline void (*saveProject)() = nullptr;
inline void (*loadProject)() = nullptr;
inline void (*profileIO)(int, bool, const char *) = nullptr;

inline void Changed()
{
    if (content.analysis && content.analysis->data && content.analysis->data->review)
        try
        {
            ConfigureSpeed(options.timing, content.track, content.analysis->data->review->seconds,
                           options.rate, options.camera == 3 && options.speedKeys);
        }
        catch (const std::exception &e)
        {
            options.timing.reset();
            error = e.what();
        }
    dirty = true;
    revision++;
}

inline void ChangedCamera()
{
    cameraEpoch++;
    Changed();
}

inline bool EditingPoint()
{
    return cameraEditing && selectedKey >= 0 && selectedKey < int(content.track.keys.size());
}

inline int EditorMode()
{
    return EditingPoint() ? content.track.keys[selectedKey].mode : options.camera;
}

inline std::optional<CameraPose> CurrentCameraPose()
{
    return CameraAt(options, content, previewAt);
}

inline void SeedCamera()
{
    if (auto pose = RecordedCamera(content, previewAt))
    {
        if (!content.orbit.ready)
        {
            content.orbit.yaw =
                std::atan2(Forward(pose->rotation).x, Forward(pose->rotation).z) * 57.2957795f;
            content.orbit.ready = true;
        }
        if (!content.freeReady)
        {
            content.freePose = *pose;
            content.freeReady = true;
        }
        shownPose = CurrentCameraPose().value_or(*pose);
        shownPoseReady = true;
        fly.Align(content.freePose);
    }
}

inline void SetCameraMode(int mode)
{
    mode = std::clamp(mode, 0, 3);
    int before = EditorMode();
    if (mode == before)
        return;
    auto current = CurrentCameraPose();
    if (mode == 1 && current && before != 0)
    {
        if (auto focus = RecordedCamera(content, previewAt))
        {
            content.orbit.Align(*current, focus->position);
            content.freePose.fov = current->fov;
        }
    }
    if (mode == 2 && current)
    {
        content.freePose = *current;
        content.freeReady = true;
        fly.Align(*current);
    }
    if (EditingPoint() && mode < 3)
    {
        auto &key = content.track.keys[selectedKey];
        key.mode = mode;
        if (current)
            key.pose = *current;
        if (mode == 1)
            key.follow = content.orbit;
        options.camera = 3;
    }
    else
    {
        options.camera = mode;
        cameraEditing = false;
    }
    cameraDrag = cameraPanDrag = false;
    error.clear();
    ChangedCamera();
}

inline void Seek(double at, bool pause = true)
{
    previewAt = std::clamp(at, 0., content.analysis->data->review->seconds);
    if (pause)
        playPreview = false;
    if (EditingPoint() && std::abs(previewAt - content.track.keys[selectedKey].seconds) > 1e-7)
        cameraEditing = false;
    cameraDrag = cameraPanDrag = false;
    Changed();
}

inline std::string PointLabel(size_t index)
{
    return ShotLabel(index);
}

inline void EditKey(size_t index)
{
    if (index >= content.track.keys.size())
        return;
    selectedKey = int(index);
    Seek(content.track.keys[index].seconds);
    options.camera = 3;
    auto &key = content.track.keys[index];
    content.orbit = key.follow;
    if (auto pose = CurrentCameraPose())
    {
        content.freePose = editorPose = *pose;
        content.freeReady = true;
        fly.Align(*pose);
    }
    cameraEditing = true;
    ChangedCamera();
}

inline void UpdateEditingKey()
{
    if (EditingPoint())
    {
        auto &key = content.track.keys[selectedKey];
        if (key.mode == 2)
            key.pose = editorPose = content.freePose;
        else if (key.mode == 1)
        {
            key.follow = content.orbit;
            key.pose.fov = content.freePose.fov;
        }
    }
}

inline void TogglePlayback()
{
    if (cameraEditing)
    {
        cameraEditing = false;
        options.camera = 3;
    }
    playPreview = !playPreview;
    previewClock = ImGui::GetTime();
    if (previewAt >= options.b || previewAt < options.a)
        previewAt = options.a;
    ChangedCamera();
}

inline void PauseViewer()
{
    if (!g_pShared || !content.analysis)
        return;
    const auto &r = *content.analysis->data->review;
    if (g_pShared->replay3dRunId == r.id && g_pShared->replay3dState >= 2 && g_pShared->replay3dState <= 3 &&
        !HasPendingRequest())
    {
        suite_replay::playback.playing = false;
        suite_replay::Apply(g_commands, false, suite_replay::camera == 2);
        g_commands.replay3dRunId = r.id;
        g_commands.replay3dSpeed = 1;
        g_commands.replay3dCmd = 3;
    }
}

inline void Open(std::shared_ptr<const practice::MapAnalysis> a,
                 std::shared_ptr<const practice::ReplayMotion> motion)
{
    if (!a || !a->data || !a->data->review || (g_pShared && g_pShared->timerState == 1) ||
        encoder.State() == 1)
        return;
    suite_replay::Bind(a->data);
    content = {};
    content.analysis = std::move(a);
    content.motion = std::move(motion);
    content.markers = suite_replay::marks;
    content.input = suite_input::preferences;
    content.palette = suite_input::Colors(1);
    content.font = suite_ui::hudFont ? suite_ui::hudFont : ImGui::GetFont();
    options = Options();
    ApplyExportPreset(options, 0);
    options.width = 1920;
    options.height = 1080;
    options.fps = 60;
    const auto &run = *content.analysis->data->review;
    options.b = run.seconds;
    options.scene3d = g_pShared && g_pShared->replay3dRunId == run.id && g_pShared->replay3dState >= 2 &&
                      g_pShared->replay3dState <= 3;
    options.camera = suite_replay::camera;
    options.hideCeiling = suite_replay::hideCeiling;
    options.fadeWalls = suite_replay::fadeWalls;
    options.simplified = suite_replay::simplified;
    options.fullbright = suite_replay::fullbright;
    options.audio =
        content.motion && content.motion->experience.audio.recorded && !content.motion->audioFile.empty();
    options.fullKeyboard = content.input.fullKeyboard;
    error.clear();
    if (loadProject)
        loadProject();
    auto marks = content.markers;
    if (marks && marks->hasClipA && marks->hasClipB)
    {
        options.a = marks->clipA;
        options.b = marks->clipB;
    }
    else if (marks && marks->LoopValid())
    {
        options.a = marks->a;
        options.b = marks->b;
    }
    previewAt = options.a;
    open = focusWindow = true;
    requestOpen = playPreview = started3d = cameraEditing = shownPoseReady = false;
    cameraDrag = cameraPanDrag = pathOverview = false;
    SeedCamera();
    cameraEpoch++;
    selectedKey = -1;
    selectedBlock = std::clamp(selectedBlock, 0, int(States));
    dragBlock = -1;
    status = "Preparing preview";
    previewRun = 0;
    Changed();
    g_overlayVisible = true;
    g_compactMode = false;
    suite_replay::controls = false;
    SetOverlayInteractive(true);
    PauseViewer();
}

inline Options FullRunOptions(const Options &output, const Content &source)
{
    if (!source.analysis || !source.analysis->data || !source.analysis->data->review || !source.motion ||
        source.motion->frames.empty())
        throw std::runtime_error("Full run video needs a recorded camera");
    auto full = output;
    if (full.format == 1 || full.format == 3)
        ApplyExportPreset(full, 0);
    full.a = 0;
    full.b = source.analysis->data->review->seconds;
    full.rate = 1;
    full.camera = 0;
    full.scene3d = true;
    full.speedKeys = full.route = full.colliders = full.fullbright = full.hideCeiling = full.fadeWalls =
        full.simplified = full.transparent = false;
    full.timing.reset();
    full.audio = source.motion->experience.audio.recorded && !source.motion->audioFile.empty();
    full.audioVolume = 1;
    full.verification = true;
    full.Validate(full.b);
    return full;
}

inline void Close()
{
    if (encoder.State() == 1)
        return;
    if (saveProject)
        saveProject();
    open = playPreview = false;
    awaitingFullRunCheck = false;
    previewRun = 0;
    if (started3d && g_pShared && content.analysis &&
        g_pShared->replay3dRunId == content.analysis->data->review->id)
        g_commands.replay3dCmd = 2;
    started3d = false;
    SetOverlayInteractive(g_overlayVisible && !g_compactMode);
}

inline void Button(std::shared_ptr<const practice::MapAnalysis> a,
                   std::shared_ptr<const practice::ReplayMotion> motion)
{
    ImGui::BeginDisabled(!a || !a->data || !a->data->review || a->data->review->seconds < .02 ||
                         encoder.State() == 1);
    if (suite_ui::Button("Export clip / GIF") || requestOpen)
        Open(a, motion);
    ImGui::EndDisabled();
    Help("Compose a clip, animate a camera track, then export MP4, GIF, RGB "
         "lossless MKV or a PNG sequence. New recordings include the original game mix.");
}

inline void Field(const char *text)
{
    ImGui::TextUnformatted(text);
    ImGui::SetNextItemWidth(-1);
}

inline void SaveKey(bool replace)
{
    auto sampled = ResolveFrame(options, content, previewAt);
    if (!sampled.pose)
    {
        error = "No recorded camera pose at this time";
        return;
    }
    try
    {
        sampled.pose->Validate();
        auto track = content.track;
        CameraKey key;
        key.seconds = previewAt;
        key.pose = *sampled.pose;
        key.mode = sampled.options.camera;
        key.follow = content.orbit;
        key.next = key.mode == 2 ? Transition::EaseInOut : Transition::Cut;
        key.name = "Shot " + PointLabel(track.keys.size());
        int existing = -1;
        if (replace && selectedKey >= 0 && selectedKey < int(track.keys.size()))
            existing = selectedKey;
        else
            for (size_t i = 0; i < track.keys.size(); i++)
            {
                double difference = std::abs(track.keys[i].seconds - previewAt);
                if (difference < 1e-7)
                    existing = int(i);
                else if (difference < .001)
                    throw std::runtime_error("Keypoints must be at least 0.001 seconds apart");
            }
        if (existing >= 0)
        {
            auto old = track.keys[existing];
            key.seconds = old.seconds;
            key.name = old.name;
            key.next = old.next;
            key.layerMask = old.layerMask;
            key.layers = old.layers;
            key.speedOverride = old.speedOverride;
            key.playbackSpeed = old.playbackSpeed;
            key.speedCurve = old.speedCurve;
        }
        size_t index = track.Put(key);
        track.Validate(content.analysis->data->review->seconds);
        content.track = std::move(track);
        EditKey(index);
        status = "Shot " + PointLabel(index) + " captured";
        error.clear();
        if (saveProject)
            saveProject();
    }
    catch (const std::exception &e)
    {
        error = e.what();
    }
}

inline bool PreviewInteraction(ImVec2 origin, ImVec2 size, bool busy)
{
    if (busy)
    {
        return false;
    }
    auto &io = ImGui::GetIO();
    bool hovered = ImGui::IsItemHovered();
    if (hovered && options.scene3d)
        ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, ImGui::GetCurrentWindow()->ID);
    bool changed = false;
    auto toScreen = [&](Bounds r) {
        return Bounds{origin.x + r.x / options.width * size.x, origin.y + r.y / options.height * size.y,
                      r.w / options.width * size.x, r.h / options.height * size.y};
    };
    auto hit = [&](Bounds r) {
        return io.MousePos.x >= r.x && io.MousePos.y >= r.y && io.MousePos.x <= r.x + r.w &&
               io.MousePos.y <= r.y + r.h;
    };
    if (editLayout)
    {
        auto *d = ImGui::GetWindowDrawList();
        for (int block = 0; block < BlockCount; block++)
            if (BlockEnabled(block, options, content))
            {
                auto r = toScreen(OverlayBounds(block, options, content));
                auto color =
                    block == selectedBlock ? IM_COL32(96, 238, 214, 255) : IM_COL32(140, 178, 185, 100);
                d->AddRect({r.x, r.y}, {r.x + r.w, r.y + r.h}, color, 3, 0, 1.5f);
                if (block == selectedBlock)
                {
                    d->AddRectFilled({r.x + r.w - 9, r.y + r.h - 9}, {r.x + r.w, r.y + r.h}, color);
                    if (hovered && ImGui::IsMouseClicked(0) && hit(r))
                    {
                        dragBlock = block;
                        resizeBlock = io.MousePos.x > r.x + r.w - 16 && io.MousePos.y > r.y + r.h - 16;
                    }
                }
            }
        if (hovered && ImGui::IsMouseClicked(0) && dragBlock < 0)
            for (int block = BlockCount - 1; block >= 0; block--)
                if (BlockEnabled(block, options, content) &&
                    hit(toScreen(OverlayBounds(block, options, content))))
                {
                    selectedBlock = dragBlock = block;
                    resizeBlock = false;
                    break;
                }
        if (dragBlock >= 0)
        {
            if (!ImGui::IsMouseDown(0))
                dragBlock = -1;
            else if (io.MouseDelta.x || io.MouseDelta.y)
            {
                auto r = OverlayBounds(dragBlock, options, content);
                auto &p = content.layout.blocks[dragBlock];
                if (resizeBlock)
                    p.width = std::clamp(p.width + io.MouseDelta.x / size.x /
                                                       (dragBlock == Mouse ? content.input.mouseScale : 1),
                                         dragBlock == RunInfo ? .55f : .04f, 1.f);
                else
                {
                    p.x = std::clamp(r.x / options.width + io.MouseDelta.x / size.x, 0.f,
                                     1 - r.w / options.width);
                    p.y = std::clamp(r.y / options.height + io.MouseDelta.y / size.y, 0.f,
                                     1 - r.h / options.height);
                }
                changed = true;
            }
        }
    }
    if (options.scene3d && shownPoseReady)
    {
        bool startDrag = hovered && ImGui::IsMouseClicked(1);
        cameraDrag = ImGui::IsMouseDown(1) && (cameraDrag || startDrag);
        cameraPanDrag = ImGui::IsMouseDown(2) && (cameraPanDrag || (hovered && ImGui::IsMouseClicked(2)));
        bool wheel = hovered && io.MouseWheel != 0;
        if (cameraDrag || cameraPanDrag || wheel)
        {
            if (options.camera == 3 && !EditingPoint())
                return changed;
            if (EditorMode() == 0)
            {
                SetCameraMode(2);
                cameraDrag = ImGui::IsMouseDown(1);
                cameraPanDrag = ImGui::IsMouseDown(2);
            }
            auto before = CurrentCameraPose();
            if (EditorMode() == 1)
            {
                content.orbit.ready = true;
                if (cameraDrag && !startDrag)
                    content.orbit.Rotate(io.MouseDelta.x, io.MouseDelta.y);
                if (wheel)
                    content.orbit.Zoom(io.MouseWheel);
                if (cameraPanDrag && before)
                {
                    auto right = Right(before->rotation), forward = Forward(before->rotation);
                    Point up{forward.y * right.z - forward.z * right.y,
                             forward.z * right.x - forward.x * right.z,
                             forward.x * right.y - forward.y * right.x};
                    content.orbit.pan =
                        content.orbit.pan +
                        (right * (-io.MouseDelta.x) + up * io.MouseDelta.y) * (content.orbit.radius * .0018f);
                }
            }
            else
            {
                if (!content.freeReady && before)
                {
                    content.freePose = *before;
                    content.freeReady = true;
                    fly.Align(*before);
                }
                if (cameraDrag && !startDrag)
                    fly.Look(content.freePose, io.MouseDelta.x, io.MouseDelta.y);
                if (wheel)
                    fly.Travel(content.freePose, io.MouseWheel);
                if (cameraPanDrag)
                    fly.Pan(content.freePose, io.MouseDelta.x, io.MouseDelta.y);
                if (cameraDrag && !io.WantTextInput)
                {
                    auto held = [](ImGuiKey key) { return ImGui::IsKeyDown(key) ? 1.f : 0.f; };
                    fly.Move(content.freePose, held(ImGuiKey_D) - held(ImGuiKey_A),
                             held(ImGuiKey_E) - held(ImGuiKey_Q), held(ImGuiKey_W) - held(ImGuiKey_S),
                             io.DeltaTime,
                             ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift));
                }
            }
            UpdateEditingKey();
            if (auto after = CurrentCameraPose(); before && after)
            {
                changed |=
                    before->position.x != after->position.x || before->position.y != after->position.y ||
                    before->position.z != after->position.z || before->rotation.x != after->rotation.x ||
                    before->rotation.y != after->rotation.y || before->rotation.z != after->rotation.z ||
                    before->rotation.w != after->rotation.w;
            }
        }
    }
    return changed;
}

inline bool ModeButton(const char *label, int mode, float width = 0)
{
    bool selected = EditorMode() == mode;
    auto style = ImGui::GetStyle();
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? ImVec4(g_headerCol.x * .30f, g_headerCol.y * .30f,
                                                             g_headerCol.z * .30f, 1)
                                                    : style.Colors[ImGuiCol_FrameBg]);
    ImGui::PushStyleColor(ImGuiCol_Text, selected ? g_headerCol : style.Colors[ImGuiCol_Text]);
    bool hit = suite_ui::Button(label, {width, 0});
    ImGui::PopStyleColor(2);
    if (hit)
        SetCameraMode(mode);
    return hit;
}

inline void CameraModes()
{
    float gap = ImGui::GetStyle().ItemSpacing.x, w = (ImGui::GetContentRegionAvail().x - 2 * gap) / 3;
    ModeButton("Recorded", 0, w);
    ImGui::SameLine();
    ModeButton("Follow", 1, w);
    ImGui::SameLine();
    ModeButton("Free", 2, w);
}

inline void CameraPointSpeedControls(bool &changed)
{
    if (selectedKey >= 0 && selectedKey < int(content.track.keys.size()))
    {
        auto &speedKey = content.track.keys[selectedKey];
        changed |= suite_ui::Checkbox("Playback speed at this point", &speedKey.speedOverride);
        Help("Absolute replay speed from this point. Cut holds it until the next "
             "speed point; other curves "
             "ramp to that point. Slow motion lengthens the clip while output FPS "
             "stays fixed. Disable to "
             "inherit the preceding speed.");
        if (speedKey.speedOverride && !options.speedKeys)
            ImGui::TextDisabled("Speed points disabled in Output");
        if (speedKey.speedOverride)
        {
            float rate = float(speedKey.playbackSpeed);
            ImGui::SetNextItemWidth(-1);
            if (ImGui::SliderFloat("##PointPlaybackSpeed", &rate, .1f, 8, "%.2fx",
                                   ImGuiSliderFlags_Logarithmic))
            {
                speedKey.playbackSpeed = rate;
                changed = true;
            }
            for (double preset : {.1, .5, 1., 2.})
            {
                if (preset != .1)
                    ImGui::SameLine();
                char label[16];
                std::snprintf(label, sizeof(label), "%.1fx", preset);
                if (suite_ui::Button(label))
                {
                    speedKey.playbackSpeed = preset;
                    changed = true;
                }
            }
            int speedCurve = int(speedKey.speedCurve);
            Field("Speed to next speed point");
            if (ImGui::Combo("##PointSpeedCurve", &speedCurve, TransitionNames, 6))
            {
                speedKey.speedCurve = Transition(speedCurve);
                changed = true;
            }
        }
    }
}

inline void CameraLayerControls(bool &changed)
{
    if (ImGui::TreeNode("Scene layers"))
    {
        bool point = EditingPoint();
        ImGui::TextDisabled(point ? "At this point / inherited until changed" : "Default layers");
        auto effective = ResolveFrame(options, content, previewAt).options;
        auto layer = [&](const char *label, uint32_t bit, bool Options::*field) {
            bool value = point ? effective.*field : options.*field;
            if (suite_ui::Checkbox(label, &value))
            {
                if (point)
                {
                    auto &key = content.track.keys[selectedKey];
                    key.layerMask |= bit;
                    key.layers = (key.layers & ~bit) | (value ? bit : 0u);
                }
                else
                    options.*field = value;
                changed = true;
            }
        };
        layer("Fullbright", Fullbright, &Options::fullbright);
        layer("Actual colliders", Colliders, &Options::colliders);
        layer("Hide ceiling", Ceiling, &Options::hideCeiling);
        layer("Route trail", Route, &Options::route);
        layer("Monster", Monster, &Options::monster);
        layer("Fade blocking walls", FadeWalls, &Options::fadeWalls);
        Help("These are replay drawing layers. Run data and validity stay "
             "unchanged. Wall fading retains its "
             "existing limitations.");
        if (point && content.track.keys[selectedKey].layerMask && suite_ui::Button("Inherit previous layers"))
        {
            content.track.keys[selectedKey].layerMask = content.track.keys[selectedKey].layers = 0;
            changed = true;
        }
        ImGui::TreePop();
    }
}

inline void CameraPoseControls(bool &changed)
{
    int mode = EditorMode();
    if (mode == 2)
    {
        Field("Fly speed");
        ImGui::SliderFloat("##FlySpeed", &fly.speed, 1, 50, "%.1f u/s");
        if (ImGui::TreeNode("Camera position"))
        {
            auto before = content.freePose;
            bool edit = ImGui::DragFloat3("Position", &content.freePose.position.x, .05f, 0, 0, "%.3f");
            float angles[2] = {fly.yaw, fly.pitch};
            if (ImGui::DragFloat2("Yaw / pitch", angles, .2f, 0, 0, "%.2f"))
            {
                fly.yaw = std::remainder(angles[0], 360.f);
                fly.pitch = std::clamp(angles[1], -89.f, 89.f);
                content.freePose.rotation = Angles(fly.yaw, fly.pitch);
                edit = true;
            }
            if (edit)
            {
                try
                {
                    content.freePose.Validate();
                    UpdateEditingKey();
                    changed = true;
                }
                catch (const std::exception &e)
                {
                    content.freePose = before;
                    fly.Align(before);
                    error = e.what();
                }
            }
            ImGui::TreePop();
        }
    }
    if (mode == 1 || mode == 2)
    {
        if (suite_ui::Button("Center on player"))
        {
            if (auto focus = RecordedCamera(content, previewAt))
            {
                Orbit centered;
                centered.yaw =
                    std::atan2(Forward(focus->rotation).x, Forward(focus->rotation).z) * 57.2957795f;
                centered.ready = true;
                content.orbit = centered;
                content.freePose = centered.Pose(focus->position, content.freePose.fov);
                content.freeReady = true;
                fly.Align(content.freePose);
                UpdateEditingKey();
                ChangedCamera();
            }
        }
        Field("Lens");
        if (ImGui::SliderFloat("##ClipFov", &content.freePose.fov, 20, 120, "%.0f deg"))
        {
            UpdateEditingKey();
            changed = true;
        }
    }
}

inline void CameraShotList()
{
    if (content.track.keys.empty())
        ImGui::TextDisabled("Time  >  K  >  choose camera mode");
    float rowHeight = 30 * suite_ui::Unit() + ImGui::GetStyle().ItemSpacing.y;
    float pointListHeight = std::clamp(float(content.track.keys.size()) * rowHeight +
                                           2 * ImGui::GetStyle().WindowPadding.y + 4 * suite_ui::Unit(),
                                       96 * suite_ui::Unit(), 180 * suite_ui::Unit());
    ImGui::BeginChild("Camera keypoints", {0, pointListHeight}, true);
    for (size_t i = 0; i < content.track.keys.size(); i++)
    {
        ImGui::PushID(int(i));
        const auto &key = content.track.keys[i];
        char time[48];
        std::snprintf(time, sizeof(time), "%.3f s", key.seconds);
        auto label = PointLabel(i) + "  " + ShotModeNames[key.mode] + "  /  " + time;
        if (ImGui::Selectable(label.c_str(), selectedKey == int(i), 0, {0, 30 * suite_ui::Unit()}))
            EditKey(i);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s / %.6f s / %s to next point", key.name.c_str(), key.seconds,
                              TransitionNames[int(key.next)]);
        ImGui::PopID();
    }
    ImGui::EndChild();
}

inline void CameraSelectedShotControls(bool &changed)
{
    if (selectedKey >= 0 && selectedKey < int(content.track.keys.size()))
    {
        auto &key = content.track.keys[selectedKey];
        char name[81];
        std::snprintf(name, sizeof(name), "%s", key.name.c_str());
        Field("Shot name");
        if (ImGui::InputText("##CameraKeyName", name, sizeof(name)))
        {
            key.name = name;
            changed = true;
        }
        double time = key.seconds;
        Field("Exact time in recording (s)");
        if (ImGui::InputDouble("##CameraKeyTime", &time,
                               std::max(.000001, options.Advance(time, 1. / options.fps) - time), 1, "%.6f"))
        {
            bool duplicate = false;
            for (size_t i = 0; i < content.track.keys.size(); i++)
                if (int(i) != selectedKey && std::abs(content.track.keys[i].seconds - time) < .001)
                    duplicate = true;
            if (!duplicate && std::isfinite(time) && time >= 0 &&
                time <= content.analysis->data->review->seconds)
            {
                auto track = content.track;
                auto copy = key;
                copy.seconds = time;
                track.keys.erase(track.keys.begin() + selectedKey);
                size_t index = track.Put(copy);
                track.Validate(content.analysis->data->review->seconds);
                content.track = std::move(track);
                EditKey(index);
                changed = true;
            }
            else
                error = "Choose a time inside the recording, at least 0.001 seconds "
                        "from another point";
        }
        auto &current = content.track.keys[selectedKey];
        int transition = int(current.next);
        Field("Transition to next point");
        if (ImGui::Combo("##CameraTransition", &transition, TransitionNames, 6))
        {
            current.next = Transition(transition);
            changed = true;
        }
        Help("Cut holds the current mode until the next exact timestamp. The other "
             "curves interpolate camera "
             "position, orientation and lens, including transitions between "
             "different modes. Scene layers "
             "switch exactly at their point.");
        if (suite_ui::Button(EditingPoint() ? "Update selected" : "Edit selected"))
        {
            if (EditingPoint())
                SaveKey(true);
            else
                EditKey(size_t(selectedKey));
        }
        SameLineIfFits("Remove point");
        if (suite_ui::Button("Remove point"))
        {
            auto pose = CurrentCameraPose();
            content.track.keys.erase(content.track.keys.begin() + selectedKey);
            selectedKey = -1;
            cameraEditing = false;
            if (content.track.keys.empty())
            {
                options.camera = 2;
                if (pose)
                {
                    content.freePose = *pose;
                    content.freeReady = true;
                    fly.Align(*pose);
                }
            }
            ChangedCamera();
        }
    }
}

inline void CameraShotsPanel(bool &changed)
{
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(g_headerCol, "CAMERA SHOTS");
    ImGui::BeginDisabled(!CurrentCameraPose());
    if (suite_ui::Button("Add keypoint", {-1, 0}))
        SaveKey(false);
    ImGui::EndDisabled();
    Help("Seek to an exact time, capture a point, then choose Recorded, Follow "
         "or Free. Cut changes mode at "
         "the next point; easing blends between modes. Before the first point, "
         "the original recording plays.");
    CameraShotList();
    CameraSelectedShotControls(changed);
    changed |= suite_ui::Checkbox("Show camera path", &showCameraPath);
    if (suite_ui::Button("Path overview"))
        pathOverview = true;
    Help("Editor guides show the path, point labels and look directions. They "
         "are excluded from the exported "
         "video.");
}

inline void CameraPanel(bool &changed)
{
    bool can3d = content.motion && !content.motion->frames.empty() && g_pShared;
    ImGui::BeginDisabled(!can3d && !options.scene3d);
    changed |= suite_ui::Checkbox("3D scene", &options.scene3d);
    ImGui::EndDisabled();
    if (!options.scene3d)
    {
        ImGui::TextDisabled("Map composition");
        Help("Enable the recorded 3D scene to compose a camera shot.");
        return;
    }
    if (suite_ui::Checkbox("Simplified graphics", &options.simplified))
    {
        ChangedCamera();
        changed = true;
    }
    Help("Off: original scene materials, textures, lighting, shadows and camera "
         "effects. On: the previous "
         "lightweight analysis style. Changing this reloads the replay scene. "
         "Recorded model animation "
         "requires a new visual recording.");
    if (!EditingPoint())
        changed |= suite_ui::Checkbox("Fullbright", &options.fullbright);
    Help("Brighten this replay without replacing original materials. While "
         "editing a point, use Scene layers "
         "to change Fullbright at that exact time. This is a display setting; "
         "run validity stays unchanged.");
    bool timeline = options.camera == 3;
    ImGui::BeginDisabled(content.track.keys.empty());
    if (suite_ui::Checkbox("Use keypoints", &timeline))
    {
        if (timeline)
            SetCameraMode(3);
        else
        {
            cameraEditing = false;
            SetCameraMode(2);
        }
    }
    ImGui::EndDisabled();
    if (EditingPoint())
    {
        ImGui::TextColored(g_headerCol, "SHOT %s / %.6f s", PointLabel(selectedKey).c_str(), previewAt);
        CameraModes();
        Help("Mode and camera edits are saved on this output point. The timeline "
             "remains enabled for preview "
             "and export.");
        if (suite_ui::Button("Done editing"))
        {
            cameraEditing = false;
            cameraDrag = cameraPanDrag = false;
            changed = true;
        }
    }
    else if (options.camera == 3)
    {
        auto frame = ResolveFrame(options, content, previewAt);
        ImGui::TextColored(g_headerCol, "TIMELINE / %s", ShotModeNames[frame.options.camera]);
        ImGui::TextDisabled("Select a point to edit its camera");
    }
    else
        CameraModes();
    Help("Free: hold RMB to look; WASD moves, Q/E lowers/raises, Shift moves "
         "faster. Wheel travels, MMB pans. "
         "Follow: RMB orbits, wheel zooms.");
    CameraPointSpeedControls(changed);
    CameraLayerControls(changed);
    CameraPoseControls(changed);
    CameraShotsPanel(changed);
}

inline void OverlaysPanel(bool &changed)
{
    ImGui::TextColored(g_headerCol, "COMPOSITION");
    changed |= suite_ui::Checkbox("Input overlay", &options.inputs);
    changed |= suite_ui::Checkbox("Run time", &options.time);
    ImGui::SameLine();
    changed |= suite_ui::Checkbox("Speed", &options.speed);
    ImGui::SameLine();
    changed |= suite_ui::Checkbox("Seed", &options.seed);
    changed |= suite_ui::Checkbox("Replay annotations", &options.events);
    if (options.inputs)
    {
        changed |= suite_ui::Checkbox("Keyboard", &content.input.keyboard);
        ImGui::SameLine();
        changed |= suite_ui::Checkbox("Mouse", &content.input.mouse);
        changed |= suite_ui::Checkbox("Controller", &content.input.controller);
        ImGui::SameLine();
        changed |= suite_ui::Checkbox("Player activity", &content.input.states);
    }
    ImGui::Separator();
    Field("Selected layer");
    ImGui::Combo("##ClipBlock", &selectedBlock, BlockNames, BlockCount - 1);
    changed |= suite_ui::Checkbox("Arrange on preview", &editLayout);
    Help("Drag a layer to move it. Drag its lower-right corner to resize. "
         "Positions scale with the output resolution.");
    auto &p = content.layout.blocks[selectedBlock];
    changed |= ImGui::SliderFloat("X", &p.x, 0, 1, "%.3f");
    changed |= ImGui::SliderFloat("Y", &p.y, 0, 1, "%.3f");
    changed |= ImGui::SliderFloat("Width", &p.width, selectedBlock == RunInfo ? .55f : .04f, 1, "%.3f");
    changed |=
        ImGui::SliderFloat("Layer opacity", &p.opacity, selectedBlock == RunInfo ? .45f : 0, 1, "%.2f");
    if (suite_ui::Button("Reset layout"))
    {
        content.layout = {};
        changed = true;
    }
    if (selectedBlock != RunInfo && options.inputs)
    {
        ImGui::Separator();
        ImGui::TextColored(g_headerCol, "INPUT APPEARANCE");
        auto before = content.input;
        suite_input::Customization(content.input, selectedBlock - 1);
        changed |= !(before == content.input);
        Help("Input style is shared by the input layers in this clip. Each layer "
             "has independent placement and opacity. Live HUD settings stay "
             "independent.");
    }
}

inline void OutputPanel(bool &changed)
{
    Field("Start (s)");
    changed |= ImGui::InputDouble("##ClipStart", &options.a, .1, 1, "%.3f");
    if (suite_ui::Button("Start here"))
    {
        options.a = previewAt;
        changed = true;
    }
    Field("End (s)");
    changed |= ImGui::InputDouble("##ClipEnd", &options.b, .1, 1, "%.3f");
    if (suite_ui::Button("End here"))
    {
        options.b = previewAt;
        changed = true;
    }
    if (content.markers && !content.markers->markers.empty() && ImGui::TreeNode("Use a marker"))
    {
        for (const auto &marker : content.markers->markers)
        {
            ImGui::PushID(marker.id);
            ImGui::TextUnformatted(marker.name.c_str());
            if (suite_ui::Button("Start"))
            {
                options.a = marker.seconds;
                changed = true;
            }
            ImGui::SameLine();
            if (suite_ui::Button("End"))
            {
                options.b = marker.seconds;
                changed = true;
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    ImGui::Separator();
    int preset = options.preset;
    Field("Export preset");
    if (ImGui::Combo("##ExportPreset", &preset, ExportPresetNames, 10))
    {
        ApplyExportPreset(options, preset);
        changed = true;
    }
    ImGui::TextWrapped("%s", ExportPresetDescriptions[options.preset]);
    const char *formats[] = {"MP4 / Windows H.264", "GIF / animation",        "MKV / FFV1 lossless",
                             "PNG sequence / RGBA", "AVI / UtVideo lossless", "MOV / ProRes HQ",
                             "MP4 / x264"};
    bool audioAvailable =
        content.motion && content.motion->experience.audio.recorded && !content.motion->audioFile.empty();
    ImGui::BeginDisabled(!audioAvailable);
    changed |= suite_ui::Checkbox("Recorded game audio", &options.audio);
    ImGui::EndDisabled();
    Help(
        "The original game mix from the recorded listening perspective. Free cameras keep this mix. Speed changes also change audio pitch. GIF and PNG are silent. Lossless video formats keep float PCM audio; MP4 uses AAC.");
    if (audioAvailable && options.audio)
        changed |= ImGui::SliderFloat("Audio volume", &options.audioVolume, 0, 1, "%.2f");
    if (!audioAvailable)
        ImGui::TextDisabled("Audio unavailable / legacy or missing recording");
    Field("Format");
    if (ImGui::Combo("##ClipFormat", &options.format, formats, 7))
    {
        options.preset = 9;
        options.fps = options.format == 1 ? 20 : 60;
        if (options.format != 3)
            options.transparent = false;
        if (options.format != 6)
            options.maxFileBytes = 0;
        changed = true;
    }
    if (options.format == 3)
    {
        changed |= suite_ui::Checkbox("Transparent overlays only", &options.transparent);
        Help("Straight-alpha RGBA PNG frames and an exact source-time manifest.");
    }
    if (options.format == 6)
    {
        const char *qualities[] = {"Fast / veryfast", "Balanced / medium", "Quality / slow"};
        Field("Compression effort");
        if (ImGui::Combo("##ExportQuality", &options.quality, qualities, 3))
        {
            options.preset = 9;
            changed = true;
        }
        bool limit = options.maxFileBytes != 0;
        if (suite_ui::Checkbox("Maximum file size", &limit))
        {
            options.maxFileBytes = limit ? 25000000 : 0;
            options.preset = 9;
            changed = true;
        }
        Help("Two-pass x264 compression from one temporary lossless capture. The "
             "completed file is checked "
             "before publication; the video is never cut short to meet the limit. "
             "Temporary storage is "
             "additional.");
        if (limit)
        {
            static int unit = 0;
            const char *units[] = {"MB", "GB"};
            double amount = double(options.maxFileBytes) / (unit ? 1000000000. : 1000000.);
            ImGui::SetNextItemWidth(-90 * suite_ui::Unit());
            if (ImGui::InputDouble("##MaxClipSize", &amount, 1, 10, "%.2f") && std::isfinite(amount) &&
                amount > 0 && amount <= 100000)
            {
                options.maxFileBytes =
                    uint64_t(std::clamp(amount * (unit ? 1000000000. : 1000000.), 256000., 100000000000.));
                options.preset = 9;
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1);
            ImGui::Combo("##ClipSizeUnit", &unit, units, 2);
            if (options.b > options.a)
            {
                try
                {
                    ImGui::TextDisabled("Video budget: %.2f Mbit/s", double(SizeBitrate(options)) / 1000000.);
                }
                catch (const std::exception &e)
                {
                    ImGui::TextWrapped("%s", e.what());
                }
            }
        }
    }
    else if (options.format == 2 || options.format == 4)
    {
        ImGui::TextWrapped(options.format == 4 ? "Lossless RGB. File size depends on the scene; "
                                                 "use MP4 for smaller files."
                                               : "Lossless RGB / alpha. File size depends on the scene; "
                                                 "use Size Limit for a capped MP4.");
    }
    const char *sizes[] = {"640 x 360", "1280 x 720", "1920 x 1080", "2560 x 1440"};
    int res = options.width == 2560 ? 3 : options.width == 1920 ? 2 : options.width == 1280 ? 1 : 0;
    Field("Resolution");
    if (ImGui::Combo("##ClipResolution", &res, sizes, 4))
    {
        options.width = res == 3 ? 2560 : res == 2 ? 1920 : res == 1 ? 1280 : 640;
        options.height = options.width * 9 / 16;
        options.preset = 9;
        changed = true;
    }
    const char *videoFps[] = {"30 FPS", "60 FPS", "120 FPS"};
    const char *gifFps[] = {"10 FPS", "20 FPS", "25 FPS"};
    bool gif = options.format == 1;
    int fps = gif ? (options.fps == 25   ? 2
                     : options.fps == 20 ? 1
                                         : 0)
                  : (options.fps == 120  ? 2
                     : options.fps == 60 ? 1
                                         : 0);
    Field("Frame rate");
    if (ImGui::Combo("##ClipFps", &fps, gif ? gifFps : videoFps, 3))
    {
        options.fps = gif ? (fps == 2 ? 25 : fps == 1 ? 20 : 10) : (fps == 2 ? 120 : fps == 1 ? 60 : 30);
        options.preset = 9;
        changed = true;
    }
    if (options.fps == 120)
        ImGui::TextWrapped("120 output FPS. Motion is interpolated from the "
                           "recorded samples; missing detail "
                           "cannot be recreated.");
    ImGui::Separator();
    Field("Default playback speed");
    float rate = float(options.rate);
    if (ImGui::SliderFloat("##ClipRate", &rate, .1f, 4, "%.2fx"))
    {
        options.rate = rate;
        changed = true;
    }
    if (options.speedKeys && options.timing)
        ImGui::TextColored(g_headerCol, "At playhead: %.2fx / speed keypoints", options.SpeedAt(previewAt));
    changed |= suite_ui::Checkbox("Apply speed keypoints", &options.speedKeys);
    Help("Camera keypoints remain active independently. Turn this off to use a "
         "constant playback speed.");
    if (suite_ui::Button("Normal speed (1x)"))
    {
        options.rate = 1;
        options.speedKeys = false;
        changed = true;
    }
    if (freeDiskGiB >= 0)
        ImGui::TextDisabled("Available disk space: %.1f GiB", freeDiskGiB);
    if (options.b > options.a && options.rate > 0)
    {
        auto frames = options.Frames();
        double raw = double(options.width) * options.height * 4 * double(frames) / 1073741824.;
        ImGui::TextDisabled("%.2f s output / %llu frames", options.OutputDuration(),
                            static_cast<unsigned long long>(frames));
        if (options.format >= 2)
            ImGui::TextWrapped(options.format == 3 ? "PNG disk estimate: %.2f GiB (stored RGBA)"
                                                   : "Raw frame volume: %.2f GiB / compressed size varies",
                               raw);
        if (options.maxFileBytes)
        {
            ImGui::TextDisabled("Temporary lossless cache required");
            Help("The scene renders once to UtVideo, then both compression passes read that cache. "
                 "Cache size varies and is separate from the MP4 file limit. MB / GB use decimal units. "
                 "The cache is removed after completion, cancellation or an error.");
            if (freeDiskGiB >= 0 && raw > freeDiskGiB)
                ImGui::TextColored(g_accentCol, "Temporary space may be insufficient");
        }
    }
    ImGui::Separator();
    if (ImGui::TreeNode("Export profiles"))
    {
        const char *slots[] = {"Profile 1", "Profile 2", "Profile 3"};
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##ClipProfile", &profileSlot, slots, 3);
        ImGui::InputText("Name", profileName, sizeof(profileName));
        ImGui::BeginDisabled(!profileIO);
        if (suite_ui::Button("Save profile"))
            profileIO(profileSlot, false, profileName);
        SameLineIfFits("Load profile");
        if (suite_ui::Button("Load profile"))
        {
            profileIO(profileSlot, true, profileName);
            changed = true;
        }
        ImGui::EndDisabled();
        Help("Profiles store output and overlay preferences. Camera keypoints and "
             "the chosen range stay attached to their "
             "own run. Closing this window saves its clip project automatically.");
        ImGui::TreePop();
    }
}

inline int narrowPage = 0;

inline void ClipRangeHere(bool start)
{
    double duration = content.analysis->data->review->seconds;
    if (duration < .02)
        return;
    if (start)
    {
        options.a = std::clamp(previewAt, 0., duration - .02);
        options.b = std::max(options.a + .02, std::isfinite(options.b) ? options.b : duration);
    }
    else
    {
        options.b = std::clamp(previewAt, .02, duration);
        options.a = std::min(options.b - .02, std::isfinite(options.a) ? options.a : 0.);
    }
    Changed();
}

inline void TimeRail(bool busy)
{
    double duration = content.analysis->data->review->seconds;
    float unit = suite_ui::Unit();
    auto *d = ImGui::GetWindowDrawList();
    auto lo = ImGui::GetCursorScreenPos();
    float width = std::max(40.f, ImGui::GetContentRegionAvail().x), height = 78 * unit;
    ImGui::BeginDisabled(busy);
    ImGui::InvisibleButton("##StudioTimeline", {width, height});
    bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    ImGui::EndDisabled();
    auto xAt = [&](double t) {
        return lo.x + float(std::clamp(t / std::max(.001, duration), 0., 1.)) * width;
    };
    auto accent = ImGui::ColorConvertFloat4ToU32(g_headerCol);
    auto ink = ImGui::GetColorU32(ImGuiCol_TextDisabled);
    float y = lo.y + 34 * unit;
    d->AddRectFilled({lo.x, y - 4 * unit}, {lo.x + width, y + 4 * unit}, IM_COL32(31, 45, 53, 255), 4 * unit);
    double a = std::isfinite(options.a) ? options.a : 0, b = std::isfinite(options.b) ? options.b : duration;
    d->AddRectFilled({xAt(a), y - 4 * unit}, {xAt(b), y + 4 * unit}, suite_input::Fade(accent, .45f),
                     4 * unit);
    int ticks = std::clamp(int(width / (110 * unit)), 2, 8);
    for (int i = 0; i <= ticks; i++)
    {
        double t = duration * i / ticks;
        auto value = FormatRunTime(t);
        auto font = ImGui::GetFont();
        float fs = 12 * unit;
        auto size = font->CalcTextSizeA(fs, FLT_MAX, 0, value.c_str());
        float x = std::clamp(xAt(t) - size.x * .5f, lo.x, lo.x + width - size.x);
        d->AddText(font, fs, {x, lo.y + 4 * unit}, ink, value.c_str());
    }
    int nearest = -1;
    float distance = 10 * unit;
    for (size_t i = 0; i < content.track.keys.size(); i++)
    {
        const auto &key = content.track.keys[i];
        float x = xAt(key.seconds), ky = lo.y + 59 * unit;
        ImU32 color = selectedKey == int(i) ? IM_COL32(255, 190, 108, 255) : accent;
        d->AddQuadFilled({x, ky - 5 * unit}, {x + 5 * unit, ky}, {x, ky + 5 * unit}, {x - 5 * unit, ky},
                         color);
        if (std::abs(ImGui::GetIO().MousePos.x - x) < distance &&
            std::abs(ImGui::GetIO().MousePos.y - ky) < 15 * unit)
        {
            nearest = int(i);
            distance = std::abs(ImGui::GetIO().MousePos.x - x);
        }
    }
    float play = xAt(previewAt);
    d->AddLine({play, y - 12 * unit}, {play, lo.y + height - 3 * unit}, accent, 1.6f * unit);
    d->AddCircleFilled({play, y}, 5 * unit, accent);
    static bool pointClick = false;
    if (!ImGui::IsMouseDown(0))
        pointClick = false;
    if (!busy && hovered && nearest >= 0)
    {
        auto &key = content.track.keys[nearest];
        ImGui::SetTooltip("Shot %s / %s / %s", PointLabel(nearest).c_str(), key.name.c_str(),
                          TransitionNames[int(key.next)]);
        if (ImGui::IsMouseClicked(0))
        {
            pointClick = true;
            EditKey(size_t(nearest));
        }
    }
    else if (!busy && held && ImGui::IsMouseDown(0) && !pointClick)
        Seek(std::clamp(double((ImGui::GetIO().MousePos.x - lo.x) / width) * duration, 0., duration));
}

inline std::string WindowTitle()
{
    return std::string("Clip Studio / PracticeSuite ") + suite_brand::Version + " / mab";
}

inline void Render()
{
    if (!open || !content.analysis)
        return;
    if (g_pShared && g_pShared->timerState == 1)
    {
        cancelRequested = true;
        open = false;
        return;
    }
    content.font = suite_ui::hudFont ? suite_ui::hudFont : ImGui::GetFont();
    content.palette = suite_input::Colors(1);
    guideCache.Bind(content, revision);
    const auto &run = *content.analysis->data->review;
    bool busy = encoder.State() == 1 || awaitingFullRunCheck;
    auto display = ImGui::GetIO().DisplaySize;
    float unit = suite_ui::Unit();
    ImGui::SetNextWindowPos({display.x * .5f, display.y * .5f}, ImGuiCond_Appearing, {.5f, .5f});
    ImGui::SetNextWindowSize({std::max(280.f, display.x - 24), std::max(200.f, display.y - 24)},
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints({std::min(600.f, display.x - 24), std::min(300.f, display.y - 24)},
                                        {display.x, display.y});
    if (focusWindow)
    {
        ImGui::SetNextWindowFocus();
        focusWindow = false;
    }
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10 * unit);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6 * unit);
    ImGui::Begin(WindowTitle().c_str(), nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings);
    bool narrow = ImGui::GetContentRegionAvail().x < 850 * unit;
    if (!narrow)
    {
        auto p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddText(suite_ui::hudFont ? suite_ui::hudFont : ImGui::GetFont(),
                                            22 * unit, p, ImGui::ColorConvertFloat4ToU32(g_headerCol),
                                            "CLIP STUDIO");
        ImGui::Dummy({180 * unit, 30 * unit});
        ImGui::SameLine();
        ImGui::TextDisabled("REPLAY   /   Seed %d   /   %.2f s output", content.analysis->data->seed,
                            std::max(0., options.OutputDuration()));
    }
    else
    {
        float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
        bool page = narrowPage == 0;
        ImGui::PushStyleColor(ImGuiCol_Button, page ? ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)
                                                    : ImGui::GetStyleColorVec4(ImGuiCol_Button));
        if (suite_ui::Button("Preview", {w, 0}))
            narrowPage = 0;
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (suite_ui::Button("Settings", {w, 0}))
            narrowPage = 1;
    }
    float footer = std::min((narrow ? 110.f : 78.f) * unit, ImGui::GetContentRegionAvail().y * .38f);
    ImGui::BeginChild("Clip body", {0, -footer}, false);
    float controls = std::min(390 * unit, ImGui::GetContentRegionAvail().x * .34f);
    bool changed = false;
    if (!narrow || narrowPage == 0)
    {
        float previewWidth =
            narrow ? 0 : std::max(120.f, ImGui::GetContentRegionAvail().x - controls - 14 * unit);
        ImGui::BeginChild("Clip preview", {previewWidth, 0}, true, ImGuiWindowFlags_HorizontalScrollbar);
        ImGui::BeginDisabled(busy);
        if (suite_ui::Button(playPreview ? "Pause preview" : "Play preview"))
            TogglePlayback();
        SameLineIfFits("100% preview");
        if (suite_ui::Checkbox("100% preview", &actualSize))
            Changed();
        SameLineIfFits("Arrange overlays");
        suite_ui::Checkbox("Arrange overlays", &editLayout);
        ImGui::EndDisabled();
        float fit = std::min(ImGui::GetContentRegionAvail().x / options.width,
                             std::max(40.f, ImGui::GetContentRegionAvail().y - 132 * unit) / options.height),
              scale = actualSize ? 1 : std::max(.03f, fit);
        ImVec2 size(options.width * scale, options.height * scale), origin = ImGui::GetCursorScreenPos();
        previewPixelWidth = std::max(320.f, size.x);
        auto *d = ImGui::GetWindowDrawList();
        d->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y}, IM_COL32(8, 15, 22, 255), 8 * unit);
        if (options.transparent)
        {
            float tile = 20 * unit;
            for (float y = 0; y < size.y; y += tile)
                for (float x = 0; x < size.x; x += tile)
                    d->AddRectFilled(
                        {origin.x + x, origin.y + y},
                        {origin.x + std::min(x + tile, size.x), origin.y + std::min(y + tile, size.y)},
                        (int(x / tile) + int(y / tile)) % 2 ? IM_COL32(35, 43, 50, 255)
                                                            : IM_COL32(23, 30, 37, 255));
        }
        bool direct = (!options.scene3d || options.transparent) && options.format != 1;
        bool imageReady = previewImage && previewRun == run.id;
        if (imageReady && !direct)
        {
            ImGui::Image(previewImage, size);
            if (showCameraPath && shownPoseReady)
                WorldCameraGuides(d, guideCache, shownPose, origin, size);
            bool updating = previewRevision != revision || previewEpoch != cameraEpoch ||
                            (!playPreview && std::abs(previewShown - previewAt) > 1e-7);
            char caption[96];
            std::snprintf(caption, sizeof(caption),
                          updating ? "Updating preview / showing %.6f s" : "Preview / %.6f s", previewShown);
            d->AddRectFilled({origin.x + 10, origin.y + size.y - 35},
                             {origin.x + std::min(size.x - 10, 410.f * unit), origin.y + size.y - 8},
                             IM_COL32(10, 18, 25, 220), 5);
            d->AddText({origin.x + 18, origin.y + size.y - 30},
                       updating ? IM_COL32(255, 195, 108, 255) : IM_COL32(130, 238, 217, 255), caption);
        }
        else
        {
            auto previewOptions = options;
            previewOptions.scene3d = false;
            ComposeViewport(d, previewOptions, content, previewAt, origin, size);
            ImGui::Dummy(size);
            if (options.scene3d && !options.transparent)
                d->AddText(
                    ImGui::GetFont(), 13 * unit,
                    {origin.x + 24 * unit, origin.y + 94 * (options.width / 1280.f) * scale + 10 * unit},
                    IM_COL32(255, 194, 109, 255), "MAP PREVIEW / waiting for 3D renderer");
        }
        ImGui::SetCursorScreenPos(origin);
        ImGui::BeginDisabled(busy);
        ImGui::InvisibleButton("##ClipCanvas", size,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        ImGui::EndDisabled();
        changed |= PreviewInteraction(origin, size, busy);
        ImGui::Spacing();
        ImGui::BeginDisabled(busy);
        if (suite_ui::Button("[ In"))
            ClipRangeHere(true);
        ImGui::SameLine();
        if (suite_ui::Button("Out ]"))
            ClipRangeHere(false);
        ImGui::SameLine();
        ImGui::TextColored(g_headerCol, "%s", FormatRunTime(previewAt).c_str());
        SameLineIfFits("60 FPS");
        ImGui::TextDisabled("/ %s   %d FPS   %.2fx", FormatRunTime(run.seconds).c_str(), options.fps,
                            options.SpeedAt(previewAt));
        ImGui::EndDisabled();
        TimeRail(busy);
        ImGui::EndChild();
        if (!narrow)
            ImGui::SameLine();
    }
    if (!narrow || narrowPage == 1)
    {
        ImGui::BeginChild("Clip settings", {0, 0}, false);
        ImGui::BeginDisabled(busy);
        if (ImGui::BeginTabBar("Clip workspace"))
        {
            if (ImGui::BeginTabItem("Camera"))
            {
                CameraPanel(changed);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Overlays"))
            {
                OverlaysPanel(changed);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Output"))
            {
                OutputPanel(changed);
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Run check"))
            {
                Help(
                    "Full run videos include a fresh replay check and a five-second result card. Clip exports keep your chosen range and camera edits.");
                auto result = practice::g_replayArchive.Get()->revalidation;
                if (result && result->run == run.id)
                    content.revalidation = result;
                if (suite_ui::Button("Recheck this replay"))
                    practice::g_replayArchive.Revalidate(content.analysis->data);
                auto check = content.revalidation;
                if (check)
                {
                    ImGui::TextColored(check->filesOk ? ImVec4(.3f, .88f, .63f, 1) : g_accentCol, "%s",
                                       check->filesOk ? "Replay files checked" : "Check failed");
                    if (!check->error.empty())
                        Help(check->error.c_str());
                    if (suite_ui::Button("Copy check report"))
                        ImGui::SetClipboardText(practice::RevalidationReport(*check).c_str());
                }
                auto preview = options;
                preview.width = 1280;
                preview.height = 720;
                auto origin = ImGui::GetCursorScreenPos();
                float width = ImGui::GetContentRegionAvail().x;
                ComposeViewport(ImGui::GetWindowDrawList(), preview, content, run.seconds, origin,
                                {width, width * 9 / 16}, 1);
                ImGui::Dummy({width, width * 9 / 16});
                Help(
                    "Local file checks preserve the original run verdict. They cannot make editable files tamper-proof or replace a leaderboard review.");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndDisabled();
        ImGui::EndChild();
    }
    ImGui::EndChild();
    std::string invalid;
    try
    {
        options.Validate(run.seconds);
        content.track.Validate(run.seconds);
        content.layout.Validate();
        if (options.scene3d && options.camera == 3 && content.track.keys.empty())
            throw std::runtime_error("Capture at least one camera shot first");
    }
    catch (const std::exception &e)
    {
        invalid = e.what();
    }
    if (awaitingFullRunCheck)
    {
        ImGui::TextColored(g_headerCol, "Checking replay files...");
        SameLineIfFits("Cancel check");
        if (suite_ui::Button("Cancel check"))
            awaitingFullRunCheck = false;
    }
    else if (busy)
    {
        char count[80];
        std::snprintf(count, sizeof(count), "%llu / %llu frames",
                      static_cast<unsigned long long>(encoder.Written()),
                      static_cast<unsigned long long>(exportFrameCount));
        ImGui::ProgressBar(float(encoder.Written()) / std::max(1.f, float(exportFrameCount)),
                           {std::max(50.f, ImGui::GetContentRegionAvail().x - 130 * unit), 0}, count);
        SameLineIfFits("Cancel export");
        if (suite_ui::Button("Cancel export"))
            cancelRequested = true;
    }
    else
    {
        ImGui::BeginDisabled(!invalid.empty() || !backendAvailable);
        if (suite_ui::Button(narrow ? "Export" : "Export clip"))
        {
            startRequested = true;
            testRequested = fullRunRequested = false;
            playPreview = false;
            error.clear();
            Changed();
        }
        SameLineIfFits(narrow ? "3 s test" : "3 s test export");
        if (suite_ui::Button(narrow ? "3 s test" : "3 s test export"))
        {
            startRequested = testRequested = true;
            fullRunRequested = false;
            playPreview = false;
            error.clear();
            Changed();
        }
        ImGui::EndDisabled();
        SameLineIfFits("Full run video");
        bool fullReady = backendAvailable;
        try
        {
            FullRunOptions(options, content);
        }
        catch (const std::exception &)
        {
            fullReady = false;
        }
        ImGui::BeginDisabled(!fullReady || awaitingFullRunCheck);
        if (suite_ui::Button("Full run video"))
        {
            content.revalidation.reset();
            startRequested = fullRunRequested = true;
            testRequested = playPreview = false;
            error.clear();
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(
                "Export the whole run: original camera, 1x speed and recorded audio.\nA fresh file check and a 5-second replay-check card are included.\n"
                "Uses your video output settings; GIF / PNG use 1080p60 MP4.\n"
                "Clip edits stay saved. This exports a rendered replay.");
        SameLineIfFits("Close");
        if (suite_ui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape))
            Close();
        if (encoder.State() == 2 && openDirectory)
        {
            SameLineIfFits("Open export folder");
            if (suite_ui::Button("Open export folder"))
                openDirectory();
        }
    }
    if (!invalid.empty())
    {
        ImGui::TextColored(g_accentCol, "Check clip settings");
        Help(invalid.c_str());
    }
    else if (!error.empty())
    {
        ImGui::TextColored(g_accentCol, "Preview / project details");
        Help(error.c_str());
    }
    else if (encoder.State() == 3)
    {
        ImGui::TextColored(g_accentCol, "Export failed / details");
        Help(encoder.Error().c_str());
    }
    else
    {
        ImGui::TextDisabled("%s", busy                   ? status.c_str()
                                  : encoder.State() == 2 ? "Export complete"
                                  : encoder.State() == 4 ? "Export cancelled"
                                                         : "K: capture shot   /   Space: play   /   "
                                                           "RMB + WASD / Q E: fly   /   Wheel: travel");
        Help("Preview fit mode uses a smaller scene render. 100% and exports use "
             "the chosen output resolution. Camera changes keep playback running. "
             "Lossless MKV uses fixed frame timing and a faster decoding profile; "
             "files are larger. Best Quality uses lossless UtVideo / AVI and requires a compatible decoder.");
    }
    if (!busy && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::GetIO().WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_K) && options.scene3d)
            SaveKey(false);
        if (ImGui::IsKeyPressed(ImGuiKey_Space))
            TogglePlayback();
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            Seek(options.Advance(previewAt, -1. / options.fps));
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            Seek(options.Advance(previewAt, 1. / options.fps));
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    if (pathOverview)
    {
        guideCache.Bind(content, revision);
        if (auto selected = CameraPathOverview(guideCache, content, previewAt, pathOverview);
            selected && !busy)
            EditKey(*selected);
    }
    if (changed)
        Changed();
}
} // namespace suite_clip

inline void RequestClipFrom3D()
{
    suite_clip::requestOpen = true;
}

inline bool ClipInputRequested()
{
    return suite_clip::open;
}
