using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Security.Cryptography;
using System.Text;
using System.Threading;

[Flags] public enum RunWarnings : uint
{
    None = 0, ObservationGap = 1, StateUnavailable = 2, SeedCoverageLost = 4,
    ProcessStopped = 8, InvalidTiming = 16, ModeInterrupted = 32, EventLogTruncated = 64,
    ExternalTool=128,IntegrityUnavailable=256,TimeScaleChanged=512,PlayerStateChanged=1024,GamePatchesChanged=2048,OverlayMissing=4096,ForbiddenCommand=8192
}

public sealed class RunEnvironment
{
    public string ToolVersion = "7.11.4", GameVersion = "UNKNOWN", UnityVersion = "UNKNOWN", GameHash = "UNKNOWN";
    public RunEnvironment Copy() { return (RunEnvironment)MemberwiseClone(); }
}
public sealed class RunEvent
{
    public double Seconds;
    public string Kind, Detail;
}

// File identity is inspected once, off Unity's frame thread. This identifies
// the file on disk, not the contents of the running process or an attestation.
public sealed class GameBuildInfo
{
    public const string SupportedHash = "ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8";
    private volatile string hash = "UNKNOWN";
    private readonly RunEnvironment environment;
    public string Hash { get { return hash; } }
    public string GameVersion { get { return environment.GameVersion; } }
    public string UnityVersion { get { return environment.UnityVersion; } }
    public bool Supported { get { return hash == SupportedHash; } }
    public GameBuildInfo(string assemblyPath, string gameVersion, string unityVersion)
    {
        environment = new RunEnvironment { GameVersion = Limit(gameVersion), UnityVersion = Limit(unityVersion) };
        new Thread(() =>
        {
            try
            {
                using (var stream = new FileStream(assemblyPath, FileMode.Open, FileAccess.Read, FileShare.Read))
                using (var sha = SHA256.Create()) hash = BitConverter.ToString(sha.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
            }
            catch { hash = "UNKNOWN"; }
        }) { IsBackground = true, Name = "ItSteals.GameIdentity" }.Start();
    }
    private static string Limit(string text) { return string.IsNullOrEmpty(text) ? "UNKNOWN" : text.Substring(0, Math.Min(64, text.Length)); }
    public RunEnvironment Snapshot() { var result = environment.Copy(); result.GameHash = hash; return result; }
}

public static class VerificationReport
{
    private static readonly CultureInfo culture = CultureInfo.InvariantCulture;
    public static string Integrity(RunRecord r) { return RunValidity.Eligible(r) ? "VALID (LOCAL)" : "UNVALID"; }
    public static string Level(int scene)
    {
        switch (scene) { case 3: return "Classic"; case 4: return "Camera"; case 5: return "Hide & Seek";
            case 7: return "Timer"; case 8: return "Ghost"; case 14: return "Classic Bonus"; case 12: return "Camera Bright"; case 13: return "Hide & Seek Outside"; default: return "Scene " + scene; }
    }
    public static string Quote(string text)
    {
        var b = new StringBuilder("\"");
        foreach (char c in text ?? "")
        {
            if (c == '"' || c == '\\') b.Append('\\').Append(c);
            else if (c < 32 || char.IsSurrogate(c)) b.Append("\\u").Append(((int)c).ToString("x4", culture));
            else b.Append(c);
        }
        return b.Append('"').ToString();
    }
    private static string Utc(long ticks) { return ticks > 0 && ticks <= DateTime.MaxValue.Ticks ? Quote(new DateTime(ticks, DateTimeKind.Utc).ToString("o", culture)) : "null"; }
    private static string Number(double value) { return double.IsNaN(value) || double.IsInfinity(value) || value < 0 ? "null" : value.ToString("R", culture); }
    public static string Encode(RunRecord r)
    {
        var e = r.Environment ?? new RunEnvironment();
        var b = new StringBuilder(2048);
        b.Append("{\n  \"reportFormat\": 1,\n  \"scope\": \"Local observations; editable report, not anti-cheat or leaderboard approval\",\n")
            .Append("  \"runId\": ").Append(r.Id).Append(",\n  \"runNumber\": ").Append(r.Number)
            .Append(",\n  \"level\": ").Append(Quote(Level(r.Scene))).Append(",\n  \"scene\": ").Append(r.Scene)
            .Append(",\n  \"modeAtStart\": ").Append(Quote((r.InitialAids & RunAids.Practice) != 0 ? "PRACTICE" : "SPEEDRUN"))
            .Append(",\n  \"toolVersion\": ").Append(Quote(e.ToolVersion)).Append(",\n  \"gameReportedVersion\": ").Append(Quote(e.GameVersion))
            .Append(",\n  \"unityVersion\": ").Append(Quote(e.UnityVersion)).Append(",\n  \"gameAssemblyFileSha256\": ").Append(Quote(e.GameHash))
            .Append(",\n  \"startedUtc\": ").Append(Utc(r.StartedUtc)).Append(",\n  \"endedUtc\": ").Append(Utc(r.EndedUtc))
            .Append(",\n  \"observedSeconds\": ").Append(Number(r.Seconds)).Append(",\n  \"outcome\": ").Append(Quote(r.Outcome.ToString()))
            .Append(",\n  \"seedType\": ").Append(Quote(r.SeedType.ToString().ToUpperInvariant()))
            .Append(",\n  \"seed\": ").Append(r.SeedKnown ? r.Seed.ToString(culture) : "null")
            .Append(",\n  \"seedMeaning\": ").Append(Quote(r.SeedType == SeedKind.Set ? "Helper practice base seed" : "First observed RNG seed; not a replayable world seed"))
            .Append(",\n  \"reseedsObserved\": ").Append(r.Reseeds).Append(",\n  \"trainerAidsUsed\": ").Append(Quote(r.Aids.ToString()))
            .Append(",\n  \"warningFlags\": ").Append(Quote(r.Warnings.ToString())).Append(",\n  \"integrity\": ").Append(Quote(Integrity(r)))
            .Append(",\n  \"validationChecked\": ").Append(r.ValidationChecked?"true":"false")
            .Append(",\n  \"validationReason\": ").Append(Quote(RunValidity.Reason(r)))
            .Append(",\n  \"localPersonalBest\": ").Append(r.PersonalBest ? "true" : "false")
            .Append(",\n  \"observedFromStart\": ").Append(r.ObservedFromStart ? "true" : "false")
            .Append(",\n  \"progress\": ").Append(Number(r.Progress)).Append(",\n  \"events\": [");
        for (int i = 0; i < r.Events.Count; i++)
        {
            RunEvent v = r.Events[i]; if (i != 0) b.Append(',');
            b.Append("\n    {\"seconds\": ").Append(Number(v.Seconds)).Append(", \"kind\": ").Append(Quote(v.Kind))
                .Append(", \"detail\": ").Append(Quote(v.Detail)).Append('}');
        }
        return b.Append("\n  ]\n}\n").ToString();
    }
    public static string PackEvents(List<RunEvent> events)
    {
        var b = new StringBuilder();
        foreach (RunEvent e in events) b.Append(Number(e.Seconds)).Append('|').Append(e.Kind.Replace("|", "").Replace("\n", ""))
            .Append('|').Append(Convert.ToBase64String(Encoding.UTF8.GetBytes(e.Detail ?? ""))).Append('\n');
        return Convert.ToBase64String(Encoding.UTF8.GetBytes(b.ToString()));
    }
    public static List<RunEvent> UnpackEvents(string text)
    {
        var events = new List<RunEvent>();
        if (text.Length > 65536) return events;
        foreach (string line in Encoding.UTF8.GetString(Convert.FromBase64String(text)).Split('\n'))
        {
            string[] a = line.Split('|'); double seconds;
            if (a.Length != 3 || a[1].Length > 64 || !double.TryParse(a[0], NumberStyles.Float, culture, out seconds) ||
                double.IsNaN(seconds) || double.IsInfinity(seconds) || seconds < 0 || events.Count >= 128) continue;
            string detail = Encoding.UTF8.GetString(Convert.FromBase64String(a[2]));
            if (detail.Length > 256) continue;
            events.Add(new RunEvent { Seconds = seconds, Kind = a[1], Detail = detail });
        }
        return events;
    }
}
