using System;
using System.Diagnostics;
using UnityEngine;
using UnityEngine.SceneManagement;

public sealed class SpeedrunManager
{
    public static bool IsHideSeekScene(int scene)
    {
        return scene == 5 || scene == 13;
    }

    public static readonly int[] LevelIndices = ModeStatistics.Scenes;
    public static SpeedrunManager Instance { get; private set; }
    public static bool IsSpeedrunActive
    {
        get {
            return Instance != null && Instance.active && Instance.IsPlayableScene;
        }
    }
    public bool Running
    {
        get {
            return timerState == 1;
        }
    }
    public bool Restricted
    {
        get {
            return active && !practice;
        }
    }
    public bool CanHandleDeath
    {
        get {
            return active && IsPlayableScene && (timerState == 1 || NeedsDeathReload);
        }
    }
    public bool Assisted
    {
        get {
            return Record != null && Record.Aids != RunAids.None;
        }
    }
    public bool NeedsDeathReload { get; private set; }
    public int SelectedLevel { get; private set; }
    private readonly RunStore store;
    private readonly RunTracker tracker;
    private readonly GameBuildInfo build;
    private bool exportReports;
    private double lastObserved;
    private MouseLook player;
    private bool active, practice;
    private byte timerState;
    private int runScene = -1;
    private long started, sessionStarted = Stopwatch.GetTimestamp();
    private double finalTime, nextCheckpoint;
    private RunAids currentFeatures;
    private TrainerCommands liveCommands;
    private ExternalToolSnapshot external = new ExternalToolSnapshot();
    private Vector3 lastAuditPosition;
    private bool haveAuditPosition;
    private double lastAuditAt;
    private double nextPatchAudit;
    private bool patchesTrusted;
    private double lastHudSeen = double.NegativeInfinity;
    private double nextPlaySave;

    public bool MovementLearningAllowed(MouseLook mouse)
    {
        return timerState == 1 && tracker.Active != null && mouse == player && MouseLook.Health > 0 &&
               !FreeCam.FreeCamEnabled && (tracker.Active.Aids & RunAids.Teleport) == 0 &&
               (tracker.Active.Warnings &
                (RunWarnings.InvalidTiming | RunWarnings.TimeScaleChanged | RunWarnings.PlayerStateChanged |
                 RunWarnings.ExternalTool | RunWarnings.GamePatchesChanged)) == 0;
    }

    public double CurrentSeconds
    {
        get {
            return Elapsed;
        }
    }
    public long ActiveRunId
    {
        get {
            return tracker.Active == null ? 0 : tracker.Active.Id;
        }
    }

    public void ObserveExternal(ExternalToolSnapshot value)
    {
        external = value ?? new ExternalToolSnapshot();
    }

    public void AuditBeforeSuppression(TrainerCommands incoming)
    {
        if (timerState != 1 || tracker.Active == null)
            return;
        RunAids observed = RunAids.None;
        if (HitboxDrawer.Enabled)
            observed |= RunAids.Hitboxes;
        if (FullbrightManager.AnyActive)
            observed |= RunAids.Fullbright;
        if (FreeCam.FreeCamEnabled)
            observed |= RunAids.FreeCamera;
        if (LiveInspector.Active)
            observed |= RunAids.LiveInspector;
        if (PracticeRandom.Enabled)
            observed |= RunAids.FixedSeed;
        tracker.Mark(observed);
        // Review commands are rejected while Running, before a replay can open.
        // A delayed F8 from the finish prompt must not invalidate a new run.
        if (Restricted && incoming.Restricted &&
            (incoming.RequestsInspector || incoming.HitboxEnabled == 1 || incoming.BrightnessEnabled == 1 ||
             incoming.TeleportCmd == 1 || incoming.PlannerRequest != 0 && incoming.PlannerRequest != 4))
            tracker.Warn(RunWarnings.ForbiddenCommand);
    }

    private void AuditRun(bool final = false)
    {
        if (tracker.Active == null || build == null)
            return;
        double elapsed = Elapsed;
        if (!build.Supported && build.Hash != "UNKNOWN")
            tracker.Warn(RunWarnings.IntegrityUnavailable);
        double realtime = Time.realtimeSinceStartupAsDouble;
        if (final || realtime >= nextPatchAudit)
        {
            patchesTrusted = HarmonyPatches.GameplayPatchesTrusted();
            nextPatchAudit = realtime + 1;
        }
        if (!PracticeRandom.SourcesVerified || !patchesTrusted)
            tracker.Warn(RunWarnings.GamePatchesChanged);
        if (external.Status == 2 && (tracker.Active.Warnings & RunWarnings.ExternalTool) == 0)
        {
            tracker.Event("ExternalTool", external.Tool);
            tracker.Warn(RunWarnings.ExternalTool);
        }
        if ((final || elapsed > 3) &&
            (external.Status != 1 || DateTime.UtcNow.Ticks - external.AtUtc > TimeSpan.TicksPerSecond * 4))
            tracker.Warn(external.Status == 2 ? RunWarnings.ExternalTool : RunWarnings.IntegrityUnavailable);
        if (Math.Abs(Time.timeScale - 1f) > .001f)
            tracker.Warn(RunWarnings.TimeScaleChanged);
        if (liveCommands.RunHudVisible == 1)
            lastHudSeen = realtime;
        // Allow normal surface/focus transitions, never a sustained missing
        // badge. A finished run must have had a recently presented status.
        if (Restricted && (final || elapsed > 1) && realtime - lastHudSeen > .35)
            tracker.Warn(RunWarnings.OverlayMissing);
        if (MouseLook.Health < 0 || MouseLook.Health > 2 ||
            player != null &&
                (player.Objectives < 0 || player.Objectives > player.TotalObjectives || player.speed > 50))
            tracker.Warn(RunWarnings.PlayerStateChanged);
        var body = PlayerContext.Body(player);
        if (body != null)
        {
            var at = body.position;
            double now = Time.realtimeSinceStartupAsDouble;
            if (float.IsNaN(at.x) || float.IsNaN(at.y) || float.IsNaN(at.z) || float.IsInfinity(at.x) ||
                float.IsInfinity(at.y) || float.IsInfinity(at.z))
                tracker.Warn(RunWarnings.PlayerStateChanged);
            if (haveAuditPosition && now - lastAuditAt < .25 &&
                (at - lastAuditPosition).magnitude > Math.Max(4, 40 * (now - lastAuditAt)))
                tracker.Warn(RunWarnings.PlayerStateChanged);
            lastAuditPosition = at;
            lastAuditAt = now;
            haveAuditPosition = true;
        }
        if (final && !build.Supported)
            tracker.Warn(RunWarnings.IntegrityUnavailable);
        if (final && tracker.Active.Environment != null && tracker.Active.Environment.GameHash == "UNKNOWN")
            tracker.Active.Environment = build.Snapshot();
        if (build.Supported && patchesTrusted && PracticeRandom.SourcesVerified && external.Status == 1 &&
            DateTime.UtcNow.Ticks - external.AtUtc < TimeSpan.TicksPerSecond * 4)
            tracker.Active.ValidationChecked = true;
        if (final && !tracker.Active.ValidationChecked)
            tracker.Warn(RunWarnings.IntegrityUnavailable);
    }

    private RunRecord Record
    {
        get {
            return tracker.Active ?? tracker.Last;
        }
    }
    public bool IsPlayableScene
    {
        get {
            return Array.IndexOf(LevelIndices, SceneManager.GetActiveScene().buildIndex) >= 0;
        }
    }
    private double Elapsed
    {
        get {
            return timerState == 1 ? (Stopwatch.GetTimestamp() - started) / (double)Stopwatch.Frequency
                                   : finalTime;
        }
    }
    private float Progress
    {
        get {
            return player != null &&
                   player.TotalObjectives > 0? Mathf.Clamp01((player.TotalObjectives - player.Objectives) /
                                                             (float)player.TotalObjectives) : 0;
        }
    }

    public SpeedrunManager(string dataDirectory, string legacyRoot = null, GameBuildInfo gameBuild = null)
    {
        build = gameBuild;
        store = new RunStore(dataDirectory, legacyRoot);
        tracker = new RunTracker(store);
        Instance = this;
        RunReviewRecorder.Restore(dataDirectory);
        MovementCapture.Initialize(store, dataDirectory, build);
    }

    public void Configure(TrainerCommands commands)
    {
        liveCommands = commands;
        RunReviewRecorder.ConfigureRetention(commands.ReplayMinSeconds, commands.ReplayMinCrystals);
        if (active && !commands.Speedrun && timerState == 1)
            End(RunOutcome.Interrupted);
        exportReports = (commands.ReportOptions & 1u) != 0;
        active = commands.Speedrun;
        practice = commands.Practice;
        SelectedLevel = Mathf.Clamp(commands.SpeedrunLevelIndex, 0, LevelIndices.Length - 1);
        if (!active)
        {
            timerState = 0;
            finalTime = 0;
            NeedsDeathReload = false;
        }
        currentFeatures = RunAids.None;
        if (active && practice)
            currentFeatures |= RunAids.Practice;
        if (PracticeRandom.Enabled)
            currentFeatures |= RunAids.FixedSeed;
        if (HitboxDrawer.Enabled)
            currentFeatures |= RunAids.Hitboxes;
        if (FullbrightManager.AnyActive)
            currentFeatures |= RunAids.Fullbright;
        if (FreeCam.FreeCamEnabled)
            currentFeatures |= RunAids.FreeCamera;
        if (!commands.Restricted && commands.RequestsMap)
            currentFeatures |= RunAids.Minimap;
        if (!commands.Restricted && commands.RequestsPath)
            currentFeatures |= RunAids.Path;
        if (commands.RequestsPlanner || (commands.PlannerRequest != 0 && commands.PlannerRequest != 4))
            currentFeatures |= RunAids.SpawnPlanner;
        if (LiveInspector.Active)
            currentFeatures |= RunAids.LiveInspector;
        tracker.ObserveFeatures(currentFeatures);
    }

    public void SceneLoaded(int index)
    {
        if (timerState == 1)
            End(RunOutcome.Interrupted);
        RunReviewRecorder.DiscardTransient();
        player = null;
        NeedsDeathReload = false;
        if (Array.IndexOf(LevelIndices, index) >= 0)
        {
            timerState = 0;
            finalTime = 0;
            runScene = index;
        }
        else
            PracticeRandom.Disable();
    }

    public void PlayerReady(MouseLook mouse, bool startObserved = true)
    {
        if (mouse == null || !IsPlayableScene)
            return;
        player = mouse;
        if (active && timerState == 0 && !NeedsDeathReload && MouseLook.Health > 0 && !Replay3D.Reserved)
            BeginRun(startObserved);
    }

    private void BeginRun(bool startObserved)
    {
        HarmonyPatches.RefreshSeedCoverage();
        runScene = SceneManager.GetActiveScene().buildIndex;
        RunAids aids = currentFeatures;
        if (practice)
            aids |= RunAids.Practice;
        if (PracticeRandom.Enabled)
            aids |= RunAids.FixedSeed;
        if (!tracker.Begin(runScene, DateTime.UtcNow.Ticks, aids, PracticeRandom.Evidence,
                           build == null ? null : build.Snapshot(), exportReports, startObserved))
            return;
        started = Stopwatch.GetTimestamp();
        timerState = 1;
        finalTime = 0;
        nextCheckpoint = Time.realtimeSinceStartupAsDouble + 5;
        lastObserved = Time.realtimeSinceStartupAsDouble;
        haveAuditPosition = false;
        nextPatchAudit = 0;
        patchesTrusted = false;
        lastHudSeen = double.NegativeInfinity;
        MovementCapture.Begin(player);
        RunReviewRecorder.Begin(tracker.Active, player);
    }

    public void Tick()
    {
        double playNow = Time.realtimeSinceStartupAsDouble;
        bool playing = IsPlayableScene && player != null && MouseLook.Health > 0 &&
                       player.TotalObjectives > 0 && player.Objectives > 0 && Time.timeScale > 0 &&
                       Application.isFocused && !MenuCursor.OwnsInput && !Replay3D.Reserved &&
                       !ReplayGamePause.Frozen;
        if (store.Modes.Observe(SceneManager.GetActiveScene().buildIndex, active ? (practice ? 2 : 1) : 0,
                                playing, playNow) &&
            playNow >= nextPlaySave)
        {
            nextPlaySave = playNow + 5;
            store.QueueSave();
        }
        if (Replay3D.Reserved)
            return;
        MovementCapture.Tick();
        if (!active || !IsPlayableScene)
            return;
        if (timerState == 1)
        {
            double now = Time.realtimeSinceStartupAsDouble;
            if (now - lastObserved > .5)
                tracker.Warn(RunWarnings.ObservationGap);
            lastObserved = now;
            if (player == null)
                tracker.Warn(RunWarnings.StateUnavailable);
            tracker.ObserveSeed(PracticeRandom.Evidence);
            tracker.Update(Elapsed, Progress);
            AuditRun();
            RunReviewRecorder.Observe(player, Elapsed);
            if (Time.realtimeSinceStartupAsDouble >= nextCheckpoint)
            {
                store.QueueSave();
                nextCheckpoint = Time.realtimeSinceStartupAsDouble + 5;
            }
            if (MouseLook.Health <= 0)
                NotifyDeath();
            else if (player != null && player.TotalObjectives > 0 && player.Objectives <= 0)
                Finish();
        }
    }

    public void MarkAssisted(RunAids aid = RunAids.Teleport)
    {
        tracker.Mark(aid);
    }

    public void NotifyDeath()
    {
        if (!active || !IsPlayableScene || timerState != 1 || NeedsDeathReload)
            return;
        End(RunOutcome.Death);
        timerState = 0;
        finalTime = 0;
        NeedsDeathReload = true;
    }

    private void End(RunOutcome outcome)
    {
        double elapsed = Elapsed;
        if (outcome == RunOutcome.Interrupted)
            tracker.Warn(RunWarnings.ModeInterrupted);
        HarmonyPatches.RefreshSeedCoverage();
        tracker.ObserveSeed(PracticeRandom.Evidence);
        AuditBeforeSuppression(liveCommands);
        AuditRun(true);
        RunReviewRecorder.Observe(player, elapsed, true);
        bool ended = tracker.End(outcome, DateTime.UtcNow.Ticks, elapsed, Progress);
        if (ended)
            RunReviewRecorder.Finish(tracker.Last, store);
        MovementCapture.End();
        // Leaving a level (including Escape -> TitleScreen) must also stop the
        // live clock. Keep the same final time in the HUD, history and report.
        finalTime = ended ? tracker.Last.Seconds : elapsed;
        timerState = 2;
    }

    public static void ObjectivesChanged(MouseLook mouse)
    {
        if (Instance != null && mouse != null && mouse == Instance.player && Instance.IsPlayableScene &&
            mouse.TotalObjectives > 0 && mouse.Objectives <= 0)
            Instance.Finish();
    }

    public static void ObjectivesObserved(MouseLook mouse)
    {
        RunReviewRecorder.ObjectivesObserved(mouse);
    }

    private void Finish()
    {
        if (!active || timerState != 1 || !IsPlayableScene || player == null || MouseLook.Health <= 0)
            return;
        End(RunOutcome.Completed);
    }

    public void BeforeReload(int scene)
    {
        if (timerState == 1)
            End(RunOutcome.Reset);
        RunReviewRecorder.DiscardTransient();
        FreeCam.DisableActive();
        MouseLook.Health = 2;
        timerState = 0;
        finalTime = 0;
        NeedsDeathReload = false;
        player = null;
        runScene = scene;
    }

    public void PrepareReplay()
    {
        if (!Running)
            NeedsDeathReload = false;
    }

    public void ResetStatistics()
    {
        if (Running || !tracker.ResetStatistics())
            return;
        timerState = 0;
        finalTime = 0;
    }

    public void WriteSnapshot(SnapshotBuffer data, TrainerCommands commands)
    {
        data.Write(SharedLayout.ModeStatsSinceUtc, store.Modes.SinceUtc);
        data.Write(SharedLayout.ModeStatsRevision, store.Modes.Revision);
        for (int i = 0; i < store.Modes.Rows.Length; i++)
        {
            var row = store.Modes.Rows[i];
            data.Write(SharedLayout.ModeAttempts + i * 4, row.Attempts);
            data.Write(SharedLayout.ModeFinished + i * 4, row.Finished);
            data.Write(SharedLayout.ModeResets + i * 4, row.Resets);
            data.Write(SharedLayout.ModeDeaths + i * 4, row.Deaths);
            data.Write(SharedLayout.ModeFinishSeconds + i * 8, row.FinishSeconds);
            data.Write(SharedLayout.ModePlaySeconds + i * 8, row.PlaySeconds);
        }
        for (int i = 0; i < LevelIndices.Length; i++)
        {
            data.Write(SharedLayout.ModePersonalBest + i * 8, store.Best(LevelIndices[i]));
            data.Write(SharedLayout.ModePbUtc + i * 8, store.Modes.BestUtc[i]);
        }
        RunRecord record = timerState == 0 ? tracker.Active : Record;
        if (record != null && record.Environment != null && record.Environment.GameHash == "UNKNOWN" &&
            build != null && build.Supported)
            record.Environment.GameHash = build.Hash;
        bool checksReady = timerState == 2 || build == null ||
                           build.Supported && external.Status == 1 &&
                               DateTime.UtcNow.Ticks - external.AtUtc < TimeSpan.TicksPerSecond * 4 &&
                               PracticeRandom.SourcesVerified;
        data.Write(SharedLayout.RunValidity, (byte)(checksReady && RunValidity.Eligible(record) ? 1 : 2));
        data.Write(SharedLayout.ExternalScanStatus, (byte)external.Status);
        data.WriteString(SharedLayout.ExternalTool, external.Tool, 96);
        data.WriteString(
            SharedLayout.ValidationDetail,
            checksReady ? RunValidity.Reason(record) : "Local integrity checks pending or unavailable", 192);
        RunReviewRecorder.WriteStatus(data, record == null ? 0 : record.Id,
                                      record == null ? -1 : record.Scene);
        data.Write(SharedLayout.SpeedrunModeActive, active);
        data.Write(SharedLayout.PracticeModeActive, practice);
        data.Write(SharedLayout.SpeedrunLevelIndex, (byte)SelectedLevel);
        data.Write(SharedLayout.PracticeSeed, commands.PracticeSeed);
        data.Write(SharedLayout.TimerState, timerState);
        data.Write(SharedLayout.CurrentRunTimeSeconds, Elapsed);
        data.Write(SharedLayout.PersonalBestSeconds, store.Best(runScene));
        data.Write(SharedLayout.PreviousBestSeconds, record == null ? 0 : record.PreviousBestSeconds);
        data.Write(SharedLayout.TotalRuns, store.Runs);
        data.Write(SharedLayout.TotalFinishedRuns, store.Finished);
        data.Write(SharedLayout.TotalDeaths, store.Deaths);
        data.Write(SharedLayout.TotalTimeForAverage, store.TotalFinishedTime);
        data.Write(SharedLayout.AveragePace,
                   store.Finished > 0 ? store.TotalFinishedTime / store.Finished : 0);
        data.Write(SharedLayout.TotalResets, store.Resets);
        data.Write(SharedLayout.CurrentRunNumber, record != null ? record.Number : 0);
        data.Write(SharedLayout.SessionAttempts, tracker.SessionAttempts);
        data.Write(SharedLayout.SessionCompleted, tracker.SessionCompleted);
        data.Write(SharedLayout.SessionResets, tracker.SessionResets);
        data.Write(SharedLayout.AttemptsToday, store.Today(0));
        data.Write(SharedLayout.CompletedToday, store.Today(1));
        data.Write(SharedLayout.ResetsToday, store.Today(2));
        data.Write(SharedLayout.SessionTimeSeconds,
                   (Stopwatch.GetTimestamp() - sessionStarted) / (double)Stopwatch.Frequency);
        data.Write(SharedLayout.AverageResetSeconds,
                   store.TimedResets > 0 ? store.TotalResetTime / store.TimedResets : 0);
        data.Write(SharedLayout.FurthestProgress,
                   Math.Max(store.FurthestProgress, record != null ? record.Progress : 0));
        data.Write(SharedLayout.RunProgress, record != null ? record.Progress : 0);
        data.Write(SharedLayout.TrainerFeatures, unchecked((int)currentFeatures));
        data.Write(SharedLayout.RunUsedFeatures, record != null ? unchecked((int)record.Aids) : 0);
        data.Write(SharedLayout.RunSceneIndex, record != null ? record.Scene : -1);
        data.Write(SharedLayout.RunEndedUtc, record != null ? record.EndedUtc : 0);
        data.Write(SharedLayout.ObservedFromStart, record != null && record.ObservedFromStart);
        data.Write(SharedLayout.RunWarnings, record != null ? unchecked((int)record.Warnings) : 0);
        data.Write(SharedLayout.RunOutcome, record != null ? (byte)record.Outcome : (byte)RunOutcome.Running);
        data.Write(SharedLayout.ReportsEnabled, exportReports);
        data.Write(SharedLayout.ReportEventCount, record != null ? record.EventCount : 0);
        data.Write(SharedLayout.ReportSaveStatus,
                   (byte)(store.LastReportError != null || store.ReportQueueWarning != null ? 2
                          : store.ReportsPending                                            ? 1
                                                                                            : 0));
        data.WriteString(SharedLayout.ReportError, store.LastReportError ?? store.ReportQueueWarning ?? "",
                         96);
        data.WriteString(SharedLayout.StatsWarning,
                         store.LoadWarning ?? RunReviewRecorder.RecoveryWarning ?? "", 96);
        data.Write(SharedLayout.StatsSaveStatus, store.LastError != null ? 2 : store.SavePending ? 1 : 0);
        data.WriteString(SharedLayout.StatsError, store.LastError ?? "", 96);
        data.Write(SharedLayout.SeedSourcesVerified, PracticeRandom.SourcesVerified);
        if (record != null)
        {
            if (record.Environment != null)
            {
                data.WriteString(SharedLayout.GameVersion, record.Environment.GameVersion, 64);
                data.WriteString(SharedLayout.UnityVersion, record.Environment.UnityVersion, 32);
                data.WriteString(SharedLayout.GameAssemblyHash, record.Environment.GameHash, 65);
            }
            data.Write(SharedLayout.SeedKind, (byte)record.SeedType);
            data.Write(SharedLayout.SeedKnown, record.SeedKnown);
            data.Write(SharedLayout.RunSeed, record.Seed);
            data.Write(SharedLayout.InitialRngSeed, record.InitialRngSeed);
            data.Write(SharedLayout.RngReseedCount, record.Reseeds);
            data.Write(SharedLayout.RunAssisted, record.Aids != RunAids.None);
            data.Write(SharedLayout.LastRunPb, record.PersonalBest);
            data.Write(SharedLayout.RunStartedUtc, record.StartedUtc);
            data.Write(SharedLayout.RunIntegrity,
                       (byte)(record.Aids != RunAids.None || record.SeedType == SeedKind.Set ? 1 : 0));
        }
        else
        {
            SeedEvidence evidence = PracticeRandom.Evidence;
            data.Write(SharedLayout.SeedKind, (byte)evidence.Kind);
            data.Write(SharedLayout.SeedKnown, evidence.Known);
            data.Write(SharedLayout.RunSeed, evidence.Seed);
            data.Write(SharedLayout.InitialRngSeed, evidence.InitialRngSeed);
            data.Write(SharedLayout.RngReseedCount, evidence.Reseeds);
            data.Write(SharedLayout.RunIntegrity,
                       (byte)(currentFeatures != RunAids.None || evidence.Kind == SeedKind.Set ? 1 : 0));
        }
        int count = Math.Min(16, store.History.Count);
        data.Write(SharedLayout.HistoryCount, count);
        for (int i = 0; i < count; i++)
        {
            RunRecord r = store.History[store.History.Count - 1 - i];
            int o = SharedLayout.HistoryBytes + i * 64;
            data.Write(o, (long)r.Number);
            data.Write(o + 8, r.StartedUtc);
            data.Write(o + 16, r.EndedUtc);
            data.Write(o + 24, r.Seconds);
            data.Write(o + 32, r.Scene);
            data.Write(o + 36, r.Seed);
            data.Write(o + 40, r.InitialRngSeed);
            data.Write(o + 44, r.Progress);
            data.Write(o + 48, (byte)r.SeedType);
            data.Write(o + 49, r.SeedKnown);
            data.Write(o + 50, (byte)r.Outcome);
            data.Write(o + 51, r.PersonalBest);
            data.Write(o + 52, unchecked((int)r.Aids));
            data.Write(o + 56, r.Reseeds);
            data.Write(o + 60,
                       unchecked((int)((uint)r.Warnings | (RunValidity.Eligible(r) ? 0x80000000u : 0u))));
        }
    }

    public void Dispose()
    {
        if (timerState == 1)
            End(RunOutcome.Interrupted);
        store.Dispose();
        if (Instance == this)
            Instance = null;
    }
}
