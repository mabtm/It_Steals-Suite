using System;

// The selected policy is frozen when recording begins. Statistics and integrity
// reports are independent; this controls only durable replay geometry/telemetry.
public struct ReplayRetention
{
    public double Seconds;
    public uint Crystals;

    public ReplayRetention(double seconds, uint crystals)
    {
        Seconds =
            double.IsNaN(seconds) || double.IsInfinity(seconds) ? 10 : Math.Max(0, Math.Min(600, seconds));
        Crystals = Math.Min(1000u, crystals);
    }

    public bool Accept(double duration, int collected)
    {
        return !double.IsNaN(duration) && !double.IsInfinity(duration) && duration >= 0 &&
               (Crystals > 0 ? collected >= Crystals : duration > Seconds);
    }
}
