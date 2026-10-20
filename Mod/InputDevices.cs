using System;
using System.IO;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;
using UnityEngine;

// Focus-scoped observations. No global hooks or background keyboard capture.
public struct InputPad
{
    public ushort Buttons;
    public byte LeftTrigger, RightTrigger;
    public short LX, LY, RX, RY;

    public bool Same(InputPad p)
    {
        return Buttons == p.Buttons && LeftTrigger == p.LeftTrigger && RightTrigger == p.RightTrigger &&
               LX == p.LX && LY == p.LY && RX == p.RX && RY == p.RY;
    }

    public static void Write(BinaryWriter w, InputPad p)
    {
        w.Write(p.Buttons);
        w.Write(p.LeftTrigger);
        w.Write(p.RightTrigger);
        w.Write(p.LX);
        w.Write(p.LY);
        w.Write(p.RX);
        w.Write(p.RY);
    }

    public static InputPad Read(BinaryReader r)
    {
        return new InputPad { Buttons = r.ReadUInt16(),    LeftTrigger = r.ReadByte(),
                              RightTrigger = r.ReadByte(), LX = r.ReadInt16(),
                              LY = r.ReadInt16(),          RX = r.ReadInt16(),
                              RY = r.ReadInt16() };
    }
}

public struct InputDeviceFrame
{
    public const int WireSize = 172;
    public double Seconds;
    public uint Frame, Tick, State, Flags, Mouse;
    public ulong K0, K1, K2, K3;
    public uint J0, J1, J2, J3, J4, J5, J6, J7;
    public InputPad P0, P1, P2, P3;
    public float Sprint, Flash, MoveX, MoveY, LookX, LookY, JoyX, JoyY;

    public uint Legacy(int i)
    {
        switch (i)
        {
        case 0:
            return J0;
        case 1:
            return J1;
        case 2:
            return J2;
        case 3:
            return J3;
        case 4:
            return J4;
        case 5:
            return J5;
        case 6:
            return J6;
        default:
            return J7;
        }
    }

    public void Legacy(int i, uint v)
    {
        switch (i)
        {
        case 0:
            J0 = v;
            break;
        case 1:
            J1 = v;
            break;
        case 2:
            J2 = v;
            break;
        case 3:
            J3 = v;
            break;
        case 4:
            J4 = v;
            break;
        case 5:
            J5 = v;
            break;
        case 6:
            J6 = v;
            break;
        default:
            J7 = v;
            break;
        }
    }

    public InputPad Pad(int i)
    {
        return i == 0 ? P0 : i == 1 ? P1 : i == 2 ? P2 : P3;
    }

    public void Pad(int i, InputPad p)
    {
        if (i == 0)
            P0 = p;
        else if (i == 1)
            P1 = p;
        else if (i == 2)
            P2 = p;
        else
            P3 = p;
    }

    public bool SameInput(InputDeviceFrame b)
    {
        return State == b.State && Flags == b.Flags && Mouse == b.Mouse && K0 == b.K0 && K1 == b.K1 &&
               K2 == b.K2 && K3 == b.K3 && J0 == b.J0 && J1 == b.J1 && J2 == b.J2 && J3 == b.J3 &&
               J4 == b.J4 && J5 == b.J5 && J6 == b.J6 && J7 == b.J7 && P0.Same(b.P0) && P1.Same(b.P1) &&
               P2.Same(b.P2) && P3.Same(b.P3) && Sprint == b.Sprint && Flash == b.Flash && MoveX == b.MoveX &&
               MoveY == b.MoveY && LookX == b.LookX && LookY == b.LookY && JoyX == b.JoyX && JoyY == b.JoyY;
    }

    public static void Write(BinaryWriter w, InputDeviceFrame f)
    {
        w.Write(f.Seconds);
        w.Write(f.Frame);
        w.Write(f.Tick);
        w.Write(f.State);
        w.Write(f.Flags);
        w.Write(f.K0);
        w.Write(f.K1);
        w.Write(f.K2);
        w.Write(f.K3);
        w.Write(f.Mouse);
        for (int i = 0; i < 8; i++)
            w.Write(f.Legacy(i));
        for (int i = 0; i < 4; i++)
            InputPad.Write(w, f.Pad(i));
        w.Write(f.Sprint);
        w.Write(f.Flash);
        w.Write(f.MoveX);
        w.Write(f.MoveY);
        w.Write(f.LookX);
        w.Write(f.LookY);
        w.Write(f.JoyX);
        w.Write(f.JoyY);
    }

    public static InputDeviceFrame Read(BinaryReader r, double duration, ref double previous, ref uint frame,
                                        ref uint tick)
    {
        var f =
            new InputDeviceFrame { Seconds = r.ReadDouble(), Frame = r.ReadUInt32(), Tick = r.ReadUInt32(),
                                   State = r.ReadUInt32(),   Flags = r.ReadUInt32(), K0 = r.ReadUInt64(),
                                   K1 = r.ReadUInt64(),      K2 = r.ReadUInt64(),    K3 = r.ReadUInt64(),
                                   Mouse = r.ReadUInt32() };
        if (double.IsNaN(f.Seconds) || double.IsInfinity(f.Seconds) || f.Seconds < previous ||
            f.Seconds < 0 || f.Seconds > duration + .000001 || f.Frame < frame || f.Tick < tick ||
            (f.State & ~2047u) != 0 || (f.Flags & ~0x00ff070fu) != 0 || (f.Mouse & ~127u) != 0)
            throw new InvalidDataException("Invalid device observation");
        for (int i = 0; i < 8; i++)
        {
            uint v = r.ReadUInt32();
            if ((v & ~0xfffffu) != 0)
                throw new InvalidDataException("Invalid joystick buttons");
            f.Legacy(i, v);
        }
        for (int i = 0; i < 4; i++)
            f.Pad(i, InputPad.Read(r));
        f.Sprint = r.ReadSingle();
        f.Flash = r.ReadSingle();
        if (float.IsNaN(f.Sprint) || float.IsNaN(f.Flash) || Math.Abs(f.Sprint) > 1.001 ||
            Math.Abs(f.Flash) > 1.001)
            throw new InvalidDataException("Invalid game input axis");
        f.MoveX = r.ReadSingle();
        f.MoveY = r.ReadSingle();
        f.LookX = r.ReadSingle();
        f.LookY = r.ReadSingle();
        f.JoyX = r.ReadSingle();
        f.JoyY = r.ReadSingle();
        foreach (float v in new[] { f.MoveX, f.MoveY, f.LookX, f.LookY, f.JoyX, f.JoyY })
            if (float.IsNaN(v) || float.IsInfinity(v) || Math.Abs(v) > 100000)
                throw new InvalidDataException("Invalid observed game axes");
        previous = f.Seconds;
        frame = f.Frame;
        tick = f.Tick;
        return f;
    }
}

public static class LiveInputCapture
{
    [StructLayout(LayoutKind.Sequential)]
    private struct NativeState
    {
        public uint Packet;
        public InputPad Pad;
    }

    [DllImport("user32.dll")]
    private static extern bool GetKeyboardState([Out] byte[] keys);
    [DllImport("xinput1_4.dll", EntryPoint = "XInputGetState")]
    private static extern uint XInput14(uint index, out NativeState state);
    [DllImport("xinput1_3.dll", EntryPoint = "XInputGetState")]
    private static extern uint XInput13(uint index, out NativeState state);
    [DllImport("xinput9_1_0.dll", EntryPoint = "XInputGetState")]
    private static extern uint XInput91(uint index, out NativeState state);
    private static readonly Func<MouseLook, bool> crouch = CreateCrouchReader();

    private static Func<MouseLook, bool> CreateCrouchReader()
    {
        var field = typeof(MouseLook).GetField("isCrouching", BindingFlags.Instance | BindingFlags.NonPublic |
                                                                  BindingFlags.Public);
        if (field == null)
            return p => false;
        var m = new DynamicMethod("ObservedCrouch", typeof(bool), new[] { typeof(MouseLook) },
                                  typeof(MouseLook), true);
        var il = m.GetILGenerator();
        il.Emit(OpCodes.Ldarg_0);
        il.Emit(OpCodes.Ldfld, field);
        il.Emit(OpCodes.Ret);
        return (Func<MouseLook, bool>)m.CreateDelegate(typeof(Func<MouseLook, bool>));
    }

    private struct Change
    {
        public double Seconds;
        public uint Frame, Tick, Changed, State;
    }

    private static readonly Change[] changes = new Change[8];
    private static int changeCount, changeNext;
    private static readonly byte[] keyboard = new byte[256];
    private static int nativeApi;
    private static bool keyboardAvailable = true;
    private static MouseLook owner;
    private static int firstFrame;
    private static uint ticks, sequence;
    private static double began, lastDevices;
    private static long run;
    private static uint joystickMask, connectedPads;
    private static double nextPadProbe;
    private static uint axisReads;
    private static int axisFrame = -1;
    public static bool Enabled;
    public static InputDeviceFrame Current;
    public static bool Ready;
    public static float Sprint, Flash, MoveX, MoveY, LookX, LookY, JoyX, JoyY;

    public static void BeginRun()
    {
        owner = null;
        run = 0;
        Ready = false;
    }

    public static void Unavailable()
    {
        Ready = false;
        var recording = ReplayInputCapture.Current;
        if (recording != null)
            recording.Coverage.Fail(ReplayCoverage.Inputs);
    }

    public static void Physics()
    {
        ticks++;
    }

    public static void Axis(string name, float value)
    {
        if (axisFrame != Time.frameCount)
        {
            axisFrame = Time.frameCount;
            axisReads = 0;
        }
        if (name == "Sprint")
        {
            Sprint = Mathf.Clamp(value, -1, 1);
            axisReads |= 512;
        }
        else if (name == "CameraFlash")
        {
            Flash = Mathf.Clamp(value, -1, 1);
            axisReads |= 1024;
        }
        else if (name == "Horizontal")
            MoveX = value;
        else if (name == "Vertical")
            MoveY = value;
        else if (name == "Mouse X")
            LookX = value;
        else if (name == "Mouse Y")
            LookY = value;
        else if (name == "Joystick X")
            JoyX = value;
        else if (name == "Joystick Y")
            JoyY = value;
    }

    public static uint PlayerState(MouseLook p)
    {
        return (Application.isFocused ? 1u : 0) | (MenuCursor.OwnsInput ? 2u : 0) |
               (p != null && p.isSprinting ? 4u : 0) | (p != null && p.usingRadar ? 8u : 0) |
               (p != null && p.playerFlashlight != null && p.playerFlashlight.enabled &&
                        p.playerFlashlight.gameObject.activeInHierarchy
                    ? 16u
                    : 0) |
               (p != null && p.isCameraFlashing ? 32u : 0) |
               (p != null && p.controller != null && p.controller.isGrounded ? 64u : 0) |
               (p != null && p.canMove > 0 ? 128u : 0) | (FreeCam.FreeCamEnabled ? 256u : 0) |
               (p != null && crouch(p) ? 512u : 0) | (p != null && p.isCheckingTime ? 1024u : 0);
    }

    private static uint Pad(uint index, out NativeState value)
    {
        value = new NativeState();
        for (int tries = 0; tries < 3; tries++)
        {
            try
            {
                return nativeApi == 0   ? XInput14(index, out value)
                       : nativeApi == 1 ? XInput13(index, out value)
                       : nativeApi == 2 ? XInput91(index, out value)
                                        : 1u;
            }
            catch (DllNotFoundException)
            {
                nativeApi++;
            }
            catch (EntryPointNotFoundException)
            {
                nativeApi++;
            }
        }
        return 1;
    }

    public static void Observe(MouseLook p)
    {
        var recording = ReplayInputCapture.Current;
        if (!Enabled && recording == null)
        {
            Ready = false;
            return;
        }
        if (p == null || Replay3D.Reserved)
        {
            Ready = false;
            return;
        }
        long identity = recording == null ? 0 : recording.Id;
        double now = Time.realtimeSinceStartupAsDouble;
        if (owner != p || run != identity)
        {
            owner = p;
            run = identity;
            firstFrame = Time.frameCount;
            ticks = 0;
            began = now;
            changeCount = changeNext = 0;
            Ready = false;
        }
        var f =
            new InputDeviceFrame { Seconds = recording == null ? now - began
                                                               : SpeedrunManager.Instance.CurrentSeconds,
                                   Frame = recording == null ? (uint)Math.Max(0, Time.frameCount - firstFrame)
                                                             : ReplayInputCapture.RecordFrame,
                                   Tick = recording == null ? ticks : ReplayInputCapture.RecordTick,
                                   State = PlayerState(p),
                                   Sprint = Sprint,
                                   Flash = Flash,
                                   MoveX = MoveX,
                                   MoveY = MoveY,
                                   LookX = LookX,
                                   LookY = LookY,
                                   JoyX = JoyX,
                                   JoyY = JoyY };
        if (axisFrame == Time.frameCount)
            f.Flags |= axisReads;
        bool focused = Application.isFocused && !MenuCursor.OwnsInput;
        if (focused)
        {
            try
            {
                if (keyboardAvailable && GetKeyboardState(keyboard))
                {
                    f.Flags |= 256;
                    for (int i = 0; i < 256; i++)
                        if ((keyboard[i] & 128) != 0)
                        {
                            ulong bit = 1UL << (i & 63);
                            if (i < 64)
                                f.K0 |= bit;
                            else if (i < 128)
                                f.K1 |= bit;
                            else if (i < 192)
                                f.K2 |= bit;
                            else
                                f.K3 |= bit;
                        }
                }
            }
            catch (DllNotFoundException)
            {
                keyboardAvailable = false;
            }
            catch (EntryPointNotFoundException)
            {
                keyboardAvailable = false;
            }
            for (int i = 0; i < 7; i++)
                if (Input.GetMouseButton(i))
                    f.Mouse |= 1u << i;
            bool probe = now >= nextPadProbe;
            if (probe)
                nextPadProbe = now + 1;
            for (uint i = 0; i < 4; i++)
            {
                if (!probe && (connectedPads & (1u << (int)i)) == 0)
                    continue;
                NativeState pad;
                if (Pad(i, out pad) == 0)
                {
                    connectedPads |= 1u << (int)i;
                    f.Flags |= 1u << (int)i;
                    f.Pad((int)i, pad.Pad);
                }
                else
                    connectedPads &= ~(1u << (int)i);
            }
            if (now >= lastDevices)
            {
                lastDevices = now + 1;
                joystickMask = 0;
                var names = Input.GetJoystickNames();
                for (int i = 0; i < Math.Min(8, names.Length); i++)
                    if (!string.IsNullOrEmpty(names[i]))
                        joystickMask |= 1u << i;
            }
            for (int i = 0; i < 8; i++)
            {
                if ((joystickMask & (1u << i)) == 0)
                    continue;
                uint buttons = 0;
                for (int j = 0; j < 20; j++)
                    if (Input.GetKey((KeyCode)(350 + i * 20 + j)))
                        buttons |= 1u << j;
                f.Legacy(i, buttons);
                f.Flags |= 1u << (16 + i);
            }
        }
        uint changed = Ready ? Current.State ^ f.State : 0;
        if ((changed & (4u | 8u | 16u | 32u | 512u | 1024u)) != 0)
        {
            changes[changeNext] = new Change { Seconds = f.Seconds, Frame = f.Frame, Tick = f.Tick,
                                               Changed = changed, State = f.State };
            changeNext = (changeNext + 1) % 8;
            changeCount = Math.Min(8, changeCount + 1);
        }
        if (recording != null)
            recording.ObserveDevices(f);
        Current = f;
        Ready = true;
        sequence++;
    }

    public static void WriteSnapshot(SnapshotBuffer b)
    {
        if (!Ready)
            return;
        var f = Current;
        b.Write(SharedLayout.InputSequence, unchecked((int)sequence));
        b.Write(SharedLayout.InputSeconds, f.Seconds);
        b.Write(SharedLayout.InputFrame, unchecked((int)f.Frame));
        b.Write(SharedLayout.InputTick, unchecked((int)f.Tick));
        b.Write(SharedLayout.InputState, (int)f.State);
        b.Write(SharedLayout.InputFlags, (int)f.Flags);
        b.Write(SharedLayout.InputKeys, unchecked((long)f.K0));
        b.Write(SharedLayout.InputKeys + 8, unchecked((long)f.K1));
        b.Write(SharedLayout.InputKeys + 16, unchecked((long)f.K2));
        b.Write(SharedLayout.InputKeys + 24, unchecked((long)f.K3));
        b.Write(SharedLayout.InputMouse, (int)f.Mouse);
        for (int i = 0; i < 8; i++)
            b.Write(SharedLayout.InputLegacy + i * 4, (int)f.Legacy(i));
        for (int i = 0; i < 4; i++)
        {
            var p = f.Pad(i);
            b.Write(SharedLayout.InputPadButtons + i * 4, (int)p.Buttons);
            int o = SharedLayout.InputPadAxes + i * 24;
            b.Write(o, p.LeftTrigger / 255f);
            b.Write(o + 4, p.RightTrigger / 255f);
            b.Write(o + 8, Math.Max(-1, p.LX / 32767f));
            b.Write(o + 12, Math.Max(-1, p.LY / 32767f));
            b.Write(o + 16, Math.Max(-1, p.RX / 32767f));
            b.Write(o + 20, Math.Max(-1, p.RY / 32767f));
        }
        b.Write(SharedLayout.InputSprint, f.Sprint);
        b.Write(SharedLayout.InputFlash, f.Flash);
        b.Write(SharedLayout.InputGameAxes, f.MoveX);
        b.Write(SharedLayout.InputGameAxes + 4, f.MoveY);
        b.Write(SharedLayout.InputGameAxes + 8, f.LookX);
        b.Write(SharedLayout.InputGameAxes + 12, f.LookY);
        b.Write(SharedLayout.InputGameAxes + 16, f.JoyX);
        b.Write(SharedLayout.InputGameAxes + 20, f.JoyY);
        b.Write(SharedLayout.InputEventCount, changeCount);
        for (int i = 0; i < changeCount; i++)
        {
            var c = changes[(changeNext - changeCount + i + 8) % 8];
            int o = SharedLayout.InputEvents + i * 24;
            b.Write(o, c.Seconds);
            b.Write(o + 8, (int)c.Frame);
            b.Write(o + 12, (int)c.Tick);
            b.Write(o + 16, (int)c.Changed);
            b.Write(o + 20, (int)c.State);
        }
    }
}
