using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

// Additional passive observations. Existing input/run identity and validation
// remain authoritative; gaps in optional channels are explicit.
public struct ReplayAudioGap
{
    public long First, Frames;
    public int Reason; // 1 start boundary, 2 interior, 3 finish boundary
}

public sealed class ReplayAudioInfo
{
    public bool Recorded;
    public int Rate, Channels, Flags, Gaps;
    public long Frames, Bytes;
    public ulong Checksum;
    // Loaded from the checksummed audio header; the input metadata stays compatible.
    public bool Diagnostics;
    public int ListenerOverruns, BypassOverruns, PeakListener, PeakBypass;
    public long MissingFrames;
    public readonly List<ReplayAudioGap> GapDetails = new List<ReplayAudioGap>();
    public double Duration
    {
        get {
            return Rate > 0? Frames / (double)Rate : 0;
        }
    }

    public void Write(BinaryWriter w)
    {
        w.Write(Recorded);
        w.Write(Rate);
        w.Write(Channels);
        w.Write(Flags);
        w.Write(Gaps);
        w.Write(Frames);
        w.Write(Bytes);
        w.Write(Checksum);
    }

    public static ReplayAudioInfo Read(BinaryReader r)
    {
        var i = new ReplayAudioInfo();
        byte yes = r.ReadByte();
        if (yes > 1)
            throw new InvalidDataException("Invalid audio flag");
        i.Recorded = yes != 0;
        i.Rate = r.ReadInt32();
        i.Channels = r.ReadInt32();
        i.Flags = r.ReadInt32();
        i.Gaps = r.ReadInt32();
        i.Frames = r.ReadInt64();
        i.Bytes = r.ReadInt64();
        i.Checksum = r.ReadUInt64();
        if ((i.Flags & ~31) != 0 || i.Gaps < 0 || i.Frames < 0 || i.Bytes < 0 ||
            i.Bytes > ReplayAudioFile.MaxBytes ||
            i.Recorded &&
                (i.Rate < 8000 || i.Rate > 192000 || i.Channels < 1 || i.Channels > 8 || i.Frames == 0 ||
                 i.Frames > (ReplayAudioFile.MaxBytes - ReplayAudioFile.HeaderBytes - 8) / (i.Channels * 4) ||
                 i.Bytes != ReplayAudioFile.HeaderBytes + i.Frames * i.Channels * 4 + 8) ||
            !i.Recorded && (i.Frames != 0 || i.Bytes != 0))
            throw new InvalidDataException("Invalid recorded audio metadata");
        return i;
    }
}

public struct ReplayProbe
{
    public double Seconds;
    public int Actor, Site, Mask;
    public uint Flags;
    public Vector3 Origin, End;
    public float Limit;
    // bit 0 actual hit/block, bit 1 linecast, bit 2 display end capped.
}

public struct ReplayObservation
{
    public double Seconds;
    public int Actor, Kind, Name;
    public float Value;
    public Vector3 Position;
    // Kind: 1 actual bool field transition, 2 original AudioSource play,
    // 3 AudioSource stop/pause. Names preserve exact game provenance.
}

public struct ReplayAnimationFrame
{
    public double Seconds;
    public int State, Next, Clip;
    public uint Flags;
    public float Phase, NextPhase, Transition, Speed, Weight;
}

public sealed class ReplayAnimationTrack
{
    public int Group, Actor, Layer, Kind, Slot;
    public string Path = "", Name = "";
    // Kind 1 animator state/phase; Kind 2 observed blendshape weight.
    public readonly List<ReplayAnimationFrame> Frames = new List<ReplayAnimationFrame>();

    public int At(double t)
    {
        int lo = 0, hi = Frames.Count;
        while (lo < hi)
        {
            int mid = lo + (hi - lo) / 2;
            if (Frames[mid].Seconds <= t)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo - 1;
    }

    public bool Break(double a, double b)
    {
        if (Kind != 1)
            return false;
        int i = At(b);
        while (i >= 0 && Frames[i].Seconds > a)
        {
            if ((Frames[i].Flags & 2) != 0)
                return true;
            i--;
        }
        return false;
    }
}

public sealed class ReplayExperienceData
{
    public const int MaxProbes = 131072, MaxEvents = 65536, MaxTracks = 512, MaxAnimation = 65536,
                     MaxWire = 16 * 1024 * 1024;
    public bool Recorded;
    public int Dropped, AnimationCount;
    public ReplayAudioInfo Audio = new ReplayAudioInfo();
    public readonly List<string> Names = new List<string>();
    private readonly Dictionary<string, int> names = new Dictionary<string, int>(StringComparer.Ordinal);
    public readonly List<ReplayProbe> Probes = new List<ReplayProbe>();
    public readonly List<ReplayObservation> Events = new List<ReplayObservation>();
    public readonly List<ReplayAnimationTrack> Animations = new List<ReplayAnimationTrack>();
    public long EstimatedBytes
    {
        get {
            return Probes.Capacity * 56L + Events.Capacity * 36L + AnimationCount * 64L +
                   Animations.Count * 128L + Names.Count * 1100L;
        }
    }
    public Func<int, bool> ReserveBytes;
    public long WireBytes
    {
        get {
            return Wire;
        }
    }

    private bool Budget(int extra)
    {
        return ReserveBytes == null || ReserveBytes(extra);
    }

    private long Wire
    {
        get {
            return Probes.Count * 56L + Events.Count * 36L + AnimationCount * 48L + Animations.Count * 1600L +
                   Names.Count * 516L + 256;
        }
    }

    public void Drop()
    {
        if (Dropped < int.MaxValue)
            Dropped++;
    }

    public int Name(string value)
    {
        if (string.IsNullOrEmpty(value))
            return 0;
        int n;
        if (names.TryGetValue(value, out n))
            return n;
        if (Names.Count >= 2048 || !Budget(1100) || System.Text.Encoding.UTF8.GetByteCount(value) > 512)
        {
            Drop();
            return 0;
        }
        Names.Add(value);
        n = Names.Count;
        names.Add(value, n);
        return n;
    }

    public void Add(ReplayProbe p)
    {
        if (p.Site < 1 || p.Site > Names.Count)
        {
            Drop();
            return;
        }
        if (Probes.Count >= MaxProbes || Wire + 56 > MaxWire || !Budget(56))
        {
            Drop();
            return;
        }
        if (Probes.Count > 0 && p.Seconds < Probes[Probes.Count - 1].Seconds)
        {
            Drop();
            return;
        }
        Probes.Add(p);
    }

    public void Add(ReplayObservation e)
    {
        if (e.Name <= 0)
            return;
        if (e.Name < 1 || e.Name > Names.Count)
        {
            Drop();
            return;
        }
        if (Events.Count >= MaxEvents || Wire + 36 > MaxWire || !Budget(36))
        {
            Drop();
            return;
        }
        if (Events.Count > 0 && e.Seconds < Events[Events.Count - 1].Seconds)
        {
            Drop();
            return;
        }
        Events.Add(e);
    }

    public void Add(ReplayAnimationTrack t, ReplayAnimationFrame f)
    {
        if (AnimationCount >= MaxAnimation || Wire + 48 > MaxWire || !Budget(48))
        {
            Drop();
            return;
        }
        if (t.Frames.Count > 0 && f.Seconds < t.Frames[t.Frames.Count - 1].Seconds)
        {
            Drop();
            return;
        }
        t.Frames.Add(f);
        AnimationCount++;
    }

    private static void V(BinaryWriter w, Vector3 p)
    {
        w.Write(p.x);
        w.Write(p.y);
        w.Write(p.z);
    }

    public void Write(BinaryWriter w)
    {
        w.Write(Recorded);
        w.Write(Dropped);
        Audio.Write(w);
        w.Write(Names.Count);
        foreach (var n in Names)
            ReplayMotionData.Text(w, n);
        w.Write(Probes.Count);
        foreach (var p in Probes)
        {
            w.Write(p.Seconds);
            w.Write(p.Actor);
            w.Write(p.Site);
            w.Write(p.Mask);
            w.Write(p.Flags);
            V(w, p.Origin);
            V(w, p.End);
            w.Write(p.Limit);
        }
        w.Write(Events.Count);
        foreach (var e in Events)
        {
            w.Write(e.Seconds);
            w.Write(e.Actor);
            w.Write(e.Kind);
            w.Write(e.Name);
            w.Write(e.Value);
            V(w, e.Position);
        }
        w.Write(Animations.Count);
        foreach (var t in Animations)
        {
            w.Write(t.Group);
            w.Write(t.Actor);
            w.Write(t.Layer);
            w.Write(t.Kind);
            w.Write(t.Slot);
            ReplayMotionData.Text(w, t.Path);
            ReplayMotionData.Text(w, t.Name);
            w.Write(t.Frames.Count);
            foreach (var f in t.Frames)
            {
                w.Write(f.Seconds);
                w.Write(f.State);
                w.Write(f.Next);
                w.Write(f.Clip);
                w.Write(f.Flags);
                w.Write(f.Phase);
                w.Write(f.NextPhase);
                w.Write(f.Transition);
                w.Write(f.Speed);
                w.Write(f.Weight);
            }
        }
    }

    private static int Count(BinaryReader r, int max)
    {
        int n = r.ReadInt32();
        if (n < 0 || n > max)
            throw new InvalidDataException("Experience count exceeds limit");
        return n;
    }

    private static float F(BinaryReader r, float max = 100000)
    {
        float f = r.ReadSingle();
        if (float.IsNaN(f) || float.IsInfinity(f) || Math.Abs(f) > max)
            throw new InvalidDataException("Invalid experience number");
        return f;
    }

    private static Vector3 V(BinaryReader r)
    {
        return new Vector3(F(r), F(r), F(r));
    }

    private static double Time(BinaryReader r, double duration, ref double previous)
    {
        double t = r.ReadDouble();
        if (double.IsNaN(t) || double.IsInfinity(t) || t < 0 || t < previous || t > duration + .000001)
            throw new InvalidDataException("Invalid experience timestamp");
        previous = t;
        return t;
    }

    public static ReplayExperienceData Read(BinaryReader r, double duration)
    {
        long start = r.BaseStream.Position;
        var d = new ReplayExperienceData();
        byte yes = r.ReadByte();
        if (yes > 1)
            throw new InvalidDataException("Invalid experience flag");
        d.Recorded = yes != 0;
        d.Dropped = Count(r, int.MaxValue);
        d.Audio = ReplayAudioInfo.Read(r);
        if (d.Audio.Recorded && d.Audio.Duration > duration + .001)
            throw new InvalidDataException("Audio duration exceeds recording");
        int n = Count(r, 2048);
        var seen = new HashSet<string>(StringComparer.Ordinal);
        for (int i = 0; i < n; i++)
        {
            string s = ReplayMotionData.Text(r, 512);
            if (s.Length == 0 || !seen.Add(s))
                throw new InvalidDataException("Duplicate experience name");
            d.Name(s);
        }
        n = Count(r, MaxProbes);
        double previous = 0;
        for (int i = 0; i < n; i++)
        {
            var p = new ReplayProbe { Seconds = Time(r, duration, ref previous),
                                      Actor = Count(r, 128),
                                      Site = Count(r, d.Names.Count),
                                      Mask = r.ReadInt32(),
                                      Flags = r.ReadUInt32(),
                                      Origin = V(r),
                                      End = V(r),
                                      Limit = F(r) };
            if (p.Site == 0 || (p.Flags & ~15u) != 0 || p.Limit < 0 && p.Limit != -1)
                throw new InvalidDataException("Invalid observed query");
            d.Probes.Add(p);
        }
        n = Count(r, MaxEvents);
        previous = 0;
        for (int i = 0; i < n; i++)
        {
            var e = new ReplayObservation { Seconds = Time(r, duration, ref previous),
                                            Actor = Count(r, 128),
                                            Kind = Count(r, 4),
                                            Name = Count(r, d.Names.Count),
                                            Value = F(r),
                                            Position = V(r) };
            if (e.Kind < 1 || e.Name < 1 || (e.Kind == 1 || e.Kind == 4) && e.Value != 0 && e.Value != 1)
                throw new InvalidDataException("Invalid observed event");
            d.Events.Add(e);
        }
        n = Count(r, MaxTracks);
        var trackKeys = new HashSet<string>(StringComparer.Ordinal);
        for (int i = 0; i < n; i++)
        {
            var t = new ReplayAnimationTrack { Group = Count(r, 3),
                                               Actor = Count(r, 1000000),
                                               Layer = Count(r, 127),
                                               Kind = Count(r, 2),
                                               Slot = Count(r, 1023),
                                               Path = ReplayMotionData.Text(r, 1024),
                                               Name = ReplayMotionData.Text(r, 512) };
            if (t.Kind == 0 || !trackKeys.Add(t.Group + "|" + t.Actor + "|" + t.Kind + "|" + t.Layer + "|" +
                                              t.Slot + "|" + t.Path))
                throw new InvalidDataException("Invalid animation track");
            int count = Count(r, MaxAnimation - d.AnimationCount);
            previous = 0;
            for (int j = 0; j < count; j++)
            {
                var f = new ReplayAnimationFrame { Seconds = Time(r, duration, ref previous),
                                                   State = r.ReadInt32(),
                                                   Next = r.ReadInt32(),
                                                   Clip = Count(r, d.Names.Count),
                                                   Flags = r.ReadUInt32(),
                                                   Phase = F(r, 1e9f),
                                                   NextPhase = F(r, 1e9f),
                                                   Transition = F(r),
                                                   Speed = F(r),
                                                   Weight = F(r) };
                if ((f.Flags & ~3u) != 0 ||
                    (t.Kind == 1 && (f.Weight < 0 || f.Weight > 1.001f) ||
                     t.Kind == 2 && Math.Abs(f.Weight) > 10000) ||
                    f.Transition < 0 || f.Transition > 1)
                    throw new InvalidDataException("Invalid animation sample");
                t.Frames.Add(f);
            }
            d.AnimationCount += count;
            d.Animations.Add(t);
        }
        if (r.BaseStream.Position - start > MaxWire)
            throw new InvalidDataException("Experience data exceeds byte limit");
        if (!d.Recorded &&
            (d.Probes.Count != 0 || d.Events.Count != 0 || d.Animations.Count != 0 || d.Audio.Recorded))
            throw new InvalidDataException("Unexpected experience observations");
        return d;
    }
}
