using System;
using System.Globalization;
public static class CaptureProfilerTests
{
    public static void Main()
    {
        CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("de-DE");
        var p = new CaptureProfiler();
        for (int i = 1; i <= 256; i++) p.Observe(CaptureProfiler.Total, i);
        p.Observe(CaptureProfiler.Total, double.NaN); p.Observe(0, -1); p.Observe(999, 1);
        string json = p.Encode();
        if (!json.Contains("\"capture_total\":{\"count\":256,\"mean\":128.5,\"p95\":249,\"max\":256}") ||
            !json.Contains("\"colliders\":{\"count\":0,")) throw new Exception(json);
        var fresh = new CaptureProfiler(); fresh.Observe(CaptureProfiler.Total, 1);
        if (!fresh.Encode().Contains("\"capture_total\":{\"count\":1,\"mean\":1,")) throw new Exception("runs independent");
        Console.WriteLine("Fixed-memory stage CPU profiling, full-run means/maxima, bounded p95 window, invariant locale and independent runs: PASS");
    }
}
