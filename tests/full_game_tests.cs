using System;
using System.IO;
using System.Threading;

public static class FullGameTests
{
    private const long Epoch = 639000000000000000L;
    private static void Check(bool ok, string message) { if (!ok) throw new Exception(message); }
    private static void Equal(double actual, double expected, string message) { Check(Math.Abs(actual - expected) < .000001, message + ": " + actual + " != " + expected); }
    private static void Reject(Action action, string message)
    {
        bool rejected = false; try { action(); } catch (Exception) { rejected = true; }
        Check(rejected, message);
    }
    private static FullGameRecord Complete(FullGamePlan plan, long id, bool bad = false)
    {
        var run = new FullGameRun(); run.Start(plan, id, 0);
        double now = 0;
        for (int i = 0; i < 8; i++)
        {
            now += 2; Check(run.PlayerReady(FullGamePlan.Scenes[i], now), "ordered player ready");
            now += i + 1;
            Check(run.EndAttempt(FullGamePlan.Scenes[i], id + i + 100, i + 1, 1, !bad, true,
                plan.Mode == FullGameMode.SetSeed ? 2u : 0u, 0, now, id + (long)(now * 10000000)), "ordered completion");
        }
        return run.Snapshot(now);
    }
    private static void ClockCases()
    {
        int[] seeds = { 1, -2, 3, 4, 5, 6, 7, int.MaxValue };
        var plan = new FullGamePlan(FullGameMode.RandomSeed, seeds, 255);
        Check(plan.Matches(FullGameMode.RandomSeed, new int[8], 0), "RSFG ignores fixed-seed drafts");
        var fixedPlan = new FullGamePlan(FullGameMode.SetSeed, seeds, 8);
        string key = fixedPlan.Key; seeds[0] = 99;
        Check(fixedPlan.Seeds[0] == 1 && fixedPlan.Key == key, "seed plan frozen before start");
        Check(!fixedPlan.Matches(FullGameMode.SetSeed, seeds, 8), "another seed plan cannot share a PB");
        Check(new FullGamePlan(FullGameMode.SetSeed, fixedPlan.Seeds, 0).Key != key, "seed recipe separates full-game PBs");
        var run = new FullGameRun(); run.Start(plan, Epoch, 0);
        Equal(run.Time(1), 0, "initial Classic loading excluded");
        Check(run.Scene == 3 && run.PlayerReady(3, 2), "starts at Classic");
        Check(!run.PlayerReady(3, 3), "duplicate ready callback does not reset clock");
        Check(!run.EndAttempt(3, Epoch + 1, 5, 2, true, true, 0, 0, 7, Epoch + 70000000), "reset does not finish a sector");
        Equal(run.Time(8), 6, "reset decision time remains counted");
        run.BeginLoading(8); Equal(run.Time(10), 6, "retry loading excluded");
        Check(run.PlayerReady(3, 11), "same level ready after retry");
        Check(run.EndAttempt(3, Epoch + 2, 4, 1, true, true, 0, 0, 15, Epoch + 150000000), "Classic completes once");
        Equal(run.Record.Sectors[0], 10, "sector includes failed attempt and successful retry");
        Check(run.Record.Retries[0] == 1 && run.Record.Replays[0] == Epoch + 2 && run.Scene == 4, "only successful replay is grouped");
        Equal(run.Time(19), 10, "automatic transition time excluded");
        Check(run.PlayerReady(4, 20), "automatically advances to Camera");
        run.EndAttempt(4, Epoch + 3, 6, 1, true, true, 0, 0, 26, Epoch + 260000000);
        double now = 26;
        for (int i = 2; i < 8; i++)
        {
            now += 1; Check(run.PlayerReady(FullGamePlan.Scenes[i], now), "bonus order respected");
            now += 3; run.EndAttempt(FullGamePlan.Scenes[i], Epoch + i + 10, 3, 1, true, true, 0, 0, now, Epoch + (long)(now * 10000000));
            if (i < 7) Check(run.Active && !run.Record.Ranked, "not finished before last bonus");
        }
        Equal(run.Time(now), 34, "all eight level sectors sum to full-game time");
        Equal(run.LoadTime(now), 16, "exact excluded loading intervals");
        Check(run.Record.Ranked && !run.Active && run.Record.Stage == 8, "last bonus completes full run");
        Equal(run.Time(now + 100), 34, "completed clock frozen");
        var snapshot = run.Snapshot(now + 100); FullGameFormat.Validate(snapshot);
        string bytes = FullGameFormat.Encode(snapshot);
        Check(FullGameFormat.Encode(FullGameFormat.Decode(bytes)) == bytes, "canonical full-game metadata roundtrip");
        snapshot.Replays[1] = snapshot.Replays[0]; Reject(() => FullGameFormat.Encode(snapshot), "duplicate level replay identity rejected");
        Reject(() => FullGameFormat.Decode(bytes.Replace("level|2|", "level|3|")), "tampered metadata rejected");
        run.Start(fixedPlan, Epoch + 1000, now + 101);
        Equal(run.Time(now + 102), 0, "full reset alone resets total to zero");
        Check(run.Scene == 3 && run.Record.Stage == 0, "full reset returns to Classic");
        run.PlayerReady(3, now + 103);
        run.EndAttempt(3, Epoch + 2000, 1, 3, false, true, 8, 0, now + 104, Epoch + 2000000000);
        run.BeginLoading(now + 104); run.PlayerReady(3, now + 105);
        run.EndAttempt(3, Epoch + 2001, 1, 1, true, true, 2, 0, now + 106, Epoch + 2100000000);
        Check(!run.Record.Eligible && run.Record.Aids == 10 && run.Record.Retries[0] == 1, "death/retry cannot wash a previous forbidden aid");
        run.SelectionChanged(); Check((run.Record.Warnings & 16384u) != 0, "changed seed/category is sticky");
        run.PlayerReady(5, now + 107);
        Check(!run.Active && run.Record.State == FullGameState.Interrupted, "skipping a level interrupts campaign");
        var clock = new FullGameRun(); clock.Start(plan, Epoch + 9999, 100); clock.PlayerReady(3, 101);
        Equal(clock.Time(103), 2, "monotonic clock advances"); Equal(clock.Time(102), 2, "backward clock cannot subtract time");
        Check(!clock.Record.Eligible, "backward timestamp invalidates attempt");
        Console.WriteLine("Full-game clock: 8 ordered levels, retry/death time, exact load exclusion, full reset, immutable seed plans, metadata and sticky integrity: PASS");
    }
    private static void StorageCases(string folder)
    {
        var plan = new FullGamePlan(FullGameMode.SetSeed, new[] { 1, 2, 3, 4, 5, 6, 7, 8 }, 128);
        var record = Complete(plan, Epoch + 10000000000L);
        string root = Path.Combine(folder, "full-game-store"); Directory.CreateDirectory(root);
        using (var store = new FullGameStore(root))
        {
            store.Queue(record); Check(store.Flush(), "completed metadata flushed");
            Check(store.Best(plan) != null && store.Best(plan).Id == record.Id, "completed clean PB indexed");
            var invalid = Complete(plan, record.Id + 10000, true); store.Queue(invalid); Check(store.Flush(), "invalid run kept for review");
            Check(store.Best(plan).Id == record.Id, "invalid completion cannot replace PB");
            Check(store.Best(new FullGamePlan(FullGameMode.RandomSeed, new int[8], 0)) == null, "RSFG and SSFG stay separate");
        }
        using (var restored = new FullGameStore(root))
        {
            var end = DateTime.UtcNow.AddSeconds(5); while (!restored.Ready && DateTime.UtcNow < end) Thread.Sleep(1);
            Check(restored.Ready && restored.Best(plan).Id == record.Id, "full-game PB survives restart");
            var copy = restored.Best(plan); copy.Sectors[0] = 99;
            Check(restored.Best(plan).Sectors[0] == 1, "published PB does not leak mutable arrays");
            Check(restored.NextId(record.Id) > record.Id, "identity never reuses existing full run");
        }
        string file = Path.Combine(root, "FullGame", "full-" + record.Id + ".isfg");
        string future = File.ReadAllText(file).Replace("ELITE_FULL_GAME|1", "ELITE_FULL_GAME|2"); File.WriteAllText(file, future);
        using (var preserved = new FullGameStore(root))
        {
            preserved.Queue(record); Check(!preserved.Flush(), "future-version overwrite rejected");
            Check(File.ReadAllText(file) == future, "future metadata left byte-for-byte intact");
        }
        File.WriteAllText(Path.Combine(folder, "full-game-wire.isfg"), FullGameFormat.Encode(record));
        Console.WriteLine("Full-game storage: worker persistence, independent seed-profile PBs, invalid exclusion, restart recovery and future preservation: PASS");
    }
    public static int Main(string[] args)
    {
        ClockCases(); StorageCases(args.Length > 0 ? args[0] : Path.Combine(Path.GetTempPath(), "elite-fg-tests"));
        return 0;
    }
}
