using System;

// Frame-time control only. A missing IPC ACK already pauses production; it is
// not evidence of a slow route worker and must not shrink the batch forever.
public sealed class SearchLoadController
{
    private readonly bool automatic;
    private double reference, windowStart, framesSum, workSum;
    private int frames, slowFrames, floorWindows;
    private bool calibrated;
    public double Milliseconds { get; private set; }

    public SearchLoadController(bool auto, double maximum, double initialFrame, double now)
    {
        automatic = auto;
        reference = Clamp(Finite(initialFrame) ? initialFrame : 16.667, 2, 50);
        windowStart = now;
        Milliseconds = auto ? 1 : Clamp(Finite(maximum) ? maximum : 2, .25, 8);
    }

    private static bool Finite(double value)
    {
        return !double.IsNaN(value) && !double.IsInfinity(value);
    }

    private static double Clamp(double value, double low, double high)
    {
        return Math.Max(low, Math.Min(high, value));
    }

    public void Observe(double frameMs, double previousWorkMs, bool ready, double now)
    {
        if (!automatic || !Finite(frameMs) || !Finite(now) || frameMs <= 0)
            return;
        if (frameMs > 100)
        {
            Milliseconds = .25;
            calibrated = false;
            floorWindows = 0;
            Reset(now);
            return;
        }
        frames++;
        framesSum += frameMs;
        workSum += Clamp(Finite(previousWorkMs) ? previousWorkMs : 0, 0, 100);
        if (frameMs > reference * 1.15 + .25)
            slowFrames++;
        if (now - windowStart < .5 || frames < 15)
            return;
        double mean = framesSum / frames, work = workSum / frames;
        if (!calibrated)
        {
            reference = Clamp(mean, 2, 50);
            calibrated = true;
        }
        bool slow = slowFrames > frames / 4 || mean > reference * 1.10 + .25;
        if (slow)
            Milliseconds *= .75;
        else if (mean <= reference * 1.05 + .20)
            Milliseconds += .25;
        Milliseconds = Clamp(Milliseconds, .25, Math.Max(.25, Math.Min(8, reference * .20)));
        // Rebase a sustained external pacing change even if it is below 30%.
        // Negligible actual work at the floor cannot explain such a slowdown.
        if (Milliseconds <= .251 && work <= .30 && slow)
        {
            if (++floorWindows >= 3)
            {
                reference = Clamp(mean - work, 2, 50);
                floorWindows = 0;
            }
        }
        else
            floorWindows = 0;
        if (mean < reference * .95)
            reference = Clamp(mean, 2, 50);
        Reset(now);
    }

    private void Reset(double now)
    {
        windowStart = now;
        frames = slowFrames = 0;
        framesSum = workSum = 0;
    }
}
