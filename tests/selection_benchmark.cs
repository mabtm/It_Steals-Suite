using System;
using System.Diagnostics;

public static class SelectionBenchmark
{
    public static void Main()
    {
        const int count = 10000;
        foreach (int points in new[] { 32, 64, 128, 256, 512, 1024 })
        {
            var random = new Random(123);
            Func<int, int, int> range = (min, max) => random.Next(min, max);
            var scratch = new SpawnSelectionGenerator(8, points, 14);
            for (int i = 0; i < 100; i++)
            {
                SpawnSelection.Select(8, points, 14, -1, range);
                scratch.Next(-1, range);
            }
            var clock = Stopwatch.StartNew();
            for (int i = 0; i < count; i++)
                SpawnSelection.Select(8, points, 14, -1, range);
            clock.Stop();
            double stock = clock.Elapsed.TotalMilliseconds;
            clock.Restart();
            for (int i = 0; i < count; i++)
                scratch.Next(-1, range);
            clock.Stop();
            double optimized = clock.Elapsed.TotalMilliseconds;
            Console.WriteLine("points={0}, stock={1:F1} ms, reusable={2:F1} ms ({3:F2}x)", points, stock,
                              optimized, stock / optimized);
        }
        Console.WriteLine("Selection-only host comparison using System.Random; Unity RNG is not executed.");
    }
}
