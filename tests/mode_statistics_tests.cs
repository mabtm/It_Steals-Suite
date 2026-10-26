using System;
using System.Globalization;
using System.IO;
using System.Text;

public static class ModeStatisticsTests
{
    private static void Check(bool ok, string label)
    {
        if (!ok)
            throw new Exception(label);
    }

    public static int Main()
    {
        var stats = new ModeStatistics();
        foreach (int scene in ModeStatistics.Scenes)
            foreach (bool practice in new[] { false, true })
            {
                int slot = ModeStatistics.Row(scene, practice ? 2 : 1);
                stats.Begin(scene, practice);
                stats.End(scene, practice, RunOutcome.Reset, 3.999, true);
                Check(stats.Rows[slot].Attempts == 0 && stats.Rows[slot].Resets == 0, "ignored reset");
                stats.Begin(scene, practice);
                stats.End(scene, practice, RunOutcome.Completed, 10, false);
                stats.Begin(scene, practice);
                stats.End(scene, practice, RunOutcome.Death, 6, false);
                stats.Begin(scene, practice);
                stats.End(scene, practice, RunOutcome.Interrupted, 5, false);
                var row = stats.Rows[slot];
                Check(row.Attempts == 3 && row.Finished == 1 && row.Resets == 1 && row.Deaths == 1 &&
                          row.FinishSeconds == 10,
                      "independent mode/category accounting");
            }
        Check(ModeStatistics.Row(-1, 0) == -1 && ModeStatistics.Row(3, 3) == -1, "catalog limits");
        stats.Begin(-1, false);
        stats.End(-1, true, RunOutcome.Completed, 1, false);
        Check(!stats.Observe(3, 0, true, 1), "first observation");
        Check(stats.Observe(3, 0, true, 1.25), "normal play counted");
        Check(!stats.Observe(3, 0, false, 1.5), "pause starts without uncertain interval");
        Check(!stats.Observe(3, 0, true, 2), "resume without paused interval");
        Check(stats.Observe(3, 0, true, 2.25), "resumed play");
        Check(!stats.Observe(3, 1, true, 2.5), "category transition");
        Check(stats.Observe(3, 1, true, 2.75), "clean play counted");
        Check(!stats.Observe(13, 1, true, 3), "scene transition");
        Check(!stats.Observe(13, 1, true, 20), "loading/observation gap");
        Check(stats.Observe(13, 1, true, 20.25), "new scene resumed");
        Check(!stats.Observe(13, 1, true, double.NaN), "invalid clock");
        Check(stats.Rows[0].PlaySeconds == .5 && stats.Rows[1].PlaySeconds == .25 &&
                  stats.Rows[22].PlaySeconds == .25,
              "exact credited intervals");
        stats.BestUtc[7] = DateTime.UtcNow.Ticks;
        CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("de-DE");
        var encoded = new StringBuilder();
        stats.Write(encoded);
        var loaded = new ModeStatistics();
        foreach (var line in encoded.ToString().Split('\n'))
        {
            int at = line.IndexOf('=');
            if (at > 0)
                Check(loaded.Read(line.Substring(0, at), line.Substring(at + 1)), "read known field");
        }
        Check(loaded.SinceUtc == stats.SinceUtc && loaded.BestUtc[7] == stats.BestUtc[7] &&
                  loaded.Rows[0].PlaySeconds == .5 && loaded.Rows[23].Finished == 1,
              "locale independent roundtrip");
        Check(!loaded.Read("unrelated", "1"), "other stats fields retained by owner");
        foreach (string value in new[] { "-1|0|0|0|0|0", "1|2|0|0|0|0", "1|0|0|1|0|0", "1|0|0|0|NaN|0",
                                         "1|0|0|0|0|Infinity", "1|0|0|0|0|-1", "1|0" })
        {
            bool rejected = false;
            try
            {
                loaded.Read("mode.0", value);
            }
            catch (IOException)
            {
                rejected = true;
            }
            Check(rejected, "corrupt counters rejected");
        }
        int revision = loaded.Revision;
        long pb = loaded.BestUtc[7];
        loaded.Clear();
        Check(loaded.Revision > revision && loaded.Rows[0].PlaySeconds == 0 &&
                  loaded.Rows[23].Attempts == 0 && loaded.BestUtc[7] == pb,
              "reset preserves PB dates");
        Console.WriteLine(
            "Eight modes, Clean/Practice counters, normal play, pause/focus/loading intervals, persistence, corruption and reset: PASS");
        return 0;
    }
}
