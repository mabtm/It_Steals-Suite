using System;
using System.Diagnostics;
using UnityEngine.SceneManagement;

public sealed partial class SpeedrunManager
{
    private readonly FullGameRun fullGame = new FullGameRun();
    private readonly FullGameStore fullGameStore;
    private readonly int[] fullGameSeedScratch = new int[8];
    private FullGameRecord fullGameReference;
    private bool fullGamePendingLoad, fullGameExpectedLoad, fullGameReferencePending, fullGameNewPb;
    private double fullGameNextSave;
    public bool FullGameActive { get { return fullGame.Active; } }
    private static double FullClock { get { return Stopwatch.GetTimestamp() / (double)Stopwatch.Frequency; } }

    private void SaveFullGame()
    {
        var copy = fullGame.Snapshot(FullClock);
        if (copy != null) fullGameStore.Queue(copy);
        fullGameNextSave = FullClock + 5;
    }
    private void StopFullGame(FullGameState reason)
    {
        if (!fullGame.Active) return;
        if (timerState == 1) End(reason == FullGameState.Reset ? RunOutcome.Reset : RunOutcome.Interrupted);
        fullGame.Stop(reason, FullClock, DateTime.UtcNow.Ticks);
        fullGamePendingLoad = fullGameExpectedLoad = false;
        SaveFullGame();
    }

    // Resolve the frozen campaign plan before RNG configuration and the normal
    // per-attempt tracker see commands. The overlay never drives auto-splits.
    public TrainerCommands ResolveFullGameCommands(TrainerCommands incoming, bool request)
    {
        var mode = incoming.FullGameMode == 1 ? FullGameMode.RandomSeed :
                   incoming.FullGameMode == 2 ? FullGameMode.SetSeed : FullGameMode.None;
        for (int i = 0; i < 8; i++) fullGameSeedScratch[i] = incoming.FullGameSeed(i);
        if (request && (incoming.FullGameCmd == 1 || incoming.FullGameCmd == 2) && incoming.Speedrun && mode != FullGameMode.None)
        {
            if (fullGame.Active) StopFullGame(FullGameState.Reset);
            else if (timerState == 1) End(RunOutcome.Interrupted);
            var plan = new FullGamePlan(mode, fullGameSeedScratch, incoming.FullGameRawMask & 255u);
            fullGame.Start(plan, fullGameStore.NextId(DateTime.UtcNow.Ticks), FullClock);
            fullGameReference = fullGameStore.Best(plan);
            fullGameReferencePending = !fullGameStore.Ready;
            fullGameNewPb = false; fullGamePendingLoad = true;
            incoming.SpeedrunLoadLevelCmd = 1;
            SaveFullGame();
        }
        else if (fullGame.Active && (!incoming.Speedrun || mode == FullGameMode.None || request && incoming.FullGameCmd == 3))
            StopFullGame(FullGameState.Interrupted);
        if (fullGame.Active)
        {
            var plan = fullGame.Record.Plan;
            if (!plan.Matches(mode, fullGameSeedScratch, incoming.FullGameRawMask & 255u)) fullGame.SelectionChanged();
            int stage = Math.Min(7, fullGame.Record.Stage);
            incoming.PracticeModeActive = 0;
            incoming.SeededPractice = (byte)(plan.Mode == FullGameMode.SetSeed ? 1 : 0);
            incoming.SpeedrunLevelIndex = (byte)stage;
            incoming.PracticeSeed = plan.Seeds[stage];
            incoming.PlannerOptions = (incoming.PlannerOptions & ~2u) | ((plan.RawMask & (1u << stage)) != 0 ? 2u : 0u);
        }
        return incoming;
    }
    public bool TakeFullGameTransition()
    {
        return fullGame.Active && fullGamePendingLoad;
    }
    public int FullGameScene { get { return fullGame.Scene; } }
    // The game's pickup method can synchronously load a cutscene/menu after
    // the final objective. Keep only that completion path in the current scene
    // until the queued campaign transition applies the next level's RNG plan.
    public static void LoadCompletionScene(string name)
    {
        if (Instance != null && Instance.fullGame.Active && Instance.fullGamePendingLoad) return;
        SceneManager.LoadScene(name);
    }

    private void FullGameBeforeReload(int index)
    {
        if (!fullGame.Active) return;
        if (index != fullGame.Scene)
        { fullGame.Observe(0, 32); StopFullGame(FullGameState.Interrupted); return; }
        fullGame.BeginLoading(FullClock);
        fullGameExpectedLoad = true; fullGamePendingLoad = false;
        SaveFullGame();
    }
    private void FullGameSceneLoaded(int index)
    {
        if (!fullGame.Active) return;
        if (!fullGameExpectedLoad || index != fullGame.Scene)
        { fullGame.Observe(0, 32); StopFullGame(FullGameState.Interrupted); }
        fullGameExpectedLoad = false;
    }
    private void FullGamePlayerReady(int scene, double now)
    {
        if (fullGame.Active && fullGame.PlayerReady(scene, now)) SaveFullGame();
    }
    private void FullGameAttemptEnded(RunRecord record, double endedAt)
    {
        if (!fullGame.Active) return;
        bool advanced = fullGame.EndAttempt(record.Scene, record.Id, record.Seconds, (int)record.Outcome,
            RunValidity.Eligible(record), record.ValidationChecked, (uint)(record.Aids | record.InitialAids),
            (uint)record.Warnings, endedAt, record.EndedUtc);
        if (!advanced && fullGame.Active && (record.Outcome == RunOutcome.Reset || record.Outcome == RunOutcome.Death))
            fullGame.BeginLoading(endedAt);
        if (advanced) fullGamePendingLoad = fullGame.Active;
        if (fullGame.Record.Ranked)
        {
            double previous = fullGameReference == null ? 0 : fullGameReference.Seconds;
            fullGameNewPb = fullGameStore.Ready && (previous <= 0 || fullGame.Record.Seconds < previous);
        }
        SaveFullGame();
    }
    private void TickFullGame()
    {
        if (!fullGame.Active) return;
        if (tracker.Active != null) fullGame.Observe((uint)(tracker.Active.Aids | tracker.Active.InitialAids), (uint)tracker.Active.Warnings);
        if (FullClock >= fullGameNextSave) SaveFullGame();
    }
    private void WriteFullGame(SnapshotBuffer data)
    {
        var record = fullGame.Record;
        if (record == null) return;
        if (fullGameReferencePending && fullGameStore.Ready)
        { fullGameReference = fullGameStore.Best(record.Plan); fullGameReferencePending = false; }
        double now = FullClock;
        data.Write(SharedLayout.FullGameRunId, record.Id);
        data.Write(SharedLayout.FullGameMode, (byte)record.Plan.Mode);
        data.Write(SharedLayout.FullGameState, (byte)record.State);
        data.Write(SharedLayout.FullGameStage, (byte)record.Stage);
        data.Write(SharedLayout.FullGameEligible, record.Eligible);
        data.Write(SharedLayout.FullGameChecked, record.Checked);
        data.Write(SharedLayout.FullGameNewPb, fullGameNewPb);
        data.Write(SharedLayout.FullGameHistoryReady, fullGameStore.Ready);
        data.Write(SharedLayout.FullGameSeconds, fullGame.Time(now));
        data.Write(SharedLayout.FullGameSectorSeconds, fullGame.SectorTime(now));
        data.Write(SharedLayout.FullGameLoadingSeconds, fullGame.LoadTime(now));
        data.Write(SharedLayout.FullGameReferenceTotal, fullGameReference == null ? 0 : fullGameReference.Seconds);
        data.Write(SharedLayout.FullGameStartedUtc, record.StartedUtc);
        data.Write(SharedLayout.FullGameEndedUtc, record.EndedUtc);
        data.Write(SharedLayout.FullGameAids, unchecked((int)record.Aids));
        data.Write(SharedLayout.FullGameWarnings, unchecked((int)record.Warnings));
        data.Write(SharedLayout.FullGameRawMask, (int)record.Plan.RawMask);
        data.Write(SharedLayout.FullGameRevision, unchecked((int)fullGame.Revision));
        for (int i = 0; i < 8; i++)
        {
            data.Write(SharedLayout.FullGameSectors + i * 8, record.Sectors[i]);
            data.Write(SharedLayout.FullGameCumulative + i * 8, record.Cumulative[i]);
            data.Write(SharedLayout.FullGameReferenceSectors + i * 8, fullGameReference == null ? -1 : fullGameReference.Sectors[i]);
            data.Write(SharedLayout.FullGameAttemptSeconds + i * 8, record.AttemptSeconds[i]);
            data.Write(SharedLayout.FullGameReplayIds + i * 8, record.Replays[i]);
            data.Write(SharedLayout.FullGameRetries + i * 4, record.Retries[i]);
            data.Write(SharedLayout.FullGameSeeds + i * 4, record.Plan.Seeds[i]);
        }
        data.WriteString(SharedLayout.FullGameStorageError, fullGameStore.Error ?? store.LastReportError ?? store.ReportQueueWarning ?? fullGameStore.Warning, 192);
    }
}
