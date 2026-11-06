using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using UnityEngine;

public struct RunPose
{
    public double Seconds;
    public Vector3 Position;
    public bool Continuous;
}

public sealed class RunReviewData
{
    public ReplayMotionInfo Motion;
    public long Id;
    public int Outcome;
    public double Seconds;
    public bool Reduced;
    public bool ValidationChecked, Valid, ObservedFromStart, SeedKnown, MonstersReduced;
    public int SeedKind, SelectedSeed, RunSeed;
    public RunCategory Category;
    public bool RawSpawnSeed;
    public uint InitialAids;
    public uint Aids, Warnings;
    public string ValidityReason = "Recorded by an older version; validation unavailable";
    public uint LightSources;
    public int LightRejected;
    public readonly List<ReplayMonster> Monsters = new List<ReplayMonster>();
    public readonly List<ReplayFlash> Flashes = new List<ReplayFlash>();
    public readonly List<RunPose> Trace = new List<RunPose>();
    public readonly List<int> Collected = new List<int>();
    public readonly List<double> PickupSeconds = new List<double>();

    // Actor zero means an emitted flash without a confirmed monster contact.
    // A subsequent reaction enriches the pulse rather than drawing it twice.
    public bool LightEvent(double seconds, Vector3 position, int kind, int actor = 0, bool contact = false)
    {
        if (double.IsNaN(seconds) || double.IsInfinity(seconds) || seconds < 0 || !Coordinate(position.x) ||
            !Coordinate(position.y) || !Coordinate(position.z) || kind < 1 || kind > 2 || actor < 0 ||
            Flashes.Count > 0 && seconds < Flashes[Flashes.Count - 1].Seconds)
        {
            MonstersReduced = true;
            LightRejected++;
            return false;
        }
        if (Flashes.Count >= 4096)
        {
            MonstersReduced = true;
            LightRejected++;
            return false;
        }
        Flashes.Add(new ReplayFlash { Seconds = seconds, Player = position, Kind = kind, Actor = actor });
        return true;
    }

    public bool ConfirmLight(int index, int actor)
    {
        if (index < 0 || index >= Flashes.Count || actor <= 0)
            return false;
        var pulse = Flashes[index];
        if (pulse.Actor == actor)
            return true;
        if (pulse.Actor != 0)
            return false;
        pulse.Actor = actor;
        Flashes[index] = pulse;
        return true;
    }

    private double nextSample;

    public bool Sample(double seconds, Vector3 position, bool force = false)
    {
        if (double.IsNaN(seconds) || double.IsInfinity(seconds) || seconds < 0 || !Coordinate(position.x) ||
            !Coordinate(position.y) || !Coordinate(position.z) ||
            Trace.Count > 0 && seconds < Trace[Trace.Count - 1].Seconds)
        {
            Reduced = true;
            return false;
        }
        if (!force && seconds + 1e-6 < nextSample)
            return false;
        nextSample = seconds + .02;
        if (Trace.Count >= 32768)
        {
            for (int i = 1; i < Trace.Count / 2; i++)
                Trace[i] = Trace[i * 2];
            Trace.RemoveRange(Trace.Count / 2, Trace.Count - Trace.Count / 2);
            Reduced = true;
        }
        Trace.Add(new RunPose { Seconds = seconds, Position = position });
        return true;
    }

    private static bool Coordinate(float value)
    {
        return !float.IsNaN(value) && !float.IsInfinity(value) && Math.Abs(value) <= 100000;
    }

    public static void Write(BinaryWriter writer, RunReviewData review)
    {
        writer.Write(review != null);
        if (review == null)
            return;
        writer.Write(review.Id);
        writer.Write(review.Outcome);
        writer.Write(review.Seconds);
        writer.Write(review.Reduced);
        writer.Write(review.Trace.Count);
        foreach (var p in review.Trace)
        {
            writer.Write(p.Seconds);
            writer.Write(p.Position.x);
            writer.Write(p.Position.y);
            writer.Write(p.Position.z);
        }
        writer.Write(review.Collected.Count);
        foreach (int id in review.Collected)
            writer.Write(id);
        writer.Write(review.PickupSeconds.Count);
        foreach (double time in review.PickupSeconds)
            writer.Write(time);
        ReplayCapture.Write(writer, review);
    }
}

public static class RunReviewRecorder
{
    public struct PickupState
    {
        public int Before;
        public Vector3 Position;
        public bool Point;
    }

    private static PracticeMapData active, transient;
    private static ReplayRetention configured = new ReplayRetention(10, 0), recording;

    public static void ConfigureRetention(double seconds, uint crystals)
    {
        configured = new ReplayRetention(seconds, crystals);
    }

    public static void DiscardTransient()
    {
        transient = null;
    }

    public static bool LastRetained
    {
        get {
            return Last != null && !object.ReferenceEquals(Last, transient);
        }
    }
    private static readonly List<PracticeMapData> recent = new List<PracticeMapData>();
    private static double nextGeometry;
    private static bool geometryReady;
    private static int startedFrame;
    private static PickupState pendingPickup;
    private static MouseLook pickupPlayer;
    private static bool pickupObserved;
    private static PracticeMapData restored;
    public static string RecoveryWarning { get; private set; }
    public static PracticeMapData Last
    {
        get {
            AdoptRestored();
            return transient ?? (recent.Count == 0 ? null : recent[recent.Count - 1]);
        }
    }

    public static void Restore(string directory)
    {
        RunSplitArchive.DirectoryPath = Path.Combine(directory, "RunReviews");
        new Thread(() =>
                   {
                       try
                       {
                           var folder = Path.Combine(directory, "RunReviews");
                           if (!Directory.Exists(folder))
                               return;
                           var files = new List<string>(Directory.GetFiles(folder, "run-*.isrun"));
                           files.Sort(StringComparer.Ordinal);
                           for (int i = files.Count - 1; i >= Math.Max(0, files.Count - 8); i--)
                           {
                               try
                               {
                                   var info = new FileInfo(files[i]);
                                   if (info.Length > PracticeMapChannel.Capacity - 64)
                                       throw new InvalidDataException("Recorded run exceeds file limit");
                                   var item = PracticeMapArchive.Decode(File.ReadAllBytes(files[i]));
                                   if (item.Review != null)
                                   {
                                       Volatile.Write(ref restored, item);
                                       return;
                                   }
                               }
                               catch (Exception e)
                               {
                                   RecoveryWarning = "Latest recorded run could not be read: " + e.Message;
                               }
                           }
                       }
                       catch (Exception e)
                       {
                           RecoveryWarning = e.Message;
                       }
                   }) { IsBackground = true, Name = "ItSteals.RunReviewRecovery" }
            .Start();
    }

    private static void AdoptRestored()
    {
        var item = Interlocked.Exchange(ref restored, null);
        if (item == null)
            return;
        if (recent.Count == 0 || recent[recent.Count - 1].Review.Id < item.Review.Id)
            recent.Add(item);
    }

    public static void Begin(RunRecord record, MouseLook player)
    {
        transient = null;
        recording = configured;
        active = SeedMapPlanner.RunSnapshot();
        nextGeometry = 0;
        geometryReady = false;
        if (active == null || record == null)
            return;
        startedFrame = Time.frameCount;
        RunSplitArchive.Begin(record.Id);
        active.Review = new RunReviewData { Id = record.Id, Category = record.Category,
            SelectedSeed = record.SelectedSeed, RawSpawnSeed = record.RawSpawnSeed };
        ReplayCapture.Begin(active.Review, player);
        ReplayInputCapture.Begin(active, player, record);
        Observe(player, 0);
    }

    public static void Observe(MouseLook player, double seconds, bool force = false)
    {
        if (active == null)
            return;
        if (!geometryReady && Time.frameCount > startedFrame &&
            Time.realtimeSinceStartupAsDouble >= nextGeometry)
        {
            nextGeometry = Time.realtimeSinceStartupAsDouble + 1;
            var capture = SeedMapPlanner.RunSnapshot(true);
            if (capture != null && capture.Epoch == active.Epoch && capture.Scene == active.Scene)
            {
                var review = active.Review;
                active = capture;
                active.Review = review;
                geometryReady = active.Indices.Length > 0 && active.AgentHeight > 0;
                if (geometryReady && active.Review.Category == RunCategory.SetSeed)
                {
                    var candidates = new List<RunSplitReference>();
                    foreach (var previous in recent)
                        if (previous.Review.Outcome == (int)RunOutcome.Completed)
                        {
                            var item = RunSplitReference.Capture(previous);
                            if (item != null) candidates.Add(item);
                        }
                    RunSplitArchive.Find(RunSplitReference.Capture(active), candidates.ToArray());
                }
            }
        }
        var body = PlayerContext.Body(player);
        if (body == null)
            return;
        active.Review.Sample(seconds, body.position, force);
        ReplayCapture.Observe(player, seconds);
    }

    public static void BeforePickup(MouseLook __instance, Collider other, out PickupState __state)
    {
        __state = new PickupState { Before = __instance.Objectives };
        if (other != null && other.CompareTag("Point"))
        {
            __state.Point = true;
            __state.Position = other.transform.position;
        }
        pendingPickup = __state;
        pickupPlayer = __instance;
        pickupObserved = false;
    }

    private static void CapturePickup(MouseLook __instance, PickupState __state)
    {
        if (active != null && __state.Point && __instance.Objectives < __state.Before)
        {
            int nearest = -1;
            float distance = 4;
            foreach (int id in active.Selected)
                if (!active.Review.Collected.Contains(id))
                {
                    float value = (active.Points[id] - __state.Position).sqrMagnitude;
                    if (value < distance)
                    {
                        distance = value;
                        nearest = id;
                    }
                }
            if (nearest >= 0)
            {
                active.Review.Collected.Add(nearest);
                active.Review.PickupSeconds.Add(
                    SpeedrunManager.Instance == null ? 0 : SpeedrunManager.Instance.CurrentSeconds);
                Observe(__instance, active.Review.PickupSeconds[active.Review.PickupSeconds.Count - 1], true);
            }
        }
    }

    public static void ObjectivesObserved(MouseLook player)
    {
        if (player == pickupPlayer && !pickupObserved)
        {
            CapturePickup(player, pendingPickup);
            pickupObserved = true;
        }
        SpeedrunManager.ObjectivesChanged(player);
    }

    public static void AfterPickup(MouseLook __instance, PickupState __state)
    {
        if (pickupPlayer != __instance || !pickupObserved)
            CapturePickup(__instance, __state);
        if (pickupPlayer == __instance)
            pickupPlayer = null;
        SpeedrunManager.ObjectivesChanged(__instance);
    }

    public static void Finish(RunRecord record, RunStore store)
    {
        if (active == null || active.Review.Id != record.Id)
        {
            if (store.RetainRankedReplay(record))
                store.LastReportError = "PB / Top 5 replay " + record.Id + " is unavailable: map capture did not start.";
            return;
        }
        active.Review.Outcome = (int)record.Outcome;
        active.Review.Seconds = record.Seconds;
        active.Movement = MovementCapture.ForMap(active, true);
        active.Review.ValidationChecked = record.ValidationChecked;
        active.Review.Valid = RunValidity.Eligible(record);
        active.Review.ObservedFromStart = record.ObservedFromStart;
        active.Review.SeedKnown = record.SeedKnown;
        active.Review.SeedKind = (int)record.SeedType;
        active.Review.Category = record.Category;
        active.Review.SelectedSeed = record.SelectedSeed;
        active.Review.RunSeed = record.Seed;
        active.Review.RawSpawnSeed = record.RawSpawnSeed;
        active.Review.InitialAids = (uint)record.InitialAids;
        active.Review.Aids = (uint)record.Aids;
        active.Review.Warnings = (uint)record.Warnings;
        active.Review.ValidityReason = RunValidity.Reason(record);
        ReplayCapture.End();
        var motion = ReplayInputCapture.Finish(record.Seconds);
        bool ranked = store.RetainRankedReplay(record);
        bool retain = ranked ||
                      recording.Accept(record.Seconds, active.Review.Collected.Count);
        if (!retain && motion != null && motion.AudioSession != null)
            motion.AudioSession.Discard();
        if (retain)
        {
            recent.Add(active);
            if (recent.Count > 8)
                recent.RemoveAt(0);
        }
        else
            transient = active;
        try
        {
            if (retain)
                store.QueueReview(record.Id, active.Encode(), motion, VerificationReport.Encode(record),
                                  ranked ? "ELITE_REPLAY_KEEP|1|" + record.Id.ToString(System.Globalization.CultureInfo.InvariantCulture) +
                                           (record.PersonalBest ? "|PB" : "|TOP5") : null);
        }
        catch (Exception e)
        {
            store.LastReportError = "Run review not saved: " + e.Message;
        }
        finally
        {
            active = null;
        }
    }

    public static PracticeMapData Find(long id)
    {
        AdoptRestored();
        if (transient != null && transient.Review.Id == id)
            return transient;
        foreach (var item in recent)
            if (item.Review.Id == id)
                return item;
        return null;
    }

    public static void WriteSplits(SnapshotBuffer data, long expectedRun)
    {
        var map = active != null && active.Review.Id == expectedRun ? active : Find(expectedRun);
        if (map == null || map.Review.Category != RunCategory.SetSeed) return;
        data.Write(SharedLayout.SplitRunId, expectedRun);
        var review = map.Review;
        int count = Math.Min(64, Math.Min(review.Collected.Count, review.PickupSeconds.Count));
        data.Write(SharedLayout.SplitPickupCount, count);
        for (int i = 0; i < count; i++)
        {
            data.Write(SharedLayout.SplitPickupIds + i * 4, review.Collected[i]);
            data.Write(SharedLayout.SplitPickupSeconds + i * 8, review.PickupSeconds[i]);
        }
        var reference = RunSplitArchive.Reference(expectedRun);
        data.Write(SharedLayout.SplitReferenceState, (byte)RunSplitArchive.State(expectedRun));
        if (reference == null) return;
        data.Write(SharedLayout.SplitReferenceId, reference.Id);
        data.Write(SharedLayout.SplitReferenceValid, reference.Valid);
        count = Math.Min(64, Math.Min(reference.Collected.Length, reference.PickupSeconds.Length));
        data.Write(SharedLayout.SplitReferenceCount, count);
        for (int i = 0; i < count; i++)
        {
            data.Write(SharedLayout.SplitReferenceIds + i * 4, reference.Collected[i]);
            data.Write(SharedLayout.SplitReferenceSeconds + i * 8, reference.PickupSeconds[i]);
        }
    }

    public static void WriteStatus(SnapshotBuffer data, long expectedRun = 0, int expectedScene = -1)
    {
        // Never attach a previous recording to a newly finished run that had
        // no usable capture. Keep the exact current identity even while absent.
        var last = expectedRun > 0 ? Find(expectedRun) : Last;
        if (last != null && expectedScene >= 0 && last.Scene != expectedScene)
            last = null;
        if (last == null)
        {
            if (expectedRun > 0)
            {
                data.Write(SharedLayout.ReviewRunId, expectedRun);
                data.Write(SharedLayout.ReviewScene, expectedScene);
            }
            return;
        }
        data.Write(SharedLayout.ReviewRetained, !object.ReferenceEquals(last, transient));
        data.Write(SharedLayout.ReviewRunId, last.Review.Id);
        data.Write(SharedLayout.ReviewScene, last.Scene);
        data.Write(SharedLayout.ReviewSpawnSeed, last.Seed);
        data.Write(SharedLayout.ReviewSamples, last.Review.Trace.Count);
        data.Write(SharedLayout.ReviewReady, last.Spawn >= 0 && last.Spawn < last.Spawns.Length &&
                                                 last.Selected.Length > 0 && last.Review.Trace.Count > 0);
    }
}
