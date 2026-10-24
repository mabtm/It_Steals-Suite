#pragma once
#include "ReplayArchive.h"
#include "Replay3DControls.h"
#include "ReplayMarkersUI.h"
#include "InputUI.h"

inline void StopReplay3D()
{
    suite_replay::controls = false;
    if (g_pShared && g_pShared->replay3dState >= 1 && g_pShared->replay3dState <= 3)
        g_commands.replay3dCmd = 2;
}

inline bool Replay3DActive(int64_t id)
{
    return g_pShared && g_pShared->timerState != 1 && g_pShared->replay3dRunId == id &&
           (g_pShared->replay3dState == 2 || g_pShared->replay3dState == 3);
}

inline void Replay3DControl(const practice::RunReview &r, const practice::ReviewPlayback &playback,
                            bool observer, bool seek = true)
{
    if (!Replay3DActive(r.id) || g_commands.replay3dCmd == 2 || g_commands.replay3dCmd == 1)
        return;
    g_commands.replay3dRunId = r.id;
    g_commands.replay3dSeconds = std::clamp(playback.seconds, 0., r.seconds);
    suite_replay::Apply(g_commands, playback.playing, observer);
    if (seek)
        g_commands.replay3dOptions |= suite_replay::Seek;
    g_commands.replay3dSpeed = playback.speed;
    g_commands.replay3dCmd = 3;
}

inline std::shared_ptr<const practice::ReplayMotion> ReplayMotionFor(
    std::shared_ptr<const practice::MapData> data, bool retained = true)
{
    static const practice::MapData *requested = nullptr;
    static int64_t id = 0;
    static double nextTry = 0;
    if (!retained || !data || !data->review || !data->review->motion)
        return {};
    auto result = practice::g_replayArchive.Motion();
    if (result && result->Matches(*data))
        return result;
    auto state = practice::g_replayArchive.Get();
    if (requested != data.get() || id != data->review->id ||
        (!state->motionError.empty() && ImGui::GetTime() >= nextTry))
    {
        requested = data.get();
        id = data->review->id;
        nextTry = ImGui::GetTime() + 2;
        practice::g_replayArchive.LoadMotion(data);
    }
    return {};
}

inline std::string ReplayInputName(uint32_t keys)
{
    std::string out;
    for (const auto &pair : std::array<std::pair<uint32_t, const char *>, 7>{
             {{1, "W"}, {2, "A"}, {4, "S"}, {8, "D"}, {16, "Left Shift"}, {32, "Right Shift"}, {64, "R"}}})
        if (keys & pair.first)
        {
            if (!out.empty())
                out += " + ";
            out += pair.second;
        }
    return out.empty() ? "--" : out;
}

inline void RenderReplayInputs(const practice::ReplayMotion &motion, practice::ReviewPlayback &replay)
{
    if (!suite_ui::CollapsingHeader("Recorded inputs", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    if (!motion.devices.empty())
    {
        auto f = suite_input::ReplayFrame(motion, replay.seconds);
        if (ImGui::BeginTabBar("Recorded input devices"))
        {
            const char *tabs[] = {"Keyboard", "Mouse", "Controller", "Player"};
            for (int device = 0; device < 4; device++)
                if (suite_ui::BeginTabItem(tabs[device]))
                {
                    auto p = suite_input::preferences;
                    p.keyboard = device == 0;
                    p.mouse = device == 1;
                    p.controller = device == 2;
                    p.states = device == 3;
                    p.events = false;
                    p.fullKeyboard = false;
                    auto size = suite_input::Size(p);
                    float w = std::min(ImGui::GetContentRegionAvail().x, size.x * suite_ui::Unit());
                    auto a = ImGui::GetCursorScreenPos();
                    suite_input::Draw(ImGui::GetWindowDrawList(), a, {a.x + w, a.y + w * size.y / size.x},
                                      f ? &*f : nullptr, p, {}, replay.seconds,
                                      suite_ui::hudFont ? suite_ui::hudFont : ImGui::GetFont(),
                                      suite_input::Colors(.9f));
                    ImGui::Dummy({w, w * size.y / size.x});
                    suite_ui::EndTabItem();
                }
            ImGui::EndTabBar();
        }
    }
    const auto *input = practice::ReplayInputAt(motion, replay.seconds);
    const auto *frame = practice::ReplayViewAt(motion, replay.seconds);
    uint32_t keys = input ? input->keys : 0;
    float u = suite_ui::Unit();
    if (motion.devices.empty())
    {
        float height = 34 * u, gap = 5 * u,
              width = std::max(28 * u, std::min(54 * u, (ImGui::GetContentRegionAvail().x - 3 * gap) / 4));
        auto origin = ImGui::GetCursorScreenPos();
        auto *draw = ImGui::GetWindowDrawList();
        auto key = [&](const char *name, int col, int row, uint32_t mask, bool pulse = false) {
            ImVec2 a{origin.x + (width + gap) * col, origin.y + (height + gap) * row},
                b{a.x + width, a.y + height};
            bool down = (keys & mask) != 0 || pulse;
            auto fill =
                down ? suite_ui::Alpha(suite_ui::heading, .27f) : suite_ui::Alpha(suite_ui::background, .6f);
            draw->AddRectFilled(a, b, ImGui::GetColorU32(fill), 5 * u);
            draw->AddRect(a, b,
                          ImGui::GetColorU32(down ? suite_ui::heading : suite_ui::Alpha(suite_ui::ink, .2f)),
                          5 * u, 0, u);
            auto text = ImGui::CalcTextSize(name);
            draw->AddText({a.x + (width - text.x) * .5f, a.y + (height - text.y) * .5f},
                          ImGui::GetColorU32(down ? suite_ui::ink : suite_ui::Alpha(suite_ui::ink, .45f)),
                          name);
        };
        key("W", 1, 0, 1);
        key("A", 0, 1, 2);
        key("S", 1, 1, 4);
        key("D", 2, 1, 8);
        key("Shift", 0, 0, 48);
        key("R", 2, 0, 64, input && input->actions && replay.seconds - input->seconds < .15);
        ImGui::Dummy({3 * width + 2 * gap, 2 * height + gap});
        Help(
            "Game-local keyboard state. Effective movement/look axes are recorded separately, including controller input. A key press is not proof that the game accepted an action. No system-wide input recording is used.");
        if (input)
        {
            ImGui::Text("Sprint %s  /  Radar %s", input->state & 4 ? "ON" : "OFF",
                        input->state & 8 ? "ON" : "OFF");
            if (!(input->state & 1) || (input->state & 2))
                ImGui::TextColored(g_accentCol, "%s",
                                   input->state & 2 ? "Trainer owned input" : "Game was unfocused");
        }
        else
            ImGui::TextDisabled("No input sample at this time");
    }
    if (frame)
    {
        ImGui::TextDisabled("Frame %u / physics %u", frame->frame, frame->tick);
        ImGui::Text("Move  %.2f / %.2f", frame->horizontal, frame->vertical);
        Help(
            "Last movement axes read by the game. Unity axis smoothing and controller inputs may differ from keyboard state.");
        auto q = frame->cameraRotation;
        double x = 2 * (double(q.x) * q.z + double(q.w) * q.y),
               y = 2 * (double(q.y) * q.z - double(q.w) * q.x),
               z = 1 - 2 * (double(q.x) * q.x + double(q.y) * q.y);
        double yaw = std::atan2(x, z) * 180 / 3.141592653589793,
               pitch = std::atan2(-y, std::sqrt(x * x + z * z)) * 180 / 3.141592653589793;
        ImGui::TextDisabled("View  %.1f / %.1f deg", yaw, pitch);
        Help(
            "Actual camera orientation. Mouse and joystick axes, camera position, field of view and delayed flashlight rotation are preserved in the recording.");
    }
    else
        ImGui::TextDisabled("Camera sample unavailable");
    if (suite_ui::Button("< Frame"))
    {
        replay.seconds = practice::ReplayFrameStep(motion, replay.seconds, false);
        replay.playing = false;
    }
    SameLineIfFits("Frame >");
    if (suite_ui::Button("Frame >"))
    {
        replay.seconds = practice::ReplayFrameStep(motion, replay.seconds, true);
        replay.playing = false;
    }
    SameLineIfFits("Events");
    if (suite_ui::Button("Events"))
        ImGui::OpenPopup("Input events");
    ImGui::SetNextWindowSize({std::min(640.f * u, ImGui::GetIO().DisplaySize.x - 30),
                              std::min(380.f * u, ImGui::GetIO().DisplaySize.y - 40)},
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopup("Input events"))
    {
        ImGui::TextDisabled("Observed transitions / run clock");
        ImGui::BeginChild("Event list", {0, 0}, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
        ImGuiListClipper clipper;
        clipper.Begin(int(motion.events.size()));
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                const auto &e = motion.events[i];
                ImGui::PushID(i);
                std::string label =
                    FormatRunTime(e.seconds) + "  " +
                    (i == 0 ? "Initial: " + ReplayInputName(e.keys)
                            : "Down: " + ReplayInputName(e.down) + " / Up: " + ReplayInputName(e.up)) +
                    (e.actions ? " / Radar requested" : "");
                if (ImGui::Selectable(label.c_str()))
                {
                    replay.seconds = e.seconds;
                    replay.playing = false;
                }
                Help(("Frame " + std::to_string(e.frame) + " / physics " + std::to_string(e.tick) +
                      "\nRadar " + (e.state & 8 ? std::string("active") : std::string("inactive")))
                         .c_str());
                ImGui::PopID();
            }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
    if (motion.info->flags)
    {
        ImGui::TextColored(g_accentCol, "Capture contains gaps or limits");
        Help(("Flags " + std::to_string(motion.info->flags) + " / dropped " +
              std::to_string(motion.info->dropped) +
              ". Missing camera samples are never presented as exact movement.")
                 .c_str());
    }
}

inline bool g_replayObserver = false;
inline bool RenderReplay3DSettings(const practice::ReplayMotion *motion);

inline void RenderReplay3D(std::shared_ptr<const practice::MapData> data, practice::ReviewPlayback &replay,
                           bool available)
{
    static int64_t waiting = 0, enter = 0;
    auto state = practice::g_replayArchive.Get();
    const auto &r = *data->review;
    bool active = Replay3DActive(r.id), loading = g_pShared && g_pShared->replay3dState == 1;
    ImGui::BeginDisabled(!available || !g_pShared || g_pShared->timerState == 1 || HasPendingRequest() ||
                         loading || waiting != 0);
    if (suite_ui::Button(active ? "Restart 3D" : "3D preview"))
    {
        if (!active)
            replay.speed = 1;
        suite_replay::Bind(data);
        practice::g_replayArchive.Stage3D(data);
        waiting = r.id;
        enter = r.id;
    }
    ImGui::EndDisabled();
    Help(
        "Original scene renderers with directional depth lighting, subtle palette variation and no shadows. Starts at 1x speed. Original renderer batching is preserved; game updates and coroutines pause temporarily. The replay camera and markers have no colliders. A different level is loaded if needed; spawn/crystal positions are checked. Monster markers use sampled tracks. Animations, radar visuals and full world history are not recorded.");
    if (waiting && waiting != r.id)
    {
        waiting = 0;
        enter = 0;
    }
    if (waiting == r.id && state->stagedRun == r.id && !HasPendingRequest() && g_pShared)
    {
        g_commands.replay3dCmd = 1;
        g_commands.replay3dRunId = r.id;
        g_commands.replay3dSeconds = std::clamp(replay.seconds, 0., r.seconds);
        suite_replay::Apply(g_commands, replay.playing, g_replayObserver);
        g_commands.replay3dSpeed = replay.speed;
        waiting = 0;
    }
    else if (waiting && !state->busy && !state->error.empty())
        waiting = 0;
    if (active || loading)
    {
        SameLineIfFits("Stop 3D");
        if (suite_ui::Button("Stop 3D"))
        {
            g_commands.replay3dCmd = 2;
            waiting = 0;
        }
        if (active)
        {
            if (suite_ui::Button("View 3D"))
            {
                suite_replay::controls = false;
                g_overlayVisible = false;
                SetOverlayInteractive(false);
            }
            SameLineIfFits("View options");
            if (suite_ui::Button("View options"))
                ImGui::OpenPopup("Replay view options");
            if (ImGui::BeginPopup("Replay view options"))
            {
                auto motion = practice::g_replayArchive.Motion();
                if (RenderReplay3DSettings(motion.get()))
                    Replay3DControl(r, replay, g_replayObserver, false);
                ImGui::EndPopup();
            }
        }
    }
    if (active)
    {
        ImGui::TextDisabled("Depth-lit scene / %.2fx", replay.speed);
        Help(g_pShared->replay3dMessage);
    }
    if (active && enter == r.id)
    {
        enter = 0;
        suite_replay::controls = false;
        g_overlayVisible = false;
        g_compactMode = false;
        SetOverlayInteractive(false);
    }
    if (loading || waiting)
        ImGui::TextDisabled("Preparing 3D replay...");
    if (g_pShared && g_pShared->replay3dState == 4)
    {
        ImGui::TextColored(g_accentCol, "3D replay unavailable");
        Help(g_pShared->replay3dMessage);
    }
}

inline void RenderReplayTransfer(std::shared_ptr<const practice::MapData> data, bool retained)
{
    static int64_t requested = 0;
    auto state = practice::g_replayArchive.Get();
    static uint64_t recheckRequest = 0;
    bool checking = recheckRequest && (!state->revalidation || state->revalidation->request < recheckRequest);
    ImGui::BeginDisabled(!retained || checking);
    if (suite_ui::Button(checking ? "Checking replay..." : "Recheck replay"))
        recheckRequest = practice::g_replayArchive.Revalidate(data);
    ImGui::EndDisabled();
    Help(
        "Read this replay and its companions again from disk. Checks identity, checksums, timelines and stored validation evidence. Historical warnings stay unchanged; this is a local file check.");
    auto checked = state->revalidation;
    if (checked && checked->run == data->review->id)
    {
        ImGui::TextColored(checked->filesOk ? ImVec4(.3f, .88f, .63f, 1) : g_accentCol, "%s",
                           checked->filesOk ? "Replay files checked" : "Replay check failed");
        if (!checked->error.empty())
            Help(checked->error.c_str());
        if (checked->coverage)
            ImGui::TextDisabled("Coverage: %d complete / %d partial / %d missing / %d unknown",
                                checked->coverage->complete, checked->coverage->partial,
                                checked->coverage->missing, checked->coverage->unknown);
        if (suite_ui::Button("Copy replay check"))
            ImGui::SetClipboardText(practice::RevalidationReport(*checked).c_str());
    }
    ImGui::BeginDisabled(!retained);
    if (suite_ui::Button("Export package"))
    {
        requested = data->review->id;
        practice::g_replayArchive.Export(data);
    }
    ImGui::EndDisabled();
    Help(
        "Export this retained run as one ZIP: original replay, input/camera recording when present, notes, validation report, versions and SHA-256 consistency hashes. Older recordings remain exportable with their available evidence.");
    if (requested == data->review->id && state->exportedRun == requested)
    {
        SameLineIfFits("Copy path");
        if (suite_ui::Button("Copy path"))
            ImGui::SetClipboardText(state->exportPath.c_str());
        Help(state->exportPath.c_str());
        ImGui::TextDisabled("Package exported");
    }
    if (!state->error.empty())
    {
        ImGui::TextColored(g_accentCol, "Archive / export warning");
        Help(state->error.c_str());
    }
}

inline void RenderReplayImport()
{
    static char path[2048]{};
    if (suite_ui::Button("Import package"))
        ImGui::OpenPopup("Import run package");
    Help(
        "Import a PracticeSuite ZIP. Checksums, run identity and companion files are checked before preserving the recording in the archive.");
    ImGui::SetNextWindowSize({std::min(600.f * suite_ui::Unit(), ImGui::GetIO().DisplaySize.x - 30), 0},
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopup("Import run package"))
    {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##Package path", "Full path to run-....zip", path, sizeof(path));
        ImGui::BeginDisabled(path[0] == 0);
        if (suite_ui::Button("Import"))
        {
            std::string value = path;
            if (value.size() > 1 && value.front() == '\"' && value.back() == '\"')
                value = value.substr(1, value.size() - 2);
            practice::g_replayArchive.ImportPackage(
                std::filesystem::path(std::u8string(value.begin(), value.end())));
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
}

inline void RequestClipFrom3D();
#include "Replay3DViewUI.h"

inline void RenderReplay3DOverlay()
{
    RenderReplay3DDock();
}

inline bool Replay3DShortcut(int action)
{
    using namespace suite_replay;
    if (!source || !Replay3DActive(source->review->id) || g_commands.replay3dCmd == 2)
        return false;
    auto motion = practice::g_replayArchive.Motion();
    if (!motion || !motion->Matches(*source))
        return false;
    auto &replay = playback;
    replay.Bind(source->review->id);
    replay.seconds = std::clamp(double(g_pShared->replay3dSeconds), 0., source->review->seconds);
    replay.playing = g_pShared->replay3dState == 2;
    replay.speed =
        std::isfinite(g_pShared->replay3dSpeed) ? std::clamp(float(g_pShared->replay3dSpeed), .1f, 8.f) : 1;
    if (action == 0)
    {
        if (replay.seconds >= source->review->seconds)
            replay.seconds = marks && marks->loop && marks->LoopValid() ? marks->a : 0;
        replay.playing = !replay.playing;
    }
    else if (action == 1 || action == 2)
    {
        replay.seconds = practice::ReplayFrameStep(*motion, replay.seconds, action == 2);
        replay.playing = false;
    }
    else if (action == 3 || action == 4)
    {
        replay.seconds = action == 3 ? 0 : source->review->seconds;
        replay.playing = false;
    }
    else if (action == 5 || action == 6)
        replay.speed = std::clamp(replay.speed * (action == 5 ? 1.25f : .8f), .1f, 8.f);
    else
        return false;
    Replay3DControl(*source->review, replay, camera == 2, action < 5);
    return true;
}
