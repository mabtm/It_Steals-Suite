using System;
using System.Diagnostics;
using System.Threading;
using System.Runtime.InteropServices;

public sealed class ExternalToolSnapshot
{
    public int Status;
    public string Tool = "";
    public long AtUtc;
}

// Local observations only. Process names cannot reveal renamed/injected
// memory editors and never prove that a detected tool changed this game.
public sealed class ExternalToolMonitor : IDisposable
{
    private readonly ManualResetEvent stop = new ManualResetEvent(false);
    private readonly Thread worker;
    private ExternalToolSnapshot snapshot = new ExternalToolSnapshot();
    public ExternalToolSnapshot Snapshot
    {
        get {
            return Volatile.Read(ref snapshot);
        }
    }

    public static bool SuspiciousName(string name)
    {
        name = (name ?? "").ToLowerInvariant().Replace("-", "").Replace("_", "").Replace(" ", "");
        return name.StartsWith("cheatengine", StringComparison.Ordinal) || name == "x64dbg" ||
               name == "x32dbg" || name == "ollydbg" ||
               name.StartsWith("artmoney", StringComparison.Ordinal) || name == "wemod" ||
               name == "reclass" || name == "reclass.net" || name == "reclassnet" || name == "squalr";
    }

    [DllImport("kernel32.dll")]
    [return:MarshalAs(UnmanagedType.Bool)]
    private static extern bool IsDebuggerPresent();
    [DllImport("kernel32.dll")]
    private static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll")]
    [return:MarshalAs(UnmanagedType.Bool)]
    private static extern bool CheckRemoteDebuggerPresent(IntPtr process,
                                                          [MarshalAs(UnmanagedType.Bool)] out bool present);

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    private struct ProcessEntry
    {
        public uint Size, Usage, Pid;
        public UIntPtr Heap;
        public uint Module, Threads, Parent;
        public int Priority;
        public uint Flags;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
        public string File;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern IntPtr CreateToolhelp32Snapshot(uint flags, uint pid);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, EntryPoint = "Process32FirstW",
               ExactSpelling = true, SetLastError = true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    private static extern bool ProcessFirst(IntPtr snapshot, ref ProcessEntry entry);
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, EntryPoint = "Process32NextW", ExactSpelling = true,
               SetLastError = true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    private static extern bool ProcessNext(IntPtr snapshot, ref ProcessEntry entry);
    [DllImport("kernel32.dll")]
    [return:MarshalAs(UnmanagedType.Bool)]
    private static extern bool CloseHandle(IntPtr handle);

    private static string FindEditor()
    {
        if (Environment.OSVersion.Platform == PlatformID.Win32NT)
        {
            // Snapshot names without opening protected processes. Requesting
            // ProcessName through handles can fail on unrelated system tasks.
            var handle = CreateToolhelp32Snapshot(2, 0);
            if (handle == new IntPtr(-1))
                throw new InvalidOperationException("Process snapshot unavailable");
            try
            {
                var entry = new ProcessEntry { Size = (uint)Marshal.SizeOf(typeof(ProcessEntry)) };
                if (!ProcessFirst(handle, ref entry))
                    throw new InvalidOperationException("Process enumeration unavailable");
                do
                {
                    string name = System.IO.Path.GetFileNameWithoutExtension(entry.File);
                    if (SuspiciousName(name))
                        return name;
                } while (ProcessNext(handle, ref entry));
                if (Marshal.GetLastWin32Error() != 18)
                    throw new InvalidOperationException("Process enumeration interrupted");
                return null;
            }
            finally
            {
                CloseHandle(handle);
            }
        }
        var processes = Process.GetProcesses();
        try
        {
            foreach (var process in processes)
            {
                try
                {
                    if (SuspiciousName(process.ProcessName))
                        return process.ProcessName;
                }
                catch (InvalidOperationException)
                {
                } // Process exited while enumerating.
            }
            return null;
        }
        finally
        {
            foreach (var process in processes)
                process.Dispose();
        }
    }

    public ExternalToolMonitor()
    {
        worker = new Thread(Scan) { IsBackground = true, Name = "ItSteals.LocalIntegrity" };
        worker.Start();
    }

    private void Scan()
    {
        do
        {
            var next = new ExternalToolSnapshot { Status = 1, AtUtc = DateTime.UtcNow.Ticks };
            try
            {
                string editor = FindEditor();
                if (editor != null)
                {
                    next.Status = 2;
                    next.Tool = editor;
                }
                bool native = false;
                if (Environment.OSVersion.Platform == PlatformID.Win32NT)
                {
                    if (!CheckRemoteDebuggerPresent(GetCurrentProcess(), out native))
                        throw new InvalidOperationException("Debugger check unavailable");
                    native |= IsDebuggerPresent();
                }
                if (native || Debugger.IsAttached)
                {
                    next.Status = 2;
                    next.Tool = "Debugger attached to game";
                }
            }
            catch (Exception error)
            {
                next.Status = 3;
                next.Tool = error.GetType().Name;
            }
            Volatile.Write(ref snapshot, next);
        } while (!stop.WaitOne(1000));
    }

    public void Dispose()
    {
        stop.Set();
        if (worker.Join(2000))
            stop.Dispose();
    }
}

public static class RunValidity
{
    public const RunWarnings InvalidWarnings =
        RunWarnings.ObservationGap | RunWarnings.StateUnavailable | RunWarnings.SeedCoverageLost |
        RunWarnings.ProcessStopped | RunWarnings.InvalidTiming | RunWarnings.ModeInterrupted |
        RunWarnings.ExternalTool | RunWarnings.IntegrityUnavailable | RunWarnings.TimeScaleChanged |
        RunWarnings.PlayerStateChanged | RunWarnings.GamePatchesChanged | RunWarnings.OverlayMissing |
        RunWarnings.ForbiddenCommand | RunWarnings.SelectionChanged;

    public static bool Eligible(RunRecord record)
    {
        return record != null && record.ValidationChecked && record.ObservedFromStart && record.SeedKnown &&
               SeedMatchesCategory(record) && DisallowedAids(record) == RunAids.None &&
               (record.Warnings & ~RunWarnings.EventLogTruncated) == 0;
    }

    public static RunAids DisallowedAids(RunRecord record)
    {
        // Only the explicitly selected category permits deterministic RNG.
        // No other aid is exempt, including Practice itself.
        RunAids aids = record.Aids | record.InitialAids;
        return record.Category == RunCategory.SetSeed ? aids & ~RunAids.FixedSeed : aids;
    }

    public static bool SeedMatchesCategory(RunRecord record)
    {
        return record.Category == RunCategory.RandomSeed
                   ? record.SeedType == SeedKind.Random
                   : record.Category == RunCategory.SetSeed && record.SeedType == SeedKind.Set &&
                     record.Seed == record.SelectedSeed &&
                     record.InitialAids == RunAids.FixedSeed &&
                     (record.Aids & RunAids.FixedSeed) != 0;
    }

    public static string Reason(RunRecord record)
    {
        if (record == null)
            return "No observed run";
        if (DisallowedAids(record) != RunAids.None)
            return "Trainer aids: " + DisallowedAids(record);
        if ((record.Warnings & ~RunWarnings.EventLogTruncated) != 0)
            return (record.Warnings & ~RunWarnings.EventLogTruncated).ToString();
        if (!record.ObservedFromStart)
            return "Run start not observed";
        if (!record.SeedKnown || !SeedMatchesCategory(record))
            return record.Category == RunCategory.SetSeed ? "Selected Set-Seed recipe not verified from start"
                                                         : "Random seed not verified";
        if (!record.ValidationChecked)
            return "Local validation not completed (pending or recorded by an older version)";
        return "Local checks passed; external memory changes are not fully detectable";
    }
}
