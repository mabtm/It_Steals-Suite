using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEngine;

// Versioned, independent of the small map/telemetry channel. All timestamps
// use the same monotonic run clock. Frame and physics sequences are distinct.
public struct ReplayViewFrame
{
    public double Seconds;
    public uint Frame, Tick, State, Keys, Reads;
    public float Horizontal, Vertical, MouseX, MouseY, JoystickX, JoystickY;
    public Vector3 Body, Camera;
    public Quaternion BodyRotation, CameraRotation, LampRotation;
    public float Fov;
    public Vector3 LampPosition, LampColor;
    public float LampIntensity, LampRange, LampAngle;
    public uint LampType;
}

public struct ReplayPhysicsFrame
{
    public double Seconds;
    public uint Frame, Tick, State;
    public float Horizontal, Vertical;
    public Vector3 Position;
    public Quaternion Rotation;
}

public struct ReplayInputEvent
{
    public double Seconds;
    public uint Frame, Tick, Keys, Down, Up, Actions, State;
}

public sealed class ReplayMotionInfo
{
    public int Frames, Ticks, Events, Dropped;
    public uint Flags;
    public string Tool = "", Game = "", Unity = "";

    public static void Write(BinaryWriter w, ReplayMotionInfo i)
    {
        w.Write(i != null);
        if (i == null)
            return;
        w.Write(i.Frames);
        w.Write(i.Ticks);
        w.Write(i.Events);
        w.Write(i.Dropped);
        w.Write(i.Flags);
        ReplayMotionData.Text(w, i.Tool);
        ReplayMotionData.Text(w, i.Game);
        ReplayMotionData.Text(w, i.Unity);
    }

    public static ReplayMotionInfo Read(BinaryReader r)
    {
        byte yes = r.ReadByte();
        if (yes > 1)
            throw new InvalidDataException("Invalid input metadata flag");
        if (yes == 0)
            return null;
        var i = new ReplayMotionInfo { Frames = r.ReadInt32(),
                                       Ticks = r.ReadInt32(),
                                       Events = r.ReadInt32(),
                                       Dropped = r.ReadInt32(),
                                       Flags = r.ReadUInt32(),
                                       Tool = ReplayMotionData.Text(r, 64),
                                       Game = ReplayMotionData.Text(r, 64),
                                       Unity = ReplayMotionData.Text(r, 64) };
        if (i.Frames < 0 || i.Frames > ReplayMotionData.MaxFrames || i.Ticks < 0 ||
            i.Ticks > ReplayMotionData.MaxTicks || i.Events < 0 || i.Events > ReplayMotionData.MaxEvents ||
            i.Dropped < 0 || (i.Flags & ~7u) != 0)
            throw new InvalidDataException("Invalid input metadata");
        return i;
    }
}

public sealed class ReplayMotionData
{
    public const int MaxFrames = 262144, MaxTicks = 524288, MaxEvents = 131072, MaxBytes = 128 * 1024 * 1024;
    public int InputFormat = 6;
    public long Id;
    public int Scene, Seed;
    public double Seconds;
    public string Hash = "", AudioNotes = "";
    public ReplayMotionInfo Info = new ReplayMotionInfo();
    public ReplayColliderData Colliders = new ReplayColliderData();
    public ReplayVisualData Visuals = new ReplayVisualData();
    public ReplayExperienceData Experience = new ReplayExperienceData();
    public ReplayAudioSession AudioSession;
    public ReplayCoverage Coverage = new ReplayCoverage();
    public readonly List<ReplayViewFrame> Frames = new List<ReplayViewFrame>(1024);
    public readonly List<ReplayPhysicsFrame> Ticks = new List<ReplayPhysicsFrame>(1024);
    public readonly List<InputDeviceFrame> Devices = new List<InputDeviceFrame>(128);

    // Reserve the remaining file budget for bounded collider/visual extensions.
    private bool Budget()
    {
        return Frames.Count * 168L + Ticks.Count * 56L + Events.Count * 36L +
                   Devices.Count * InputDeviceFrame.WireSize <
               56L * 1024 * 1024;
    }

    public long EstimatedBytes
    {
        get {
            return 8192L + Frames.Capacity * 168L + Ticks.Capacity * 56L + Events.Capacity * 36L +
                   Devices.Capacity * InputDeviceFrame.WireSize + Colliders.EstimatedBytes * 2 +
                   Visuals.EstimatedBytes + Experience.EstimatedBytes +
                   (AudioSession == null ? 0 : AudioSession.MemoryBytes);
        }
    }
    public double DeviceObservedThrough;
    public int DeviceDropped;

    public void ObserveDevices(InputDeviceFrame f)
    {
        if (DeviceDropped > 0)
        {
            Coverage.Fail(ReplayCoverage.Inputs);
            Drop();
            if (DeviceDropped < int.MaxValue)
                DeviceDropped++;
            return;
        }
        if (Devices.Count > 0 && Devices[Devices.Count - 1].SameInput(f))
        {
            DeviceObservedThrough = f.Seconds;
            Coverage.Observe(ReplayCoverage.Inputs, f.Seconds);
            return;
        }
        if (Devices.Count >= 131072 || !Budget())
        {
            Coverage.Fail(ReplayCoverage.Inputs);
            Drop();
            if (DeviceDropped < int.MaxValue)
                DeviceDropped++;
            return;
        }
        Devices.Add(f);
        DeviceObservedThrough = f.Seconds;
        Coverage.Observe(ReplayCoverage.Inputs, f.Seconds);
    }

    public readonly List<ReplayInputEvent> Events = new List<ReplayInputEvent>(128);

    public void Drop()
    {
        Info.Flags |= 1;
        if (Info.Dropped < int.MaxValue)
            Info.Dropped++;
    }

    public void Add(ReplayViewFrame f)
    {
        if (Frames.Count >= MaxFrames || !Budget())
        {
            Drop();
            return;
        }
        Frames.Add(f);
        Info.Frames = Frames.Count;
    }

    public void Add(ReplayPhysicsFrame f)
    {
        if (Ticks.Count >= MaxTicks || !Budget())
        {
            Drop();
            return;
        }
        Ticks.Add(f);
        Info.Ticks = Ticks.Count;
    }

    public void Add(ReplayInputEvent e)
    {
        if (Events.Count >= MaxEvents || !Budget())
        {
            Drop();
            return;
        }
        Events.Add(e);
        Info.Events = Events.Count;
    }

    public static void Text(BinaryWriter w, string value)
    {
        byte[] b = Encoding.UTF8.GetBytes(value ?? "");
        w.Write(b.Length);
        w.Write(b);
    }

    public static string Text(BinaryReader r, int limit)
    {
        int n = r.ReadInt32();
        if (n < 0 || n > limit)
            throw new InvalidDataException("Input text exceeds limit");
        var bytes = r.ReadBytes(n);
        if (bytes.Length != n)
            throw new EndOfStreamException();
        string s = new UTF8Encoding(false, true).GetString(bytes);
        if (s.IndexOf('\0') >= 0)
            throw new InvalidDataException("Invalid input text");
        return s;
    }

    private static void V(BinaryWriter w, Vector3 p)
    {
        w.Write(p.x);
        w.Write(p.y);
        w.Write(p.z);
    }

    private static void Q(BinaryWriter w, Quaternion q)
    {
        w.Write(q.x);
        w.Write(q.y);
        w.Write(q.z);
        w.Write(q.w);
    }

    private static float Number(BinaryReader r, float limit = 100000)
    {
        float f = r.ReadSingle();
        if (float.IsNaN(f) || float.IsInfinity(f) || Math.Abs(f) > limit)
            throw new InvalidDataException("Invalid input number");
        return f;
    }

    private static Vector3 V(BinaryReader r)
    {
        return new Vector3(Number(r), Number(r), Number(r));
    }

    private static Quaternion Q(BinaryReader r)
    {
        var q = new Quaternion(Number(r, 1.01f), Number(r, 1.01f), Number(r, 1.01f), Number(r, 1.01f));
        double n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        if (n < .98 || n > 1.02)
            throw new InvalidDataException("Invalid camera rotation");
        return q;
    }

    private static void Stamp(BinaryReader r, double duration, ref double previous, ref uint lastFrame,
                              ref uint lastTick, out double seconds, out uint frame, out uint tick)
    {
        seconds = r.ReadDouble();
        frame = r.ReadUInt32();
        tick = r.ReadUInt32();
        if (double.IsNaN(seconds) || double.IsInfinity(seconds) || seconds < previous || seconds < 0 ||
            seconds > duration + .000001 || frame < lastFrame || tick < lastTick)
            throw new InvalidDataException("Invalid input timeline");
        previous = seconds;
        lastFrame = frame;
        lastTick = tick;
    }

    private static void Bits(uint state, uint keys, uint reads = 0, uint allowed = 2047u)
    {
        if ((state & ~allowed) != 0 || (keys & ~127u) != 0 || (reads & ~63u) != 0)
            throw new InvalidDataException("Invalid input flags");
    }

    public void Write(Stream target)
    {
        // Hash during the sequential write; never build a second file-sized
        // byte array on the game thread or issue IO from input callbacks.
        using (var hash =
                   new ReplayHashStream(target)) using (var w = new BinaryWriter(hash, Encoding.UTF8, true))
        {
            w.Write(0x4e495349u);
            w.Write(7);
            w.Write(Id);
            w.Write(Scene);
            w.Write(Seed);
            w.Write(Seconds);
            Text(w, Hash);
            ReplayMotionInfo.Write(w, Info);
            w.Write(Frames.Count);
            foreach (var f in Frames)
            {
                w.Write(f.Seconds);
                w.Write(f.Frame);
                w.Write(f.Tick);
                w.Write(f.State);
                w.Write(f.Keys);
                w.Write(f.Reads);
                w.Write(f.Horizontal);
                w.Write(f.Vertical);
                w.Write(f.MouseX);
                w.Write(f.MouseY);
                w.Write(f.JoystickX);
                w.Write(f.JoystickY);
                V(w, f.Body);
                Q(w, f.BodyRotation);
                V(w, f.Camera);
                Q(w, f.CameraRotation);
                w.Write(f.Fov);
                Q(w, f.LampRotation);
                V(w, f.LampPosition);
                V(w, f.LampColor);
                w.Write(f.LampIntensity);
                w.Write(f.LampRange);
                w.Write(f.LampAngle);
                w.Write(f.LampType);
            }
            w.Write(Ticks.Count);
            foreach (var f in Ticks)
            {
                w.Write(f.Seconds);
                w.Write(f.Frame);
                w.Write(f.Tick);
                w.Write(f.State);
                w.Write(f.Horizontal);
                w.Write(f.Vertical);
                V(w, f.Position);
                Q(w, f.Rotation);
            }
            w.Write(Events.Count);
            foreach (var e in Events)
            {
                w.Write(e.Seconds);
                w.Write(e.Frame);
                w.Write(e.Tick);
                w.Write(e.Keys);
                w.Write(e.Down);
                w.Write(e.Up);
                w.Write(e.Actions);
                w.Write(e.State);
            }
            Colliders.Write(w);
            w.Write(DeviceObservedThrough);
            w.Write(DeviceDropped);
            w.Write(Devices.Count);
            foreach (var f in Devices)
                InputDeviceFrame.Write(w, f);
            Visuals.Write(w);
            Experience.Write(w);
            Coverage.Write(w);
            Text(w, AudioNotes);
            w.Flush();
            ulong sum = hash.Hash;
            hash.Hashing = false;
            w.Write(sum);
            w.Flush();
        }
    }

    public static ReplayMotionData Read(byte[] bytes)
    {
        if (bytes == null || bytes.Length < 64 || bytes.Length > MaxBytes ||
            BitConverter.ToUInt64(bytes, bytes.Length - 8) !=
                PracticeMapData.Checksum(bytes, bytes.Length - 8))
            throw new InvalidDataException("Input recording checksum/size mismatch");
        using (var r = new BinaryReader(new MemoryStream(bytes, false)))
        {
            if (r.ReadUInt32() != 0x4e495349u)
                throw new InvalidDataException("Unsupported input recording");
            int format = r.ReadInt32();
            if (format < 1 || format > 7)
                throw new InvalidDataException("Unsupported input recording");
            var d = new ReplayMotionData { InputFormat = format,           Id = r.ReadInt64(),
                                           Scene = r.ReadInt32(),          Seed = r.ReadInt32(),
                                           Seconds = r.ReadDouble(),       Hash = Text(r, 65),
                                           Info = ReplayMotionInfo.Read(r) };
            if (d.Id <= 0 || Array.IndexOf(SpeedrunManager.LevelIndices, d.Scene) < 0 ||
                double.IsNaN(d.Seconds) || double.IsInfinity(d.Seconds) || d.Seconds < 0 ||
                d.Seconds > 315360000 || d.Info == null)
                throw new InvalidDataException("Invalid input identity");
            int n = r.ReadInt32();
            if (n != d.Info.Frames)
                throw new InvalidDataException("Input frame count mismatch");
            double previous = 0;
            uint frame = 0, tick = 0;
            for (int k = 0; k < n; k++)
            {
                var f = new ReplayViewFrame();
                Stamp(r, d.Seconds, ref previous, ref frame, ref tick, out f.Seconds, out f.Frame,
                      out f.Tick);
                f.State = r.ReadUInt32();
                f.Keys = r.ReadUInt32();
                f.Reads = r.ReadUInt32();
                Bits(f.State, f.Keys, f.Reads, format >= 3 ? 2047u : 511u);
                f.Horizontal = Number(r);
                f.Vertical = Number(r);
                f.MouseX = Number(r);
                f.MouseY = Number(r);
                f.JoystickX = Number(r);
                f.JoystickY = Number(r);
                f.Body = V(r);
                f.BodyRotation = Q(r);
                f.Camera = V(r);
                f.CameraRotation = Q(r);
                f.Fov = Number(r, 179);
                if (f.Fov <= 0)
                    throw new InvalidDataException("Invalid camera FOV");
                f.LampRotation = Q(r);
                f.LampPosition = V(r);
                f.LampColor = V(r);
                f.LampIntensity = Number(r);
                f.LampRange = Number(r);
                f.LampAngle = Number(r, 179);
                f.LampType = r.ReadUInt32();
                if (f.LampIntensity < 0 || f.LampRange < 0 || f.LampAngle < 0 || f.LampType > 4)
                    throw new InvalidDataException("Invalid recorded light");
                d.Frames.Add(f);
            }
            n = r.ReadInt32();
            if (n != d.Info.Ticks)
                throw new InvalidDataException("Input physics count mismatch");
            previous = 0;
            frame = tick = 0;
            for (int k = 0; k < n; k++)
            {
                var f = new ReplayPhysicsFrame();
                Stamp(r, d.Seconds, ref previous, ref frame, ref tick, out f.Seconds, out f.Frame,
                      out f.Tick);
                f.State = r.ReadUInt32();
                Bits(f.State, 0, 0, format >= 3 ? 2047u : 511u);
                f.Horizontal = Number(r);
                f.Vertical = Number(r);
                f.Position = V(r);
                f.Rotation = Q(r);
                d.Ticks.Add(f);
            }
            n = r.ReadInt32();
            if (n != d.Info.Events)
                throw new InvalidDataException("Input event count mismatch");
            previous = 0;
            frame = tick = 0;
            uint keys = 0;
            for (int k = 0; k < n; k++)
            {
                var e = new ReplayInputEvent();
                Stamp(r, d.Seconds, ref previous, ref frame, ref tick, out e.Seconds, out e.Frame,
                      out e.Tick);
                e.Keys = r.ReadUInt32();
                e.Down = r.ReadUInt32();
                e.Up = r.ReadUInt32();
                e.Actions = r.ReadUInt32();
                e.State = r.ReadUInt32();
                Bits(e.State, e.Keys, 0, format >= 3 ? 2047u : 511u);
                if ((e.Down & ~127u) != 0 || (e.Up & ~127u) != 0 || (e.Down & e.Up) != 0 ||
                    (e.Actions & ~15u) != 0 || (k > 0 && ((keys | e.Down) & ~e.Up) != e.Keys) ||
                    (e.Down & ~e.Keys) != 0 || (e.Up & e.Keys) != 0)
                    throw new InvalidDataException("Invalid input transitions");
                keys = e.Keys;
                d.Events.Add(e);
            }
            if (format >= 2)
                d.Colliders = ReplayColliderData.Read(r, d.Seconds);
            if (format >= 3)
            {
                d.DeviceObservedThrough = r.ReadDouble();
                d.DeviceDropped = r.ReadInt32();
                if (double.IsNaN(d.DeviceObservedThrough) || double.IsInfinity(d.DeviceObservedThrough) ||
                    d.DeviceObservedThrough < 0 || d.DeviceObservedThrough > d.Seconds + .000001 ||
                    d.DeviceDropped < 0)
                    throw new InvalidDataException("Invalid device recording span");
                n = r.ReadInt32();
                if (n < 0 || n > 131072 ||
                    r.BaseStream.Length - r.BaseStream.Position < (long)n * InputDeviceFrame.WireSize + 8)
                    throw new InvalidDataException("Invalid device frame count");
                previous = 0;
                frame = tick = 0;
                for (int k = 0; k < n; k++)
                    d.Devices.Add(InputDeviceFrame.Read(r, d.Seconds, ref previous, ref frame, ref tick));
                if (previous > d.DeviceObservedThrough + .000001)
                    throw new InvalidDataException("Device span ends before samples");
            }
            if (format >= 4)
                d.Visuals = ReplayVisualData.Read(r, d.Seconds, format);
            if (format >= 5)
                d.Experience = ReplayExperienceData.Read(r, d.Seconds);
            if (format >= 6)
                d.Coverage = ReplayCoverage.Read(r, d.Seconds);
            if (format >= 7)
                d.AudioNotes = Text(r, 4096);
            r.ReadUInt64();
            if (r.BaseStream.Position != bytes.Length)
                throw new InvalidDataException("Trailing input data");
            return d;
        }
    }

    public bool Matches(PracticeMapData map)
    {
        var i = map == null || map.Review == null ? null : map.Review.Motion;
        return i != null && Id == map.Review.Id && Scene == map.Scene && Seed == map.Seed &&
               Hash == map.Hash && Math.Abs(Seconds - map.Review.Seconds) < .000001 &&
               i.Frames == Info.Frames && i.Ticks == Info.Ticks && i.Events == Info.Events &&
               i.Flags == Info.Flags && i.Dropped == Info.Dropped && i.Tool == Info.Tool &&
               i.Game == Info.Game && i.Unity == Info.Unity;
    }
}

public sealed class ReplayHashStream : Stream
{
    private readonly Stream output;
    public ulong Hash = 14695981039346656037UL;
    public bool Hashing = true;

    public ReplayHashStream(Stream stream)
    {
        output = stream;
    }

    public override bool CanRead
    {
        get {
            return false;
        }
    }
    public override bool CanSeek
    {
        get {
            return false;
        }
    }
    public override bool CanWrite
    {
        get {
            return true;
        }
    }
    public override long Length
    {
        get {
            return output.Length;
        }
    }
    public override long Position
    {
        get {
            return output.Position;
        }
        set {
            throw new NotSupportedException();
        }
    }

    public override void Write(byte[] bytes, int offset, int count)
    {
        if (Hashing)
            unchecked
            {
                for (int i = offset; i < offset + count; i++)
                {
                    Hash ^= bytes[i];
                    Hash *= 1099511628211UL;
                }
            }
        output.Write(bytes, offset, count);
    }

    public override void WriteByte(byte value)
    {
        if (Hashing)
            unchecked
            {
                Hash ^= value;
                Hash *= 1099511628211UL;
            }
        output.WriteByte(value);
    }

    public override void Flush()
    {
        output.Flush();
    }

    public override int Read(byte[] b, int o, int n)
    {
        throw new NotSupportedException();
    }

    public override long Seek(long p, SeekOrigin o)
    {
        throw new NotSupportedException();
    }

    public override void SetLength(long n)
    {
        throw new NotSupportedException();
    }
}
