using System;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Threading;
using System.Text;

// The mutex protects BOTH directions. Unity never waits for the overlay: a busy
// mutex skips this frame. Packed telemetry is copied only while holding it.
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct TrainerCommands
{
    public uint Serial, Heartbeat;
    public byte HitboxEnabled, BrightnessEnabled, HidePlayerHitbox, AccurateHitboxes;
    public byte SpeedrunModeActive, PracticeModeActive, SpeedrunLevelIndex, LevelLoadCmd;
    public int LevelLoadIndex, PracticeSeed;
    public byte SetSeedCmd, SpeedrunLoadLevelCmd, TeleportCmd, ResetMapCmd;
    public float TeleportX, TeleportY, TeleportZ;
    public byte SeededPractice;
    public byte TeleportExitFreecam, Padding1, Padding2;
    public uint TelemetryOptions, WindowModeCmd, InputCapture, ReportOptions, TeleportScene;

    public uint PlannerRequest;
    public int PlannerSeed;
    public uint PlannerLevel, PlannerOptions;
    public ulong PlannerSearchCount;
    public uint PlannerBudgetMicros;
    public ulong PlannerSearchKey;
    public uint FrameLimitMode, VSyncMode;
    public int GameFrameLimit;
    public uint RunReviewRequest;
    public long RunReviewId;
    public uint RunHudVisible;
    public float ReplayMinSeconds;
    public uint ReplayMinCrystals;
    public uint Replay3dCmd;
    public long Replay3dRunId;
    public double Replay3dSeconds;
    public uint Replay3dOptions;
    public float Replay3dSpeed;
    public double Replay3dLoopA, Replay3dLoopB;
    public float Replay3dFollowDistance, Replay3dFollowHeight, Replay3dTrailSeconds;
    public bool RequestsPlanner
    {
        get {
            return (TelemetryOptions & 128u) != 0 && !Restricted;
        }
    }

    public bool RequestsPlannerGeometry
    {
        get {
            return (TelemetryOptions & 256u) != 0 && !Restricted;
        }
    }

    // Optional extension in the existing reserved bytes. Older v3 trainers
    // request the full export; v4 explicitly requests only what it displays.
    public bool RequestsMap
    {
        get {
            return (TelemetryOptions & 0x80000000u) == 0 || (TelemetryOptions & 1u) != 0;
        }
    }
    public bool RequestsPath
    {
        get {
            return (TelemetryOptions & 0x80000000u) == 0 || (TelemetryOptions & 2u) != 0;
        }
    }
    public bool RequestsDetails
    {
        get {
            return (TelemetryOptions & 0x80000000u) == 0 || (TelemetryOptions & 4u) != 0;
        }
    }
    public bool RequestsInputs
    {
        get {
            return (TelemetryOptions & 1024u) != 0;
        }
    }
    public bool RequestsInspector
    {
        get {
            return (TelemetryOptions & 2048u) != 0;
        }
    }
    public bool RequestsLive
    {
        get {
            return (TelemetryOptions & 8u) != 0;
        }
    }
    public bool RequestsWalls
    {
        get {
            return (TelemetryOptions & 0x80000000u) == 0 || (TelemetryOptions & 16u) != 0;
        }
    }
    public bool RequestsCrystals
    {
        get {
            return (TelemetryOptions & 0x80000000u) == 0 || (TelemetryOptions & 32u) != 0;
        }
    }
    public bool RequestsMonster
    {
        get {
            return (TelemetryOptions & 0x80000000u) == 0 || (TelemetryOptions & 64u) != 0;
        }
    }

    public bool Speedrun
    {
        get {
            return SpeedrunModeActive == 1;
        }
    }
    // A current trainer owns restart even while typing, rebinding or unassigned.
    // Old trainers keep the game's T fallback; losing the heartbeat restores it.
    public bool LegacyRestartAllowed(bool trainerConnected)
    {
        return !trainerConnected || (TelemetryOptions & 8192u) == 0;
    }
    public bool Practice
    {
        get {
            return PracticeModeActive == 1;
        }
    }
    public bool Restricted
    {
        get {
            return Speedrun && !Practice;
        }
    }
}

public sealed class SnapshotBuffer
{
    public readonly byte[] Bytes = new byte[SharedMemoryProtocol.PayloadSize];
    private readonly byte[] stringScratch = new byte[512];

    [StructLayout(LayoutKind.Explicit)]
    private struct Bits
    {
        [FieldOffset(0)]
        public float Float;
        [FieldOffset(0)]
        public int Int;
        [FieldOffset(0)]
        public double Double;
        [FieldOffset(0)]
        public long Long;
    }

    public void Clear()
    {
        Array.Clear(Bytes, 0, Bytes.Length);
    }

    public void Write(int offset, byte value)
    {
        Bytes[offset] = value;
    }

    public void Write(int offset, bool value)
    {
        Write(offset, (byte)(value ? 1 : 0));
    }

    public void Write(int offset, int value)
    {
        unchecked
        {
            for (int i = 0; i < 4; i++)
                Bytes[offset + i] = (byte)(value >> (8 * i));
        }
    }

    public void Write(int offset, long value)
    {
        unchecked
        {
            for (int i = 0; i < 8; i++)
                Bytes[offset + i] = (byte)(value >> (8 * i));
        }
    }

    public void Write(int offset, float value)
    {
        Write(offset, new Bits { Float = value }.Int);
    }

    public void Write(int offset, double value)
    {
        Write(offset, new Bits { Double = value }.Long);
    }

    public void WriteString(int offset, string value, int capacity)
    {
        Array.Clear(Bytes, offset, capacity);
        if (string.IsNullOrEmpty(value) || capacity <= 1)
            return;
        int chars = Math.Min(value.Length, Math.Min(capacity - 1, 127));
        int count = Encoding.UTF8.GetBytes(value, 0, chars, stringScratch, 0),
            length = Math.Min(count, capacity - 1);
        // Do not terminate halfway through a UTF-8 character.
        while (length > 0 && length < count && (stringScratch[length] & 0xC0) == 0x80)
            length--;
        Buffer.BlockCopy(stringScratch, 0, Bytes, offset, length);
    }
}

public sealed class SharedMemoryProtocol : IDisposable
{
    public const string MapName = "ItStealsSharedMem.v30";
    public const string MutexName = "ItStealsSharedMem.v30.Lock";
    public const int Capacity = 32768, PayloadOffset = 256, PayloadSize = SharedLayout.Size;
    public const uint Magic = 0x53535449, Version = 30;
    private MemoryMappedFile map;
    private MemoryMappedViewAccessor view;
    private Mutex gate;
    private uint serial, snapshotId;
    private readonly uint ownerPid;
    public uint Session { get; private set; }
    public TrainerCommands Commands;

    public SharedMemoryProtocol()
    {
        if (!BitConverter.IsLittleEndian || Marshal.SizeOf(typeof(TrainerCommands)) != 192)
            throw new NotSupportedException("Unsupported shared-memory layout.");
        using (var process = System.Diagnostics.Process.GetCurrentProcess()) ownerPid = (uint)process.Id;
        try
        {
            gate = new Mutex(false, MutexName);
            map = MemoryMappedFile.CreateOrOpen(MapName, Capacity);
            view = map.CreateViewAccessor(0, Capacity);
            // Initialization is done once per mod lifetime, never per level.
            bool entered;
            try
            {
                entered = gate.WaitOne(1000);
            }
            catch (AbandonedMutexException)
            {
                entered = true;
            }
            if (!entered)
                throw new TimeoutException("Shared-memory initialization timed out.");
            try
            {
                uint previousPid = view.ReadUInt32(20);
                bool occupied = false;
                if (view.ReadUInt32(0) == Magic && previousPid != 0 && previousPid != ownerPid)
                {
                    try
                    {
                        using (var previous = System.Diagnostics.Process.GetProcessById((int)previousPid))
                            occupied = !previous.HasExited;
                    }
                    catch (ArgumentException)
                    {
                        occupied = false;
                    }
                    catch
                    {
                        occupied = true;
                    }
                }
                if (occupied)
                    throw new InvalidOperationException(
                        "Another running game owns the trainer channel. Close the other game instance first.");
                byte[] zero = new byte[Capacity];
                view.WriteArray(0, zero, 0, zero.Length);
                view.Write(0, Magic);
                view.Write(4, Version);
                view.Write(8, PayloadSize);
                view.Write(12, 192);
                Session = unchecked((uint)Environment.TickCount);
                view.Write(16, Session);
                view.Write(20, ownerPid);
            }
            finally
            {
                gate.ReleaseMutex();
            }
        }
        catch
        {
            Dispose();
            throw;
        }
    }

    private bool TryEnter(out bool abandoned)
    {
        abandoned = false;
        if (gate == null)
            return false;
        try
        {
            return gate.WaitOne(0);
        }
        catch (AbandonedMutexException)
        {
            abandoned = true;
            return true;
        }
    }

    public bool Poll(out bool newRequest)
    {
        newRequest = false;
        bool abandoned;
        if (!TryEnter(out abandoned))
            return false;
        try
        {
            if (view.ReadUInt32(20) != ownerPid)
            {
                Commands = new TrainerCommands();
                return false;
            }
            if (abandoned)
            {
                // Never execute a possibly half-written command from a crashed trainer.
                Commands = new TrainerCommands();
                Commands.Serial = serial;
                view.Write(64, ref Commands);
                view.Write(40, serial);
                return true;
            }
            view.Read(64, out Commands);
            newRequest = Commands.Serial != serial;
            if (newRequest)
            {
                serial = Commands.Serial;
                view.Write(40, serial);
            }
            return true;
        }
        finally
        {
            gate.ReleaseMutex();
        }
    }

    public void Publish(SnapshotBuffer data, double gameTime)
    {
        bool abandoned;
        if (!TryEnter(out abandoned))
            return;
        try
        {
            if (view.ReadUInt32(20) != ownerPid)
                return;
            if (abandoned)
            {
                Commands = new TrainerCommands { Serial = serial };
                view.Write(64, ref Commands);
                view.Write(40, serial);
            }
            view.Write(28, 0u); // incomplete/offline until the entire copy is committed
            view.WriteArray(PayloadOffset, data.Bytes, 0, data.Bytes.Length);
            view.Write(32, gameTime);
            view.Write(24, unchecked(++snapshotId));
            view.Write(28, 1u);
        }
        finally
        {
            gate.ReleaseMutex();
        }
    }

    public void Dispose()
    {
        bool abandoned;
        if (view != null && TryEnter(out abandoned))
        {
            try
            {
                if (view.ReadUInt32(20) == ownerPid)
                    view.Write(28, 0u);
            }
            finally
            {
                gate.ReleaseMutex();
            }
        }
        if (view != null)
        {
            view.Dispose();
            view = null;
        }
        if (map != null)
        {
            map.Dispose();
            map = null;
        }
        if (gate != null)
        {
            gate.Dispose();
            gate = null;
        }
    }
}
