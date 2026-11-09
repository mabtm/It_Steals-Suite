using System;
using System.IO;
using System.Linq;

public static class RunLedgerTests
{
    static void Check(bool ok, string why) { if (!ok) throw new Exception(why); }
    static RunRecord Run(long id, double seconds, int category = 0)
    {
        return new RunRecord { Id = id, StartedUtc = id, EndedUtc = id + 1000, Scene = 3,
            Seconds = seconds, Outcome = RunOutcome.Completed, Category = (RunCategory)category,
            SeedType = category == 1 ? SeedKind.Set : SeedKind.Random, SeedKnown = true,
            Seed = 42, SelectedSeed = 42, ValidationChecked = true, ObservedFromStart = true,
            Aids = category == 1 ? RunAids.FixedSeed : RunAids.None,
            InitialAids = category == 1 ? RunAids.FixedSeed : RunAids.None, Progress = 1 };
    }
    public static void Main(string[] args)
    {
        string root = args[0]; Directory.CreateDirectory(root);
        long id = DateTime.UtcNow.Ticks;
        var ledger = new RunLedger(root);
        ledger.Append(RunLedger.Session(id));
        for (int i = 0; i < 200; i++) ledger.Append(RunLedger.Encode(Run(id + i, 200 - i), id, false, false));
        string file = Path.Combine(root, "run-ledger-v1.tsv");
        Check(File.ReadAllLines(file).Length == 202, "all records retained beyond 64 entries");
        var old = Run(id + 999, 2);
        Check(ledger.ShouldRetain(old), "top five accepted");
        Check(!ledger.ShouldRetain(Run(id + 1000, 100)), "slow completion does not displace top five");
        Check(ledger.ShouldRetain(Run(id + 1001, 100, 1)), "categories independent");
        var invalid = Run(id + 1002, .5); invalid.Aids = RunAids.Fullbright;
        Check(!ledger.ShouldRetain(invalid), "invalid runs cannot qualify");
        ledger.Append(RunLedger.Encode(old, id, false, false));
        long size = new FileInfo(file).Length;
        ledger.Append(RunLedger.Encode(old, 0, false, true));
        Check(new FileInfo(file).Length == size, "legacy migration never duplicates a stored identity");
        bool conflict = false;
        try { ledger.Append(RunLedger.Encode(Run(old.Id, 1), id, false, false)); }
        catch (IOException) { conflict = true; }
        Check(conflict && new FileInfo(file).Length == size, "conflicting identity cannot overwrite evidence");
        File.AppendAllText(file, "uncommitted-tail");
        var reopened = new RunLedger(root);
        reopened.Append(RunLedger.Encode(Run(id + 1003, 1), id, false, false));
        Check(reopened.Warning != null && Directory.GetFiles(root, "*.incomplete-*.bak").Length == 1,
              "crash tail recovered with exact original backup");
        Check(File.ReadAllLines(file).Last().Contains((id + 1003).ToString()), "append follows recovered tail");
        var damaged = File.ReadAllText(file).Replace("200|", "201|");
        string corrupt = Path.Combine(root, "damaged"); Directory.CreateDirectory(corrupt);
        File.WriteAllText(Path.Combine(corrupt, "run-ledger-v1.tsv"), damaged);
        bool rejected = false;
        try { new RunLedger(corrupt).Append(RunLedger.Encode(Run(id + 2000, 1), id, false, false)); }
        catch (InvalidDataException) { rejected = true; }
        Check(rejected && File.ReadAllText(Path.Combine(corrupt, "run-ledger-v1.tsv")) == damaged,
              "damaged archive preserved and append refused");
        string integrated = Path.Combine(root, "integrated");
        using (var store = new RunStore(integrated))
        {
            var tracker = new RunTracker(store); var seed = new SeedEvidence();
            seed.Reset(true); seed.Observe(42, 42, false, 0);
            for (int i = 0; i < 90; i++)
            {
                Check(tracker.Begin(3, id + 10000 + i, RunAids.None, seed), "begin");
                tracker.Active.ValidationChecked = true;
                tracker.End(i < 2 ? RunOutcome.Reset : RunOutcome.Completed, id + 20000 + i, i < 2 ? 1 : 5 + i, i < 2 ? 0 : 1);
            }
        }
        string[] rows = File.ReadAllLines(Path.Combine(integrated, "run-ledger-v1.tsv"));
        Check(rows.Count(s => !s.StartsWith("SESSION|") && s != RunLedger.Header) == 90,
              "real tracker archives quick resets and completed runs while short history stays bounded");
        long last = rows.Where(s => s != RunLedger.Header && !s.StartsWith("SESSION|")).Select(s => RunLedger.Parse(s).Id).Max();
        using (var store = new RunStore(integrated))
            Check(store.NextId(1) > last, "monotonic identity survives restart and backward system clock");
        long fixedId = 638000000000000000L;
        var fixture = RunLedger.Header + "\n" + RunLedger.Session(fixedId) + "\n";
        for (int i = 0; i < 8; i++)
        {
            var run = Run(fixedId + i + 1, i == 7 ? 1 : 10 + i, i % 3);
            if (run.Category == RunCategory.Practice) run.Aids = run.InitialAids = RunAids.Practice;
            if (i == 6) run.Aids |= RunAids.Fullbright;
            if (i == 7) { run.Outcome = RunOutcome.Reset; run.Progress = 0; }
            fixture += RunLedger.Encode(run, fixedId, i == 7, false) + "\n";
        }
        File.WriteAllText(Path.Combine(root, "run-data-wire.tsv"), fixture, new System.Text.UTF8Encoding(false));
        if (args.Length > 1) Check(File.ReadAllText(args[1]) == fixture, "C# journal bytes match shared native fixture");
        string rankedFolder = Path.Combine(root, "ranked-priority");
        long protectedId = fixedId + 5000;
        using (var store = new RunStore(rankedFolder))
        {
            object gate = typeof(RunStore).GetField("queueGate", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance).GetValue(store);
            lock (gate)
            {
                for (int i = 0; i < 64; i++) store.QueueReview(fixedId + i, new byte[] { 1 });
                store.QueueReview(protectedId, new byte[] { 2 }, null, "{}", "ELITE_REPLAY_KEEP|1|" + protectedId + "|PB");
                Check(store.ReportQueueWarning != null, "backpressure remains visible when an ordinary recording is omitted");
            }
        }
        string rankedPath = Path.Combine(rankedFolder, "RunReviews", "run-" + protectedId);
        Check(File.Exists(rankedPath + ".isrun") && File.Exists(rankedPath + ".keep"), "protected replay committed with durable retention marker");
        Check(!File.Exists(Path.Combine(rankedFolder, "RunReviews", "run-" + fixedId + ".isrun")), "oldest queued ordinary replay gives priority to PB");
        string fullFolder=Path.Combine(root,"full-game-storage");
        int mainThread=System.Threading.Thread.CurrentThread.ManagedThreadId,encodedThread=0;
        using(var store=new RunStore(fullFolder))
        {
            object gate=typeof(RunStore).GetField("queueGate",System.Reflection.BindingFlags.NonPublic|System.Reflection.BindingFlags.Instance).GetValue(store);
            lock(gate)
            {
                store.QueueFullGameReview(protectedId,()=> {encodedThread=System.Threading.Thread.CurrentThread.ManagedThreadId;return new byte[]{3,4,5};},
                    null,"{}","ELITE_REPLAY_KEEP|1|"+protectedId+"|FULLGAME");
                Check(encodedThread==0,"full-game map encoding deferred until storage worker runs");
            }
        }
        string fullPath=Path.Combine(fullFolder,"RunReviews","run-"+protectedId);
        Check(encodedThread!=0 && encodedThread!=mainThread && File.ReadAllBytes(fullPath+".isrun").SequenceEqual(new byte[]{3,4,5}) &&
              File.ReadAllText(fullPath+".keep").EndsWith("|FULLGAME"),"background full-game encoding commits original bytes and protection without Unity scene dependencies");
        string counters = Path.Combine(root, "counter-wire");
        using (var store = new RunStore(counters))
        {
            store.Modes.SinceUtc = fixedId;
            for (int level = 0; level < 8; level++)
            {
                store.RecordBest(ModeStatistics.Scenes[level], 10 + level);
                store.RecordBest(ModeStatistics.Scenes[level], 11 + level, RunCategory.SetSeed);
                store.Modes.BestUtc[level] = store.Modes.SetSeedBestUtc[level] = fixedId + level;
            }
            for (int i = 0; i < store.Modes.Rows.Length; i++)
            {
                var row = store.Modes.Rows[i]; row.Attempts = 20 + i; row.Finished = 3;
                row.Resets = 4; row.Deaths = 1; row.FinishSeconds = 33; row.PlaySeconds = 100 + i;
            }
            store.Save();
        }
        string counterBytes = File.ReadAllText(Path.Combine(counters, "speedruns-v3.txt"));
        File.WriteAllText(Path.Combine(root, "run-counter-wire.txt"), counterBytes, new System.Text.UTF8Encoding(false));
        if (args.Length > 2) Check(File.ReadAllText(args[2]) == counterBytes, "C# counter bytes match shared native fixture");
        Console.WriteLine("Durable ledger: 200 records, categories, top five, identity, migration, crash-tail backup, corrupt archive, real tracker quick resets and restart: PASS");
    }
}
