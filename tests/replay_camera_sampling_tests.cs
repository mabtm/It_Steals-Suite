using System;
using System.IO;

public static class ReplayCameraSamplingTests
{
    static byte[] Camera(float x, double angle, bool enabled = true, bool ortho = false)
    {
        using (var s = new MemoryStream()) using (var w = new BinaryWriter(s))
        {
            w.Write(enabled); w.Write(x); w.Write(0f); w.Write(0f);
            w.Write(0f); w.Write((float)Math.Sin(angle / 2)); w.Write(0f); w.Write((float)Math.Cos(angle / 2));
            w.Write(1f); w.Write(1f); w.Write(1f); w.Write(60f);
            for (int i = 0; i < 5; i++) w.Write(1f);
            w.Write(-1); w.Write(0); w.Write(ortho);
            w.Write(0f); w.Write(0f); w.Write(1f); w.Write(1f);
            return s.ToArray();
        }
    }
    static void Check(bool ok, string text) { if (!ok) throw new Exception(text); }
    static ReplaySceneTrack Track(double end, bool enabled = true, bool ortho = false)
    {
        var t = new ReplaySceneTrack { Kind = ReplaySceneData.Camera };
        t.Frames.Add(new ReplaySceneFrame { Seconds = 0, Values = Camera(0, 0) });
        t.Frames.Add(new ReplaySceneFrame { Seconds = end, Values = Camera(1, Math.PI / 2, enabled, ortho) });
        return t;
    }
    public static void Main()
    {
        var t = Track(.1); var sampler = new ReplayCameraSampling(t);
        foreach (int fps in new[] { 30, 60, 120 })
            for (int i = 0; i <= fps / 10; i++)
            {
                double at = i / (double)fps;
                var p = sampler.At(t.At(at), at);
                double u = Math.Min(1, at / .1);
                Check(Math.Abs(p.X - u) < 1e-6, "camera/arm/light subframe alignment " + fps);
                Check(Math.Abs(p.Qy - Math.Sin(Math.PI * u / 4)) < 1e-6, "shortest quaternion interpolation");
            }
        var expected = sampler.At(0, .025);
        for (int i = 0; i < 1000; i++)
        {
            sampler.At(0, .075); sampler.At(1, .1);
            var again = sampler.At(0, .025);
            Check(again.X == expected.X && again.Qy == expected.Qy, "export seek/preview restore deterministic");
        }
        Check(new ReplayCameraSampling(Track(.5)).At(0, .25).X == 0, "never interpolate recording gaps");
        Check(new ReplayCameraSampling(Track(.1, false)).At(0, .05).X == 0, "visibility boundary held");
        Check(new ReplayCameraSampling(Track(.1, true, true)).At(0, .05).X == 0, "projection boundary held");
        Check(sampler.At(0, -1).X == 0 && sampler.At(0, 1).X == 1, "bounded evaluation");
        var recorded = new ReplaySceneData { Recorded = true };
        var held = recorded.Track(ReplaySceneData.Camera, "arms-camera");
        byte[] still = Camera(0, 0), moved = Camera(1, .1);
        recorded.Add(held, 0, still, still.Length);
        recorded.Add(held, .04, still, still.Length);
        recorded.Add(held, .08, still, still.Length);
        recorded.Add(held, .1, moved, moved.Length);
        Check(held.Frames.Count == 3 && held.Frames[1].Seconds == .08, "stationary camera endpoint retained");
        var sparse = new ReplayCameraSampling(held);
        Check(sparse.At(held.At(.06), .06).X == 0, "future camera motion never smeared into a stationary interval");
        Check(Math.Abs(sparse.At(held.At(.09), .09).X - .5) < 1e-6, "motion starts at last observed stationary frame");
        bool failed = false;
        t.Frames[0].Values = new byte[1];
        try { new ReplayCameraSampling(t); } catch (ArgumentException) { failed = true; }
        Check(failed, "corrupt camera payload rejected");
        Console.WriteLine("Subframe arm-camera/main-camera/light alignment 30/60/120; seek/restore; gaps and camera cuts: PASS");
    }
}
