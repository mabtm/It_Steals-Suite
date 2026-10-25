using System;

public enum SeedKind : byte
{
    Unknown,
    Random,
    Set
}

public enum RunOutcome : byte
{
    Running,
    Completed,
    Reset,
    Death,
    Interrupted,
    Abandoned
}

[Flags]
public enum RunAids : uint
{
    None = 0,
    Practice = 1,
    FixedSeed = 2,
    Hitboxes = 4,
    Fullbright = 8,
    FreeCamera = 16,
    Teleport = 32,
    Minimap = 64,
    Path = 128,
    SpawnPlanner = 256,
    LiveInspector = 512
}

// Describes observations of this build's managed RNG calls, not an attestation
// of the game process. A random run has multiple reseeds, not one world seed.
public sealed class SeedEvidence
{
    public SeedKind Kind { get; private set; }
    public bool Known { get; private set; }
    public int Seed { get; private set; }
    public int InitialRngSeed { get; private set; }
    public int Reseeds { get; private set; }
    public bool Coverage { get; private set; }

    public void Reset(bool coverage)
    {
        Coverage = coverage;
        Kind = SeedKind.Unknown;
        Known = false;
        Seed = InitialRngSeed = Reseeds = 0;
    }

    public void Observe(int original, int applied, bool forced, int baseSeed)
    {
        if (Reseeds < int.MaxValue)
            Reseeds++;
        if (forced)
        {
            if (!Known)
                InitialRngSeed = applied;
            Kind = SeedKind.Set;
            Seed = baseSeed;
            Known = true;
        }
        else if (!Known)
        {
            Kind = Coverage ? SeedKind.Random : SeedKind.Unknown;
            Seed = InitialRngSeed = original;
            Known = true;
        }
    }

    public void RevokeOriginalPath()
    {
        Coverage = false;
        if (Kind == SeedKind.Random)
            Kind = SeedKind.Unknown;
    }
}

public sealed class RunRecord
{
    public long Id, StartedUtc, EndedUtc;
    public int Number, Scene, Seed, InitialRngSeed, Reseeds;
    public double Seconds, PreviousBestSeconds;
    public float Progress;
    public SeedKind SeedType;
    public bool SeedKnown, PersonalBest;
    public RunOutcome Outcome;
    public RunAids Aids, InitialAids;
    public RunWarnings Warnings;
    public bool ObservedFromStart, ExportReport, ValidationChecked;
    public int EventCount;
    public RunEnvironment Environment;
    public System.Collections.Generic.List<RunEvent> Events = new System.Collections.Generic.List<RunEvent>();

    public RunRecord Copy()
    {
        var copy = (RunRecord)MemberwiseClone();
        copy.Environment = Environment == null ? null : Environment.Copy();
        copy.Events = new System.Collections.Generic.List<RunEvent>(Events);
        return copy;
    }
}

// All transitions are on Unity's main thread. Repeated hooks or reload callbacks
// cannot count an attempt/end twice; disk writes receive immutable text only.
public sealed class RunTracker
{
    private readonly RunStore store;
    public RunRecord Active
    {
        get {
            return store.Active;
        }
    }
    public RunRecord Last { get; private set; }
    public int SessionAttempts { get; private set; }
    public int SessionCompleted { get; private set; }
    public int SessionResets { get; private set; }

    public RunTracker(RunStore data)
    {
        store = data;
    }

    private RunAids lastFeatures;

    public bool ResetStatistics()
    {
        if (!store.ResetStatistics())
            return false;
        SessionAttempts = SessionCompleted = SessionResets = 0;
        Last = null;
        return true;
    }

    public bool Begin(int scene, long utcTicks, RunAids aids, SeedEvidence seed,
                      RunEnvironment environment = null, bool exportReport = false,
                      bool observedFromStart = true)
    {
        if (Active != null || utcTicks <= 0 || utcTicks > DateTime.MaxValue.Ticks || scene < 0)
            return false;
        if (store.Runs < int.MaxValue)
            store.Runs++;
        store.Active = new RunRecord { Id = store.NextId(utcTicks),
                                       StartedUtc = utcTicks,
                                       Scene = scene,
                                       Number = store.Runs,
                                       Aids = aids,
                                       InitialAids = aids,
                                       ObservedFromStart = observedFromStart,
                                       Environment = environment == null ? null : environment.Copy(),
                                       ExportReport = exportReport };
        lastFeatures = aids;
        Event("RunStart", aids.ToString());
        if (!observedFromStart)
            Warn(RunWarnings.StateUnavailable);
        if (SessionAttempts < int.MaxValue)
            SessionAttempts++;
        store.CountDay(utcTicks, true, false, false);
        ObserveSeed(seed);
        store.QueueSave();
        return true;
    }

    public void ObserveSeed(SeedEvidence evidence)
    {
        if (Active == null || evidence == null)
            return;
        bool changed = false;
        if (Active.SeedType == SeedKind.Random && evidence.Kind == SeedKind.Unknown)
        {
            Active.SeedType = SeedKind.Unknown;
            changed = true;
            Warn(RunWarnings.SeedCoverageLost);
        }
        if (evidence.Known &&
            (!Active.SeedKnown || evidence.Kind == SeedKind.Set && Active.SeedType != SeedKind.Set))
        {
            Active.SeedKnown = true;
            Active.Seed = evidence.Seed;
            Active.InitialRngSeed = evidence.InitialRngSeed;
            Active.SeedType = evidence.Kind;
            changed = true;
        }
        Active.Reseeds = evidence.Reseeds;
        if (evidence.Kind == SeedKind.Set)
            Mark(RunAids.FixedSeed);
        if (changed)
        {
            Event("Seed", Active.SeedType + ": " + Active.Seed);
            store.QueueSave();
        }
    }

    public void Event(string kind, string detail)
    {
        if (Active == null || !Active.ExportReport)
            return;
        if (Active.EventCount < int.MaxValue)
            Active.EventCount++;
        if (Active.Events.Count >= 128)
        {
            Active.Warnings |= RunWarnings.EventLogTruncated;
            return;
        }
        Active.Events.Add(new RunEvent { Seconds = Active.Seconds, Kind = kind, Detail = detail });
    }

    public void Warn(RunWarnings warning)
    {
        if (Active == null || (Active.Warnings & warning) == warning)
            return;
        Active.Warnings |= warning;
        Event("Warning", warning.ToString());
        store.QueueSave();
    }

    public void ObserveFeatures(RunAids features)
    {
        if (Active == null)
            return;
        if (features != lastFeatures)
        {
            Event("FeaturesChanged", features.ToString());
            lastFeatures = features;
            store.QueueSave();
        }
        Mark(features);
    }

    public void Mark(RunAids aids)
    {
        if (Active == null || (Active.Aids | aids) == Active.Aids)
            return;
        Active.Aids |= aids;
        Event("TrainerAidUsed", aids.ToString());
        store.QueueSave();
    }

    public void Update(double elapsed, float progress)
    {
        if (Active == null)
            return;
        if (double.IsNaN(elapsed) || double.IsInfinity(elapsed) || elapsed < Active.Seconds ||
            elapsed > 315360000)
            Warn(RunWarnings.InvalidTiming);
        else
            Active.Seconds = Math.Max(0, elapsed);
        Active.Progress =
            Math.Max(Active.Progress, float.IsNaN(progress) ? 0 : Math.Max(0, Math.Min(1, progress)));
    }

    public bool End(RunOutcome outcome, long utcTicks, double elapsed, float progress)
    {
        if (Active == null || outcome == RunOutcome.Running || (byte)outcome > 5)
            return false;
        Update(elapsed, progress);
        RunRecord record = Active;
        record.EndedUtc = Math.Max(record.StartedUtc, utcTicks);
        record.Outcome = outcome;
        bool quickReset = (outcome == RunOutcome.Reset || outcome == RunOutcome.Interrupted) &&
                          record.Seconds < 4 && record.Progress <= 0;
        if (quickReset)
        {
            store.UncountAttempt(record.StartedUtc);
            SessionAttempts = Math.Max(0, SessionAttempts - 1);
        }
        if (outcome == RunOutcome.Completed)
        {
            record.Progress = 1;
            if (store.Finished < int.MaxValue)
                store.Finished++;
            if (SessionCompleted < int.MaxValue)
                SessionCompleted++;
            store.TotalFinishedTime += record.Seconds;
            // "Local PB" eligibility; external mods/memory/input are not verified.
            record.PreviousBestSeconds = store.Best(record.Scene);
            if (RunValidity.Eligible(record))
                record.PersonalBest = store.RecordBest(record.Scene, record.Seconds);
        }
        bool reset = !quickReset && (outcome == RunOutcome.Reset || outcome == RunOutcome.Death);
        if (reset)
        {
            if (store.Resets < int.MaxValue)
                store.Resets++;
            if (SessionResets < int.MaxValue)
                SessionResets++;
            store.TotalResetTime += record.Seconds;
            if (store.TimedResets < int.MaxValue)
                store.TimedResets++;
        }
        if (outcome == RunOutcome.Death && store.Deaths < int.MaxValue)
            store.Deaths++;
        store.FurthestProgress = Math.Max(store.FurthestProgress, record.Progress);
        store.CountDay(utcTicks, false, outcome == RunOutcome.Completed, reset);
        Event("RunEnd", outcome.ToString());
        if (!quickReset)
            store.AddHistory(record);
        Last = record;
        store.Active = null;
        store.QueueSave();
        if (record.ExportReport)
            store.QueueReport(record);
        return true;
    }
}
