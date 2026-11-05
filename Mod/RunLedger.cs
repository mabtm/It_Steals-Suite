using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;

// Append-only, independently checksummed metadata. Only RunStore's IO worker
// touches the file; Unity receives immutable ranking snapshots, never handles.
// Checksums detect damaged writes, not external authenticity.
public sealed class RunLedger
{
    public const string Header = "ELITE_RUN_LEDGER|1";
    private static readonly CultureInfo C = CultureInfo.InvariantCulture;
    private readonly string path;
    private readonly Dictionary<long, ulong> known = new Dictionary<long, ulong>();
    private readonly Dictionary<int, List<Entry>> top = new Dictionary<int, List<Entry>>();
    private sealed class Ranking { public readonly Dictionary<int, Entry[]> Rows = new Dictionary<int, Entry[]>(); }
    private volatile Ranking ranking;
    private bool loaded;
    public string Warning { get; private set; }

    public sealed class Entry
    {
        public long Id, Started, Ended, Session;
        public int Scene, Category, SeedKind, SelectedSeed, Seed, Outcome;
        public double Seconds, Progress;
        public uint Aids, Warnings, InitialAids;
        public bool SeedKnown, Raw, Eligible, Quick, Pb, Legacy;
        public bool Ranked { get { return Eligible && Outcome == 1 && Category < 2 && Seconds > 0; } }
    }

    public RunLedger(string directory) { path = Path.Combine(directory, "run-ledger-v1.tsv"); }
    public static string Session(long id)
    {
        if (id <= 0 || id > DateTime.MaxValue.Ticks) throw new ArgumentOutOfRangeException("id");
        string body = "SESSION|" + id.ToString(C);
        return body + "|" + Hash(body).ToString("x16", C);
    }
    private static bool IsSession(string line)
    {
        if (!line.StartsWith("SESSION|", StringComparison.Ordinal)) return false;
        var fields = line.Split('|'); long id;
        int cut = line.LastIndexOf('|');
        if (fields.Length != 3 || !long.TryParse(fields[1], out id) || id <= 0 || id > DateTime.MaxValue.Ticks ||
            Hash(line.Substring(0, cut)).ToString("x16", C) != fields[2])
            throw new InvalidDataException("Invalid archive session marker");
        return true;
    }

    public static ulong Hash(string text)
    {
        unchecked
        {
            ulong hash = 14695981039346656037UL;
            foreach (byte b in Encoding.UTF8.GetBytes(text)) hash = (hash ^ b) * 1099511628211UL;
            return hash;
        }
    }

    public static string Encode(RunRecord r, long session, bool quick, bool legacy)
    {
        string body = string.Join("|", new[] {
            r.Id.ToString(C), r.Scene.ToString(C), ((int)r.Category).ToString(C),
            r.StartedUtc.ToString(C), r.EndedUtc.ToString(C), r.Seconds.ToString("R", C),
            ((int)r.SeedType).ToString(C), r.SeedKnown ? "1" : "0", r.SelectedSeed.ToString(C),
            r.Seed.ToString(C), r.RawSpawnSeed ? "1" : "0", ((int)r.Outcome).ToString(C),
            RunValidity.Eligible(r) ? "1" : "0", ((uint)r.Aids).ToString(C),
            ((uint)r.Warnings).ToString(C), quick ? "1" : "0", session.ToString(C),
            r.PersonalBest ? "1" : "0", r.Progress.ToString("R", C),
            ((uint)r.InitialAids).ToString(C), legacy ? "1" : "0" });
        return body + "|" + Hash(body).ToString("x16", C);
    }

    private static bool Flag(string value)
    {
        if (value != "0" && value != "1") throw new InvalidDataException("Invalid archive flag");
        return value == "1";
    }
    public static Entry Parse(string line)
    {
        var a = line.Split('|');
        int cut = line.LastIndexOf('|');
        if (a.Length != 22 || line.Length > 1024 || cut < 0 ||
            Hash(line.Substring(0, cut)).ToString("x16", C) != a[21])
            throw new InvalidDataException("Run archive checksum mismatch");
        var r = new Entry { Id = long.Parse(a[0], C), Scene = int.Parse(a[1], C),
            Category = int.Parse(a[2], C), Started = long.Parse(a[3], C), Ended = long.Parse(a[4], C),
            Seconds = double.Parse(a[5], C), SeedKind = int.Parse(a[6], C), SeedKnown = Flag(a[7]),
            SelectedSeed = int.Parse(a[8], C), Seed = int.Parse(a[9], C), Raw = Flag(a[10]),
            Outcome = int.Parse(a[11], C), Eligible = Flag(a[12]), Aids = uint.Parse(a[13], C),
            Warnings = uint.Parse(a[14], C), Quick = Flag(a[15]), Session = long.Parse(a[16], C),
            Pb = Flag(a[17]), Progress = double.Parse(a[18], C), InitialAids = uint.Parse(a[19], C),
            Legacy = Flag(a[20]) };
        if (r.Id <= 0 || r.Id > DateTime.MaxValue.Ticks || r.Started <= 0 || r.Started > DateTime.MaxValue.Ticks ||
            r.Ended < 0 || r.Ended > DateTime.MaxValue.Ticks || r.Ended != 0 && r.Ended < r.Started ||
            Array.IndexOf(ModeStatistics.Scenes, r.Scene) < 0 || r.Category < 0 || r.Category > 2 ||
            r.SeedKind < 0 || r.SeedKind > 2 || r.Outcome < 1 || r.Outcome > 5 ||
            r.Session < 0 || r.Session > DateTime.MaxValue.Ticks || double.IsNaN(r.Seconds) ||
            double.IsInfinity(r.Seconds) || r.Seconds < 0 || r.Seconds > 315360000 ||
            double.IsNaN(r.Progress) || r.Progress < 0 || r.Progress > 1)
            throw new InvalidDataException("Invalid run archive record");
        return r;
    }

    private void Rank(Entry r)
    {
        if (!r.Ranked) return;
        int key = r.Scene * 4 + r.Category;
        List<Entry> rows;
        if (!top.TryGetValue(key, out rows)) top[key] = rows = new List<Entry>(6);
        rows.Add(r);
        rows.Sort((a, b) => { int c = a.Seconds.CompareTo(b.Seconds); return c != 0 ? c : a.Id.CompareTo(b.Id); });
        if (rows.Count > 5) rows.RemoveAt(5);
    }
    private void Publish()
    {
        var next = new Ranking();
        foreach (var item in top) next.Rows.Add(item.Key, item.Value.ToArray());
        ranking = next;
    }

    // Main-thread call: fixed small lookup, no locks, IO, scene search or quality
    // adaptation. Until the disk index is ready, keep every eligible completion.
    public bool ShouldRetain(RunRecord r)
    {
        if (r.Outcome != RunOutcome.Completed || !RunValidity.Eligible(r) || (byte)r.Category > 1)
            return false;
        var view = ranking;
        Entry[] rows;
        if (r.PersonalBest || view == null || !view.Rows.TryGetValue(r.Scene * 4 + (int)r.Category, out rows) || rows.Length < 5)
            return true;
        foreach (var row in rows) if (row.Id == r.Id) return true;
        var last = rows[rows.Length - 1];
        return r.Seconds < last.Seconds || r.Seconds == last.Seconds && r.Id < last.Id;
    }

    private void Load()
    {
        if (loaded) return;
        known.Clear(); top.Clear();
        Directory.CreateDirectory(Path.GetDirectoryName(path));
        if (!File.Exists(path))
        {
            using (var file = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.Read))
            {
                var bytes = Encoding.UTF8.GetBytes(Header + "\n"); file.Write(bytes, 0, bytes.Length); file.Flush(true);
            }
        }
        long committed = 0;
        bool tail = false;
        using (var file = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read))
        using (var buffered = new BufferedStream(file, 65536))
        {
            var bytes = new List<byte>(512);
            bool header = true;
            int value; long position = 0;
            while ((value = buffered.ReadByte()) != -1)
            {
                position++;
                if (value != 10)
                {
                    if (bytes.Count >= 1024) throw new InvalidDataException("Run archive line exceeds limit");
                    bytes.Add((byte)value); continue;
                }
                string line = Encoding.UTF8.GetString(bytes.ToArray()); bytes.Clear();
                if (header)
                {
                    if (line != Header) throw new InvalidDataException("Unknown run archive version");
                    header = false;
                }
                else if (!IsSession(line))
                {
                    var row = Parse(line);
                    ulong hash = Hash(line);
                    ulong previous;
                    if (known.TryGetValue(row.Id, out previous))
                    { if (previous != hash) throw new InvalidDataException("Conflicting run archive identity"); }
                    else { known.Add(row.Id, hash); Rank(row); }
                }
                committed = position;
            }
            if (header) throw new InvalidDataException("Run archive header is incomplete");
            tail = bytes.Count != 0;
        }
        if (tail)
        {
            File.Copy(path, path + ".incomplete-" + Guid.NewGuid().ToString("N") + ".bak", false);
            using (var file = new FileStream(path, FileMode.Open, FileAccess.Write, FileShare.Read))
            { file.SetLength(committed); file.Flush(true); }
            Warning = "Recovered an incomplete run archive tail; original bytes retained in a backup.";
        }
        loaded = true; Publish();
    }

    public void Append(string line)
    {
        Load();
        bool session = IsSession(line);
        var r = session ? null : Parse(line);
        ulong hash = Hash(line), previous;
        if (!session && known.TryGetValue(r.Id, out previous))
        {
            if (r.Legacy || previous == hash) return;
            throw new IOException("Run identity already exists with different evidence");
        }
        byte[] bytes = Encoding.UTF8.GetBytes(line + "\n");
        try
        {
            using (var file = new FileStream(path, FileMode.Append, FileAccess.Write, FileShare.Read))
            { file.Write(bytes, 0, bytes.Length); file.Flush(true); }
        }
        catch { loaded = false; throw; }
        if (!session) { known.Add(r.Id, hash); Rank(r); Publish(); }
    }
}
