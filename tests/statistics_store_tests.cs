using System;
using System.Globalization;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

// Runs the shipped mod's real storage/tracker without Unity or game assemblies.
public static class StatisticsStoreTests
{
    private static void Check(bool ok, string label)
    {
        if (!ok)
            throw new Exception(label);
    }

    private static string WithChecksum(string body)
    {
        using (var hash = SHA256.Create()) return body + "checksum=" +
            BitConverter.ToString(hash.ComputeHash(Encoding.UTF8.GetBytes(body)))
                .Replace("-", "")
                .ToLowerInvariant() +
            "\n";
    }

    public static int Main(string[] args)
    {
        string wanted =
            Regex.Match(File.ReadAllText(args[1]), "#define SUITE_MOD_VERSION \"([^\"]+)\"").Groups[1].Value;
        Check(typeof(RunStore).Assembly.GetName().Version.ToString() == wanted + ".0", "shipped mod version");
        string root = args[0], legacy = Path.Combine(root, "legacy");
        Directory.CreateDirectory(legacy);
        File.WriteAllText(Path.Combine(legacy, "speedruns-v3.txt"),
                          "version=3\nruns=25\nfinished=4\ndeaths=6\ntotalSeconds=75\npb.3=12.5\n");
        using (var store = new RunStore(legacy))
        {
            Check(store.Runs == 25 && store.Best(3) == 12.5 && store.Modes.Rows[1].Attempts == 0,
                  "legacy totals and PB preserved without inventing mode history");
            store.Save();
        }
        using (var store = new RunStore(legacy)) Check(
            store.Runs == 25 && store.Best(3) == 12.5 && store.Modes.SinceUtc > 0, "migration persisted");
        string folder = Path.Combine(root, "current");
        long now = DateTime.UtcNow.Ticks, since;
        using (var store = new RunStore(folder))
        {
            since = store.Modes.SinceUtc;
            var tracker = new RunTracker(store);
            var seed = new SeedEvidence();
            seed.Reset(true);
            seed.Observe(12, 12, false, 0);
            foreach (int scene in ModeStatistics.Scenes)
            {
                Check(tracker.Begin(scene, now++, RunAids.None, seed), "begin clean");
                Check(!tracker.Begin(scene, now++, RunAids.None, seed), "duplicate start ignored");
                tracker.End(RunOutcome.Completed, now++, 10, 1);
                Check(!tracker.End(RunOutcome.Completed, now++, 10, 1), "duplicate end ignored");
                tracker.Begin(scene, now++, RunAids.Practice, seed);
                tracker.End(RunOutcome.Death, now++, 5, .1f);
                tracker.Begin(scene, now++, RunAids.None, seed);
                tracker.End(RunOutcome.Reset, now++, 1, 0);
            }
            store.Modes.Observe(13, 0, true, 0);
            store.Modes.Observe(13, 0, true, .25);
            store.RecordBest(13, 9);
            CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("de-DE");
            store.Save();
        }
        CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("en-US");
        using (var store = new RunStore(folder))
        {
            Check(store.Runs == 16 && store.Finished == 8 && store.Deaths == 8 && store.Resets == 8,
                  "global counters");
            Check(store.Modes.SinceUtc == since && store.Modes.Rows[21].PlaySeconds == .25,
                  "persistent time");
            foreach (int scene in ModeStatistics.Scenes)
            {
                var clean = store.Modes.Rows[ModeStatistics.Row(scene, 1)];
                var practice = store.Modes.Rows[ModeStatistics.Row(scene, 2)];
                Check(clean.Attempts == 1 && clean.Finished == 1 && practice.Attempts == 1 &&
                          practice.Deaths == 1,
                      "per-mode/category counters roundtrip");
            }
            long date = store.Modes.BestUtc[7];
            Check(store.ResetStatistics() && store.Best(13) == 9 && store.Modes.BestUtc[7] == date &&
                      store.Modes.Rows[22].Attempts == 0,
                  "reset preserves personal best and date");
            store.Save();
        }
        string file = Path.Combine(folder, "speedruns-v3.txt");
        File.Copy(file, file + ".bak", true);
        string body = File.ReadAllText(file);
        body = body.Substring(0, body.LastIndexOf("checksum=", StringComparison.Ordinal));
        File.WriteAllText(file, WithChecksum(body + "mode.0=-1|0|0|0|0|0\n"));
        using (var store = new RunStore(folder))
            Check(store.Best(13) == 9 && !string.IsNullOrEmpty(store.LoadWarning),
                  "corrupt mode rows recover from backup");
        string future = Path.Combine(root, "future");
        Directory.CreateDirectory(future);
        string futureFile = Path.Combine(future, "speedruns-v3.txt");
        string newer = WithChecksum(body.Replace("version=8", "version=9"));
        File.WriteAllText(futureFile, newer);
        using (var store = new RunStore(future))
        {
            Check(!store.ResetStatistics(), "future format stays read-only");
            store.Save();
        }
        Check(File.ReadAllText(futureFile) == newer, "future data preserved byte for byte");
        Console.WriteLine(
            "Shipped mod: version, legacy migration, eight-mode tracking, quick/duplicate hooks, locale, atomic persistence, reset/PB dates, corrupt backup recovery and future-format preservation: PASS");
        return 0;
    }
}
