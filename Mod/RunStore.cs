using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Security.Cryptography;
using System.Threading;
using MelonLoader;

// Unity owns the model. One worker receives immutable, coalesced text snapshots.
public sealed class RunStore : IDisposable
{
    private readonly string path;
    private readonly RunLedger ledger;
    private long lastRunId;
    private bool loading = true;
    private readonly Queue<string> ledgerRows = new Queue<string>();
    public volatile string LastLedgerError;
    public volatile string LedgerWarning;
    private readonly Dictionary<int, double> best = new Dictionary<int, double>();
    private readonly Dictionary<int, double> setSeedBest = new Dictionary<int, double>();
    private readonly Dictionary<int, double> practiceBest = new Dictionary<int, double>();
    private readonly SortedDictionary<int, int[]> days = new SortedDictionary<int, int[]>();
    public readonly List<RunRecord> History = new List<RunRecord>();
    public RunRecord Active;
    public readonly ModeStatistics Modes = new ModeStatistics();
    public int Runs, Finished, Deaths, Resets, TimedResets;
    public double TotalFinishedTime, TotalResetTime;
    public float FurthestProgress;
    private readonly object queueGate = new object(), diskGate = new object();
    private AutoResetEvent wake;
    private Thread worker;
    private string pending;
    private byte[] pendingMovement;

    private sealed class ReportJob
    {
        public string Name, Text, ReviewReport, Protection;
        public byte[] Bytes;
        public Func<byte[]> ReviewEncoder;
        public ReplayMotionData Motion;
        public long Memory;
    }

    private long reviewQueueBytes;
    private readonly Queue<ReportJob> reports = new Queue<ReportJob>();
    private readonly Queue<ReportJob> rankedReviews = new Queue<ReportJob>();
    private bool stopping;
    private volatile bool writing, writingReport;
    private bool readOnly, preserveBackup;
    private byte[] preUpgradeBytes;
    public volatile string LastError, LastReportError, ReportQueueWarning, LastMovementError;
    public string LoadWarning { get; private set; }
    public string PreUpgradeBackupPath { get; private set; }
    public bool SavePending
    {
        get {
            lock (queueGate) return pending != null || pendingMovement != null || writing || ledgerRows.Count != 0;
        }
    }
    public bool ReportsPending
    {
        get {
            lock (queueGate) return reports.Count != 0 || rankedReviews.Count != 0 || writingReport;
        }
    }
    private static readonly CultureInfo culture = CultureInfo.InvariantCulture;

    public RunStore(string directory, string legacyRoot = null)
    {
        path = Path.Combine(directory, "speedruns-v3.txt");
        ledger = new RunLedger(directory);
        try
        {
            Directory.CreateDirectory(directory);
        }
        catch (Exception e)
        {
            LastError = e.Message;
        }
        if (File.Exists(path) || File.Exists(path + ".bak"))
            Load();
        else if (legacyRoot != null)
            LoadLegacyStats(legacyRoot);
        if (Active != null)
        {
            // Recover an interrupted process without inventing a death or reset.
            Active.Outcome = RunOutcome.Abandoned;
            Active.EndedUtc = 0; // Stop time was not observed.
            Active.Warnings |= RunWarnings.ProcessStopped;
            if (Active.ExportReport && Active.Events.Count < 128)
                Active.Events.Add(new RunEvent { Seconds = Active.Seconds, Kind = "Warning",
                                                 Detail = "ProcessStopped; end time unknown" });
            RunRecord recovered = Active;
            AddHistory(recovered);
            Active = null;
            Save();
            if (recovered.ExportReport)
                QueueReport(recovered);
        }
        loading = false;
        foreach (var record in History) Archive(record, legacy: true);
    }

    public bool RetainRankedReplay(RunRecord record) { return ledger.ShouldRetain(record); }

    public void Archive(RunRecord record, long session = 0, bool quick = false, bool legacy = false)
    {
        if (loading || readOnly || Array.IndexOf(ModeStatistics.Scenes, record.Scene) < 0) return;
        string line = RunLedger.Encode(record, session, quick, legacy);
        QueueArchiveLine(line);
    }

    public void ArchiveSession(long id)
    {
        if (!readOnly) QueueArchiveLine(RunLedger.Session(id));
    }

    private void QueueArchiveLine(string line)
    {
        lock (queueGate)
        {
            if (stopping) return;
            if (ledgerRows.Count >= 4096)
            {
                LastLedgerError = "Run archive queue is full; archive data could not be queued.";
                return;
            }
            EnsureWriter(); ledgerRows.Enqueue(line); wake.Set();
        }
    }

    public double Best(int scene, RunCategory category = RunCategory.RandomSeed)
    {
        if ((byte)category > 2)
            return 0;
        double value;
        return (category == RunCategory.Practice ? practiceBest : category == RunCategory.SetSeed ? setSeedBest : best).TryGetValue(scene, out value) ? value : 0;
    }

    public long NextId(long utcTicks)
    {
        long id = Math.Max(utcTicks, lastRunId < DateTime.MaxValue.Ticks ? lastRunId + 1 : lastRunId);
        foreach (RunRecord r in History)
            if (r.Id >= id && r.Id < long.MaxValue)
                id = r.Id + 1;
        if (id >= DateTime.MaxValue.Ticks) throw new InvalidOperationException("Run identity space exhausted");
        lastRunId = id;
        return id;
    }

    public bool RecordBest(int scene, double time, RunCategory category = RunCategory.RandomSeed)
    {
        if (!ValidTime(time) || (byte)category > 2)
            return false;
        double old = Best(scene, category);
        if (old > 0 && time >= old)
            return false;
        (category == RunCategory.Practice ? practiceBest : category == RunCategory.SetSeed ? setSeedBest : best)[scene] = time;
        int mode = Array.IndexOf(ModeStatistics.Scenes, scene);
        if (mode >= 0)
            (category == RunCategory.Practice ? Modes.PracticeBestUtc : category == RunCategory.SetSeed ? Modes.SetSeedBestUtc : Modes.BestUtc)[mode] = DateTime.UtcNow.Ticks;
        Modes.Revision++;
        return true;
    }

    private static bool ValidTime(double value)
    {
        return value > 0 && !double.IsNaN(value) && !double.IsInfinity(value);
    }

    public void AddHistory(RunRecord record)
    {
        FurthestProgress = Math.Max(FurthestProgress, record.Progress);
        foreach (RunRecord old in History)
            if (old.Id == record.Id && old.Number == record.Number)
                return;
        lastRunId = Math.Max(lastRunId, record.Id);
        // Practice has a separate unranked best; migrate retained historical
        // finishes without turning assisted runs into clean speedrun PBs.
        if (record.Category == RunCategory.Practice && record.Outcome == RunOutcome.Completed && ValidTime(record.Seconds) &&
            (Best(record.Scene, record.Category) <= 0 || record.Seconds < Best(record.Scene, record.Category)))
        {
            practiceBest[record.Scene] = record.Seconds;
            int mode = Array.IndexOf(ModeStatistics.Scenes, record.Scene);
            if (mode >= 0) Modes.PracticeBestUtc[mode] = record.EndedUtc;
        }
        var copy = record.Copy();
        copy.Events.Clear();
        History.Add(copy);
        if (History.Count > 64)
            History.RemoveAt(0);
    }

    private static int Day(long utcTicks)
    {
        DateTime local = new DateTime(utcTicks, DateTimeKind.Utc).ToLocalTime();
        return local.Year * 10000 + local.Month * 100 + local.Day;
    }

    public void CountDay(long utcTicks, bool attempt, bool completed, bool reset)
    {
        int day = Day(utcTicks);
        int[] counts;
        if (!days.TryGetValue(day, out counts))
            days[day] = counts = new int[3];
        if (attempt && counts[0] < int.MaxValue)
            counts[0]++;
        if (completed && counts[1] < int.MaxValue)
            counts[1]++;
        if (reset && counts[2] < int.MaxValue)
            counts[2]++;
        while (days.Count > 366)
        {
            int first = 0;
            foreach (int key in days.Keys)
            {
                first = key;
                break;
            }
            days.Remove(first);
        }
    }

    public int Today(int index)
    {
        int[] counts;
        return days.TryGetValue(Day(DateTime.UtcNow.Ticks), out counts) ? counts[index] : 0;
    }

    public void UncountAttempt(long startedUtc)
    {
        Runs = Math.Max(0, Runs - 1);
        int[] counts;
        if (days.TryGetValue(Day(startedUtc), out counts))
            counts[0] = Math.Max(0, counts[0] - 1);
    }

    public bool ResetStatistics()
    {
        if (Active != null || readOnly)
            return false;
        Runs = Finished = Deaths = Resets = TimedResets = 0;
        TotalFinishedTime = TotalResetTime = 0;
        FurthestProgress = 0;
        days.Clear();
        History.Clear();
        Modes.Clear();
        QueueSave();
        return true;
    }

    private void LoadLegacyStats(string directory)
    {
        string legacy = Path.Combine(directory, "stats.txt");
        if (!File.Exists(legacy))
            return;
        try
        {
            string[] lines = File.ReadAllLines(legacy);
            int runs, finished, deaths;
            double total;
            if (lines.Length < 4 || !int.TryParse(lines[0], out runs) ||
                !int.TryParse(lines[1], out finished) || !int.TryParse(lines[2], out deaths) || runs < 0 ||
                finished < 0 || deaths < 0)
                return;
            if (!double.TryParse(lines[3], NumberStyles.Float, culture, out total) &&
                !double.TryParse(lines[3], NumberStyles.Float, CultureInfo.GetCultureInfo("de-DE"),
                                 out total))
                return;
            if (total < 0 || double.IsNaN(total) || double.IsInfinity(total))
                return;
            Runs = runs;
            Finished = finished;
            Deaths = deaths;
            Resets = deaths;
            TotalFinishedTime = total;
            Save();
        }
        catch (Exception e)
        {
            LastError = e.Message;
        }
    }

    private static string Pack(RunRecord r, bool active = false)
    {
        RunEnvironment e = r.Environment ?? new RunEnvironment { ToolVersion = "UNKNOWN" };
        return string.Join("|", new[] { r.Id.ToString(culture),
                                        r.Number.ToString(culture),
                                        r.Scene.ToString(culture),
                                        r.StartedUtc.ToString(culture),
                                        r.EndedUtc.ToString(culture),
                                        r.Seconds.ToString("R", culture),
                                        r.Seed.ToString(culture),
                                        r.InitialRngSeed.ToString(culture),
                                        ((int)r.SeedType).ToString(culture),
                                        r.SeedKnown ? "1" : "0",
                                        ((int)r.Outcome).ToString(culture),
                                        ((uint)r.Aids).ToString(culture),
                                        r.Progress.ToString("R", culture),
                                        r.PersonalBest ? "1" : "0",
                                        r.Reseeds.ToString(culture),
                                        ((uint)r.Warnings).ToString(culture),
                                        r.ObservedFromStart ? "1" : "0",
                                        ((uint)r.InitialAids).ToString(culture),
                                        r.ExportReport ? "1" : "0",
                                        r.EventCount.ToString(culture),
                                        ToBase64(e.ToolVersion),
                                        ToBase64(e.GameVersion),
                                        ToBase64(e.UnityVersion),
                                        ToBase64(e.GameHash),
                                        active ? VerificationReport.PackEvents(r.Events) : "",
                                        r.ValidationChecked ? "1" : "0",
                                        ((byte)r.Category).ToString(culture),
                                        r.SelectedSeed.ToString(culture),
                                        r.RawSpawnSeed ? "1" : "0" });
    }

    private static string ToBase64(string value)
    {
        return Convert.ToBase64String(Encoding.UTF8.GetBytes(value ?? "UNKNOWN"));
    }

    private static string FromBase64(string value)
    {
        string result = Encoding.UTF8.GetString(Convert.FromBase64String(value));
        if (result.Length > 256)
            throw new FormatException();
        return result;
    }

    private static RunRecord Unpack(string text)
    {
        string[] a = text.Split('|');
        if (a.Length != 15 && a.Length != 25 && a.Length != 26 && a.Length != 29)
            return null;
        try
        {
            var r = new RunRecord { Id = long.Parse(a[0], culture),
                                    Number = int.Parse(a[1], culture),
                                    Scene = int.Parse(a[2], culture),
                                    StartedUtc = long.Parse(a[3], culture),
                                    EndedUtc = long.Parse(a[4], culture),
                                    Seconds = double.Parse(a[5], culture),
                                    Seed = int.Parse(a[6], culture),
                                    InitialRngSeed = int.Parse(a[7], culture),
                                    SeedType = (SeedKind)byte.Parse(a[8], culture),
                                    SeedKnown = a[9] == "1",
                                    Outcome = (RunOutcome)byte.Parse(a[10], culture),
                                    Aids = (RunAids)uint.Parse(a[11], culture),
                                    Progress = float.Parse(a[12], culture),
                                    PersonalBest = a[13] == "1",
                                    Reseeds = int.Parse(a[14], culture) };
            if (a.Length >= 25)
            {
                r.Warnings = (RunWarnings)uint.Parse(a[15], culture);
                r.ObservedFromStart = a[16] == "1";
                r.InitialAids = (RunAids)uint.Parse(a[17], culture);
                r.ExportReport = a[18] == "1";
                r.EventCount = int.Parse(a[19], culture);
                r.Environment =
                    new RunEnvironment { ToolVersion = FromBase64(a[20]), GameVersion = FromBase64(a[21]),
                                         UnityVersion = FromBase64(a[22]), GameHash = FromBase64(a[23]) };
                r.Events = VerificationReport.UnpackEvents(a[24]);
                if (a.Length >= 26)
                {
                    if (a[25] != "0" && a[25] != "1")
                        return null;
                    r.ValidationChecked = a[25] == "1";
                }
            }
            if (a.Length == 29)
            {
                r.Category = (RunCategory)byte.Parse(a[26], culture);
                r.SelectedSeed = int.Parse(a[27], culture);
                if ((byte)r.Category > 2 || a[28] != "0" && a[28] != "1")
                    return null;
                r.RawSpawnSeed = a[28] == "1";
            }
            else
            {
                // Historical set seeds were practice aids, never category opt-ins.
                r.Category = ((r.Aids | r.InitialAids) & (RunAids.Practice | RunAids.FixedSeed)) != 0 ? RunCategory.Practice : RunCategory.RandomSeed;
            }
            if (r.Id <= 0 || r.Number < 0 || r.Scene < 0 || r.StartedUtc <= 0 ||
                r.StartedUtc > DateTime.MaxValue.Ticks || r.EndedUtc < 0 ||
                r.EndedUtc > DateTime.MaxValue.Ticks || r.Seconds < 0 || double.IsNaN(r.Seconds) ||
                double.IsInfinity(r.Seconds) || r.Seconds > 315360000 || r.Progress < 0 || r.Progress > 1 ||
                float.IsNaN(r.Progress) || (byte)r.SeedType > 2 || (byte)r.Outcome > 5 || r.Reseeds < 0 ||
                r.EventCount < 0 || (r.EndedUtc != 0 && r.EndedUtc < r.StartedUtc))
                return null;
            return r;
        }
        catch (FormatException)
        {
            return null;
        }
        catch (OverflowException)
        {
            return null;
        }
    }

    private void Load()
    {
        try
        {
            LoadFile(path);
            return;
        }
        catch (NotSupportedException e)
        {
            ClearLoaded();
            readOnly = true;
            LastError = e.Message;
            return;
        }
        catch (Exception primary)
        {
            ClearLoaded();
            try
            {
                LoadFile(path + ".bak");
                preserveBackup = File.Exists(path);
                LoadWarning = "Run data recovered from backup: " + primary.Message;
            }
            catch (NotSupportedException e)
            {
                ClearLoaded();
                readOnly = true;
                LastError = e.Message;
            }
            catch
            {
                ClearLoaded();
                preserveBackup = File.Exists(path);
                LoadWarning =
                    "Run data unreadable; using defaults and preserving damaged data: " + primary.Message;
            }
        }
    }

    private void ClearLoaded()
    {
        Modes.Clear();
        Array.Clear(Modes.BestUtc, 0, Modes.BestUtc.Length);
        Array.Clear(Modes.SetSeedBestUtc, 0, Modes.SetSeedBestUtc.Length);
        Runs = Finished = Deaths = Resets = TimedResets = 0;
        TotalFinishedTime = TotalResetTime = 0;
        FurthestProgress = 0;
        Active = null;
        preUpgradeBytes = null;
        PreUpgradeBackupPath = null;
        lastRunId = 0;
        best.Clear();
        setSeedBest.Clear();
        practiceBest.Clear();
        Array.Clear(Modes.PracticeBestUtc, 0, Modes.PracticeBestUtc.Length);
        days.Clear();
        History.Clear();
    }

    private static string Checksum(string text)
    {
        using (var sha = SHA256.Create()) return BitConverter
            .ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(text)))
            .Replace("-", "")
            .ToLowerInvariant();
    }

    private void LoadFile(string candidate)
    {
        if (new FileInfo(candidate).Length > 262144)
            throw new IOException("Stats file exceeds 256 KiB.");
        byte[] loadedBytes = File.ReadAllBytes(candidate);
        if (loadedBytes.Length > 256 * 1024)
            throw new IOException("Stats file exceeds 256 KiB.");
        string text;
        using (var source = new MemoryStream(loadedBytes))
        using (var reader = new StreamReader(source, Encoding.UTF8, true))
            text = reader.ReadToEnd();
        string[] lines = text.Split('\n');
        int version = 0;
        foreach (string line in lines)
            if (line.StartsWith("version=", StringComparison.Ordinal))
                int.TryParse(line.Substring(8).Trim(), out version);
        if (version > 9)
            throw new NotSupportedException(
                "Run data belongs to a newer suite; writes are disabled to preserve it.");
        if (version < 3)
            throw new IOException("Missing or unsupported stats version.");
        if (version >= 6)
        {
            int end = text.LastIndexOf("checksum=", StringComparison.Ordinal);
            if (end <= 0 || text.Substring(end + 9).Trim() != Checksum(text.Substring(0, end)))
                throw new IOException("Stats checksum mismatch.");
        }
        foreach (string key in new[] { "runs", "finished", "deaths", "totalSeconds" })
        {
            bool found = false;
            foreach (string line in lines)
                if (line.StartsWith(key + "=", StringComparison.Ordinal))
                {
                    double value;
                    if (!double.TryParse(line.Substring(key.Length + 1).Trim(), NumberStyles.Float, culture,
                                         out value) ||
                        double.IsNaN(value) || double.IsInfinity(value) || value < 0 ||
                        (key != "totalSeconds" && (value > int.MaxValue || value != Math.Floor(value))))
                        throw new IOException("Invalid stats counter: " + key);
                    found = true;
                }
            if (!found)
                throw new IOException("Missing stats counter: " + key);
        }
        bool hasResets = false;
        foreach (string raw in lines)
        {
            string line = raw.TrimEnd('\r');
            int split = line.IndexOf('=');
            if (split <= 0)
                continue;
            string key = line.Substring(0, split), value = line.Substring(split + 1);
            if (version >= 8 && Modes.Read(key, value))
                continue;
            if (key == "lastRunId")
            {
                long id;
                if (long.TryParse(value, out id) && id >= 0 && id < DateTime.MaxValue.Ticks)
                    lastRunId = Math.Max(lastRunId, id);
                continue;
            }
            if (key == "history" || key == "active")
            {
                RunRecord r = Unpack(value);
                if (r == null)
                {
                    LoadWarning = "Some invalid run history records were ignored.";
                    continue;
                }
                if (key == "active" && r.Outcome == RunOutcome.Running)
                    Active = r;
                else if (key == "history" && r.Outcome != RunOutcome.Running)
                    AddHistory(r);
                continue;
            }
            if (key.StartsWith("day.", StringComparison.Ordinal))
            {
                int day;
                string[] a = value.Split('|');
                int x, y, z;
                DateTime date;
                if (DateTime.TryParseExact(key.Substring(4), "yyyyMMdd", culture, DateTimeStyles.None,
                                           out date) &&
                    int.TryParse(key.Substring(4), out day) && a.Length == 3 && int.TryParse(a[0], out x) &&
                    int.TryParse(a[1], out y) && int.TryParse(a[2], out z) && x >= 0 && y >= 0 && z >= 0)
                    days[day] = new[] { x, y, z };
                continue;
            }
            int number;
            if (key == "runs" && int.TryParse(value, out number) && number >= 0)
                Runs = number;
            else if (key == "finished" && int.TryParse(value, out number) && number >= 0)
                Finished = number;
            else if (key == "deaths" && int.TryParse(value, out number) && number >= 0)
                Deaths = number;
            else if (key == "resets" && int.TryParse(value, out number) && number >= 0)
            {
                Resets = number;
                hasResets = true;
            }
            else if (key == "timedResets" && int.TryParse(value, out number) && number >= 0)
                TimedResets = number;
            else
            {
                double time;
                if (!double.TryParse(value, NumberStyles.Float, culture, out time) || time < 0 ||
                    double.IsNaN(time) || double.IsInfinity(time))
                    continue;
                if (key == "totalSeconds")
                    TotalFinishedTime = time;
                else if (key == "resetSeconds")
                    TotalResetTime = time;
                else if (key == "furthest")
                    FurthestProgress = (float)Math.Min(1, time);
                else if (key.StartsWith("practicePb.", StringComparison.Ordinal) &&
                         int.TryParse(key.Substring(11), out number) && ValidTime(time))
                    practiceBest[number] = time;
                else if (key.StartsWith("setSeedPb.", StringComparison.Ordinal) &&
                         int.TryParse(key.Substring(10), out number) && ValidTime(time))
                    setSeedBest[number] = time;
                else if (key.StartsWith("pb.", StringComparison.Ordinal) &&
                         int.TryParse(key.Substring(3), out number) && ValidTime(time))
                    best[number] = time;
            }
        }
        if (!hasResets)
            Resets = Deaths; // Historical manual resets were not recorded.
        foreach (RunRecord r in History)
            Runs = Math.Max(Runs, r.Number);
        if (Active != null)
            Runs = Math.Max(Runs, Active.Number);
        Finished = Math.Min(Finished, Runs);
        Resets = Math.Min(Resets, Runs);
        Deaths = Math.Min(Deaths, Resets);
        TimedResets = Math.Min(TimedResets, Resets);
        while (days.Count > 366)
        {
            int first = 0;
            foreach (int key in days.Keys)
            {
                first = key;
                break;
            }
            days.Remove(first);
        }
        // Preserve the exact successfully loaded stable-format bytes. Ordinary
        // .bak rotates on every save, so it cannot serve as a preview rollback.
        if (version < 9)
        {
            preUpgradeBytes = loadedBytes;
            PreUpgradeBackupPath = path + ".pre-v9-" + Guid.NewGuid().ToString("N") + ".bak";
        }
    }

    private string Encode()
    {
        var text = new StringBuilder(4096);
        text.Append("version=9\nruns=")
            .Append(Runs)
            .Append("\nfinished=")
            .Append(Finished)
            .Append("\ndeaths=")
            .Append(Deaths)
            .Append("\nresets=")
            .Append(Resets)
            .Append("\ntimedResets=")
            .Append(TimedResets)
            .Append("\ntotalSeconds=")
            .Append(TotalFinishedTime.ToString("R", culture))
            .Append("\nresetSeconds=")
            .Append(TotalResetTime.ToString("R", culture))
            .Append("\nfurthest=")
            .Append(FurthestProgress.ToString("R", culture))
            .Append('\n');
        text.Append("lastRunId=").Append(lastRunId).Append('\n');
        Modes.Write(text);
        var keys = new List<int>(best.Keys);
        keys.Sort();
        foreach (int key in keys)
            text.Append("pb.").Append(key).Append('=').Append(best[key].ToString("R", culture)).Append('\n');
        keys = new List<int>(setSeedBest.Keys);
        keys.Sort();
        foreach (int key in keys)
            text.Append("setSeedPb.").Append(key).Append('=').Append(setSeedBest[key].ToString("R", culture)).Append('\n');
        keys = new List<int>(practiceBest.Keys);
        keys.Sort();
        foreach (int key in keys)
            text.Append("practicePb.").Append(key).Append('=').Append(practiceBest[key].ToString("R", culture)).Append('\n');
        foreach (var day in days)
            text.Append("day.")
                .Append(day.Key)
                .Append('=')
                .Append(day.Value[0])
                .Append('|')
                .Append(day.Value[1])
                .Append('|')
                .Append(day.Value[2])
                .Append('\n');
        foreach (RunRecord r in History)
            text.Append("history=").Append(Pack(r)).Append('\n');
        if (Active != null)
            text.Append("active=").Append(Pack(Active, true)).Append('\n');
        string body = text.ToString();
        return body + "checksum=" + Checksum(body) + "\n";
    }

    private void Write(string text)
    {
        lock (diskGate)
        {
            try
            {
                if (readOnly)
                    return;
                Directory.CreateDirectory(Path.GetDirectoryName(path));
                if (preUpgradeBytes != null)
                {
                    // Each upgrade owns a new rollback file; never replace an
                    // existing backup, including one from an earlier preview.
                    string rollback = PreUpgradeBackupPath;
                    bool created = false;
                    try
                    {
                        using (var output = new FileStream(rollback, FileMode.CreateNew, FileAccess.Write,
                                                           FileShare.Read))
                        {
                            created = true;
                            output.Write(preUpgradeBytes, 0, preUpgradeBytes.Length);
                            output.Flush(true);
                        }
                        preUpgradeBytes = null;
                    }
                    catch (Exception error)
                    {
                        if (created)
                            try { File.Delete(rollback); } catch { }
                        LastError = "Could not preserve pre-v9 run data; upgrade paused: " + error.Message;
                        return; // The original file must remain untouched.
                    }
                }
                if (preserveBackup)
                {
                    File.Copy(path, path + ".corrupt-" + DateTime.UtcNow.Ticks, false);
                }
                string temp = path + ".tmp";
                File.WriteAllText(temp, text);
                if (File.Exists(path))
                    File.Replace(temp, path, preserveBackup ? null : path + ".bak");
                else
                    File.Move(temp, path);
                preserveBackup = false;
                LastError = null;
            }
            catch (Exception e)
            {
                LastError = e.Message;
            }
        }
    }

    public void Save()
    {
        Write(Encode());
    }

    private void EnsureWriter()
    {
        if (worker != null)
            return;
        wake = new AutoResetEvent(false);
        worker = new Thread(Writer) { IsBackground = true, Name = "ItSteals.RunStore",
                                      Priority = ThreadPriority.BelowNormal };
        worker.Start();
    }

    public void QueueReport(RunRecord record)
    {
        var job = new ReportJob { Name = "run-" + record.Id.ToString(culture) + "-" +
                                         record.Number.ToString(culture) + ".json",
                                  Text = VerificationReport.Encode(record) };
        lock (queueGate)
        {
            if (stopping)
                return;
            if (reports.Count + rankedReviews.Count >= 64)
            {
                ReportQueueWarning = "Report queue was full; at least one report could not be saved.";
                return;
            }
            EnsureWriter();
            reports.Enqueue(job);
            wake.Set();
        }
    }

    public void QueueReview(long id, byte[] bytes)
    {
        QueueReview(id, bytes, null, null);
    }

    public void QueueReview(long id, byte[] bytes, ReplayMotionData motion, string report, string protection = null)
    {
        QueueReviewInternal(id, bytes, null, motion, report, protection);
    }
    public void QueueFullGameReview(long id, Func<byte[]> encoder, ReplayMotionData motion, string report, string protection)
    {
        // The recorder has finished every numeric track and relinquishes the
        // map. Encoding is pure numeric IO, so it can run off Unity's thread.
        // Keep the storage queue independent of Unity's scene data types.
        if(encoder == null)throw new ArgumentNullException("encoder");
        QueueReviewInternal(id, null, encoder, motion, report, protection);
    }
    private void QueueReviewInternal(long id, byte[] bytes, Func<byte[]> encoder, ReplayMotionData motion, string report, string protection)
    {
        long memory = (encoder != null ? PracticeMapChannel.Capacity : bytes == null ? 0 : bytes.LongLength) + (motion == null ? 0 : motion.EstimatedBytes);
        lock (queueGate)
        {
            if (stopping)
            {
                if (motion != null && motion.AudioSession != null)
                    motion.AudioSession.Discard();
                return;
            }
            // Rank-worthy recordings take priority over queued ordinary reviews.
            // All memory remains bounded; disk failures are reported, never hidden.
            while (protection != null && reports.Count != 0 &&
                   (reports.Count + rankedReviews.Count >= 64 || reviewQueueBytes + memory > 192L * 1024 * 1024))
            {
                var omitted = reports.Dequeue();
                reviewQueueBytes -= omitted.Memory;
                if (omitted.Motion != null && omitted.Motion.AudioSession != null) omitted.Motion.AudioSession.Discard();
                ReportQueueWarning = "Ordinary recording omitted to prioritize a PB / Top 5 replay; storage is falling behind.";
            }
            if (reports.Count + rankedReviews.Count >= 64 || reviewQueueBytes + memory > 192L * 1024 * 1024)
            {
                ReportQueueWarning = (protection == null ? "Run" : "PB / Top 5") + " recording " + id +
                                     " was not saved: storage queue limit reached";
                if (motion != null && motion.AudioSession != null)
                    motion.AudioSession.Discard();
                return;
            }
            EnsureWriter();
            reviewQueueBytes += memory;
            var job = new ReportJob { Name = "run-" + id + ".isrun", Bytes = bytes, ReviewEncoder = encoder, Motion = motion,
                                      ReviewReport = report, Memory = memory, Protection = protection };
            if (protection == null) reports.Enqueue(job); else rankedReviews.Enqueue(job);
            wake.Set();
        }
    }

    private static void ImmutableText(string path, string text)
    {
        if (File.Exists(path))
            return;
        File.WriteAllText(path + ".tmp", text, new UTF8Encoding(false));
        File.Move(path + ".tmp", path);
    }

    private void WriteReport(ReportJob job)
    {
        try
        {
            if(job.ReviewEncoder != null)
            {
                job.Bytes = job.ReviewEncoder();
                job.ReviewEncoder = null;
                if(job.Bytes == null || job.Bytes.Length == 0 || job.Bytes.Length > PracticeMapChannel.Capacity)
                    throw new InvalidDataException("Full-game replay encoder returned an invalid payload");
            }
            string directory =
                Path.Combine(Path.GetDirectoryName(path), job.Bytes == null ? "Reports" : "RunReviews");
            Directory.CreateDirectory(directory);
            string target = Path.Combine(directory, job.Name), temp = target + ".tmp";
            if (File.Exists(target))
            {
                LastReportError =
                    "Existing report preserved; recovered journal was not allowed to overwrite it.";
                return;
            }
            // Commit the small .isrun last. Its presence means all companion
            // files were written, so archive scans never publish partial runs.
            if (job.Motion != null && job.Motion.AudioSession != null)
            {
                bool recorded = job.Motion.AudioSession.Commit(Path.ChangeExtension(target, ".isaudio"));
                var source = job.Motion.AudioSession.Info;
                // Detach metadata before serialization, including a timed-out
                // optional writer. Its thread must never mutate .isinput data.
                job.Motion.Experience.Audio = recorded ? new ReplayAudioInfo { Recorded = true,
                                                                               Rate = source.Rate,
                                                                               Channels = source.Channels,
                                                                               Frames = source.Frames,
                                                                               Bytes = source.Bytes,
                                                                               Checksum = source.Checksum,
                                                                               Flags = source.Flags,
                                                                               Gaps = source.Gaps }
                                                       : new ReplayAudioInfo { Flags = source.Flags };
            }
            if (job.Motion != null)
            {
                string motion = Path.ChangeExtension(target, ".isinput");
                if (File.Exists(motion))
                    throw new IOException("Existing input recording preserved");
                using (var stream = new FileStream(motion + ".tmp", FileMode.Create, FileAccess.Write,
                                                   FileShare.None, 65536)) job.Motion.Write(stream);
                File.Move(motion + ".tmp", motion);
            }
            if (job.ReviewReport != null)
                ImmutableText(Path.ChangeExtension(target, ".validation.json"), job.ReviewReport);
            if (job.Motion != null && job.Motion.CaptureProfile != null)
                ImmutableText(Path.ChangeExtension(target, ".capture.json"), job.Motion.CaptureProfile);
            if (job.Protection != null)
                ImmutableText(Path.ChangeExtension(target, ".keep"), job.Protection);
            if (job.Bytes != null)
                File.WriteAllBytes(temp, job.Bytes);
            else
                File.WriteAllText(temp, job.Text);
            File.Move(temp, target);
            LastReportError = null;
        }
        catch (Exception e)
        {
            LastReportError = e.Message;
        }
        finally
        {
            if (job.Motion != null && job.Motion.AudioSession != null)
                job.Motion.AudioSession.Discard();
        }
    }

    public void QueueSave()
    {
        string text = Encode();
        lock (queueGate)
        {
            if (stopping)
                return;
            EnsureWriter();
            pending = text;
            wake.Set();
        }
    }

    public void QueueMovement(byte[] bytes)
    {
        lock (queueGate)
        {
            if (stopping)
                return;
            EnsureWriter();
            pendingMovement = bytes;
            wake.Set();
        }
    }

    private void WriteMovement(byte[] bytes)
    {
        string target = Path.Combine(Path.GetDirectoryName(path), "movement-v1.bin");
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            if (MovementCapture.PreservingDamaged && File.Exists(target))
                File.Copy(target, target + ".corrupt-" + DateTime.UtcNow.Ticks, false);
            File.WriteAllBytes(target + ".tmp", bytes);
            if (File.Exists(target))
                File.Replace(target + ".tmp", target,
                             MovementCapture.PreservingDamaged ? null : target + ".bak");
            else
                File.Move(target + ".tmp", target);
            MovementCapture.Saved();
            LastMovementError = null;
        }
        catch (Exception e)
        {
            LastMovementError = "Movement learning not saved: " + e.Message;
        }
    }

    private void Writer()
    {
        long ledgerRetry = 0;
        try
        {
            while (true)
            {
                wake.WaitOne(1000);
                while (true)
                {
                    string text;
                    ReportJob report;
                    byte[] movement;
                    string ledgerRow;
                    lock (queueGate)
                    {
                        text = pending;
                        pending = null;
                        movement = pendingMovement;
                        pendingMovement = null;
                        report = rankedReviews.Count > 0 ? rankedReviews.Dequeue() : reports.Count > 0 ? reports.Dequeue() : null;
                        ledgerRow = ledgerRows.Count != 0 && DateTime.UtcNow.Ticks >= ledgerRetry
                                        ? ledgerRows.Dequeue() : null;
                        if (text == null && report == null && movement == null && ledgerRow == null)
                        {
                            if (stopping)
                                return;
                            break;
                        }
                        writing = text != null || movement != null || ledgerRow != null;
                        writingReport = report != null;
                    }
                    if (text != null)
                        Write(text);
                    if (movement != null)
                        WriteMovement(movement);
                    if (ledgerRow != null)
                        try
                        {
                            ledger.Append(ledgerRow); LastLedgerError = null; LedgerWarning = ledger.Warning;
                        }
                        catch (Exception error)
                        {
                            LastLedgerError = "Run archive not saved: " + error.Message;
                            lock (queueGate) if (!stopping) ledgerRows.Enqueue(ledgerRow);
                            ledgerRetry = DateTime.UtcNow.Ticks + TimeSpan.TicksPerSecond;
                        }
                    writing = false;
                    if (report != null)
                    {
                        WriteReport(report);
                        lock (queueGate) reviewQueueBytes -= report.Memory;
                    }
                    writingReport = false;
                }
            }
        }
        finally
        {
            wake.Dispose();
        }
    }

    public void Dispose()
    {
        if (stopping)
            return;
        if (worker == null)
        {
            stopping = true;
            Save();
            return;
        }
        lock (queueGate)
        {
            pending = Encode();
            stopping = true;
            wake.Set();
        }
        if (!worker.Join(3000))
            MelonLogger.Warning("Run data is still being written; disk did not respond within 3 seconds.");
    }
}
