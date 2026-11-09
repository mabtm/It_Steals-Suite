using System;
using System.Globalization;

public enum FullGameMode { None, RandomSeed, SetSeed }
public enum FullGameState { None, Loading, Running, Completed, Reset, Interrupted }

// A full-game clock owns the ordered campaign; individual recordings keep
// their original per-attempt clocks and file formats.
public sealed class FullGamePlan
{
    public static readonly int[] Scenes = { 3, 4, 5, 7, 8, 14, 12, 13 };
    public readonly FullGameMode Mode;
    public readonly int[] Seeds;
    public readonly uint RawMask;
    public readonly string Key;

    public FullGamePlan(FullGameMode mode, int[] seeds, uint rawMask)
    {
        if ((mode != FullGameMode.RandomSeed && mode != FullGameMode.SetSeed) ||
            seeds == null || seeds.Length != 8 || (rawMask & ~255u) != 0)
            throw new ArgumentException("Invalid full-game seed plan");
        Mode = mode;
        Seeds = mode == FullGameMode.SetSeed ? (int[])seeds.Clone() : new int[8];
        RawMask = mode == FullGameMode.SetSeed ? rawMask : 0;
        ulong hash = 14695981039346656037UL;
        unchecked
        {
            Action<uint> word = value => {
                for (int n = 0; n < 4; n++) { hash ^= (byte)(value >> (8 * n)); hash *= 1099511628211UL; }
            };
            word((uint)Mode); word(RawMask);
            foreach (int seed in Seeds) word((uint)seed);
        }
        Key = ((int)Mode).ToString(CultureInfo.InvariantCulture) + "-" + hash.ToString("x16", CultureInfo.InvariantCulture);
    }

    public bool Matches(FullGameMode mode, int[] seeds, uint rawMask)
    {
        if (mode != Mode || seeds == null || seeds.Length != 8) return false;
        if (Mode == FullGameMode.RandomSeed) return true;
        if (rawMask != RawMask) return false;
        for (int i = 0; i < 8; i++) if (seeds[i] != Seeds[i]) return false;
        return true;
    }
}

public sealed class FullGameRecord
{
    public long Id, StartedUtc, EndedUtc;
    public FullGamePlan Plan;
    public FullGameState State;
    public int Stage;
    public double Seconds, LoadingSeconds;
    public bool Eligible = true, Checked = true;
    public uint Aids, Warnings;
    public readonly double[] Sectors = new double[8], Cumulative = new double[8], AttemptSeconds = new double[8];
    public readonly long[] Replays = new long[8];
    public readonly int[] Retries = new int[8];

    public FullGameRecord()
    {
        for (int i = 0; i < 8; i++) Sectors[i] = Cumulative[i] = AttemptSeconds[i] = -1;
    }
    public bool Ranked { get { return State == FullGameState.Completed && Stage == 8 && Eligible && Checked; } }
    public FullGameRecord Copy()
    {
        var result = new FullGameRecord { Id = Id, StartedUtc = StartedUtc, EndedUtc = EndedUtc,
            Plan = Plan, State = State, Stage = Stage, Seconds = Seconds, LoadingSeconds = LoadingSeconds,
            Eligible = Eligible, Checked = Checked, Aids = Aids, Warnings = Warnings };
        Array.Copy(Sectors, result.Sectors, 8); Array.Copy(Cumulative, result.Cumulative, 8);
        Array.Copy(AttemptSeconds, result.AttemptSeconds, 8); Array.Copy(Replays, result.Replays, 8);
        Array.Copy(Retries, result.Retries, 8);
        return result;
    }
}

// Pure monotonic-time state machine. Loading is explicit and only the player
// ready hook resumes the clock. Deaths and resets never discard earned time.
public sealed class FullGameRun
{
    private FullGameRecord record;
    private double accumulated, resumedAt, loadingAt, lastNow;
    public FullGameRecord Record { get { return record; } }
    public bool Active { get { return record != null && (record.State == FullGameState.Loading || record.State == FullGameState.Running); } }
    public int Scene { get { return record == null ? 3 : FullGamePlan.Scenes[Math.Min(7, record.Stage)]; } }
    public uint Revision { get; private set; }

    private double Now(double value)
    {
        if (double.IsNaN(value) || double.IsInfinity(value) || value < lastNow)
        {
            if (record != null) { record.Eligible = false; record.Warnings |= 16u; }
            return lastNow;
        }
        lastNow = value;
        return value;
    }
    public void Start(FullGamePlan plan, long id, double now)
    {
        if (plan == null || id <= 0 || id > DateTime.MaxValue.Ticks || double.IsNaN(now) || double.IsInfinity(now) || now < 0)
            throw new ArgumentException("Invalid full-game start");
        record = new FullGameRecord { Id = id, StartedUtc = id, Plan = plan, State = FullGameState.Loading };
        accumulated = 0; resumedAt = loadingAt = lastNow = now; Revision++;
    }
    public double Time(double now)
    {
        now = Now(now);
        return record == null ? 0 : record.State == FullGameState.Running ? accumulated + now - resumedAt : accumulated;
    }
    public double SectorTime(double now)
    {
        double total = Time(now);
        if (record == null || record.Stage == 8) return 0;
        return Math.Max(0, total - (record.Stage == 0 ? 0 : record.Cumulative[record.Stage - 1]));
    }
    public double LoadTime(double now)
    {
        now = Now(now);
        return record == null ? 0 : record.LoadingSeconds + (record.State == FullGameState.Loading ? now - loadingAt : 0);
    }
    public void BeginLoading(double now)
    {
        if (!Active) return;
        now = Now(now);
        if (record.State == FullGameState.Loading) return;
        accumulated += now - resumedAt;
        record.Seconds = accumulated;
        loadingAt = now; record.State = FullGameState.Loading; Revision++;
    }
    public bool PlayerReady(int scene, double now)
    {
        if (!Active) return false;
        now = Now(now);
        if (scene != Scene)
        {
            record.Eligible = false; record.Warnings |= 32u;
            Stop(FullGameState.Interrupted, now, 0); return false;
        }
        if (record.State == FullGameState.Running) return false; // duplicate Start/fallback callbacks
        record.LoadingSeconds += now - loadingAt;
        resumedAt = now; record.State = FullGameState.Running; Revision++;
        return true;
    }
    public void Observe(uint aids, uint warnings)
    {
        if (!Active) return;
        uint allowed = record.Plan.Mode == FullGameMode.SetSeed ? 2u : 0u;
        record.Aids |= aids; record.Warnings |= warnings;
        if ((aids & ~allowed) != 0 || (warnings & ~64u) != 0) record.Eligible = false;
    }
    public void SelectionChanged()
    {
        if (!Active || (record.Warnings & 16384u) != 0) return;
        record.Eligible = false; record.Warnings |= 16384u; Revision++;
    }
    public bool EndAttempt(int scene, long replay, double attemptSeconds, int outcome,
                           bool eligible, bool checkedRun, uint aids, uint warnings, double now, long endedUtc)
    {
        if (!Active) return false;
        now = Now(now);
        Observe(aids, warnings);
        record.Eligible &= eligible; record.Checked &= checkedRun;
        if (scene != Scene || record.State != FullGameState.Running || replay <= 0 ||
            double.IsNaN(attemptSeconds) || double.IsInfinity(attemptSeconds) || attemptSeconds < 0 || outcome < 1 || outcome > 5)
        {
            record.Eligible = false; record.Warnings |= 32u;
            Stop(FullGameState.Interrupted, now, endedUtc); return false;
        }
        if (outcome != 1)
        {
            if (record.Retries[record.Stage] < int.MaxValue) record.Retries[record.Stage]++;
            Revision++; return false;
        }
        double total = Time(now), previous = record.Stage == 0 ? 0 : record.Cumulative[record.Stage - 1];
        int stage = record.Stage;
        record.Sectors[stage] = Math.Max(0, total - previous);
        record.Cumulative[stage] = total; record.Replays[stage] = replay;
        record.AttemptSeconds[stage] = attemptSeconds;
        BeginLoading(now); record.Stage++;
        if (record.Stage == 8)
        {
            record.State = FullGameState.Completed;
            record.EndedUtc = Math.Max(record.StartedUtc, endedUtc);
        }
        Revision++; return true;
    }
    public void Stop(FullGameState state, double now, long endedUtc)
    {
        if (!Active) return;
        if (state != FullGameState.Reset && state != FullGameState.Interrupted)
            throw new ArgumentException("Invalid full-game stop");
        now = Now(now);
        accumulated = Time(now);
        if (record.State == FullGameState.Loading) record.LoadingSeconds += now - loadingAt;
        record.Seconds = accumulated; record.State = state;
        record.EndedUtc = endedUtc == 0 ? 0 : Math.Max(record.StartedUtc, endedUtc);
        Revision++;
    }
    public FullGameRecord Snapshot(double now)
    {
        if (record == null) return null;
        double total = Time(now), loads = LoadTime(now);
        var copy = record.Copy(); copy.Seconds = total; copy.LoadingSeconds = loads;
        return copy;
    }
}
