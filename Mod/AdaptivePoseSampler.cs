using System;
using UnityEngine;

// Bounded online simplification. Every sampled pose between two stored knots
// is checked against the same shortest-path interpolation used by the viewer.
// Constant poses need no repeated file samples; motion/transition detail can
// rise from 60 to 120 probes/s without writing every intermediate pose.
public sealed class AdaptivePoseSampler
{
    public const float PositionError = .00075f, ScaleError = .0001f, RotationErrorDegrees = .15f;
    public const double MaxSpan = .1, NormalInterval = 1.0 / 60, FastInterval = 1.0 / 120;
    private static readonly double RotationDot = Math.Cos(RotationErrorDegrees * Math.PI / 360);
    private readonly ReplayVisualData data;
    private readonly ReplayVisualTrack track;
    private readonly ReplayCoverage coverage;
    private readonly ReplayVisualFrame[] window = new ReplayVisualFrame[16];
    private int count;
    private ReplayVisualFrame anchor, previous;
    private bool seen;
    private double fastUntil;

    public double Interval(double at)
    {
        return at < fastUntil ? FastInterval : NormalInterval;
    }

    public AdaptivePoseSampler(ReplayVisualData source, ReplayVisualTrack target, ReplayCoverage health)
    {
        data = source;
        track = target;
        coverage = health;
    }

    private static float L(float a, float b, double u)
    {
        return (float)(a + (b - a) * u);
    }

    private static Vector3 V(Vector3 a, Vector3 b, double u)
    {
        return new Vector3(L(a.x, b.x, u), L(a.y, b.y, u), L(a.z, b.z, u));
    }

    private static double Dot(Quaternion a, Quaternion b)
    {
        return a.x * (double)b.x + a.y * (double)b.y + a.z * (double)b.z + a.w * (double)b.w;
    }

    private static double Cosine(Quaternion a, Quaternion b)
    {
        double norm = Dot(a, a) * Dot(b, b);
        return norm > 0 ? Math.Min(1, Math.Abs(Dot(a, b)) / Math.Sqrt(norm)) : 0;
    }

    public static Quaternion Rotation(Quaternion a, Quaternion b, double u)
    {
        double dot = Dot(a, b);
        if (dot < 0)
        {
            dot = -dot;
            b = new Quaternion(-b.x, -b.y, -b.z, -b.w);
        }
        dot = Math.Min(1, dot);
        double x = 1 - u, y = u;
        if (dot < .9995)
        {
            double angle = Math.Acos(dot), sin = Math.Sin(angle);
            x = Math.Sin((1 - u) * angle) / sin;
            y = Math.Sin(u * angle) / sin;
        }
        var q = new Quaternion((float)(a.x * x + b.x * y), (float)(a.y * x + b.y * y),
                               (float)(a.z * x + b.z * y), (float)(a.w * x + b.w * y));
        double norm = Math.Sqrt(Dot(q, q));
        return norm > 0 ? new Quaternion((float)(q.x / norm), (float)(q.y / norm), (float)(q.z / norm),
                                         (float)(q.w / norm))
                        : a;
    }

    private static double Distance2(Vector3 a, Vector3 b)
    {
        double x = a.x - b.x, y = a.y - b.y, z = a.z - b.z;
        return x * x + y * y + z * z;
    }

    private static bool Same(ReplayVisualFrame a, ReplayVisualFrame b)
    {
        return Distance2(a.Position, b.Position) < 1e-10 && Distance2(a.Scale, b.Scale) < 1e-10 &&
               Cosine(a.Rotation, b.Rotation) > .9999999;
    }

    private static bool Identical(ReplayVisualFrame a, ReplayVisualFrame b)
    {
        return a.Position.x == b.Position.x && a.Position.y == b.Position.y && a.Position.z == b.Position.z &&
               a.Scale.x == b.Scale.x && a.Scale.y == b.Scale.y && a.Scale.z == b.Scale.z &&
               a.Rotation.x == b.Rotation.x && a.Rotation.y == b.Rotation.y && a.Rotation.z == b.Rotation.z &&
               a.Rotation.w == b.Rotation.w;
    }

    private static bool Finite(float n)
    {
        return !float.IsNaN(n) && !float.IsInfinity(n) && Math.Abs(n) <= 100000;
    }

    private static bool Finite(Vector3 v)
    {
        return Finite(v.x) && Finite(v.y) && Finite(v.z);
    }

    private static bool Valid(ReplayVisualFrame f)
    {
        double norm = Dot(f.Rotation, f.Rotation);
        return Finite(f.Position) && Finite(f.Scale) && norm >= .98 && norm <= 1.02 &&
               !double.IsNaN(f.Seconds) && !double.IsInfinity(f.Seconds) && f.Seconds >= 0;
    }

    private bool Emit(ReplayVisualFrame f)
    {
        int before = track.Frames.Count;
        data.Add(track, f);
        if (track.Frames.Count == before)
        {
            coverage.Fail(ReplayCoverage.Visuals);
            return false;
        }
        anchor = f;
        return true;
    }

    private void Flush()
    {
        if (count > 0)
        {
            Emit(window[count - 1]);
            count = 0;
        }
    }

    private bool Fits(ReplayVisualFrame end)
    {
        double dt = end.Seconds - anchor.Seconds;
        if (dt <= 0)
            return false;
        for (int i = 0; i < count; i++)
        {
            var f = window[i];
            double u = (f.Seconds - anchor.Seconds) / dt;
            if (Distance2(f.Position, V(anchor.Position, end.Position, u)) > PositionError * PositionError ||
                Distance2(f.Scale, V(anchor.Scale, end.Scale, u)) > ScaleError * ScaleError ||
                Cosine(f.Rotation, Rotation(anchor.Rotation, end.Rotation, u)) < RotationDot)
                return false;
        }
        return true;
    }

    public void Push(ReplayVisualFrame f, bool boundary = false, bool detail = false)
    {
        if (!Valid(f) || seen && f.Seconds < previous.Seconds)
        {
            data.Drop();
            coverage.Fail(ReplayCoverage.Visuals);
            return;
        }
        if (!seen)
        {
            seen = true;
            Emit(f);
            previous = f;
            return;
        }
        if (f.Seconds == previous.Seconds)
        {
            if (count > 0)
            {
                window[count - 1] = f;
                if (boundary)
                    Flush();
            }
            else if (track.Frames.Count > 0 && anchor.Seconds == f.Seconds)
            {
                track.Frames[track.Frames.Count - 1] = f;
                anchor = f;
            }
            else if (boundary)
                Emit(f);
            previous = f;
            return;
        }
        double dt = f.Seconds - previous.Seconds;
        // Most scene transforms are stationary. Keep validation, transition
        // boundaries and detail scheduling, without computing angular speed.
        if (!boundary && dt <= .25 && count == 0 && Identical(anchor, f) && Identical(previous, f))
        {
            if (detail)
                fastUntil = f.Seconds + .2;
            previous = f;
            return;
        }
        double distance = Math.Sqrt(Distance2(previous.Position, f.Position));
        double turn = 2 * Math.Acos(Cosine(previous.Rotation, f.Rotation)) * 180 / Math.PI;
        bool cut = boundary || dt > .25 || distance > Math.Max(4, dt * 40);
        if (detail || cut || distance / dt > 4 || turn / dt > 120 ||
            Math.Sqrt(Distance2(previous.Scale, f.Scale)) / dt > .2)
            fastUntil = f.Seconds + .2;
        if (cut)
        {
            Flush();
            if (anchor.Seconds < previous.Seconds && !Same(previous, f))
                Emit(previous);
            Emit(f);
            previous = f;
            return;
        }
        if (count == 0 && Same(anchor, f))
        {
            previous = f;
            return;
        }
        // Preserve the end of a long stationary hold before movement starts.
        if (count == 0 && Same(anchor, previous) && previous.Seconds > anchor.Seconds)
            Emit(previous);
        if (count > 0 && (count == window.Length || f.Seconds - anchor.Seconds > MaxSpan || !Fits(f)))
            Flush();
        window[count++] = f;
        previous = f;
    }

    public void Finish(ReplayVisualFrame end)
    {
        Push(end, true);
    }

    public static double Next(double due, double at, double interval)
    {
        return due <= 0 || at - due > interval * 4 ? at + interval : due + interval;
    }
}
