using System;
using System.Diagnostics;
using System.Globalization;
using System.Text;

// Fixed-size measurements shared by every machine. No quality or rate setting
// depends on CPU, GPU, measured load or a benchmark result.
public sealed class CaptureProfiler
{
    public const int Collider = 0, Audio = 1, Scene = 2, Effects = 3, Poses = 4, View = 5, Total = 6;
    private static readonly string[] names = { "colliders", "audio_sync", "scene_cameras_ui", "effects", "visual_poses", "view_frame", "capture_total" };
    private readonly FrameCostMeter[] windows = new FrameCostMeter[7];
    private readonly double[] sum = new double[7], max = new double[7];
    private readonly long[] count = new long[7];
    public CaptureProfiler() { for (int i = 0; i < windows.Length; i++) windows[i] = new FrameCostMeter(); }
    public static long Now { get { return Stopwatch.GetTimestamp(); } }
    public void Elapsed(int channel, long start) { Observe(channel, (Now - start) * 1000.0 / Stopwatch.Frequency); }
    public void Observe(int channel, double ms)
    {
        if (channel < 0 || channel >= windows.Length || double.IsNaN(ms) || double.IsInfinity(ms) || ms < 0) return;
        windows[channel].Observe(ms); sum[channel] += ms; count[channel]++; max[channel] = Math.Max(max[channel], ms);
    }
    public string Encode()
    {
        var text = new StringBuilder("{\"version\":1,\"unit\":\"ms\",\"p95_window\":128,\"stages\":{");
        for (int i = 0; i < windows.Length; i++)
        {
            float mean, p95; windows[i].Snapshot(double.MaxValue, out mean, out p95);
            if (i > 0) text.Append(',');
            text.Append('"').Append(names[i]).Append("\":{\"count\":").Append(count[i]);
            text.Append(",\"mean\":").Append((count[i] == 0 ? 0 : sum[i] / count[i]).ToString("R", CultureInfo.InvariantCulture));
            text.Append(",\"p95\":").Append(p95.ToString("R", CultureInfo.InvariantCulture));
            text.Append(",\"max\":").Append(max[i].ToString("R", CultureInfo.InvariantCulture)).Append('}');
        }
        return text.Append("}}\n").ToString();
    }
}
