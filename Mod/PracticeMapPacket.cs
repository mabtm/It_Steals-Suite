using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Text;
using System.Threading;
using UnityEngine;

public sealed class PracticeMapData
{
    public int Pid, Scene, Epoch, Seed, Recipe, SourceKind, Spawn, Keep, Flags, Stage, Requested, Completed,
        LiveSpawn = -1, LiveSeed, LiveRecipe;
    public long CapturedUtc;
    public uint RequestSerial;
    public string Hash = "UNKNOWN", Message = "";
    public Vector3[] Spawns = new Vector3[0], Points = new Vector3[0], Vertices = new Vector3[0];
    public int[] Indices = new int[0], Selected = new int[0], LiveSelected = new int[0];
    public Vector3[] NavigationNodes = new Vector3[0];
    public int[] NavigationEdges = new int[0], NavigationAnchors = new int[0];
    public int NavigationStatus, NavigationQueries, NavigationTotal;
    public string NavigationNote = "";
    public WallOutline[] WallOutlines = new WallOutline[0];
    public RunReviewData Review;
    public int NavigationModel; // 0: legacy AI; 1: isolated player bake + wall-checked legs.
    public int RngStatus, RngInit, RngRange, RngAdvance, RngSite, RngProbes, SearchBackend;
    public string RngNote = "";
    public Rect[] Walls = new Rect[0];
    public bool PhysicalCaptured;
    public int PhysicalOmitted;
    public MapFootprint[] PhysicalParts = new MapFootprint[0];
    public PickupRegion[] PickupRegions = new PickupRegion[0];
    public float AgentRadius, AgentHeight, AgentStep, AgentSlope, AgentFloor;
    public float MovementTick, MovementSprintStep;
    public MovementSummary Movement;
    public WallOutline[] Obstacles = new WallOutline[0];

    public PracticeMapData Copy()
    {
        var copy = (PracticeMapData)MemberwiseClone();
        copy.Selected = (int[])Selected.Clone();
        copy.LiveSelected = (int[])LiveSelected.Clone();
        copy.Samples = new System.Collections.Generic.List<SpawnSelection>(Samples);
        return copy;
    }

    public System.Collections.Generic.List<SpawnSelection> Samples =
        new System.Collections.Generic.List<SpawnSelection>();
    // Flags: model verified, wall overflow, navmesh overflow, live match,
    // first Timer spawn override, Timer catalog, prediction mismatch.
    public bool Verified
    {
        get {
            return (Flags & 1) != 0 && (Flags & 64) == 0;
        }
    }

    public byte[] Encode()
    {
        using (var stream = new MemoryStream()) using (var writer =
                                                           new BinaryWriter(stream, Encoding.UTF8, true))
        {
            writer.Write(0x504D5349u);
            writer.Write(15);
            foreach (int i in new[] { Pid, Scene, Epoch, Seed, Recipe, SourceKind, Spawn, Keep, Flags, Stage,
                                      Requested, Completed, LiveSpawn, LiveSeed, LiveRecipe })
                writer.Write(i);
            writer.Write(CapturedUtc);
            writer.Write(RequestSerial);
            Text(writer, Hash);
            Text(writer, Message);
            Vectors(writer, Spawns);
            Vectors(writer, Points);
            Ints(writer, Selected);
            Ints(writer, LiveSelected);
            Vectors(writer, Vertices);
            Ints(writer, Indices);
            writer.Write(Walls.Length);
            foreach (Rect r in Walls)
            {
                writer.Write(r.xMin);
                writer.Write(r.yMin);
                writer.Write(r.xMax);
                writer.Write(r.yMax);
            }
            writer.Write(Samples.Count);
            foreach (SpawnSelection s in Samples)
            {
                writer.Write(s.Spawn);
                Ints(writer, s.Crystals);
            }
            writer.Write(NavigationStatus);
            writer.Write(NavigationQueries);
            writer.Write(NavigationTotal);
            Text(writer, NavigationNote);
            Vectors(writer, NavigationNodes);
            Ints(writer, NavigationEdges);
            Ints(writer, NavigationAnchors);
            foreach (int value in new[] { RngStatus, RngInit, RngRange, RngAdvance, RngSite, RngProbes,
                                          SearchBackend })
                writer.Write(value);
            Text(writer, RngNote);
            writer.Write(NavigationModel);
            writer.Write(WallOutlines.Length);
            foreach (var outline in WallOutlines)
            {
                writer.Write(outline.Approximate);
                Vectors(writer, outline.Corners);
            }
            foreach (float value in new[] { AgentRadius, AgentHeight, AgentStep, AgentSlope, AgentFloor })
                writer.Write(value);
            writer.Write(Obstacles.Length);
            foreach (var outline in Obstacles)
            {
                writer.Write(outline.Approximate);
                Vectors(writer, outline.Corners);
            }
            RunReviewData.Write(writer, Review);
            writer.Write(MovementTick);
            writer.Write(MovementSprintStep);
            MovementSummary.Write(writer, Movement);
            ReplayMotionInfo.Write(writer, Review == null ? null : Review.Motion);
            MapPhysicalData.Write(writer, PhysicalCaptured, PhysicalOmitted, PhysicalParts, PickupRegions);
            writer.Flush();
            writer.Write(Checksum(stream.ToArray()));
            writer.Flush();
            if (stream.Length > PracticeMapChannel.Capacity - 64)
                throw new InvalidDataException("Practice map exceeds channel limit");
            return stream.ToArray();
        }
    }

    public static ulong Checksum(byte[] bytes, int count = -1)
    {
        ulong h = 14695981039346656037UL;
        unchecked
        {
            for (int i = 0; i < (count < 0 ? bytes.Length : count); i++)
            {
                h ^= bytes[i];
                h *= 1099511628211UL;
            }
        }
        return h;
    }

    private static void Text(BinaryWriter writer, string text)
    {
        byte[] bytes = Encoding.UTF8.GetBytes(text ?? "");
        writer.Write(bytes.Length);
        writer.Write(bytes);
    }

    private static void Ints(BinaryWriter writer, int[] values)
    {
        writer.Write(values.Length);
        foreach (int i in values)
            writer.Write(i);
    }

    private static void Vectors(BinaryWriter writer, Vector3[] values)
    {
        writer.Write(values.Length);
        foreach (Vector3 v in values)
        {
            writer.Write(v.x);
            writer.Write(v.y);
            writer.Write(v.z);
        }
    }
}

// A separate, event-driven channel keeps large static catalogs out of the
// game's 60 Hz telemetry. Both sides try the mutex without waiting.
public sealed class PracticeMapChannel : IDisposable
{
    public const int Capacity = 4194304;
    private MemoryMappedFile map;
    private MemoryMappedViewAccessor view;
    private Mutex gate;
    private uint revision;
    private byte[] pending;

    public PracticeMapChannel(int pid, uint session)
    {
        string name = "ItStealsPracticeMap.v10." + pid;
        try
        {
            gate = new Mutex(false, name + ".Lock");
            map = MemoryMappedFile.CreateOrOpen(name, Capacity);
            view = map.CreateViewAccessor(0, Capacity);
            bool entered;
            try
            {
                entered = gate.WaitOne(1000);
            }
            catch (AbandonedMutexException)
            {
                entered = true;
            }
            if (!entered)
                throw new TimeoutException("Practice map initialization timed out");
            try
            {
                view.Write(0, 0);
                view.Write(4, 0u);
                view.Write(8, session);
                view.Write(12, 0u);
            }
            finally
            {
                gate.ReleaseMutex();
            }
        }
        catch
        {
            Dispose();
            throw;
        }
    }

    public void Queue(PracticeMapData data)
    {
        pending = data.Encode();
    }

    // Search packets may never overwrite an unread packet. The trainer writes
    // the consumed revision after accepting a bounded batch into its queue.
    public bool ReadyForBatch()
    {
        if (pending != null || view == null || gate == null)
            return false;
        bool entered;
        try
        {
            entered = gate.WaitOne(0);
        }
        catch (AbandonedMutexException)
        {
            entered = true;
        }
        if (!entered)
            return false;
        try
        {
            return view.ReadUInt32(12) == revision;
        }
        finally
        {
            gate.ReleaseMutex();
        }
    }

    public bool TryPublishBatch(byte[] bytes)
    {
        return TryPublishBatch(bytes, bytes == null ? 0 : bytes.Length);
    }

    public bool TryPublishBatch(byte[] bytes, int length)
    {
        if (bytes == null || length <= 0 || length > bytes.Length || length > Capacity - 64 ||
            pending != null || view == null || gate == null)
            return false;
        bool entered;
        try
        {
            entered = gate.WaitOne(0);
        }
        catch (AbandonedMutexException)
        {
            entered = true;
        }
        if (!entered)
            return false;
        try
        {
            if (view.ReadUInt32(12) != revision)
                return false;
            view.Write(0, 0);
            view.WriteArray(64, bytes, 0, length);
            view.Write(4, unchecked(++revision));
            view.Write(0, length);
            return true;
        }
        finally
        {
            gate.ReleaseMutex();
        }
    }

    public void Flush()
    {
        if (pending == null || gate == null)
            return;
        bool entered;
        try
        {
            entered = gate.WaitOne(0);
        }
        catch (AbandonedMutexException)
        {
            entered = true;
        }
        if (!entered)
            return;
        try
        {
            view.Write(0, 0);
            view.WriteArray(64, pending, 0, pending.Length);
            view.Write(4, unchecked(++revision));
            view.Write(0, pending.Length);
            pending = null;
        }
        finally
        {
            gate.ReleaseMutex();
        }
    }

    public void Dispose()
    {
        pending = null;
        if (view != null)
        {
            view.Dispose();
            view = null;
        }
        if (map != null)
        {
            map.Dispose();
            map = null;
        }
        if (gate != null)
        {
            gate.Dispose();
            gate = null;
        }
    }
}
