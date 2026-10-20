using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

// Collider observations, independent of input metadata. Box matrices retain
// the actual transform; spheres/capsules retain Unity's world radius/height
// rules. Shape dimensions are sampled too (crouch/trigger changes are real).
public struct ReplayColliderShape
{
    public Vector3 Center, Size;
    public float Radius, Height;
    public int Direction;
    public Matrix4x4 Matrix;

    public static int Kind(Collider c)
    {
        return c is BoxCollider           ? 1
               : c is SphereCollider      ? 2
               : c is CapsuleCollider     ? 3
               : c is CharacterController ? 4
                                          : 0;
    }

    public static bool Capture(Collider c, out ReplayColliderShape s)
    {
        s = new ReplayColliderShape();
        if (c == null)
            return false;
        var t = c.transform;
        var box = c as BoxCollider;
        if (box != null)
        {
            s.Center = box.center;
            s.Size = box.size;
            s.Matrix = t.localToWorldMatrix;
            return true;
        }
        Vector3 scale = t.lossyScale;
        scale = new Vector3(Math.Abs(scale.x), Math.Abs(scale.y), Math.Abs(scale.z));
        var sphere = c as SphereCollider;
        if (sphere != null)
        {
            s.Center = t.TransformPoint(sphere.center);
            s.Radius = sphere.radius * Math.Max(scale.x, Math.Max(scale.y, scale.z));
            s.Matrix = Matrix4x4.identity;
            return true;
        }
        var capsule = c as CapsuleCollider;
        var controller = c as CharacterController;
        if (capsule == null && controller == null)
            return false;
        s.Direction = capsule != null ? capsule.direction : 1;
        float radius = capsule != null ? capsule.radius : controller.radius,
              height = capsule != null ? capsule.height : controller.height;
        s.Center = t.TransformPoint(capsule != null ? capsule.center : controller.center);
        float along = s.Direction == 0 ? scale.x : s.Direction == 2 ? scale.z : scale.y;
        float across = s.Direction == 0   ? Math.Max(scale.y, scale.z)
                       : s.Direction == 2 ? Math.Max(scale.x, scale.y)
                                          : Math.Max(scale.x, scale.z);
        s.Radius = radius * across;
        s.Height = Math.Max(height * along, 2 * s.Radius);
        s.Matrix = Matrix4x4.Rotate(t.rotation);
        return true;
    }

    public bool Same(ReplayColliderShape other)
    {
        if (Center != other.Center || Size != other.Size || Radius != other.Radius ||
            Height != other.Height || Direction != other.Direction)
            return false;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++)
                if (Matrix[i, j] != other.Matrix[i, j])
                    return false;
        return true;
    }

    private static void V(BinaryWriter w, Vector3 v)
    {
        w.Write(v.x);
        w.Write(v.y);
        w.Write(v.z);
    }

    private static float F(BinaryReader r)
    {
        float f = r.ReadSingle();
        if (float.IsNaN(f) || float.IsInfinity(f) || Math.Abs(f) > 100000)
            throw new InvalidDataException("Invalid collider coordinate");
        return f;
    }

    private static Vector3 V(BinaryReader r)
    {
        return new Vector3(F(r), F(r), F(r));
    }

    public void Write(BinaryWriter w)
    {
        V(w, Center);
        V(w, Size);
        w.Write(Radius);
        w.Write(Height);
        w.Write(Direction);
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++)
                w.Write(Matrix[i, j]);
    }

    public static ReplayColliderShape Read(BinaryReader r)
    {
        var s = new ReplayColliderShape { Center = V(r), Size = V(r), Radius = F(r), Height = F(r),
                                          Direction = r.ReadInt32() };
        s.Matrix.m33 = 1;
        if (s.Size.x < 0 || s.Size.y < 0 || s.Size.z < 0 || s.Radius < 0 || s.Height < 0 || s.Direction < 0 ||
            s.Direction > 2)
            throw new InvalidDataException("Invalid collider dimensions");
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++)
                s.Matrix[i, j] = F(r);
        return s;
    }
}

public struct ReplayColliderFrame
{
    public double Seconds;
    public uint State;
    public ReplayColliderShape Shape;
}

public sealed class ReplayColliderTrack
{
    public int Group, Owner, Kind;
    public string Name = "";
    public readonly List<ReplayColliderFrame> Samples = new List<ReplayColliderFrame>(32);

    public bool At(double seconds, out ReplayColliderFrame sample)
    {
        int lo = 0, hi = Samples.Count;
        while (lo < hi)
        {
            int m = lo + (hi - lo) / 2;
            if (Samples[m].Seconds <= seconds)
                lo = m + 1;
            else
                hi = m;
        }
        sample = lo == 0 ? new ReplayColliderFrame() : Samples[lo - 1];
        // Hold observations, never invent smooth/exact physics between them.
        return lo > 0 && (sample.State & 1) != 0 && seconds - sample.Seconds <= .6;
    }
}

public sealed class ReplayColliderData
{
    public const int MaxTracks = 256, MaxSamples = 262144;
    public uint Flags;
    public int Dropped, Count;
    public bool Recorded;
    public readonly List<ReplayColliderTrack> Tracks = new List<ReplayColliderTrack>();

    public void Drop(uint flag)
    {
        Flags |= flag;
        if (Dropped < int.MaxValue)
            Dropped++;
    }

    public void Add(ReplayColliderTrack track, ReplayColliderFrame sample)
    {
        if (Count >= MaxSamples)
        {
            Drop(1);
            return;
        }
        track.Samples.Add(sample);
        Count++;
    }

    public long EstimatedBytes
    {
        get {
            return Count * 112L + Tracks.Count * 256L + 20;
        }
    }

    public void Write(BinaryWriter w)
    {
        w.Write(Recorded);
        w.Write(Flags);
        w.Write(Dropped);
        w.Write(Count);
        w.Write(Tracks.Count);
        foreach (var t in Tracks)
        {
            w.Write(t.Group);
            w.Write(t.Owner);
            w.Write(t.Kind);
            ReplayMotionData.Text(w, t.Name);
            w.Write(t.Samples.Count);
            foreach (var s in t.Samples)
            {
                w.Write(s.Seconds);
                w.Write(s.State);
                s.Shape.Write(w);
            }
        }
    }

    public static ReplayColliderData Read(BinaryReader r, double duration)
    {
        byte recorded = r.ReadByte();
        var d = new ReplayColliderData { Recorded = recorded == 1, Flags = r.ReadUInt32(),
                                         Dropped = r.ReadInt32(), Count = r.ReadInt32() };
        int n = r.ReadInt32();
        if (recorded > 1 || (d.Flags & ~7u) != 0 || d.Dropped < 0 || d.Count < 0 || d.Count > MaxSamples ||
            n < 0 || n > MaxTracks || (!d.Recorded && (d.Count != 0 || n != 0)))
            throw new InvalidDataException("Invalid collider header");
        if (r.BaseStream.Length - r.BaseStream.Position < (long)d.Count * 96 + n * 20L + 8)
            throw new InvalidDataException("Truncated collider recording");
        int total = 0;
        for (int i = 0; i < n; i++)
        {
            var t = new ReplayColliderTrack { Group = r.ReadInt32(), Owner = r.ReadInt32(),
                                              Kind = r.ReadInt32(), Name = ReplayMotionData.Text(r, 128) };
            int count = r.ReadInt32();
            if (t.Group < 1 || t.Group > 3 || t.Owner < 0 || (t.Group == 1 && t.Owner != 0) || t.Kind < 1 ||
                t.Kind > 4 || count < 1 || count > d.Count - total)
                throw new InvalidDataException("Invalid collider track");
            double previous = 0;
            for (int j = 0; j < count; j++)
            {
                var s = new ReplayColliderFrame { Seconds = r.ReadDouble(), State = r.ReadUInt32() };
                if (double.IsNaN(s.Seconds) || double.IsInfinity(s.Seconds) || s.Seconds < previous ||
                    s.Seconds < 0 || s.Seconds > duration + .000001 || (s.State & ~3u) != 0)
                    throw new InvalidDataException("Invalid collider timeline");
                previous = s.Seconds;
                s.Shape = ReplayColliderShape.Read(r);
                t.Samples.Add(s);
            }
            total += count;
            d.Tracks.Add(t);
        }
        if (total != d.Count)
            throw new InvalidDataException("Collider sample count mismatch");
        return d;
    }
}
