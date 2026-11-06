using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Security.Cryptography;
using System.Text;

// The split worker owns this small journal. Replay retention and aggregate
// counters cannot remove observed sector bests from short, clean reset runs.
public sealed class RunSegmentBook
{
    public sealed class Sector
    {
        public double Best = -1, Finish = -1;
        public Sector Copy() { return new Sector { Best = Best, Finish = Finish }; }
    }
    public sealed class Profile
    {
        public readonly Dictionary<string, Sector> Nodes = new Dictionary<string, Sector>(StringComparer.Ordinal);
        public double Best(int[] order, int count)
        {
            Sector value;
            return Nodes.TryGetValue(Prefix(order, count), out value) ? value.Best : -1;
        }
        public double Exit(int[] order)
        {
            Sector value;
            return Nodes.TryGetValue(Prefix(order, order.Length), out value) ? value.Finish : -1;
        }
        public double Theoretical(int[] order)
        {
            double total = Exit(order);
            if (total < 0 || order.Length == 0) return -1;
            for (int i = 1; i <= order.Length; i++)
            {
                double part = Best(order, i);
                if (part < 0) return -1;
                total += part;
            }
            return total;
        }
        public Profile Copy()
        {
            var result = new Profile();
            foreach (var node in Nodes) result.Nodes.Add(node.Key, node.Value.Copy());
            return result;
        }
    }
    private sealed class FutureFormat : IOException { public FutureFormat() : base("Newer sector history version") {} }
    private const int MaxBytes = 32 * 1024 * 1024, MaxNodes = 131072, MaxProfiles = 8192;
    private readonly Dictionary<string, Profile> profiles = new Dictionary<string, Profile>(StringComparer.Ordinal);
    private readonly string path;
    private int nodes;
    private bool dirty, readOnly, preserveBackup;
    public string Error { get; private set; }
    public RunSegmentBook(string path = null)
    {
        this.path = path;
        if (string.IsNullOrEmpty(path)) return;
        if (!File.Exists(path) && !File.Exists(path + ".bak")) return;
        try { Load(path); }
        catch (FutureFormat e) { readOnly = true; Error = "Sector history preserved: " + e.Message; }
        catch (Exception first)
        {
            profiles.Clear(); nodes = 0;
            try { Load(path + ".bak"); preserveBackup = true; }
            catch (Exception)
            {
                profiles.Clear(); nodes = 0; readOnly = true;
                Error = "Sector history preserved; cannot read it: " + first.Message;
            }
        }
    }
    public static string Prefix(int[] order, int count)
    {
        var text = new StringBuilder(count * 5);
        for (int i = 0; i < count; i++)
        { if (i > 0) text.Append(','); text.Append(order[i].ToString(CultureInfo.InvariantCulture)); }
        return text.ToString();
    }
    public static string LayoutKey(RunSplitReference item)
    {
        if (item.LayoutIdentity != null) return item.LayoutIdentity;
        using (var stream = new MemoryStream())
        using (var w = new BinaryWriter(stream, Encoding.UTF8))
        {
            w.Write(item.Scene); w.Write(item.Seed); w.Write(item.SelectedSeed); w.Write(item.RawSeed);
            w.Write(item.Spawn); w.Write(item.Keep); w.Write(item.PointCount); w.Write(item.Hash ?? "");
            w.Write(item.SpawnPosition.x == 0 ? 0 : item.SpawnPosition.x);
            w.Write(item.SpawnPosition.y == 0 ? 0 : item.SpawnPosition.y);
            w.Write(item.SpawnPosition.z == 0 ? 0 : item.SpawnPosition.z);
            for (int i = 0; i < item.Selected.Length; i++)
            {
                w.Write(item.Selected[i]);
                w.Write(item.Positions[i].x == 0 ? 0 : item.Positions[i].x);
                w.Write(item.Positions[i].y == 0 ? 0 : item.Positions[i].y);
                w.Write(item.Positions[i].z == 0 ? 0 : item.Positions[i].z);
            }
            using (var hash = SHA256.Create())
                return item.LayoutIdentity = BitConverter.ToString(hash.ComputeHash(stream.ToArray())).Replace("-", "").ToLowerInvariant();
        }
    }
    public Profile Snapshot(RunSplitReference target)
    {
        Profile profile;
        return target != null && profiles.TryGetValue(LayoutKey(target), out profile) ? profile.Copy() : new Profile();
    }
    private static bool Time(double value) { return !double.IsNaN(value) && !double.IsInfinity(value) && value >= 0 && value <= 315360000; }
    public bool Observe(RunSplitReference run)
    {
        if (readOnly || run == null || !run.Valid || run.Outcome < 1 || run.Outcome > 5 || run.Collected.Length == 0 || run.Collected.Length > 64 ||
            run.Collected.Length != run.PickupSeconds.Length || !Time(run.Seconds)) return false;
        var selected = new HashSet<int>(run.Selected); var seen = new HashSet<int>();
        double previous = 0;
        for (int i = 0; i < run.Collected.Length; i++)
        {
            double at = run.PickupSeconds[i];
            if (!selected.Contains(run.Collected[i]) || !seen.Add(run.Collected[i]) || !Time(at) || at < previous || at > run.Seconds) return false;
            previous = at;
        }
        string key = LayoutKey(run); Profile profile;
        if (!profiles.TryGetValue(key, out profile))
        {
            if (profiles.Count >= MaxProfiles) { Error = "Sector history profile capacity reached; new bests were not saved."; return false; }
            profiles.Add(key, profile = new Profile());
        }
        bool changed = false; previous = 0;
        for (int i = 0; i < run.Collected.Length; i++)
        {
            string prefix = Prefix(run.Collected, i + 1); Sector sector;
            if (!profile.Nodes.TryGetValue(prefix, out sector))
            {
                if (nodes >= MaxNodes) { Error = "Sector history capacity reached; new bests were not saved."; break; }
                profile.Nodes.Add(prefix, sector = new Sector()); nodes++;
            }
            double time = run.PickupSeconds[i] - previous;
            if (sector.Best < 0 || time < sector.Best) { sector.Best = time; changed = true; }
            previous = run.PickupSeconds[i];
            if (i + 1 == run.Collected.Length && run.Outcome == 1 && seen.Count == selected.Count && run.Seconds > 0)
            {
                double finish = run.Seconds - previous;
                if (sector.Finish < 0 || finish < sector.Finish) { sector.Finish = finish; changed = true; }
            }
        }
        dirty |= changed;
        return changed;
    }
    private static void Text(BinaryWriter w, string value)
    { byte[] data = Encoding.UTF8.GetBytes(value); w.Write(data.Length); w.Write(data); }
    private static string Text(BinaryReader r, int maximum)
    {
        int count = r.ReadInt32();
        if (count < 0 || count > maximum) throw new IOException("Sector text exceeds limit");
        byte[] data = r.ReadBytes(count);
        if (data.Length != count) throw new EndOfStreamException();
        return new UTF8Encoding(false, true).GetString(data);
    }
    private void Load(string file)
    {
        var info = new FileInfo(file);
        if (info.Length < 44 || info.Length > MaxBytes) throw new IOException("Sector file size exceeds limit");
        byte[] bytes = File.ReadAllBytes(file);
        using (var hash = SHA256.Create())
        {
            var digest = hash.ComputeHash(bytes, 0, bytes.Length - 32);
            for (int i = 0; i < 32; i++) if (digest[i] != bytes[bytes.Length - 32 + i]) throw new IOException("Sector checksum mismatch");
        }
        using (var stream = new MemoryStream(bytes, 0, bytes.Length - 32, false))
        using (var r = new BinaryReader(stream, Encoding.UTF8))
        {
            if (r.ReadInt32() != 0x53475345) throw new IOException("Unsupported sector history file");
            int version = r.ReadInt32();
            if (version > 1) throw new FutureFormat();
            if (version != 1) throw new IOException("Unsupported sector history version");
            int count = r.ReadInt32();
            if (count < 0 || count > MaxProfiles) throw new IOException("Sector profile count exceeds limit");
            for (int i = 0; i < count; i++)
            {
                string key = Text(r, 64);
                if (key.Length != 64 || profiles.ContainsKey(key)) throw new IOException("Invalid sector profile identity");
                int n = r.ReadInt32();
                if (n < 0 || n > MaxNodes - nodes) throw new IOException("Sector count exceeds limit");
                var profile = new Profile();
                for (int j = 0; j < n; j++)
                {
                    string prefix = Text(r, 768); var order = prefix.Split(',');
                    if (order.Length < 1 || order.Length > 64) throw new IOException("Invalid sector route");
                    var seen = new HashSet<int>();
                    foreach (string part in order)
                    {
                        int id;
                        if (!int.TryParse(part, NumberStyles.None, CultureInfo.InvariantCulture, out id) || id < 0 || !seen.Add(id))
                            throw new IOException("Invalid sector checkpoint");
                    }
                    var sector = new Sector { Best = r.ReadDouble(), Finish = r.ReadDouble() };
                    if ((!Time(sector.Best) && sector.Best != -1) || (!Time(sector.Finish) && sector.Finish != -1) || profile.Nodes.ContainsKey(prefix))
                        throw new IOException("Invalid sector time");
                    profile.Nodes.Add(prefix, sector);
                }
                profiles.Add(key, profile); nodes += n;
            }
            if (stream.Position != stream.Length) throw new IOException("Unexpected sector history data");
        }
    }
    public bool Save()
    {
        if (!dirty || readOnly || string.IsNullOrEmpty(path)) return !readOnly;
        string temp = path + ".tmp";
        try
        {
            byte[] payload;
            using (var stream = new MemoryStream())
            using (var w = new BinaryWriter(stream, Encoding.UTF8))
            {
                w.Write(0x53475345); w.Write(1); w.Write(profiles.Count);
                var keys = new List<string>(profiles.Keys); keys.Sort(StringComparer.Ordinal);
                foreach (string key in keys)
                {
                    Text(w, key); var profile = profiles[key]; w.Write(profile.Nodes.Count);
                    var prefixes = new List<string>(profile.Nodes.Keys); prefixes.Sort(StringComparer.Ordinal);
                    foreach (string prefix in prefixes)
                    { Text(w, prefix); w.Write(profile.Nodes[prefix].Best); w.Write(profile.Nodes[prefix].Finish); }
                }
                payload = stream.ToArray();
            }
            if (payload.Length > MaxBytes - 32) throw new IOException("Sector file capacity reached");
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            using (var file = new FileStream(temp, FileMode.Create, FileAccess.Write, FileShare.None))
            using (var hash = SHA256.Create())
            { file.Write(payload, 0, payload.Length); var digest = hash.ComputeHash(payload); file.Write(digest, 0, digest.Length); file.Flush(true); }
            if (File.Exists(path)) File.Replace(temp, path, preserveBackup ? null : path + ".bak");
            else File.Move(temp, path);
            dirty = false; preserveBackup = false; Error = null;
            return true;
        }
        catch (Exception e) { Error = "Sector bests not saved: " + e.Message; return false; }
    }
}
