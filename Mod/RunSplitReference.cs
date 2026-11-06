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
    internal string LayoutIdentity;
    internal int ObservedBookGeneration;
    public int Scene, Seed, Spawn, Keep, SelectedSeed, PointCount, Outcome;
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
            Seconds = r.Seconds, Outcome = r.Outcome, Valid = r.ValidationChecked && r.Valid };
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

    public bool Follows(int[] prefix)
    {
        if (prefix.Length > Collected.Length) return false;
        for (int i = 0; i < prefix.Length; i++) if (prefix[i] != Collected[i]) return false;
        return true;
    }

    public static bool Better(RunSplitReference candidate, RunSplitReference current)
    {
        return candidate != null && candidate.Outcome == 1 && candidate.Seconds > 0 && !double.IsInfinity(candidate.Seconds) &&
            !double.IsNaN(candidate.Seconds) && candidate.Collected.Length > 0 &&
            candidate.Collected.Length == candidate.PickupSeconds.Length &&
            (current == null || candidate.Valid && !current.Valid || candidate.Valid == current.Valid &&
                (candidate.Seconds < current.Seconds || candidate.Seconds == current.Seconds && candidate.Id < current.Id));
    }
}

public sealed class RunSplitHistory
{
    public long TargetId;
    public RunSegmentBook.Profile Profile;
    public bool Ready;
    public double Theoretical = -1, BestExit = -1;
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
        public int Version;
    }
    private static readonly object gate = new object();
    private static readonly AutoResetEvent wake = new AutoResetEvent(false);
    private static readonly ManualResetEvent idle = new ManualResetEvent(true);
    private static readonly byte[] checksumBuffer = new byte[64 * 1024];
    private static readonly Dictionary<string, Cached> cache = new Dictionary<string, Cached>(StringComparer.OrdinalIgnoreCase);
    private static Request pending;
    private static readonly Queue<RunSplitReference> ended = new Queue<RunSplitReference>();
    private static RunSegmentBook book;
    private static string bookDirectory;
    private static int bookGeneration;
    private static RunSplitHistory history;
    public static volatile string Error;
    private static void EnsureWorker()
    {
        if (worker != null) return;
        worker = new Thread(Work) { IsBackground = true, Name = "Elite.SetSeedSplits", Priority = System.Threading.ThreadPriority.BelowNormal };
        worker.Start();
    }
    private static void EnsureBook(string directory)
    {
        if (book != null && directory == bookDirectory) return;
        if (book != null) book.Save();
        bookDirectory = directory; bookGeneration++;
        book = new RunSegmentBook(string.IsNullOrEmpty(directory) ? null :
            Path.Combine(Path.GetDirectoryName(directory), "set-seed-sectors-v1.bin"));
        Error = book.Error;
    }
    private static void ObserveOnce(RunSplitReference item)
    {
        if (item == null || item.ObservedBookGeneration == bookGeneration) return;
        book.Observe(item); item.ObservedBookGeneration = bookGeneration;
    }
    public static void WarmUp(string directory)
    {
        lock (gate)
        {
            if (pending != null || Interlocked.Read(ref targetId) != 0) return;
            pending = new Request { Directory = directory, Recent = new RunSplitReference[0] };
            idle.Reset(); EnsureWorker(); wake.Set();
        }
    }
    public static RunSplitHistory History(long run)
    {
        var item = Volatile.Read(ref history);
        return item != null && item.TargetId == run ? item : null;
    }
    public static void RecordEnd(RunSplitReference run)
    {
        if (run == null || !run.Valid || run.Collected.Length == 0) return;
        lock (gate)
        {
            if (ended.Count >= 128) { Error = "Sector-save queue full; latest sectors were not saved."; return; }
            ended.Enqueue(run); idle.Reset(); EnsureWorker(); wake.Set();
        }
    }
    private static Thread worker;
    private static long targetId;
    private static int version, requestedCount = -1;
    public static int RequestedCount(long run) { return Interlocked.Read(ref targetId) == run ? Volatile.Read(ref requestedCount) : -1; }
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
            version++; Volatile.Write(ref requestedCount, -1);
            pending = null;
            Volatile.Write(ref result, null);
            Volatile.Write(ref history, null);
            Volatile.Write(ref state, 0);
        }
    }
    public static void Find(RunSplitReference target, RunSplitReference[] recent)
    {
        if (target == null || target.Id != Interlocked.Read(ref targetId)) return;
        lock (gate)
        {
            idle.Reset();
            pending = new Request { Target = target, Recent = recent, Directory = DirectoryPath, Version = ++version };
            Volatile.Write(ref requestedCount, target.Collected.Length);
            Volatile.Write(ref state, 1);
            EnsureWorker();
            wake.Set();
        }
    }
    public static bool Flush(int milliseconds = 2500)
    {
        lock (gate)
        {
            if (worker == null) return true;
            Interlocked.Exchange(ref targetId, 0); pending = null; wake.Set();
        }
        return idle.WaitOne(milliseconds) && Error == null;
    }
    private static void Work()
    {
        for (;;)
        {
            wake.WaitOne();
            Request request; RunSplitReference[] completed;
            lock (gate) { request = pending; pending = null; completed = ended.ToArray(); ended.Clear(); }
            try
            {
                EnsureBook(request == null ? DirectoryPath : request.Directory);
                foreach (var run in completed) ObserveOnce(run);
                if (request == null) { book.Save(); Error = book.Error; continue; }
                bool warming = request.Target == null;
                RunSplitReference best = null;
                bool failed = false;
                Action<RunSplitReference> consider = item => {
                    ObserveOnce(item);
                    if (item == null || warming || item.Id == request.Target.Id) return;
                    if (request.Target.Matches(item) && item.Follows(request.Target.Collected) && RunSplitReference.Better(item, best)) best = item;
                };
                foreach (var item in request.Recent) consider(item);
                try
                {
                    if (!string.IsNullOrEmpty(request.Directory) && Directory.Exists(request.Directory))
                    {
                        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                        bool scannedAll = true;
                        foreach (string path in Directory.GetFiles(request.Directory, "run-*.isrun"))
                        {
                            if (warming ? Volatile.Read(ref pending) != null || Interlocked.Read(ref targetId) != 0 : request.Target.Id != Interlocked.Read(ref targetId))
                            { scannedAll = false; break; }
                            seen.Add(path);
                            try
                            {
                                var info = new FileInfo(path);
                                Cached entry;
                                if (!cache.TryGetValue(path, out entry) || entry.Size != info.Length || entry.Modified != info.LastWriteTimeUtc.Ticks)
                                {
                                    entry = new Cached { Size = info.Length, Modified = info.LastWriteTimeUtc.Ticks };
                                    cache[path] = entry; // Cache failed signatures too; modified files retry next scan.
                                    if (info.Length <= PracticeMapChannel.Capacity - 64)
                                    {
                                        entry.Item = RunSplitFile.Read(path,checksumBuffer);
                                    }
                                }
                                consider(entry.Item);
                            }
                            catch (InvalidDataException) { /* An unchanged invalid file is not reparsed every reset. */ }
                            catch (IOException) { cache.Remove(path); /* Sharing/commit failures retry next run. */ }
                            catch (Exception) { /* Corrupt recordings cannot be a reference. */ }
                        }
                        if (scannedAll)
                            foreach (var path in new List<string>(cache.Keys)) if (!seen.Contains(path)) cache.Remove(path);
                    }
                }
                catch (Exception) { failed = true; }
                book.Save(); Error = book.Error;
                if (warming) continue;
                var profile = book.Snapshot(request.Target);
                var summary = new RunSplitHistory { TargetId = request.Target.Id, Profile = profile, Ready = book.Error == null };
                if (best != null && best.Collected.Length == best.Selected.Length)
                {
                    summary.Theoretical = profile.Theoretical(best.Collected);
                    summary.BestExit = profile.Exit(best.Collected);
                }
                lock (gate)
                {
                    if (request.Target.Id != Interlocked.Read(ref targetId) || request.Version != version) continue;
                    Volatile.Write(ref history, summary);
                    if (best != null) best.TargetId = request.Target.Id;
                    Volatile.Write(ref result, best);
                    Volatile.Write(ref state, best != null ? 2 : failed ? 3 : 0);
                }
            }
            catch (Exception e)
            {
                Error = "Split history unavailable: " + e.Message;
                if (request != null && request.Target != null && request.Target.Id == Interlocked.Read(ref targetId))
                    Volatile.Write(ref state, 3);
            }
            finally
            {
                lock (gate)
                    if (pending == null && ended.Count == 0) idle.Set();
                    else wake.Set();
            }
        }
    }
}
