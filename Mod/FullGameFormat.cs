using System;
using System.Globalization;
using System.IO;
using System.Text;

// Compact metadata, never an all-level movement recording. Individual replay
// IDs point at existing, independently verified .isrun / companion files.
public static class FullGameFormat
{
    private static readonly CultureInfo C = CultureInfo.InvariantCulture;
    public const int MaxBytes = 16384;
    public static ulong Hash(string text)
    {
        ulong value = 14695981039346656037UL;
        unchecked { foreach (byte b in Encoding.UTF8.GetBytes(text)) { value ^= b; value *= 1099511628211UL; } }
        return value;
    }
    private static string Number(double value) { return value.ToString("R", C); }
    public static string Encode(FullGameRecord r)
    {
        Validate(r);
        var s = new StringBuilder(1024);
        s.Append("ELITE_FULL_GAME|1\nrun|").Append(r.Id.ToString(C)).Append('|').Append((int)r.Plan.Mode)
            .Append('|').Append((int)r.State).Append('|').Append(r.Stage).Append('|').Append(r.StartedUtc.ToString(C))
            .Append('|').Append(r.EndedUtc.ToString(C)).Append('|').Append(Number(r.Seconds)).Append('|').Append(Number(r.LoadingSeconds))
            .Append('|').Append(r.Eligible ? 1 : 0).Append('|').Append(r.Checked ? 1 : 0)
            .Append('|').Append(r.Aids.ToString(C)).Append('|').Append(r.Warnings.ToString(C)).Append('\n');
        s.Append("plan|").Append(r.Plan.RawMask.ToString(C));
        foreach (int seed in r.Plan.Seeds) s.Append('|').Append(seed.ToString(C));
        s.Append('\n');
        for (int i = 0; i < 8; i++)
            s.Append("level|").Append(i).Append('|').Append(Number(r.Sectors[i])).Append('|').Append(Number(r.Cumulative[i]))
                .Append('|').Append(r.Replays[i].ToString(C)).Append('|').Append(Number(r.AttemptSeconds[i]))
                .Append('|').Append(r.Retries[i].ToString(C)).Append('\n');
        string body = s.ToString();
        return body + "checksum|" + Hash(body).ToString("x16", C) + "\n";
    }
    private static long Long(string value) { return long.Parse(value, NumberStyles.AllowLeadingSign, C); }
    private static int Int(string value) { return checked((int)Long(value)); }
    private static uint UInt(string value) { return checked((uint)Long(value)); }
    private static double Double(string value) { return double.Parse(value, NumberStyles.Float, C); }
    private static bool Flag(string value)
    {
        if (value != "0" && value != "1") throw new InvalidDataException("Invalid full-game flag");
        return value == "1";
    }
    public static FullGameRecord Decode(string text)
    {
        if (text == null || Encoding.UTF8.GetByteCount(text) > MaxBytes) throw new InvalidDataException("Full-game metadata exceeds limit");
        if (!text.StartsWith("ELITE_FULL_GAME|1\n", StringComparison.Ordinal))
        {
            if(text.StartsWith("ELITE_FULL_GAME|", StringComparison.Ordinal))
                throw new NotSupportedException("Unknown full-game metadata version; source preserved");
            throw new InvalidDataException("Invalid full-game metadata header");
        }
        string[] lines = text.Split('\n');
        if (lines.Length != 13 || lines[12] != "") throw new InvalidDataException("Incomplete full-game metadata");
        int checksum = text.LastIndexOf("checksum|", StringComparison.Ordinal);
        if (checksum < 0 || lines[11] != "checksum|" + Hash(text.Substring(0, checksum)).ToString("x16", C))
            throw new InvalidDataException("Full-game metadata checksum mismatch");
        string[] a = lines[1].Split('|'), p = lines[2].Split('|');
        if (a.Length != 13 || a[0] != "run" || p.Length != 10 || p[0] != "plan")
            throw new InvalidDataException("Invalid full-game metadata fields");
        var seeds = new int[8]; for (int i = 0; i < 8; i++) seeds[i] = Int(p[i + 2]);
        if (Int(a[2]) == 1 && (UInt(p[1]) != 0 || Array.Exists(seeds, value => value != 0)))
            throw new InvalidDataException("Random full game contains a fixed seed plan");
        var r = new FullGameRecord { Id = Long(a[1]), Plan = new FullGamePlan((FullGameMode)Int(a[2]), seeds, UInt(p[1])),
            State = (FullGameState)Int(a[3]), Stage = Int(a[4]), StartedUtc = Long(a[5]), EndedUtc = Long(a[6]),
            Seconds = Double(a[7]), LoadingSeconds = Double(a[8]), Eligible = Flag(a[9]), Checked = Flag(a[10]),
            Aids = UInt(a[11]), Warnings = UInt(a[12]) };
        for (int i = 0; i < 8; i++)
        {
            string[] row = lines[i + 3].Split('|');
            if (row.Length != 7 || row[0] != "level" || Int(row[1]) != i) throw new InvalidDataException("Invalid full-game level order");
            r.Sectors[i] = Double(row[2]); r.Cumulative[i] = Double(row[3]); r.Replays[i] = Long(row[4]);
            r.AttemptSeconds[i] = Double(row[5]); r.Retries[i] = Int(row[6]);
        }
        Validate(r); return r;
    }
    private static bool Time(double n) { return !double.IsNaN(n) && !double.IsInfinity(n) && n >= 0 && n <= 315360000; }
    public static void Validate(FullGameRecord r)
    {
        if (r == null || r.Plan == null || r.Id <= 0 || r.Id > DateTime.MaxValue.Ticks || r.StartedUtc != r.Id ||
            r.EndedUtc < 0 || r.EndedUtc > DateTime.MaxValue.Ticks || r.EndedUtc != 0 && r.EndedUtc < r.StartedUtc ||
            r.State < FullGameState.Loading || r.State > FullGameState.Interrupted || r.Stage < 0 || r.Stage > 8 ||
            (r.State == FullGameState.Completed) != (r.Stage == 8) || !Time(r.Seconds) || !Time(r.LoadingSeconds) ||
            r.State == FullGameState.Completed && r.EndedUtc == 0 || (r.Aids & ~1023u) != 0 || (r.Warnings & ~32767u) != 0 ||
            r.Eligible && ((r.Aids & ~(r.Plan.Mode == FullGameMode.SetSeed ? 2u : 0u)) != 0 || (r.Warnings & ~64u) != 0))
            throw new InvalidDataException("Invalid full-game result");
        double previous = 0;
        var ids = new System.Collections.Generic.HashSet<long>();
        for (int i = 0; i < 8; i++)
        {
            if (r.Retries[i] < 0) throw new InvalidDataException("Invalid full-game retry count");
            if (i < r.Stage)
            {
                if (!Time(r.Sectors[i]) || !Time(r.Cumulative[i]) || !Time(r.AttemptSeconds[i]) ||
                    r.Replays[i] <= 0 || r.Replays[i] > DateTime.MaxValue.Ticks || !ids.Add(r.Replays[i]) ||
                    Math.Abs(previous + r.Sectors[i] - r.Cumulative[i]) > .000001 ||
                    r.Cumulative[i] > r.Seconds + .000001 || r.AttemptSeconds[i] > r.Sectors[i] + .000001)
                    throw new InvalidDataException("Inconsistent full-game level split");
                previous = r.Cumulative[i];
            }
            else if (r.Sectors[i] != -1 || r.Cumulative[i] != -1 || r.AttemptSeconds[i] != -1 || r.Replays[i] != 0)
                throw new InvalidDataException("Full-game result contains an uncompleted level");
        }
        if (r.Stage == 8 && Math.Abs(previous - r.Seconds) > .000001)
            throw new InvalidDataException("Full-game finish does not match level splits");
    }
}
