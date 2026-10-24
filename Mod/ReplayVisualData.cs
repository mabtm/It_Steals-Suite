using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

// Passive render observations, separate from inputs and run validity. Assets
// are referenced by stable names; their bytes stay in the installed game.
public struct ReplayVisualFrame
{
    public double Seconds;
    public Vector3 Position, Scale;
    public Quaternion Rotation;
    public Color Color;
    public float Intensity, Range, Angle, InnerAngle;
    public uint Flags;
    public int Asset, Texture;
}

public sealed class ReplayVisualTrack
{
    public int Kind, Slot; // 1 transform, 2 renderer, 3 sprite, 4 light
    public string Path = "";
    public readonly List<ReplayVisualFrame> Frames = new List<ReplayVisualFrame>();

    public int At(double at)
    {
        int lo = 0, hi = Frames.Count;
        while (lo < hi)
        {
            int m = lo + (hi - lo) / 2;
            if (Frames[m].Seconds <= at)
                lo = m + 1;
            else
                hi = m;
        }
        return lo - 1;
    }
}

public sealed class ReplayVisualOwner
{
    public int Group, Id, Kind; // 0 world, 1 player, 2 monster, 3 objective
    public string Root = "";
    public readonly List<ReplayVisualTrack> Tracks = new List<ReplayVisualTrack>();
}

public sealed class ReplayVisualData
{
    public const int MaxOwners = 512, MaxTracks = 8192, MaxSamples = 1048576, MaxAssets = 4096,
                     MaxWire = 64 * 1024 * 1024;
    public bool Recorded, Full;
    public int Dropped, Count, TrackCount;
    private int estimated = 64;
    public Func<int, bool> ReserveBytes;
    public long WireBytes
    {
        get {
            return estimated;
        }
    }
    public long EstimatedBytes
    {
        get {
            return estimated + Count * 192L + TrackCount * 64L;
        }
    }

    private bool Reserve(int bytes)
    {
        if (estimated + bytes <= MaxWire && (ReserveBytes == null || ReserveBytes(bytes)))
            return true;
        Full = true;
        Drop();
        return false;
    }

    public readonly List<string> Assets = new List<string>();
    public readonly List<ReplayVisualOwner> Owners = new List<ReplayVisualOwner>();
    private readonly Dictionary<string, int> assetIds = new Dictionary<string, int>(StringComparer.Ordinal);

    public void Drop()
    {
        if (Dropped < int.MaxValue)
            Dropped++;
    }

    public int Asset(string name)
    {
        if (string.IsNullOrEmpty(name))
            return 0;
        int id;
        if (assetIds.TryGetValue(name, out id))
            return id;
        if (Assets.Count >= MaxAssets || System.Text.Encoding.UTF8.GetByteCount(name) > 1024)
        {
            Drop();
            return 0;
        }
        if (!Reserve(name.Length * 4 + 4))
            return 0;
        Assets.Add(name);
        id = Assets.Count;
        assetIds.Add(name, id);
        estimated += name.Length * 4 + 4;
        return id;
    }

    public ReplayVisualOwner Owner(int group, int id, int kind, string root)
    {
        if (Owners.Count >= MaxOwners || System.Text.Encoding.UTF8.GetByteCount(root) > 1024)
        {
            Drop();
            return null;
        }
        if (!Reserve(32 + root.Length * 4))
            return null;
        var owner = new ReplayVisualOwner { Group = group, Id = id, Kind = kind, Root = root };
        Owners.Add(owner);
        estimated += 32 + root.Length * 4;
        return owner;
    }

    public ReplayVisualTrack Track(ReplayVisualOwner owner, int kind, string path, int slot = 0)
    {
        if (TrackCount >= MaxTracks || System.Text.Encoding.UTF8.GetByteCount(path) > 1024)
        {
            Drop();
            return null;
        }
        if (!Reserve(path.Length * 4 + 20))
            return null;
        var t = new ReplayVisualTrack { Kind = kind, Path = path, Slot = slot };
        owner.Tracks.Add(t);
        TrackCount++;
        estimated += path.Length * 4 + 20;
        return t;
    }

    public void Add(ReplayVisualTrack track, ReplayVisualFrame frame)
    {
        int bytes = track.Kind == 1 ? 48 : track.Kind == 4 ? 72 : 36;
        if (Count >= MaxSamples || estimated + bytes > MaxWire ||
            ReserveBytes != null && !ReserveBytes(bytes))
        {
            Full = true;
            Drop();
            return;
        }
        if (track.Frames.Count > 0 && frame.Seconds < track.Frames[track.Frames.Count - 1].Seconds)
        {
            Drop();
            return;
        }
        track.Frames.Add(frame);
        Count++;
        estimated += bytes;
    }

    private static void V(BinaryWriter w, Vector3 v)
    {
        w.Write(v.x);
        w.Write(v.y);
        w.Write(v.z);
    }

    private static void Q(BinaryWriter w, Quaternion q)
    {
        w.Write(q.x);
        w.Write(q.y);
        w.Write(q.z);
        w.Write(q.w);
    }

    private static void C(BinaryWriter w, Color c)
    {
        w.Write(c.r);
        w.Write(c.g);
        w.Write(c.b);
        w.Write(c.a);
    }

    public void Write(BinaryWriter w)
    {
        w.Write(Recorded);
        w.Write(Dropped);
        w.Write(Assets.Count);
        foreach (string a in Assets)
            ReplayMotionData.Text(w, a);
        w.Write(Owners.Count);
        foreach (var o in Owners)
        {
            w.Write(o.Group);
            w.Write(o.Id);
            w.Write(o.Kind);
            ReplayMotionData.Text(w, o.Root);
            w.Write(o.Tracks.Count);
            foreach (var t in o.Tracks)
            {
                w.Write(t.Kind);
                w.Write(t.Slot);
                ReplayMotionData.Text(w, t.Path);
                w.Write(t.Frames.Count);
                foreach (var f in t.Frames)
                {
                    w.Write(f.Seconds);
                    if (t.Kind == 1)
                    {
                        V(w, f.Position);
                        Q(w, f.Rotation);
                        V(w, f.Scale);
                    }
                    else
                    {
                        w.Write(f.Flags);
                        C(w, f.Color);
                        if (t.Kind == 4)
                        {
                            V(w, f.Position);
                            Q(w, f.Rotation);
                            w.Write(f.Intensity);
                            w.Write(f.Range);
                            w.Write(f.Angle);
                            w.Write(f.InnerAngle);
                        }
                        else
                        {
                            w.Write(f.Asset);
                            w.Write(f.Texture);
                        }
                    }
                }
            }
        }
    }

    private static float N(BinaryReader r, float limit = 100000)
    {
        float f = r.ReadSingle();
        if (float.IsNaN(f) || float.IsInfinity(f) || Math.Abs(f) > limit)
            throw new InvalidDataException("Invalid visual number");
        return f;
    }

    private static int Size(BinaryReader r, int max)
    {
        int n = r.ReadInt32();
        if (n < 0 || n > max)
            throw new InvalidDataException("Visual recording exceeds limit");
        return n;
    }

    private static Vector3 V(BinaryReader r)
    {
        return new Vector3(N(r), N(r), N(r));
    }

    private static Quaternion Q(BinaryReader r)
    {
        var q = new Quaternion(N(r, 1.01f), N(r, 1.01f), N(r, 1.01f), N(r, 1.01f));
        double length = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (length < .98 || length > 1.02)
            throw new InvalidDataException("Invalid visual rotation");
        return q;
    }

    private static Color C(BinaryReader r)
    {
        return new Color(N(r), N(r), N(r), N(r));
    }

    public static ReplayVisualData Read(BinaryReader r, double duration, int inputFormat = 7)
    {
        long start = r.BaseStream.Position;
        var d = new ReplayVisualData();
        int yes = r.ReadByte();
        if (yes > 1)
            throw new InvalidDataException("Invalid visual flag");
        d.Recorded = yes == 1;
        d.Dropped = Size(r, int.MaxValue);
        int n = Size(r, MaxAssets);
        var assets = new HashSet<string>(StringComparer.Ordinal);
        for (int i = 0; i < n; i++)
        {
            string key = ReplayMotionData.Text(r, 1024);
            if (!assets.Add(key) || key.Length == 0)
                throw new InvalidDataException("Duplicate visual asset");
            d.Assets.Add(key);
        }
        n = Size(r, MaxOwners);
        var owners = new HashSet<string>();
        for (int i = 0; i < n; i++)
        {
            var o = new ReplayVisualOwner { Group = r.ReadInt32(), Id = r.ReadInt32(), Kind = r.ReadInt32(),
                                            Root = ReplayMotionData.Text(r, 1024) };
            if (o.Group < 0 || o.Group > 3 || o.Id < 0 || o.Kind < 0 || o.Kind > 4 ||
                !owners.Add(o.Group + ":" + o.Id))
                throw new InvalidDataException("Invalid visual owner");
            d.Owners.Add(o);
            int tracks = Size(r, MaxTracks - d.TrackCount);
            var paths = new HashSet<string>();
            for (int j = 0; j < tracks; j++)
            {
                var t = new ReplayVisualTrack { Kind = r.ReadInt32(), Slot = r.ReadInt32(),
                                                Path = ReplayMotionData.Text(r, 1024) };
                if (t.Kind < 1 || t.Kind > 4 || t.Slot < 0 || t.Slot > 31 ||
                    !paths.Add(t.Kind + ":" + t.Slot + ":" + t.Path))
                    throw new InvalidDataException("Invalid visual track");
                d.TrackCount++;
                o.Tracks.Add(t);
                int frames = Size(r, (inputFormat >= 7 ? MaxSamples : 393216) - d.Count);
                double previous = -1;
                for (int k = 0; k < frames; k++)
                {
                    var f = new ReplayVisualFrame { Seconds = r.ReadDouble() };
                    if (double.IsNaN(f.Seconds) || double.IsInfinity(f.Seconds) || f.Seconds < 0 ||
                        f.Seconds < previous || f.Seconds > duration + .000001)
                        throw new InvalidDataException("Invalid visual time");
                    previous = f.Seconds;
                    if (t.Kind == 1)
                    {
                        f.Position = V(r);
                        f.Rotation = Q(r);
                        f.Scale = V(r);
                    }
                    else
                    {
                        f.Flags = r.ReadUInt32();
                        f.Color = C(r);
                        if ((f.Flags & ~31u) != 0)
                            throw new InvalidDataException("Invalid visual state");
                        if (t.Kind == 4)
                        {
                            f.Position = V(r);
                            f.Rotation = Q(r);
                            f.Intensity = N(r);
                            f.Range = N(r);
                            f.Angle = N(r, 179);
                            f.InnerAngle = N(r, 179);
                            if (f.Intensity < 0 || f.Range < 0 || f.Angle < 0 || f.InnerAngle < 0)
                                throw new InvalidDataException("Invalid visual light");
                        }
                        else
                        {
                            f.Asset = Size(r, MaxAssets);
                            f.Texture = Size(r, MaxAssets);
                            if (f.Asset > d.Assets.Count || f.Texture > d.Assets.Count)
                                throw new InvalidDataException("Missing visual asset reference");
                        }
                    }
                    t.Frames.Add(f);
                    d.Count++;
                    if (r.BaseStream.Position - start > (inputFormat >= 7 ? MaxWire : 24 * 1024 * 1024))
                        throw new InvalidDataException("Visual recording exceeds byte limit");
                }
            }
        }
        if (r.BaseStream.Position - start > (inputFormat >= 7 ? MaxWire : 24 * 1024 * 1024))
            throw new InvalidDataException("Visual recording exceeds byte limit");
        if (!d.Recorded && (d.Count != 0 || d.Owners.Count != 0))
            throw new InvalidDataException("Unexpected visual observations");
        return d;
    }
}
