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

    private static void SetSeedCategory(string root)
    {
        string folder = Path.Combine(root, "set-seed");
        var seed = new SeedEvidence();
        seed.Reset(true);
        seed.Observe(13, 99, true, 0); // Zero is a real, reproducible selected seed.
        long now = DateTime.UtcNow.Ticks;
        RunRecord set;
        using (var store = new RunStore(folder))
        {
            var tracker = new RunTracker(store);
            Check(tracker.Begin(7, now++, RunAids.FixedSeed, seed,
                                category: RunCategory.SetSeed, selectedSeed: 0), "explicit Set-Seed start");
            tracker.Active.ValidationChecked = true;
            Check(RunValidity.Eligible(tracker.Active), "selected seed zero passes local category checks");
            tracker.End(RunOutcome.Completed, now++, 10, 1);
            set = tracker.Last.Copy();
            Check(set.PersonalBest && store.Best(7, RunCategory.SetSeed) == 10 && store.Best(7) == 0,
                  "Set-Seed PB never enters Random Seed PBs");
            tracker.Begin(7, now++, RunAids.Practice, seed, category: RunCategory.Practice);
            tracker.End(RunOutcome.Completed, now++, 6, 1);
            Check(!tracker.Last.PersonalBest && !RunValidity.Eligible(tracker.Last) &&
                      store.Best(7, RunCategory.Practice) == 6 && store.Best(7, RunCategory.SetSeed) == 10,
                  "Practice best stays unranked and cannot overwrite clean categories");
            var random = new SeedEvidence();
            random.Reset(true);
            random.Observe(123, 123, false, 0);
            tracker.Begin(7, now++, RunAids.None, random);
            tracker.Active.ValidationChecked = true;
            tracker.End(RunOutcome.Completed, now++, 20, 1);
            Check(tracker.Last.PersonalBest && tracker.Last.PreviousBestSeconds == 0 &&
                      store.Best(7) == 20 && store.Best(7, RunCategory.SetSeed) == 10,
                  "Random Seed PB remains independent even when slower");
            tracker.Begin(7, now++, RunAids.FixedSeed, seed,
                          category: RunCategory.SetSeed, selectedSeed: 0);
            tracker.Active.ValidationChecked = true;
            tracker.ObserveSelection(RunCategory.SetSeed, 1, false);
            tracker.ObserveSelection(RunCategory.SetSeed, 0, false);
            Check(!RunValidity.Eligible(tracker.Active) &&
                      (tracker.Active.Warnings & RunWarnings.SelectionChanged) != 0,
                  "changing seed and reverting cannot restore eligibility");
            tracker.End(RunOutcome.Completed, now++, 5, 1);
            Check(!tracker.Last.PersonalBest && store.Best(7, RunCategory.SetSeed) == 10,
                  "modified seed cannot improve category PB");
            tracker.Begin(7, now++, RunAids.FixedSeed, seed,
                          category: RunCategory.SetSeed, selectedSeed: 0);
            tracker.Active.ValidationChecked = true;
            tracker.ObserveSelection(RunCategory.RandomSeed, 0, false);
            Check(!RunValidity.Eligible(tracker.Active), "category change invalidates running attempt");
            tracker.End(RunOutcome.Reset, now++, 5, .2f);
            tracker.Begin(7, now++, RunAids.FixedSeed, seed,
                          category: RunCategory.SetSeed, selectedSeed: 1);
            tracker.Active.ValidationChecked = true;
            Check(!RunValidity.Eligible(tracker.Active), "observed seed must match preselected seed");
            tracker.End(RunOutcome.Reset, now++, 5, .2f);
            var rawSeed = new SeedEvidence();
            rawSeed.Reset(true);
            rawSeed.Observe(13, 0, true, 0, true);
            tracker.Begin(7, now++, RunAids.FixedSeed, rawSeed,
                          category: RunCategory.SetSeed, selectedSeed: 0, rawSpawnSeed: true);
            tracker.Active.ValidationChecked = true;
            Check(RunValidity.Eligible(tracker.Active), "raw recipe selected before start is eligible");
            tracker.ObserveSelection(RunCategory.SetSeed, 0, false);
            tracker.ObserveSelection(RunCategory.SetSeed, 0, true);
            Check(!RunValidity.Eligible(tracker.Active), "recipe change and revert stays invalid");
            tracker.End(RunOutcome.Reset, now++, 5, .2f);
            store.Save();
            Check(store.Modes.Rows[ModeStatistics.Row(7, 1)].Attempts == 1 &&
                      store.Modes.Rows[ModeStatistics.Row(7, 3)].Attempts == 5,
                  "Set-Seed attempt statistics do not enter Random Seed rows");
        }
        foreach (RunAids aid in Enum.GetValues(typeof(RunAids)))
        {
            if (aid == RunAids.None || aid == RunAids.FixedSeed)
                continue;
            var assisted = set.Copy();
            assisted.Aids |= aid;
            Check(!RunValidity.Eligible(assisted), "Set-Seed still rejects aid " + aid);
        }
        foreach (RunWarnings warning in Enum.GetValues(typeof(RunWarnings)))
        {
            if (warning == RunWarnings.None || warning == RunWarnings.EventLogTruncated)
                continue;
            var warned = set.Copy();
            warned.Warnings |= warning;
            Check(!RunValidity.Eligible(warned), "Set-Seed still rejects warning " + warning);
        }
        var missing = set.Copy();
        missing.ObservedFromStart = false;
        Check(!RunValidity.Eligible(missing), "late capture cannot validate Set-Seed");
        missing = set.Copy(); missing.ValidationChecked = false;
        Check(!RunValidity.Eligible(missing), "unfinished hook/build/external audit cannot validate Set-Seed");
        missing = set.Copy(); missing.InitialAids = RunAids.None;
        Check(!RunValidity.Eligible(missing), "RNG control must be selected at start");
        missing = set.Copy(); missing.InitialAids |= RunAids.Practice;
        Check(!RunValidity.Eligible(missing), "initial aids cannot be hidden by later flags");
        missing = set.Copy(); missing.RawSpawnSeed = true;
        Check(RunValidity.Eligible(missing), "explicitly selected raw-first-spawn recipe is supported");
        missing = set.Copy(); missing.Category = RunCategory.Practice;
        Check(!RunValidity.Eligible(missing), "practice is not a valid speedrun category");
        var pack = typeof(RunStore).GetMethod("Pack", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static);
        var unpack = typeof(RunStore).GetMethod("Unpack", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Static);
        string[] fields = ((string)pack.Invoke(null, new object[] {set, false})).Split('|');
        var legacy = (RunRecord)unpack.Invoke(null, new object[] {string.Join("|", fields, 0, 26)});
        Check(legacy != null && legacy.Category != RunCategory.SetSeed && !RunValidity.Eligible(legacy),
              "historical fixed-seed evidence is never reclassified by its seed kind");
        string upgrade = Path.Combine(root, "v8-upgrade");
        Directory.CreateDirectory(upgrade);
        string upgradeFile = Path.Combine(upgrade, "speedruns-v3.txt");
        string oldBody = "version=8\nruns=1\nfinished=1\ndeaths=0\ntotalSeconds=10\npb.7=20\nhistory=" +
                         string.Join("|", fields, 0, 26) + "\n";
        var preamble = new UTF8Encoding(true).GetPreamble();
        byte[] originalText = Encoding.UTF8.GetBytes(WithChecksum(oldBody));
        byte[] original = new byte[preamble.Length + originalText.Length];
        Buffer.BlockCopy(preamble, 0, original, 0, preamble.Length);
        Buffer.BlockCopy(originalText, 0, original, preamble.Length, originalText.Length);
        File.WriteAllBytes(upgradeFile, original);
        using (var upgraded = new RunStore(upgrade))
        {
            Check(upgraded.History.Count == 1 && !RunValidity.Eligible(upgraded.History[0]),
                  "v8 fixed-seed history remains practice during migration");
            upgraded.Save();
            upgraded.Save();
        }
        string[] rollbackFiles = Directory.GetFiles(upgrade, "speedruns-v3.txt.pre-v9-*.bak");
        Check(rollbackFiles.Length == 1 &&
                  Convert.ToBase64String(File.ReadAllBytes(rollbackFiles[0])) == Convert.ToBase64String(original),
              "one-time v8 rollback backup is byte exact including BOM and survives rotating saves");
        using (var upgraded = new RunStore(upgrade)) upgraded.Save();
        Check(Directory.GetFiles(upgrade, "speedruns-v3.txt.pre-v9-*.bak").Length == 1 &&
                  Convert.ToBase64String(File.ReadAllBytes(rollbackFiles[0])) == Convert.ToBase64String(original),
              "subsequent v9 saves never overwrite or duplicate the stable rollback backup");
        string blocked = Path.Combine(root, "v8-blocked-upgrade");
        Directory.CreateDirectory(blocked);
        string blockedFile = Path.Combine(blocked, "speedruns-v3.txt");
        File.WriteAllBytes(blockedFile, original);
        using (var upgraded = new RunStore(blocked))
        {
            // A directory occupying the reserved unique file models an OS
            // refusing backup creation, without relying on user permissions.
            string rollback = upgraded.PreUpgradeBackupPath;
            Directory.CreateDirectory(rollback);
            upgraded.Save();
            Check(!string.IsNullOrEmpty(upgraded.LastError) &&
                      Convert.ToBase64String(File.ReadAllBytes(blockedFile)) == Convert.ToBase64String(original),
                  "failed rollback preservation cannot write v9 over stable v8 data");
            Directory.Delete(rollback);
            upgraded.Save();
            Check(upgraded.LastError == null && File.Exists(rollback) &&
                      Convert.ToBase64String(File.ReadAllBytes(rollback)) == Convert.ToBase64String(original),
                  "upgrade retries only after rollback preservation succeeds");
        }
        using (var store = new RunStore(folder))
        {
            Check(store.Best(7) == 20 && store.Best(7, RunCategory.SetSeed) == 10 && store.Best(7, RunCategory.Practice) == 6 &&
                      store.History[0].Category == RunCategory.SetSeed &&
                      store.History[0].SelectedSeed == 0 && RunValidity.Eligible(store.History[0]),
                  "category, selected seed and separate PBs survive persistence");
            long setDate = store.Modes.SetSeedBestUtc[3];
            Check(setDate > 0 && store.ResetStatistics() &&
                      store.Best(7, RunCategory.SetSeed) == 10 && store.Modes.SetSeedBestUtc[3] == setDate,
                  "statistics reset preserves separate Set-Seed PB and date");
        }
        Console.WriteLine("Set-Seed: explicit selection, zero seed, assist/integrity checks, mid-run edits, legacy isolation, separate PB/statistics and persistence: PASS");
    }

    public static int Main(string[] args)
    {
        string wanted =
            Regex.Match(File.ReadAllText(args[1]), "#define SUITE_MOD_VERSION \"([^\"]+)\"").Groups[1].Value;
        Check(typeof(RunStore).Assembly.GetName().Version.ToString() == wanted + ".0", "shipped mod version");
        string root = args[0], legacy = Path.Combine(root, "legacy");
        SetSeedCategory(root);
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
        string newer = WithChecksum(body.Replace("version=9", "version=10"));
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
