using System;

// Pure, allocation-free evaluation after binding. Extra cameras (notably the
// first-person arm camera) must use the same absolute replay time as the main
// camera and light, including fractional frames and non-monotonic export seeks.
public sealed class ReplayCameraSampling
{
    public struct Pose
    {
        public double Seconds;
        public float X, Y, Z, Qx, Qy, Qz, Qw, Sx, Sy, Sz, Fov;
        public bool Enabled, Orthographic;
    }

    private readonly Pose[] poses;
    public ReplayCameraSampling(ReplaySceneTrack track)
    {
        poses = new Pose[track.Frames.Count];
        for (int i = 0; i < poses.Length; i++)
        {
            var f = track.Frames[i];
            var b = f.Values;
            if (b == null || b.Length != 90)
                throw new ArgumentException("Invalid recorded camera payload");
            poses[i] = new Pose { Seconds = f.Seconds, Enabled = b[0] != 0,
                X = BitConverter.ToSingle(b, 1), Y = BitConverter.ToSingle(b, 5),
                Z = BitConverter.ToSingle(b, 9), Qx = BitConverter.ToSingle(b, 13),
                Qy = BitConverter.ToSingle(b, 17), Qz = BitConverter.ToSingle(b, 21),
                Qw = BitConverter.ToSingle(b, 25), Sx = BitConverter.ToSingle(b, 29),
                Sy = BitConverter.ToSingle(b, 33), Sz = BitConverter.ToSingle(b, 37),
                Fov = BitConverter.ToSingle(b, 41), Orthographic = b[73] != 0 };
        }
    }

    private static float L(float a, float b, double t) { return (float)(a + (b - a) * t); }

    public Pose At(int index, double seconds)
    {
        if (index < 0 || index >= poses.Length)
            throw new ArgumentOutOfRangeException("index");
        var a = poses[index];
        if (index + 1 == poses.Length || double.IsNaN(seconds) || double.IsInfinity(seconds))
            return a;
        var b = poses[index + 1];
        double dt = b.Seconds - a.Seconds, dx = b.X - a.X, dy = b.Y - a.Y, dz = b.Z - a.Z;
        double limit = Math.Max(4, dt * 40);
        if (dt <= 0 || dt > .25 || a.Enabled != b.Enabled || a.Orthographic != b.Orthographic ||
            dx * dx + dy * dy + dz * dz > limit * limit)
            return a;
        double t = Math.Max(0, Math.Min(1, (seconds - a.Seconds) / dt));
        double dot = a.Qx * (double)b.Qx + a.Qy * (double)b.Qy + a.Qz * (double)b.Qz + a.Qw * (double)b.Qw;
        if (dot < 0) { dot = -dot; b.Qx = -b.Qx; b.Qy = -b.Qy; b.Qz = -b.Qz; b.Qw = -b.Qw; }
        dot = Math.Min(1, dot);
        double u = 1 - t, v = t;
        if (dot < .9995)
        {
            double angle = Math.Acos(dot), sin = Math.Sin(angle);
            u = Math.Sin((1 - t) * angle) / sin; v = Math.Sin(t * angle) / sin;
        }
        double x = a.Qx * u + b.Qx * v, y = a.Qy * u + b.Qy * v,
               z = a.Qz * u + b.Qz * v, w = a.Qw * u + b.Qw * v;
        double norm = Math.Sqrt(x * x + y * y + z * z + w * w);
        if (norm > 0)
        { a.Qx = (float)(x / norm); a.Qy = (float)(y / norm); a.Qz = (float)(z / norm); a.Qw = (float)(w / norm); }
        a.X = L(a.X, b.X, t); a.Y = L(a.Y, b.Y, t); a.Z = L(a.Z, b.Z, t);
        a.Sx = L(a.Sx, b.Sx, t); a.Sy = L(a.Sy, b.Sy, t); a.Sz = L(a.Sz, b.Sz, t);
        a.Fov = L(a.Fov, b.Fov, t);
        return a;
    }
}
