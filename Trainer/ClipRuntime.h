#pragma once
#include "ClipSurface.h"
#include "ClipUI.h"
#ifdef _WIN32
namespace suite_clip
{
inline Surface surface;
inline FrameBridge bridge;
inline Options frozen;
inline Content frozenContent, pendingComposition;
inline Options pendingOptions;
inline bool exporting = false, finishing = false;
inline uint64_t submitted = 0, pendingRevision = 0;
inline FrameWindow exportWindow;
inline uint64_t composed = 0, exportRevision = 0, exportEpoch = 0;
inline std::shared_ptr<ExportPipelineStats> pipelineStats;
inline int frameWidth = 0, frameHeight = 0;
inline uint64_t frameCameraEpoch = 0;
inline ULONGLONG previewTick = 0;
inline std::vector<uint8_t> scenePixels;
inline int64_t frameRun = 0;
inline double requestedAt = 0;
inline ULONGLONG waitingSince = 0, channelSince = 0;
inline int64_t staged = 0;

inline std::filesystem::path Directory()
{
    return g_settings.Directory() / L"Clips";
}

inline void OpenDirectory()
{
    auto dir = Directory();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

inline Project CurrentProject()
{
    Project p;
    p.run = content.analysis->data->review->id;
    p.duration = content.analysis->data->review->seconds;
    p.gameHash = content.analysis->data->hash;
    p.options = options;
    p.content = content;
    return p;
}

inline std::filesystem::path ProjectPath()
{
    return Directory() / L"Projects" /
           (L"run-" + std::to_wstring(content.analysis->data->review->id) + L".isclip");
}

inline void SaveProject()
{
    if (!content.analysis)
        return;
    try
    {
        StoreProject(ProjectPath(), CurrentProject());
    }
    catch (const std::exception &e)
    {
        error = e.what();
    }
}

inline void LoadProject()
{
    try
    {
        auto path = ProjectPath();
        if (!std::filesystem::exists(path))
            return;
        auto &map = *content.analysis->data;
        auto p = DecodeProject(ReadProjectFile(path), map.review->id, map.review->seconds, map.hash);
        ApplyProject(p, options, content);
    }
    catch (const std::exception &e)
    {
        error = e.what();
    }
}

inline void ProfileIO(int slot, bool load, const char *name)
{
    try
    {
        auto path =
            Directory() / L"Profiles" / (L"profile-" + std::to_wstring(std::clamp(slot, 0, 2)) + L".isclip");
        if (load)
        {
            auto p = DecodeProject(ReadProjectFile(path), 0, 1, "");
            ApplyProject(p, options, content, true);
            std::snprintf(profileName, sizeof(profileName), "%s", p.name.c_str());
            status = "Export profile loaded";
        }
        else
        {
            auto p = CurrentProject();
            p.run = 0;
            p.duration = 1;
            p.gameHash.clear();
            p.name = name;
            p.options.a = 0;
            p.options.b = 1;
            p.content.track.keys.clear();
            p.content.orbit = {};
            p.content.freePose = {};
            p.content.freeReady = false;
            StoreProject(path, p);
            status = "Export profile saved";
        }
    }
    catch (const std::exception &e)
    {
        error = e.what();
    }
}

inline uint32_t CaptureOptions(const Options &o)
{
    return (o.camera >= 2   ? 2u
            : o.camera == 1 ? 4u
                            : 0u) |
           (o.colliders ? (8u | 32u | (o.monster ? 16u : 0u) | 512u) : 0u) | (o.route ? 128u : 0u) |
           (o.hideCeiling ? 2048u : 0u) | (o.fadeWalls && o.camera == 1 ? 4096u : 0u) |
           (o.monster ? 8192u : 0u) | (o.simplified ? 16384u : 0u) | (o.fullbright ? 32768u : 0u);
}

inline bool RequestSceneFrame(const Content &c, const Options &o, double at)
{
    auto frame = ResolveFrame(o, c, at);
    if (!frame.pose)
        throw std::runtime_error("No camera recording at the requested time");
    if (!bridge.Request(c.analysis->data->review->id, at, o.width, o.height, CaptureOptions(frame.options),
                        &*frame.pose, (playPreview && o.AudioEnabled()) ? float(o.SpeedAt(at)) : 0,
                        o.audioVolume))
        return false;
    requestedAt = at;
    frameWidth = o.width;
    frameHeight = o.height;
    frameRun = c.analysis->data->review->id;
    pendingRevision = revision;
    frameCameraEpoch = cameraEpoch;
    pendingOptions = frame.options;
    pendingComposition = CompositionSnapshot(c);
    previewTick = GetTickCount64();
    return true;
}

inline void ResetGraphics()
{
    surface.Reset();
    bridge.Close();
    previewImage = {};
    previewRun = 0;
    pendingComposition = {};
    backendAvailable = false;
    if (encoder.State() == 1)
    {
        encoder.Cancel();
        error = "Graphics changed; clip export was cancelled. Retry after the "
                "window is ready.";
    }
    exporting = finishing = false;
    staged = 0;
    waitingSince = channelSince = 0;
}

inline bool Prepare3D(const Content &c, const Options &o)
{
    if (!g_pShared)
    {
        throw std::runtime_error("Connect the game to export a 3D replay");
    }
    int64_t id = c.analysis->data->review->id;
    if (g_pShared->replay3dRunId == id && (g_pShared->replay3dState == 2 || g_pShared->replay3dState == 3))
    {
        bool simple = (g_pShared->replay3dOptions & 16384u) != 0;
        if (simple == o.simplified)
            return true;
        if (!HasPendingRequest())
        {
            g_commands.replay3dCmd = 3;
            g_commands.replay3dRunId = id;
            g_commands.replay3dSeconds = previewAt;
            g_commands.replay3dOptions = CaptureOptions(o) & 56319u;
            g_commands.replay3dSpeed = 1;
        }
        status = "Changing replay graphics profile";
        return false;
    }
    if (g_pShared->replay3dState == 4 && g_pShared->replay3dRunId == id)
        throw std::runtime_error(g_pShared->replay3dMessage);
    if (staged != id)
    {
        practice::g_replayArchive.Stage3D(c.analysis->data);
        staged = id;
        waitingSince = GetTickCount64();
        status = "Loading the recorded level for 3D export";
        return false;
    }
    auto state = practice::g_replayArchive.Get();
    if (state->stagedRun == id && !HasPendingRequest() && g_pShared->replay3dState != 1)
    {
        g_commands.replay3dCmd = 1;
        g_commands.replay3dRunId = id;
        g_commands.replay3dSeconds = o.a;
        g_commands.replay3dOptions = CaptureOptions(o) & 56319u;
        g_commands.replay3dSpeed = 1;
        started3d = true;
    }
    if (GetTickCount64() - waitingSince > 20000)
        throw std::runtime_error(state->error.empty() ? "3D replay loading timed out" : state->error);
    return false;
}

inline void TickExportPipeline(ID3D11Device *device, ID3D11DeviceContext *gpu, DWORD pid)
{
    if (finishing)
    {
        status = encoder.PhaseName();
        return;
    }
    const auto &o = frozen;
    auto &c = frozenContent;
    bool scene = o.scene3d && !o.transparent;
    if (scene)
    {
        if (!Prepare3D(c, o))
            return;
        if (!bridge.Connect(pid))
        {
            if (!channelSince)
                channelSince = GetTickCount64();
            if (GetTickCount64() - channelSince > 10000)
                throw std::runtime_error(
                    "Replay pipeline channel v4 is unavailable; update both trainer and helper");
            status = "Waiting for replay frame pipeline";
            return;
        }
        channelSince = 0;
    }
    // Drain composition in strict output order. The encoder writes on its own
    // worker while later scene/composition GPU transfers remain in flight.
    for (int n = 0; n < FrameSlots && encoder.Ready(); n++)
    {
        std::vector<uint8_t> pixels;
        uint64_t frame = 0;
        double latency = 0;
        if (!surface.PollReadback(gpu, pixels, frame, latency,
                                  [](size_t size) { return encoder.AcquireFrame(size); }))
            break;
        if (frame != exportWindow.Consumed())
            throw std::runtime_error("Clip composition frame order mismatch");
        exportWindow.Complete(frame);
        if (!encoder.Push(std::move(pixels)))
            throw std::runtime_error("Encoder stopped accepting ordered frames");
        submitted = exportWindow.Consume() + 1;
        pipelineStats->compositionReadbackMicros += ExportPipelineStats::Micros(latency);
    }
    if (submitted == o.Frames())
    {
        if (composed != submitted || exportWindow.InFlight() != 0 || bridge.pending ||
            surface.PendingReadbacks() != 0)
            throw std::runtime_error("Clip pipeline ended with incomplete frames");
        encoder.Finish();
        finishing = true;
        status = "Finalizing clip file";
        return;
    }
    // Credit is returned only after a complete frame reaches the encoder.
    // At most three frames span all GPU and transfer stages together.
    if (scene)
        while (exportWindow.CanRequest() && bridge.Available() && exportWindow.Requested() < o.SourceFrames())
        {
            auto index = exportWindow.Requested();
            double at = o.At(index);
            auto frame = ResolveFrame(o, c, at);
            if (!frame.pose)
                throw std::runtime_error("No camera recording at the requested time");
            if (!bridge.RequestIndexed(index, c.analysis->data->review->id, at, o.width, o.height,
                                       CaptureOptions(frame.options), &*frame.pose, 0, o.audioVolume))
                break;
            exportWindow.Request();
            pipelineStats->peakInFlight =
                std::max(pipelineStats->peakInFlight.load(), exportWindow.InFlight());
        }
    c.font = suite_ui::hudFont ? suite_ui::hudFont : ImGui::GetIO().FontDefault;
    for (int n = 0; n < FrameSlots && surface.CanQueue() && composed < o.Frames(); n++)
    {
        double at = o.At(composed);
        auto resolved = ResolveFrame(o, c, at);
        bool sceneFrame = scene && !o.IsOutro(composed);
        if (sceneFrame)
        {
            if (composed >= exportWindow.Requested())
                break;
            std::string failure;
            int result = bridge.Poll(scenePixels, failure);
            if (!result)
                break;
            if (result < 0)
                throw std::runtime_error(failure);
            if (bridge.renderedFrame != composed)
                throw std::runtime_error("Replay scene frame order mismatch");
            shownPose = bridge.renderedPose;
            shownPoseReady = true;
            pipelineStats->sceneFrames++;
            if (bridge.readbackMode == 1)
                pipelineStats->asyncSceneFrames++;
            pipelineStats->sceneRenderMicros += ExportPipelineStats::Micros(bridge.renderMilliseconds);
            pipelineStats->sceneReadbackMicros += ExportPipelineStats::Micros(bridge.readbackMilliseconds);
            pipelineStats->sceneTransferMicros += ExportPipelineStats::Micros(bridge.transferMilliseconds);
        }
        else
        {
            if (!exportWindow.CanRequest())
                break;
            if (exportWindow.Request() != composed)
                throw std::runtime_error("Clip source frame order mismatch");
            pipelineStats->peakInFlight =
                std::max(pipelineStats->peakInFlight.load(), exportWindow.InFlight());
        }
        if (pid && (composed == 0 || composed % 500 == 0))
        {
            HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (process)
            {
                PROCESS_MEMORY_COUNTERS counters{};
                if (K32GetProcessMemoryInfo(process, &counters, sizeof(counters)))
                    pipelineStats->gameWorkingSet = uint64_t(counters.WorkingSetSize);
                CloseHandle(process);
            }
        }
        auto start = std::chrono::steady_clock::now();
        surface.Render(device, gpu, resolved.options, c, at, sceneFrame ? &scenePixels : nullptr, false, {},
                       true, o.OutroAt(composed));
        surface.QueueReadback(device, gpu, composed, resolved.options);
        pipelineStats->composeMicros += uint64_t(
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start)
                .count());
        pipelineStats->compositionFrames++;
        composed++;
        previewImage = surface.View();
        previewShown = previewAt = at;
        previewRun = c.analysis->data->review->id;
        previewRevision = exportRevision;
        previewEpoch = exportEpoch;
    }
    status =
        "Export pipeline / " + std::to_string(submitted) + " / " + std::to_string(o.Frames()) + " frames";
    if (scene && pipelineStats->sceneFrames)
        status += pipelineStats->asyncSceneFrames == pipelineStats->sceneFrames ? " / GPU async"
                                                                                : " / GPU fallback";
}

inline void Tick(ID3D11Device *device, ID3D11DeviceContext *gpu, DWORD pid)
{
    openDirectory = &OpenDirectory;
    saveProject = &SaveProject;
    loadProject = &LoadProject;
    profileIO = &ProfileIO;
    backendAvailable = device && gpu && ImGui::GetCurrentContext() && ImGui::GetIO().Fonts->IsBuilt() &&
                       ImGui::GetIO().BackendRendererUserData;
    if (cancelRequested)
    {
        cancelRequested = false;
        encoder.Cancel();
        exporting = false;
        surface.CancelReadbacks();
        bridge.Close();
        finishing = false;
        status = "Cancelling export";
    }
    if (!open && !exporting)
    {
        bridge.Close();
        staged = 0;
        waitingSince = channelSince = 0;
        return;
    }
    if (g_pShared && g_pShared->timerState == 1)
    {
        encoder.Cancel();
        exporting = false;
        open = false;
        surface.CancelReadbacks();
        bridge.Close();
        return;
    }
    if (!backendAvailable || !content.analysis)
        return;
    static ULONGLONG diskCheck = 0;
    if (GetTickCount64() - diskCheck > 2000)
    {
        diskCheck = GetTickCount64();
        std::error_code ec;
        auto path = Directory();
        if (!std::filesystem::exists(path, ec))
            path = g_settings.Directory();
        auto space = std::filesystem::space(path, ec);
        freeDiskGiB = ec ? -1 : double(space.available) / 1073741824.;
    }
    if (g_pShared && g_pShared->replay3dState == 2)
        PauseViewer();
    if (awaitingFullRunCheck)
    {
        auto result = practice::g_replayArchive.Get()->revalidation;
        if (result && result->request >= fullRunRecheck)
        {
            awaitingFullRunCheck = false;
            if (!result->filesOk)
                error = "Full run check failed: " + result->error;
            else if (content.analysis && content.analysis->data->review->id == result->run)
            {
                content.revalidation = result;
                content.motion = practice::g_replayArchive.Motion();
                startRequested = fullRunRequested = true;
            }
            else
                error = "The selected replay changed during its file check. Start full run export again.";
        }
        else
            status = "Checking replay files before full run export";
    }
    if (startRequested && fullRunRequested && !content.revalidation)
    {
        startRequested = fullRunRequested = false;
        fullRunRecheck = practice::g_replayArchive.Revalidate(content.analysis->data);
        awaitingFullRunCheck = true;
        status = "Checking replay files before full run export";
    }
    if (startRequested)
    {
        startRequested = false;
        bool fullRun = fullRunRequested;
        fullRunRequested = false;
        try
        {
            frozen = fullRun ? FullRunOptions(options, content) : options;
            frozenContent = content;
            if (fullRun)
                frozenContent.track = {};
            frozen.Validate(content.analysis->data->review->seconds);
            frozenContent.track.Validate(content.analysis->data->review->seconds);
            if (frozen.scene3d && frozen.camera == 3 && frozenContent.track.keys.empty())
                throw std::runtime_error("Add a cinematic camera keypoint first");
            SaveProject();
            ConfigureSpeed(frozen.timing, frozenContent.track, frozenContent.analysis->data->review->seconds,
                           frozen.rate, frozen.camera == 3 && frozen.speedKeys);
            if (testRequested)
            {
                frozen.a = std::clamp(previewAt, frozen.a, std::max(frozen.a, frozen.b - .03));
                frozen.b = std::min(frozen.b, frozen.Advance(frozen.a, 3));
            }
            testRequested = false;
            frozen.Validate(content.analysis->data->review->seconds);
            exportFrameCount = frozen.Frames();
            submitted = composed = 0;
            exportWindow.Reset(exportFrameCount);
            exportRevision = revision;
            exportEpoch = cameraEpoch;
            pipelineStats = std::make_shared<ExportPipelineStats>();
            surface.CancelReadbacks();
            finishing = false;
            staged = 0;
            bridge.Close();
            auto id = content.analysis->data->review->id;
            auto name = L"clip-" + std::to_wstring(id) + L"-" +
                        std::to_wstring(std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count()) +
                        Extension(frozen.format);
            std::filesystem::create_directories(Directory());
            std::error_code ec;
            auto space = std::filesystem::space(Directory(), ec);
            uint64_t minimum = frozen.format == 3
                                   ? uint64_t(frozen.width) * frozen.height * 4 * frozen.Frames()
                                   : 32ull * 1024 * 1024;
            if (!ec && space.available < minimum + 16ull * 1024 * 1024)
                throw std::runtime_error("Not enough disk space for this export");
            ExportSource exportSource;
            exportSource.pipeline = pipelineStats;
            exportSource.revalidation = frozenContent.revalidation;
            exportSource.recordedValid = frozenContent.analysis->data->review->valid;
            exportSource.recordedWarnings = frozenContent.analysis->data->review->warnings;
            exportSource.run = id;
            exportSource.seed = frozenContent.analysis->data->seed;
            exportSource.scene = frozenContent.analysis->data->scene;
            exportSource.hash = frozenContent.analysis->data->hash;
            if (frozenContent.motion)
            {
                exportSource.audio = frozenContent.motion->experience.audio;
                exportSource.audioFile = frozenContent.motion->audioFile;
            }
            if (!encoder.Start(frozen, Directory() / name, exportSource))
                throw std::runtime_error(encoder.Error());
            exporting = true;
            status = "Exporting replay frames";
        }
        catch (const std::exception &e)
        {
            error = e.what();
        }
    }
    if (exporting && encoder.State() != 1)
    {
        exporting = false;
        finishing = false;
        surface.CancelReadbacks();
        bridge.Close();
        dirty = true;
        if (encoder.State() == 2)
        {
            status = "Export complete";
            suite_ui::Notify("Replay clip exported");
        }
        else if (encoder.State() == 3)
            error = encoder.Error();
    }
    if (!exporting && !open)
        return;
    auto &sourceOptions = exporting ? frozen : options;
    Options drawOptions = exporting ? frozen : PreviewOptions(options, previewPixelWidth, actualSize);
    const auto &o = drawOptions;
    auto &c = exporting ? frozenContent : content;
    try
    {
        sourceOptions.Validate(c.analysis->data->review->seconds);
    }
    catch (const std::exception &e)
    {
        if (exporting)
        {
            encoder.Cancel();
            exporting = finishing = false;
            bridge.Close();
            surface.CancelReadbacks();
        }
        error = e.what();
        return;
    }
    if (exporting)
    {
        try
        {
            TickExportPipeline(device, gpu, pid);
        }
        catch (const std::exception &e)
        {
            error = e.what();
            status = "Export pipeline stopped";
            encoder.Cancel();
            bridge.Close();
            surface.CancelReadbacks();
            exporting = finishing = false;
            staged = 0;
            waitingSince = channelSince = 0;
            dirty = true;
        }
        return;
    }
    if (playPreview)
    {
        double now = ImGui::GetTime();
        if (previewClock == 0)
            previewClock = now;
        previewAt = o.Advance(previewAt, std::clamp(now - previewClock, 0., .2));
        previewClock = now;
        if (previewAt >= o.b)
            previewAt = o.a;
        dirty = true;
    }
    if ((!o.scene3d || o.transparent) && bridge.pending)
        bridge.Close();
    if (!dirty && !bridge.pending)
        return;
    // UI edits and the playback clock remain independent of the asynchronous
    // scene request. Never drop/export a frame on a wall-clock deadline: export
    // time is solely n / FPS.
    if (!bridge.pending && GetTickCount64() - previewTick < DWORD(1000 / std::max(1, o.fps)))
        return;
    double at = std::clamp(previewAt, 0., c.analysis->data->review->seconds);
    bool outdatedPreview = false;
    try
    {
        if ((!o.scene3d || o.transparent) && o.format != 1)
        {
            // Native 2D composition draws directly in the window; no GPU round trip
            // is required.
            previewShown = at;
            previewRun = c.analysis->data->review->id;
            previewImage = {};
            previewRevision = revision;
            previewEpoch = cameraEpoch;
            dirty = false;
            previewTick = GetTickCount64();
            return;
        }
        if (o.scene3d && !o.transparent)
        {
            if (!Prepare3D(c, o))
                return;
            if (!bridge.Connect(pid))
            {
                if (!channelSince)
                    channelSince = GetTickCount64();
                if (GetTickCount64() - channelSince > 10000)
                    throw std::runtime_error("Replay frame channel is unavailable");
                status = "Waiting for replay frame channel";
                return;
            }
            channelSince = 0;
            if (!bridge.pending)
            {
                RequestSceneFrame(c, o, at);
                return;
            }
            std::string failure;
            int result = bridge.Poll(scenePixels, failure);
            if (!result)
                return;
            if (result < 0 && (pendingOptions.simplified != o.simplified || frameCameraEpoch != cameraEpoch))
            {
                dirty = true;
                return;
            }
            if (result < 0)
                throw std::runtime_error(failure);
            if (frameWidth != o.width || frameHeight != o.height ||
                frameRun != c.analysis->data->review->id || (frameCameraEpoch != cameraEpoch))
            {
                dirty = true;
                return;
            }
            outdatedPreview = pendingRevision != revision;
            at = requestedAt;
            shownPose = bridge.renderedPose;
            shownPoseReady = true;
            // A returned image is an observation, never the authoritative editable
            // camera state.
        }
        c.font = suite_ui::hudFont ? suite_ui::hudFont : ImGui::GetIO().FontDefault;
        bool scene = o.scene3d && !o.transparent;
        // Compose with the request's settings and time, never newer editor edits.
        // Only the font pointer is refreshed after an atlas rebuild.
        pendingComposition.font = c.font;
        surface.Render(device, gpu, scene ? pendingOptions : o, scene ? pendingComposition : c, at,
                       scene ? &scenePixels : nullptr, false);
        previewImage = surface.View();
        previewShown = at;
        previewRun = c.analysis->data->review->id;
        previewRevision = scene ? pendingRevision : revision;
        previewEpoch = scene ? frameCameraEpoch : cameraEpoch;
        previewTick = GetTickCount64();
        dirty = outdatedPreview;
        status = o.format == 1                      ? "Actual output preview / GIF palette"
                 : (o.format == 2 || o.format == 4) ? "Actual output preview / lossless RGB"
                 : o.transparent                    ? "Actual output preview / transparent overlays"
                                                    : "Actual output preview / rendered source frame";
    }
    catch (const std::exception &e)
    {
        error = e.what();
        status = "Preview unavailable";
        dirty = false;
        bridge.Close();
        staged = 0;
        waitingSince = channelSince = 0;
    }
}
} // namespace suite_clip
#endif
