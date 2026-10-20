using System;
using System.IO;
using UnityEngine;

public sealed class MovementSummary
{
    public int Scene = -1, Revision, Rejected;
    public float Tick, Radius, Height;
    public readonly int[] Counts = new int[5];
    public readonly float[] Speeds = new float[15], Lengths = new float[5];
    public bool Ready
    {
        get {
            return Counts[0] >= 24;
        }
    }

    public bool Matches(PracticeMapData data)
    {
        return data != null && Scene == data.Scene && data.MovementTick > 0 &&
               Math.Abs(Tick - data.MovementTick) < .000001 && Math.Abs(Radius - data.AgentRadius) < .0001 &&
               Math.Abs(Height - data.AgentHeight) < .0001;
    }

    public static void Write(BinaryWriter w, MovementSummary s)
    {
        w.Write(s != null);
        if (s == null)
            return;
        w.Write(s.Scene);
        w.Write(s.Revision);
        w.Write(s.Rejected);
        w.Write(s.Tick);
        w.Write(s.Radius);
        w.Write(s.Height);
        for (int k = 0; k < 5; k++)
        {
            w.Write(s.Counts[k]);
            for (int q = 0; q < 3; q++)
                w.Write(s.Speeds[k * 3 + q]);
            w.Write(s.Lengths[k]);
        }
    }

    public static MovementSummary Read(BinaryReader r)
    {
        byte has = r.ReadByte();
        if (has > 1)
            throw new InvalidDataException("Invalid movement flag");
        if (has == 0)
            return null;
        var s =
            new MovementSummary { Scene = r.ReadInt32(), Revision = r.ReadInt32(), Rejected = r.ReadInt32(),
                                  Tick = r.ReadSingle(), Radius = r.ReadSingle(),  Height = r.ReadSingle() };
        if (Array.IndexOf(SpeedrunManager.LevelIndices, s.Scene) < 0 || s.Revision < 0 || s.Rejected < 0 ||
            !Finite(s.Tick) || s.Tick <= 0 || s.Tick > 1 || !Finite(s.Radius) || s.Radius <= 0 ||
            s.Radius > 5 || !Finite(s.Height) || s.Height <= 0 || s.Height > 10)
            throw new InvalidDataException("Invalid movement identity");
        for (int k = 0; k < 5; k++)
        {
            s.Counts[k] = r.ReadInt32();
            if (s.Counts[k] < 0 || s.Counts[k] > 20000)
                throw new InvalidDataException("Invalid movement count");
            for (int q = 0; q < 3; q++)
            {
                float v = r.ReadSingle();
                if (!Finite(v) || v < 0 || v > 25.6f || q > 0 && v < s.Speeds[k * 3 + q - 1])
                    throw new InvalidDataException("Invalid movement speed");
                s.Speeds[k * 3 + q] = v;
            }
            s.Lengths[k] = r.ReadSingle();
            if (!Finite(s.Lengths[k]) || s.Lengths[k] < 0 || s.Lengths[k] > 5 ||
                s.Counts[k] > 0 && (s.Speeds[k * 3] <= 0 || s.Lengths[k] <= 0) ||
                s.Counts[k] == 0 && (s.Speeds[k * 3 + 2] != 0 || s.Lengths[k] != 0))
                throw new InvalidDataException("Invalid movement window");
        }
        return s;
    }

    private static bool Finite(float v)
    {
        return !float.IsNaN(v) && !float.IsInfinity(v);
    }
}

// Bounded online statistics independent of replay opening, seed and location.
public sealed class MovementModel
{
    public const int Classes = 5, Bins = 1024, Maximum = 20000;
    private const double BinsPerUnit = 40; // 0.025 u/s bins; 20 KiB/model.
    public readonly int Scene;
    public readonly float Tick, Radius, Height;
    private readonly int[][] bins = new int [Classes][];
    private readonly int[] counts = new int[Classes];
    private readonly double[] lengths = new double[Classes];
    private int revision, rejected;

    public MovementModel(int scene, float tick, float radius, float height)
    {
        Scene = scene;
        Tick = tick;
        Radius = radius;
        Height = height;
        for (int k = 0; k < Classes; k++)
            bins[k] = new int[Bins];
    }

    public bool Matches(float tick, float radius, float height)
    {
        return Math.Abs(Tick - tick) < .000001 && Math.Abs(Radius - radius) < .0001 &&
               Math.Abs(Height - height) < .0001;
    }

    public void Reject()
    {
        if (rejected < int.MaxValue)
            rejected++;
        if (revision < int.MaxValue)
            revision++;
    }

    public void Add(int kind, double speed, double distance)
    {
        if (kind < 0 || kind >= Classes || double.IsNaN(speed) || double.IsInfinity(speed) || speed <= 0 ||
            speed > 25.6 || double.IsNaN(distance) || double.IsInfinity(distance) || distance <= 0 ||
            distance > 5)
        {
            Reject();
            return;
        }
        if (counts[kind] >= Maximum)
        {
            int old = counts[kind], n = 0;
            for (int b = 0; b < Bins; b++)
            {
                bins[kind][b] /= 2;
                n += bins[kind][b];
            }
            counts[kind] = n;
            lengths[kind] *= n / (double)old;
        }
        int at = Math.Max(0, Math.Min(Bins - 1, (int)Math.Floor(speed * BinsPerUnit)));
        bins[kind][at]++;
        counts[kind]++;
        lengths[kind] += distance;
        if (revision < int.MaxValue)
            revision++;
    }

    private float Quantile(int kind, double fraction)
    {
        if (counts[kind] == 0)
            return 0;
        int wanted = (int)Math.Floor((counts[kind] - 1) * fraction), n = 0;
        for (int b = 0; b < Bins; b++)
        {
            n += bins[kind][b];
            if (n > wanted)
                return (float)((b + .5) / BinsPerUnit);
        }
        return (float)((Bins - .5) / BinsPerUnit);
    }

    public MovementSummary Summary()
    {
        var s = new MovementSummary { Scene = Scene,   Tick = Tick,         Radius = Radius,
                                      Height = Height, Revision = revision, Rejected = rejected };
        for (int k = 0; k < Classes; k++)
        {
            s.Counts[k] = counts[k];
            s.Speeds[k * 3] = Quantile(k, .25);
            s.Speeds[k * 3 + 1] = Quantile(k, .60);
            s.Speeds[k * 3 + 2] = Quantile(k, .85);
            s.Lengths[k] = counts[k] > 0 ? (float)(lengths[k] / counts[k]) : 0;
        }
        return s;
    }

    public void Write(BinaryWriter w)
    {
        w.Write(Scene);
        w.Write(Tick);
        w.Write(Radius);
        w.Write(Height);
        w.Write(revision);
        w.Write(rejected);
        for (int k = 0; k < Classes; k++)
        {
            w.Write(lengths[k]);
            for (int b = 0; b < Bins; b++)
                w.Write(bins[k][b]);
        }
    }

    public static MovementModel Read(BinaryReader r)
    {
        int scene = r.ReadInt32();
        float tick = r.ReadSingle(), radius = r.ReadSingle(), height = r.ReadSingle();
        if (Array.IndexOf(SpeedrunManager.LevelIndices, scene) < 0 || float.IsNaN(tick) ||
            float.IsInfinity(tick) || tick <= 0 || tick > 1 || float.IsNaN(radius) ||
            float.IsInfinity(radius) || radius <= 0 || radius > 5 || float.IsNaN(height) ||
            float.IsInfinity(height) || height <= 0 || height > 10)
            throw new InvalidDataException("Invalid saved movement identity");
        var m = new MovementModel(scene, tick, radius, height);
        m.revision = r.ReadInt32();
        m.rejected = r.ReadInt32();
        if (m.revision < 0 || m.rejected < 0)
            throw new InvalidDataException("Invalid movement revision");
        for (int k = 0; k < Classes; k++)
        {
            m.lengths[k] = r.ReadDouble();
            for (int b = 0; b < Bins; b++)
            {
                int n = r.ReadInt32();
                if (n < 0 || n > Maximum - m.counts[k])
                    throw new InvalidDataException("Invalid movement histogram");
                m.bins[k][b] = n;
                m.counts[k] += n;
            }
            if (double.IsNaN(m.lengths[k]) || double.IsInfinity(m.lengths[k]) || m.lengths[k] < 0 ||
                m.lengths[k] > m.counts[k] * 5 || m.counts[k] > 0 && m.lengths[k] <= 0)
                throw new InvalidDataException("Invalid movement distance");
        }
        return m;
    }

    public void Merge(MovementModel other)
    {
        if (Scene != other.Scene || !Matches(other.Tick, other.Radius, other.Height))
            return;
        for (int k = 0; k < Classes; k++)
        {
            for (int b = 0; b < Bins; b++)
                bins[k][b] += other.bins[k][b];
            counts[k] += other.counts[k];
            lengths[k] += other.lengths[k];
            while (counts[k] > Maximum)
            {
                int old = counts[k], n = 0;
                for (int b = 0; b < Bins; b++)
                {
                    bins[k][b] /= 2;
                    n += bins[k][b];
                }
                counts[k] = n;
                lengths[k] *= n / (double)old;
            }
        }
        revision = (int)Math.Min(int.MaxValue, (long)revision + other.revision + 1);
        rejected = (int)Math.Min(int.MaxValue, (long)rejected + other.rejected);
    }
}

public sealed class MovementWindow
{
    private double duration, distance, vertical, turn, axisSum;
    private int steps;
    private Vector3 prior;
    private bool stopped = true, starting = true;

    public void Reset()
    {
        duration = distance = vertical = turn = axisSum = 0;
        steps = 0;
        prior = Vector3.zero;
        stopped = starting = true;
    }

    public void Observe(MovementModel model, Vector3 delta, double dt, double input, bool eligible)
    {
        double horizontal = Math.Sqrt(delta.x * delta.x + delta.z * delta.z),
               length = Math.Sqrt(horizontal * horizontal + delta.y * delta.y), expected = .14 * input;
        if (!eligible || dt <= 0 || dt > .2 || double.IsNaN(dt) || double.IsInfinity(dt) ||
            double.IsNaN(input) || double.IsInfinity(input) || double.IsNaN(length) ||
            double.IsInfinity(length) || input < .15 || length / dt > 25 ||
            horizontal > expected * 1.15 + .002 || horizontal < expected * .65 || length < .00001)
        {
            if (eligible && input >= .15)
                model.Reject();
            Reset();
            return;
        }
        if (steps == 0)
            starting = stopped;
        double priorLength = Math.Sqrt(prior.x * prior.x + prior.z * prior.z);
        if (priorLength > .00001 && horizontal > .00001)
            turn += Math.Acos(Math.Max(
                -1, Math.Min(1, (prior.x * delta.x + prior.z * delta.z) / (priorLength * horizontal))));
        prior = delta;
        duration += dt;
        distance += length;
        vertical += Math.Abs(delta.y);
        axisSum += input;
        steps++;
        if (duration + .000001 < .12)
            return;
        int kind = starting ? 4 : vertical > distance * .15 ? 3 : turn > .7 ? 2 : turn > .18 ? 1 : 0;
        if (kind != 0 || axisSum / steps >= 1.30)
            model.Add(kind, distance / duration, distance);
        duration = distance = vertical = turn = axisSum = 0;
        steps = 0;
        stopped = starting = false;
    }
}
