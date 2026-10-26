using System;
using System.Globalization;
using System.IO;
using System.Text;

public enum RunOutcome : byte
{
    Running,
    Completed,
    Reset,
    Death,
    Interrupted,
    Abandoned
}

public sealed class ModeTotals
{
    public int Attempts, Finished, Resets, Deaths;
    public double FinishSeconds, PlaySeconds;
}

// Fixed, bounded lifetime counters. Categories are Normal, Clean and Practice;
// normal play contributes time, while the existing run tracker owns attempts.
public sealed class ModeStatistics
{
    public static readonly int[] Scenes = { 3, 4, 5, 7, 8, 14, 12, 13 };
    public readonly ModeTotals[] Rows = new ModeTotals[24];
    public readonly long[] BestUtc = new long[8];
    public long SinceUtc = DateTime.UtcNow.Ticks;
    public int Revision;
    private double previousAt = double.NaN;
    private int previousRow = -1;
    private bool previousPlaying;
    private static readonly CultureInfo culture = CultureInfo.InvariantCulture;

    public ModeStatistics()
    {
        Clear();
    }

    public static int Row(int scene, int category)
    {
        int mode = Array.IndexOf(Scenes, scene);
        return mode < 0 || category < 0 || category > 2 ? -1 : mode * 3 + category;
    }

    public void Clear()
    {
        for (int i = 0; i < Rows.Length; i++)
            Rows[i] = new ModeTotals();
        SinceUtc = DateTime.UtcNow.Ticks;
        previousPlaying = false;
        previousAt = double.NaN;
        Revision++;
    }

    private static void Increment(ref int count)
    {
        if (count < int.MaxValue)
            count++;
    }

    public void Begin(int scene, bool practice)
    {
        int row = Row(scene, practice ? 2 : 1);
        if (row < 0)
            return;
        Increment(ref Rows[row].Attempts);
        Revision++;
    }

    public void End(int scene, bool practice, RunOutcome outcome, double seconds, bool ignored)
    {
        int row = Row(scene, practice ? 2 : 1);
        if (row < 0)
            return;
        var value = Rows[row];
        if (ignored)
            value.Attempts = Math.Max(0, value.Attempts - 1);
        else if (outcome == RunOutcome.Completed)
        {
            Increment(ref value.Finished);
            value.FinishSeconds += seconds;
        }
        if (!ignored && (outcome == RunOutcome.Reset || outcome == RunOutcome.Death))
            Increment(ref value.Resets);
        if (outcome == RunOutcome.Death)
            Increment(ref value.Deaths);
        Revision++;
    }

    // Credit only an observed interval within the same playable mode/category.
    // Loading, focus/menu transitions, paused replay and long stalls add no time.
    public bool Observe(int scene, int category, bool playing, double now)
    {
        int row = Row(scene, category);
        double delta = now - previousAt;
        bool count = playing && previousPlaying && row >= 0 && row == previousRow && delta > 0 && delta <= .5;
        if (count)
            Rows[row].PlaySeconds += delta;
        previousAt = now;
        previousRow = row;
        previousPlaying = playing;
        return count;
    }

    public void Write(StringBuilder text)
    {
        text.Append("modeSince=").Append(SinceUtc).Append('\n');
        for (int i = 0; i < Rows.Length; i++)
        {
            var r = Rows[i];
            text.Append("mode.")
                .Append(i)
                .Append('=')
                .Append(r.Attempts)
                .Append('|')
                .Append(r.Finished)
                .Append('|')
                .Append(r.Resets)
                .Append('|')
                .Append(r.Deaths)
                .Append('|')
                .Append(r.FinishSeconds.ToString("R", culture))
                .Append('|')
                .Append(r.PlaySeconds.ToString("R", culture))
                .Append('\n');
        }
        for (int i = 0; i < BestUtc.Length; i++)
            if (BestUtc[i] > 0)
                text.Append("pbAt.").Append(i).Append('=').Append(BestUtc[i]).Append('\n');
    }

    public bool Read(string key, string value)
    {
        long ticks;
        if (key == "modeSince")
        {
            if (!long.TryParse(value, out ticks) || ticks <= 0 || ticks > DateTime.MaxValue.Ticks)
                throw new IOException("Invalid mode statistics start date.");
            SinceUtc = ticks;
            return true;
        }
        int index;
        if (key.StartsWith("pbAt.", StringComparison.Ordinal))
        {
            if (!int.TryParse(key.Substring(5), out index) || index < 0 || index >= BestUtc.Length ||
                !long.TryParse(value, out ticks) || ticks < 0 || ticks > DateTime.MaxValue.Ticks)
                throw new IOException("Invalid personal-best date.");
            BestUtc[index] = ticks;
            return true;
        }
        if (!key.StartsWith("mode.", StringComparison.Ordinal))
            return false;
        if (!int.TryParse(key.Substring(5), out index) || index < 0 || index >= Rows.Length)
            throw new IOException("Invalid mode statistics index.");
        var fields = value.Split('|');
        var row = new ModeTotals();
        if (fields.Length != 6 || !int.TryParse(fields[0], out row.Attempts) || row.Attempts < 0 ||
            !int.TryParse(fields[1], out row.Finished) || row.Finished < 0 || row.Finished > row.Attempts ||
            !int.TryParse(fields[2], out row.Resets) || row.Resets < 0 || row.Resets > row.Attempts ||
            !int.TryParse(fields[3], out row.Deaths) || row.Deaths < 0 || row.Deaths > row.Resets ||
            !double.TryParse(fields[4], NumberStyles.Float, culture, out row.FinishSeconds) ||
            !double.TryParse(fields[5], NumberStyles.Float, culture, out row.PlaySeconds) ||
            row.FinishSeconds < 0 || double.IsNaN(row.FinishSeconds) ||
            double.IsInfinity(row.FinishSeconds) || row.PlaySeconds < 0 || double.IsNaN(row.PlaySeconds) ||
            double.IsInfinity(row.PlaySeconds))
            throw new IOException("Invalid mode statistics values.");
        Rows[index] = row;
        return true;
    }
}
