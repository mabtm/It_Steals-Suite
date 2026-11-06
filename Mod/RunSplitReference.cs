using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using UnityEngine;

// Only numeric snapshots cross the worker boundary. No Unity scene reads and
// no archive IO run on Update / pickup / render. One frozen reference per run.
public sealed class RunSplitReference
{
    public long Id, TargetId;
    public int Scene, Seed, Spawn, Keep, SelectedSeed, PointCount;
    public bool RawSeed, Valid;
    public string Hash;
    public double Seconds;
    public int[] Selected, Collected;
    public Vector3[] Positions;
    public Vector3 SpawnPosition;
    public double[] PickupSeconds;

    public static RunSplitReference Capture(PracticeMapData map)
    {
        if (map == null || map.Review == null || map.Review.Category != RunCategory.SetSeed ||
            map.Spawn < 0 || map.Spawn >= map.Spawns.Length || map.Selected.Length == 0) return null;
        var r = map.Review;
        var positions = new Vector3[map.Selected.Length];
        for (int i = 0; i < positions.Length; i++)
        {
            int id = map.Selected[i];
            if (id < 0 || id >= map.Points.Length) return null;
            positions[i] = map.Points[id];
        }
        return new RunSplitReference { Id = r.Id, Scene = map.Scene, Seed = map.Seed,
            Spawn = map.Spawn, SpawnPosition = map.Spawns[map.Spawn], Keep = map.Keep,
            SelectedSeed = r.SelectedSeed, RawSeed = r.RawSpawnSeed, Hash = map.Hash,
            PointCount = map.Points.Length, Selected = (int[])map.Selected.Clone(), Positions = positions,
            Collected = r.Collected.ToArray(), PickupSeconds = r.PickupSeconds.ToArray(),
            Seconds = r.Seconds, Valid = r.ValidationChecked && r.Valid };
    }

    public bool Matches(RunSplitReference other)
    {
        if (other == null || Scene != other.Scene || Seed != other.Seed || Spawn != other.Spawn ||
            Keep != other.Keep || SelectedSeed != other.SelectedSeed || RawSeed != other.RawSeed ||
            Hash != other.Hash || PointCount != other.PointCount || Selected.Length != other.Selected.Length ||
            (SpawnPosition - other.SpawnPosition).sqrMagnitude > .000001f) return false;
        for (int i = 0; i < Selected.Length; i++)
            if (Selected[i] != other.Selected[i] || (Positions[i] - other.Positions[i]).sqrMagnitude > .000001f) return false;
        return true;
    }

    public static bool Better(RunSplitReference candidate, RunSplitReference current)
    {
        return candidate != null && candidate.Seconds > 0 && !double.IsInfinity(candidate.Seconds) &&
            !double.IsNaN(candidate.Seconds) && candidate.Collected.Length > 0 &&
            candidate.Collected.Length == candidate.PickupSeconds.Length &&
            (current == null || candidate.Valid && !current.Valid || candidate.Valid == current.Valid &&
                (candidate.Seconds < current.Seconds || candidate.Seconds == current.Seconds && candidate.Id < current.Id));
    }
}

public static class RunSplitArchive
{
    private sealed class Cached
    {
        public long Size, Modified;
        public RunSplitReference Item;
    }
    private sealed class Request
    {
        public RunSplitReference Target;
        public RunSplitReference[] Recent;
        public string Directory;
    }
    private static readonly object gate = new object();
    private static readonly AutoResetEvent wake = new AutoResetEvent(false);
    private static readonly Dictionary<string, Cached> cache = new Dictionary<string, Cached>(StringComparer.OrdinalIgnoreCase);
    private static Request pending;
    private static Thread worker;
    private static long targetId;
    private static RunSplitReference result;
    private static int state;
    public static string DirectoryPath;
    public static int State(long run) { return Interlocked.Read(ref targetId) == run ? Volatile.Read(ref state) : 0; }
    public static RunSplitReference Reference(long run)
    {
        var item = Volatile.Read(ref result);
        return item != null && item.TargetId == run ? item : null;
    }
    public static void Begin(long run)
    {
        lock (gate)
        {
            Interlocked.Exchange(ref targetId, run);
            pending = null;
            Volatile.Write(ref result, null);
            Volatile.Write(ref state, 0);
        }
    }
    public static void Find(RunSplitReference target, RunSplitReference[] recent)
    {
        if (target == null || target.Id != Interlocked.Read(ref targetId)) return;
        lock (gate)
        {
            pending = new Request { Target = target, Recent = recent, Directory = DirectoryPath };
            Volatile.Write(ref state, 1);
            if (worker == null)
            {
                worker = new Thread(Work) { IsBackground = true, Name = "Elite.SetSeedSplits", Priority = System.Threading.ThreadPriority.BelowNormal };
                worker.Start();
            }
            wake.Set();
        }
    }
    private static void Work()
    {
        for (;;)
        {
            wake.WaitOne();
            Request request;
            lock (gate) { request = pending; pending = null; }
            if (request == null) continue;
            RunSplitReference best = null;
            bool failed = false;
            Action<RunSplitReference> consider = item => {
                if (item != null && item.Id != request.Target.Id && request.Target.Matches(item) && RunSplitReference.Better(item, best)) best = item;
            };
            foreach (var item in request.Recent) consider(item);
            try
            {
                if (!string.IsNullOrEmpty(request.Directory) && Directory.Exists(request.Directory))
                {
                    var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                    foreach (string path in Directory.GetFiles(request.Directory, "run-*.isrun"))
                    {
                        if (request.Target.Id != Interlocked.Read(ref targetId)) break;
                        seen.Add(path);
                        try
                        {
                            var info = new FileInfo(path);
                            Cached entry;
                            if (!cache.TryGetValue(path, out entry) || entry.Size != info.Length || entry.Modified != info.LastWriteTimeUtc.Ticks)
                            {
                                entry = new Cached { Size = info.Length, Modified = info.LastWriteTimeUtc.Ticks };
                                if (info.Length <= PracticeMapChannel.Capacity - 64)
                                {
                                    var map = PracticeMapArchive.Decode(File.ReadAllBytes(path));
                                    if (map.Review != null && map.Review.Outcome == (int)RunOutcome.Completed)
                                        entry.Item = RunSplitReference.Capture(map);
                                }
                                cache[path] = entry;
                            }
                            consider(entry.Item);
                        }
                        catch (IOException) { /* A recording can still be committing. Try again next run. */ }
                        catch (Exception) { /* Corrupt recordings cannot be a reference. */ }
                    }
                    foreach (var path in new List<string>(cache.Keys)) if (!seen.Contains(path)) cache.Remove(path);
                }
            }
            catch (Exception) { failed = true; }
            lock (gate)
            {
                if (request.Target.Id != Interlocked.Read(ref targetId)) continue;
                if (best != null) best.TargetId = request.Target.Id;
                Volatile.Write(ref result, best);
                Volatile.Write(ref state, best != null ? 2 : failed ? 3 : 0);
            }
        }
    }
}
