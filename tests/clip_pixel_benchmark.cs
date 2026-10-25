using System;
using System.Diagnostics;

public static class ClipPixelBenchmark
{
    public static void Main(string[] args)
    {
        const int width = 1920, height = 1080;
        int count = args.Length == 0 ? 300 : int.Parse(args[0]);
        var source = new byte[width * height * 4];
        var output = new byte[source.Length];
        new Random(715).NextBytes(source);
        foreach (bool bottomUp in new[] { false, true })
            foreach (bool bgra in new[] { true, false })
            {
                ClipPixelLayout.TopDownBgra(source, output, width, height, bottomUp, bgra);
                var watch = Stopwatch.StartNew();
                for (int i = 0; i < count; i++)
                    ClipPixelLayout.TopDownBgra(source, output, width, height, bottomUp, bgra);
                watch.Stop();
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width * 4; x++)
                    {
                        int channel = !bgra && x % 4 == 0 ? x + 2 : !bgra && x % 4 == 2 ? x - 2 : x;
                        int at = (bottomUp ? height - 1 - y : y) * width * 4 + channel;
                        if (output[y * width * 4 + x] != source[at])
                            throw new Exception("Pixel conversion changed");
                    }
                Console.WriteLine("{0},{1},{2:F3},{3}", bottomUp, bgra, watch.Elapsed.TotalMilliseconds,
                                  count);
            }
        Console.WriteLine(
            "1080p CPU pixel layout only; every output byte checked. No GPU or game FPS measurement.");
    }
}
