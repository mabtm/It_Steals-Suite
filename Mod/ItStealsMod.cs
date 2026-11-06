using System;
using System.IO;
using HarmonyLib;
using MelonLoader;
using UnityEngine;
using UnityEngine.SceneManagement;

[assembly:MelonInfo(typeof(ItStealsMod), "ItStealsHelper", ItStealsMod.Version, "mab")]
[assembly:MelonGame("Zeekerss3", "It Steals")]
[assembly:System.Reflection.AssemblyVersion(ItStealsMod.Version + ".0")]
[assembly:System.Reflection.AssemblyFileVersion(ItStealsMod.Version + ".0")]

public sealed class ItStealsMod : MelonMod
{
    public const string Version = "7.24.0";
    private readonly FrameCostMeter workCost = new FrameCostMeter();
    private long updateWork;
    private SharedMemoryProtocol connection;
    private readonly SnapshotBuffer snapshot = new SnapshotBuffer();
    private SpeedrunManager speedrun;
    private HarmonyLib.Harmony harmony;
    private DataExporter exporter;
    private MouseLook player;
    private Camera attachedCamera;
    private string viewCameraLabel = "none";
    private FullbrightManager fullbright;
    private GameObject runtime;
    private TrainerCommands commands;
    private float nextAttach, nextError, lastHeartbeatTime;
    private double nextPublish, nextPoll, fpsStarted, nextCoverage;
    private int fpsFrames;
    private float gameFps;
    private readonly MenuCursor menuCursor = new MenuCursor();
    private uint heartbeat;
    private bool trainerConnected;
    private bool originalRunInBackground, backgroundChanged;
    private int pid;
    private long baseAddress;
    private GameBuildInfo build;
    private SeedMapPlanner planner;
    private uint sceneEpoch, lastTeleportSerial;
    private readonly FramePolicy framePolicy = new FramePolicy();
    private ExternalToolMonitor externalTools;
    private LiveInspector inspector;
    private CaptureOverlay captureOverlay;

    public override void OnInitializeMelon()
    {
        originalRunInBackground = Application.runInBackground;
        try
        {
            framePolicy.Initialize(Environment.GetCommandLineArgs());
            string gameRoot = Path.GetFullPath(Path.Combine(Application.dataPath, ".."));
            build = new GameBuildInfo(typeof(MouseLook).Assembly.Location, Application.version,
                                      Application.unityVersion);
            connection = new SharedMemoryProtocol();
            speedrun = new SpeedrunManager(Path.Combine(gameRoot, "UserData", "ItStealsPracticeSuite"),
                                           gameRoot, build);
            externalTools = new ExternalToolMonitor();
            Application.runInBackground = true;
            backgroundChanged = true;
            runtime = new GameObject("ItStealsHelper.Runtime");
            Replay3D.Initialize(Path.Combine(gameRoot, "UserData", "ItStealsPracticeSuite"), build);
            UnityEngine.Object.DontDestroyOnLoad(runtime);
            fullbright = runtime.AddComponent<FullbrightManager>();
            harmony = new HarmonyLib.Harmony("itsteals.practice.v3");
            HarmonyPatches.Install(harmony);
            using (var process = System.Diagnostics.Process.GetCurrentProcess())
            {
                pid = process.Id;
                try
                {
                    baseAddress = process.MainModule.BaseAddress.ToInt64();
                }
                catch
                {
                    baseAddress = 0;
                }
            }
            try
            {
                planner = new SeedMapPlanner(pid, build, connection.Session);
            }
            catch (Exception error)
            {
                MelonLogger.Warning("Seed Map unavailable: " + error.Message);
            }
            inspector = new LiveInspector(pid, connection.Session);
            try
            {
                captureOverlay = new CaptureOverlay(pid);
            }
            catch (Exception error)
            {
                MelonLogger.Warning("Capture overlay unavailable: " + error.Message);
            }
            lastHeartbeatTime = Time.realtimeSinceStartup;
            MelonLogger.Msg("ItStealsHelper " + Version + " loaded; shared-memory protocol v" +
                            SharedMemoryProtocol.Version + ".");
        }
        catch (Exception error)
        {
            MelonLogger.Error("ItStealsHelper initialization: " + error);
            OnDeinitializeMelon();
        }
    }

    public override void OnSceneWasLoaded(int buildIndex, string sceneName)
    {
        Replay3D.SceneLoaded(buildIndex);
        unchecked
        {
            sceneEpoch++;
        }
        if (inspector != null)
            inspector.SceneLoaded(sceneEpoch);
        if (planner != null)
            planner.SceneLoaded(sceneEpoch, buildIndex);
        menuCursor.Restore();
        FreeCam.DisableActive();
        PlayerContext.Observed = null;
        SceneGeometry.Reset();
        framePolicy.SceneLoaded();
        exporter = null;
        player = null;
        attachedCamera = null;
        viewCameraLabel = "none";
        nextAttach = 0;
        nextPublish = 0;
        HitboxDrawer.Clear();
        if (speedrun != null)
            speedrun.SceneLoaded(buildIndex);
        PracticeRandom.SceneLoaded(buildIndex);
        ReplayAudioCapture.Prepare();
    }

    public override void OnSceneWasInitialized(int buildIndex, string sceneName)
    {
        if (connection == null)
            return;
        // Fallback for a persistent player whose Start does not run again.
        Attach();
        // Attachment alone does not prove that the actual Start hook ran.
        if (player != null && speedrun != null)
            speedrun.PlayerReady(player, false);
    }

    private void Attach()
    {
        MouseLook mouse = PlayerContext.Find();
        if (mouse == null)
            return;
        GameObject body = mouse.controller != null ? mouse.controller.gameObject : mouse.gameObject;
        player = mouse;
        if (exporter == null)
            exporter = body.GetComponent<DataExporter>();
        if (exporter == null)
            exporter = body.AddComponent<DataExporter>();
        exporter.Bind(mouse);
        // Do not stop retrying if the player exists before Camera.main does.
        Camera camera = PlayerContext.View(mouse);
        if (camera == null)
            return;
        if (attachedCamera == camera)
            return;
        if (attachedCamera != null)
        {
            var old = attachedCamera.GetComponent<FreeCam>();
            if (old != null)
                old.Disable();
            var oldDrawer = attachedCamera.GetComponent<HitboxDrawer>();
            if (oldDrawer != null)
                oldDrawer.enabled = false;
        }
        if (camera.GetComponent<HitboxDrawer>() == null)
            camera.gameObject.AddComponent<HitboxDrawer>();
        camera.GetComponent<HitboxDrawer>().enabled = true;
        FreeCam freecam = camera.GetComponent<FreeCam>();
        if (freecam == null)
            freecam = camera.gameObject.AddComponent<FreeCam>();
        freecam.Bind(mouse);
        attachedCamera = camera;
        viewCameraLabel = camera.name + (camera == Camera.main ? " [main tag]" : " [player reference]");
    }

    public override void OnUpdate()
    {
        long workStarted = System.Diagnostics.Stopwatch.GetTimestamp();
        if (connection == null)
            return;
        try
        {
            float now = Time.realtimeSinceStartup;
            double preciseNow = Time.realtimeSinceStartupAsDouble;
            if (preciseNow >= nextCoverage)
            {
                HarmonyPatches.SupportedBuild = build != null && build.Supported;
                HarmonyPatches.RefreshSeedCoverage();
                nextCoverage = preciseNow + 2;
            }
            fpsFrames++;
            if (preciseNow - fpsStarted >= .5)
            {
                gameFps = preciseNow - fpsStarted < 2 ? (float)(fpsFrames / (preciseNow - fpsStarted)) : 0;
                fpsFrames = 0;
                fpsStarted = preciseNow;
            }
            bool request = false;
            bool polled = false;
            if (preciseNow + .00001 >= nextPoll)
            {
                nextPoll = Math.Max(nextPoll + 1.0 / 60, preciseNow - 1.0 / 60);
                polled = connection.Poll(out request);
            }
            if (polled)
            {
                TrainerCommands incoming = connection.Commands;
                if (incoming.Heartbeat != heartbeat)
                {
                    heartbeat = incoming.Heartbeat;
                    lastHeartbeatTime = now;
                    trainerConnected = true;
                }
                if (trainerConnected)
                {
                    if (incoming.TelemetryOptions != commands.TelemetryOptions)
                        nextPublish = 0;
                    commands = incoming;
                }
                else
                    request = false;
            }
            else
                request = false;
            // Stop retaining cheats if the overlay is closed or stops updating.
            if (trainerConnected && now - lastHeartbeatTime > 3f)
            {
                commands = new TrainerCommands();
                request = false;
                trainerConnected = false;
                PracticeRandom.Disable();
                FreeCam.DisableActive();
            }
            if (request && (commands.LevelLoadCmd == 1 || commands.SpeedrunLoadLevelCmd == 1 ||
                            commands.SetSeedCmd == 1))
                Replay3D.Stop();
            if (Replay3D.Reserved || request && commands.Replay3dCmd == 1)
                fullbright.SetFullbright(false);
            Replay3D.Tick(commands, request, trainerConnected);
            if ((!commands.Speedrun || !commands.UsesControlledSeed) && !Replay3D.Reserved)
                PracticeRandom.Disable();
            LiveInputCapture.Enabled = trainerConnected && commands.RequestsInputs;
            bool allowed = !commands.Restricted && !Replay3D.Reserved;
            speedrun.ObserveExternal(externalTools == null ? null : externalTools.Snapshot);
            if (request && commands.RunReviewRequest == 1 && !speedrun.Running && planner != null)
                planner.Review(commands.RunReviewId);
            else if (request && commands.RunReviewRequest == 2 && planner != null)
                planner.CloseReview();
            else if (request && commands.RunReviewRequest == 3 && !speedrun.Running)
                speedrun.ResetStatistics();
            speedrun.AuditBeforeSuppression(commands);
            if (framePolicy.Tick(commands, trainerConnected))
                return;
            bool hitboxes = allowed && commands.HitboxEnabled == 1;
            if (HitboxDrawer.Enabled && !hitboxes)
                HitboxDrawer.Clear();
            HitboxDrawer.Enabled = hitboxes;
            HitboxDrawer.HidePlayerHitbox = commands.HidePlayerHitbox == 1;

            fullbright.SetFullbright(allowed && commands.BrightnessEnabled == 1);
            if (commands.Restricted)
                FreeCam.DisableActive();
            speedrun.Configure(commands);
            menuCursor.Tick(trainerConnected && commands.InputCapture == 1);
            if (now >= nextAttach)
            {
                Attach();
                nextAttach = now + (player == null || attachedCamera == null ? 0.25f : 1f);
            }

            if (request && !Replay3D.Reserved)
            {
                if (commands.PlannerRequest != 0 && planner != null)
                    try
                    {
                        planner.Request(commands);
                    }
                    catch (Exception error)
                    {
                        planner.Fail(error);
                        Report(error);
                    }
                if (commands.WindowModeCmd == 1)
                    Screen.fullScreenMode = FullScreenMode.FullScreenWindow;
                if (commands.SetSeedCmd == 1 && commands.Speedrun && commands.Practice)
                {
                    commands.SeededPractice = 1;
                    commands.SpeedrunLoadLevelCmd = 1;
                }
                if (commands.SpeedrunLoadLevelCmd == 1 && commands.Speedrun)
                {
                    LoadSpeedrunLevel();
                    return;
                }
                if (commands.LevelLoadCmd == 1 && !commands.Speedrun)
                {
                    LoadScene(commands.LevelLoadIndex, false);
                    return;
                }
                if (commands.TeleportCmd == 1 && allowed && player != null &&
                    (commands.TeleportScene == 0 ||
                     commands.TeleportScene == (uint)(SceneManager.GetActiveScene().buildIndex + 1)))
                {
                    if (commands.TeleportExitFreecam == 1)
                        FreeCam.DisableActive();
                    if (Teleport(new Vector3(commands.TeleportX, commands.TeleportY, commands.TeleportZ)))
                        lastTeleportSerial = commands.Serial;
                }
                if (commands.ResetMapCmd == 1 && exporter != null)
                {
                    exporter.ResetMapBounds();
                    if (planner != null)
                        planner.RefreshGeometryIfIdle();
                }
            }
            if (!Replay3D.Reserved && planner != null && !planner.TickReview())
                try
                {
                    planner.Tick(trainerConnected &&
                                     (commands.RequestsPlanner || commands.RequestsPlannerGeometry),
                                 commands.RequestsPlanner && (commands.PlannerOptions & 8u) == 0);
                }
                catch (Exception error)
                {
                    planner.Fail(error);
                    Report(error);
                }
            speedrun.Tick();
            if (!Replay3D.Reserved &&
                (speedrun.NeedsDeathReload ||
                 (commands.LegacyRestartAllowed(trainerConnected) && MenuCursor.GameHotkeysAllowed &&
                  commands.Speedrun && Input.GetKeyDown(KeyCode.T) &&
                  !Input.GetKey(KeyCode.LeftShift) && !Input.GetKey(KeyCode.RightShift))))
            {
                LoadSpeedrunLevel();
                return;
            }
            if (!trainerConnected && MenuCursor.GameHotkeysAllowed && Input.GetKeyDown(KeyCode.F5) &&
                exporter != null)
            {
                exporter.ResetMapBounds();
                if (planner != null)
                    planner.RefreshGeometryIfIdle();
            }
        }
        catch (Exception e)
        {
            Report(e);
        }
        finally
        {
            updateWork = System.Diagnostics.Stopwatch.GetTimestamp() - workStarted;
        }
    }

    private void LoadSpeedrunLevel()
    {
        int selection = Mathf.Clamp(commands.SpeedrunLevelIndex, 0, SpeedrunManager.LevelIndices.Length - 1);
        LoadScene(SpeedrunManager.LevelIndices[selection], true);
    }

    private void LoadScene(int index, bool controlled)
    {
        if (index < 0 || index >= SceneManager.sceneCountInBuildSettings)
            return;
        if (controlled)
        {
            if (planner != null && commands.Practice && commands.SeededPractice == 1)
                try
                {
                    planner.ExpectPracticeLaunch(index, commands.PracticeSeed,
                                                 (commands.PlannerOptions & 2u) != 0);
                }
                catch (Exception error)
                {
                    planner.Fail(error);
                    Report(error);
                }
            speedrun.BeforeReload(index);
            PracticeRandom.Begin(commands.PracticeSeed, index, commands.UsesControlledSeed,
                                 commands.UsesControlledSeed && (commands.PlannerOptions & 2u) != 0);
        }
        else
        {
            FreeCam.DisableActive();
            PracticeRandom.Disable();
        }
        menuCursor.Restore();
        SceneManager.LoadScene(index);
    }

    private static bool Finite(float value)
    {
        return !float.IsNaN(value) && !float.IsInfinity(value) && Mathf.Abs(value) < 100000f;
    }

    private bool Teleport(Vector3 position)
    {
        if (!Finite(position.x) || !Finite(position.y) || !Finite(position.z))
            return false;
        speedrun.MarkAssisted();
        CharacterController controller = player.controller;
        bool wasEnabled = controller != null && controller.enabled;
        if (wasEnabled)
            controller.enabled = false;
        try
        {
            (player.playerBody != null ? player.playerBody : player.transform).position = position;
            Physics.SyncTransforms();
        }
        finally
        {
            if (wasEnabled && controller != null)
                controller.enabled = true;
        }
        if (exporter != null)
            exporter.NotifyTeleport();
        return true;
    }

    public override void OnLateUpdate()
    {
        long workStarted = System.Diagnostics.Stopwatch.GetTimestamp();
        if (captureOverlay != null)
            captureOverlay.Tick(trainerConnected);
        if (connection == null)
            return;
        try
        {
            menuCursor.Tick(trainerConnected && commands.InputCapture == 1);
            menuCursor.ObserveFocused();
            ReplayInputCapture.CaptureInactiveCamera();
            Replay3D.LateTick();
            ReplayClipChannel.Tick(pid, trainerConnected);
            // An unavailable inspector object must not interrupt telemetry,
            // cursor recovery or the independent run recorder.
            if (inspector != null)
                try
                {
                    inspector.Tick(commands, trainerConnected, player, sceneEpoch);
                }
                catch (Exception e)
                {
                    Report(e);
                }
            double now = Time.realtimeSinceStartupAsDouble;
            bool details = trainerConnected && commands.RequestsDetails;
            double period = details || HitboxDrawer.Enabled || Replay3D.Reserved ||
                                    trainerConnected && commands.RequestsLive
                                ? 1.0 / 60
                                : .5;
            if (now + .00001 < nextPublish)
                return;
            nextPublish += period;
            if (nextPublish < now - period || nextPublish > now + period * 2)
                nextPublish = now + period;
            snapshot.Clear();
            snapshot.WriteString(SharedLayout.Title, "Elite-Suite", 64);
            snapshot.WriteString(SharedLayout.Author, "mab", 64);
            snapshot.Write(SharedLayout.PlannerGenerationMs,
                           planner == null ? 0 : planner.GenerationMilliseconds);
            snapshot.WriteString(SharedLayout.Version, "v" + Version, 64);
            snapshot.Write(SharedLayout.GameFps, gameFps);
            snapshot.Write(SharedLayout.Pid, pid);
            snapshot.Write(SharedLayout.BaseAddress, baseAddress);
            snapshot.Write(SharedLayout.RngPatchComplete, PracticeRandom.SourcesVerified);
            snapshot.Write(SharedLayout.SceneEpoch, unchecked((int)sceneEpoch));
            snapshot.Write(SharedLayout.LastTeleportSerial, unchecked((int)lastTeleportSerial));
            snapshot.Write(SharedLayout.PlayerReady, player != null);
            snapshot.Write(SharedLayout.GameTargetFps, Application.targetFrameRate);
            snapshot.Write(SharedLayout.GameVSync, QualitySettings.vSyncCount);
            snapshot.Write(SharedLayout.GameRefreshHz, (float)Screen.currentResolution.refreshRate);
            snapshot.WriteString(SharedLayout.ViewCamera, attachedCamera == null ? "none" : viewCameraLabel,
                                 64);
            snapshot.Write(SharedLayout.CursorLock, (int)Cursor.lockState);
            snapshot.Write(SharedLayout.MenuInputOwned, MenuCursor.OwnsInput);
            if (build != null)
            {
                snapshot.WriteString(SharedLayout.GameVersion, build.GameVersion, 64);
                snapshot.WriteString(SharedLayout.UnityVersion, build.UnityVersion, 32);
                snapshot.WriteString(SharedLayout.GameAssemblyHash, build.Hash, 65);
            }
            snapshot.Write(SharedLayout.HitboxEnabled, HitboxDrawer.Enabled);
            snapshot.Write(SharedLayout.BrightnessEnabled, FullbrightManager.AnyActive);
            snapshot.Write(SharedLayout.HidePlayerHitbox, HitboxDrawer.HidePlayerHitbox);
            snapshot.Write(SharedLayout.AccurateHitboxes, true);
            if (!Replay3D.Reserved && exporter != null &&
                (details || commands.RequestsLive || HitboxDrawer.Enabled))
                exporter.WriteSnapshot(snapshot, trainerConnected && commands.RequestsMap,
                                       trainerConnected && commands.RequestsPath,
                                       trainerConnected && commands.RequestsWalls,
                                       trainerConnected && commands.RequestsCrystals,
                                       trainerConnected && commands.RequestsMonster);
            snapshot.Write(SharedLayout.CurrentSceneIndex, SceneManager.GetActiveScene().buildIndex);
            MovementCapture.WriteSnapshot(snapshot, SceneManager.GetActiveScene().buildIndex);
            float average, percentile;
            workCost.Snapshot(now, out average, out percentile);
            snapshot.Write(SharedLayout.HelperWorkMean, average);
            snapshot.Write(SharedLayout.HelperWorkP95, percentile);
            snapshot.Write(SharedLayout.PlayerDead, MouseLook.Health <= 0);
            speedrun.WriteSnapshot(snapshot, commands);
            Replay3D.WriteSnapshot(snapshot);
            LiveInputCapture.WriteSnapshot(snapshot);
            connection.Publish(snapshot, now);
        }
        catch (Exception e)
        {
            Report(e);
        }
        finally
        {
            workCost.Observe((updateWork + ReplayInputCapture.TakeCameraWork() + System.Diagnostics.Stopwatch.GetTimestamp() - workStarted) *
                             1000.0 / System.Diagnostics.Stopwatch.Frequency);
        }
    }

    private void Report(Exception error)
    {
        if (Time.realtimeSinceStartup < nextError)
            return;
        nextError = Time.realtimeSinceStartup + 5f;
        MelonLogger.Error("ItStealsHelper: " + error);
    }

    private static void Release(Action action, string resource)
    {
        try
        {
            action();
        }
        catch (Exception error)
        {
            MelonLogger.Warning("Could not release " + resource + ": " + error.Message);
        }
    }

    public override void OnGUI()
    {
        if (captureOverlay != null)
            try
            {
                captureOverlay.Draw();
            }
            catch (Exception error)
            {
                Report(error);
            }
    }

    public override void OnDeinitializeMelon()
    {
        if (captureOverlay != null)
        {
            Release(captureOverlay.Dispose, "capture overlay");
            captureOverlay = null;
        }
        Release(ReplayClipChannel.Close, "clip capture");
        Release(Replay3D.Stop, "3D replay");
        if (inspector != null)
        {
            Release(inspector.Dispose, "live inspector");
            inspector = null;
        }
        HitboxDrawer.Enabled = false;
        HitboxDrawer.Clear();
        Release(FreeCam.DisableActive, "camera");
        PracticeRandom.Disable();
        Release(menuCursor.Restore, "menu cursor");
        Release(framePolicy.Restore, "game frame settings");
        if (externalTools != null)
        {
            Release(externalTools.Dispose, "local integrity monitor");
            externalTools = null;
        }
        if (speedrun != null)
        {
            Release(speedrun.Dispose, "run store");
            speedrun = null;
        }
        ReplayInputCapture.Stop();
        if (planner != null)
        {
            Release(planner.Dispose, "practice map");
            planner = null;
        }
        if (connection != null)
        {
            Release(connection.Dispose, "shared memory");
            connection = null;
        }
        if (harmony != null)
        {
            Release(harmony.UnpatchSelf, "patches");
            harmony = null;
        }
        if (runtime != null)
        {
            Release(() => UnityEngine.Object.Destroy(runtime), "materials/runtime");
            runtime = null;
        }
        if (backgroundChanged)
        {
            Release(() => Application.runInBackground = originalRunInBackground, "background setting");
            backgroundChanged = false;
        }
        exporter = null;
        player = null;
        attachedCamera = null;
    }
}
