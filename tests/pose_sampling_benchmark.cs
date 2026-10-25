using System;
using System.Diagnostics;
using System.IO;
using UnityEngine;

// Compile with adaptive_capture_tests.cs for value-only Unity contracts, and
// select this entry point. Hashes cover serialized poses and sampling rates.
public static class PoseSamplingBenchmark
{
    public static ulong Run(int profile, int repeats)
    {
        var input = new ReplayVisualFrame[2401];
        for (int n = 0; n < input.Length; n++)
        {
            double t = n / 120.0, motion = profile == 0 ? 0 : profile == 1 ? t : Math.Sin(t * 11);
            double angle = profile < 2 ? .19 : motion * .7;
            input[n] = new ReplayVisualFrame {
                Seconds = t, Position = new Vector3((float)motion, 0, 0), Scale = new Vector3(1, 1, 1),
                Rotation = new Quaternion(0, (float)Math.Sin(angle), 0, (float)Math.Cos(angle))
            };
            if (profile == 3 && n % 137 < 40)
                input[n].Position = new Vector3(10, 0, 0);
        }
        ulong hash = 14695981039346656037UL;
        int frames = 0;
        var clock = Stopwatch.StartNew();
        for (int repeat = 0; repeat < repeats; repeat++)
        {
            var data = new ReplayVisualData();
            var track = data.Track(data.Owner(1, 1, 1, ""), 1, "");
            var sampler = new AdaptivePoseSampler(data, track, new ReplayCoverage());
            for (int n = 0; n < input.Length; n++)
            {
                sampler.Push(input[n], n == input.Length - 1 || profile == 3 && n % 137 == 0,
                             profile == 3 && n % 71 == 0);
                hash = unchecked(
                    hash * 31 +
                    (sampler.Interval(input[n].Seconds) == AdaptivePoseSampler.FastInterval ? 1UL : 0UL));
            }
            frames = track.Frames.Count;
            clock.Stop();
            using (var memory = new MemoryStream())
            {
                using (var writer = new BinaryWriter(memory, System.Text.Encoding.UTF8, true))
                    data.Write(writer);
                foreach (byte b in memory.ToArray())
                    hash = unchecked((hash ^ b) * 1099511628211UL);
            }
            clock.Start();
        }
        clock.Stop();
        Console.WriteLine("profile={0}, samples={1}, seconds={2:F6}, frames/track={3}, hash={4}", profile,
                          input.Length * repeats, clock.Elapsed.TotalSeconds, frames, hash);
        return hash;
    }

    public static void Main(string[] args)
    {
        int repeats = args.Length > 0 ? int.Parse(args[0]) : 200;
        for (int profile = 0; profile < 4; profile++)
            Run(profile, repeats);
        Console.WriteLine("Value-only pose sampler benchmark; excludes Unity Transform reads and rendering.");
    }
}
