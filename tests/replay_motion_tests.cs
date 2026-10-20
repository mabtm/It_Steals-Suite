using System;
using System.IO;
using System.Reflection;
using System.Reflection.Emit;
using System.Collections.Generic;
using System.Diagnostics;
using HarmonyLib;
using UnityEngine;

public static class ReplayMotionManagedTests
{
    static void Check(bool value, string message)
    {
        if (!value)
            throw new Exception(message);
    }

    static byte[] Encode(ReplayMotionData d)
    {
        using (var s = new MemoryStream())
        {
            d.Write(s);
            return s.ToArray();
        }
    }

    static int CoverageLength(ReplayMotionData data)
    {
        using (var stream = new MemoryStream())
        {
            using (var writer = new BinaryWriter(stream, System.Text.Encoding.UTF8, true))
                data.Coverage.Write(writer);
            return (int)stream.Length;
        }
    }

    static int ExperienceLength(ReplayMotionData data)
    {
        using (var stream = new MemoryStream())
        {
            using (var writer = new BinaryWriter(stream, System.Text.Encoding.UTF8, true))
                data.Experience.Write(writer);
            return (int)stream.Length;
        }
    }

    static void Reject(byte[] b)
    {
        bool no = false;
        try
        {
            ReplayMotionData.Read(b);
        }
        catch (IOException)
        {
            no = true;
        }
        catch (InvalidDataException)
        {
            no = true;
        }
        Check(no, "malformed input accepted");
    }

    public static int Main(string[] args)
    {
        string folder = args[0];
        var map = PracticeMapArchive.Decode(File.ReadAllBytes(Path.Combine(folder, "review-wire.bin")));
        var motion = new ReplayMotionData { Id = map.Review.Id, Scene = map.Scene, Seed = map.Seed,
                                            Hash = map.Hash, Seconds = map.Review.Seconds };
        motion.Info.Tool = "7.2.0";
        motion.Info.Game = "9";
        motion.Info.Unity = "2021.2.19f1";
        var ownership = new PracticeMapData { Points = new[] { new Vector3(0, 0, 0), new Vector3(.1f, 0, 0),
                                                               new Vector3(10, 0, 0) },
                                              Selected = new[] { 0, 2 } };
        Check(
            ReplayColliderCapture.CrystalOwner(ownership, new Vector3(.3f, 0, 0)) == 0 &&
                ReplayColliderCapture.CrystalOwner(ownership, new Vector3(4, 0, 0)) == -1,
            "crystal colliders reuse selected-point pickup tolerance; nearby unselected candidates cannot steal ownership");
        var identity = new Quaternion(0, 0, 0, 1);
        for (int i = 0; i <= 20; i++)
        {
            double at = motion.Seconds * i / 20;
            motion.Add(new ReplayViewFrame { Seconds = at,
                                             Frame = (uint)i,
                                             Tick = (uint)(i * 2),
                                             State = 1 | 4 | 128,
                                             Keys = 1,
                                             Reads = 63,
                                             Horizontal = .5f,
                                             Vertical = 1,
                                             MouseX = i * .25f,
                                             MouseY = -.2f,
                                             JoystickX = .1f,
                                             JoystickY = -.1f,
                                             Body = new Vector3(i * .1f, 1, 0),
                                             BodyRotation = identity,
                                             Camera = new Vector3(i * .1f, 2, 0),
                                             CameraRotation = identity,
                                             LampRotation = identity,
                                             Fov = 75,
                                             LampPosition = new Vector3(0, 2, 0),
                                             LampColor = new Vector3(1, .9f, .8f),
                                             LampIntensity = 2.5f,
                                             LampRange = 19,
                                             LampAngle = 62,
                                             LampType = 0 });
        }
        for (int i = 0; i <= 40; i++)
            motion.Add(
                new ReplayPhysicsFrame { Seconds = motion.Seconds * i / 40, Frame = (uint)((i + 1) / 2),
                                         Tick = (uint)i, State = 1 | 4 | 128, Horizontal = .5f, Vertical = 1,
                                         Position = new Vector3(i * .05f, 1, 0), Rotation = identity });
        motion.Add(new ReplayInputEvent { Seconds = 0, Keys = 1, State = 1 | 128 });
        motion.Add(new ReplayInputEvent { Seconds = .2, Frame = 4, Tick = 8, Keys = 65, Down = 64,
                                          Actions = 1, State = 1 | 128 });
        motion.Add(new ReplayInputEvent { Seconds = .3, Frame = 6, Tick = 12, Keys = 1, Up = 64,
                                          State = 1 | 8 | 128 });
        motion.Add(new ReplayInputEvent { Seconds = .4, Frame = 8, Tick = 16, Keys = 0, Up = 1, State = 2 });
        // Verify old input format 1 remains playable without fabricated collider history.
        byte[] old = Encode(motion);
        int bodyEnd = old.Length - 8 - 17 - 16 - 13 - ExperienceLength(motion) - CoverageLength(motion);
        byte[] legacyInput = new byte[bodyEnd + 8];
        Array.Copy(old, legacyInput, bodyEnd);
        Array.Copy(BitConverter.GetBytes(1), 0, legacyInput, 4, 4);
        Array.Copy(BitConverter.GetBytes(PracticeMapData.Checksum(legacyInput, bodyEnd)), 0, legacyInput,
                   bodyEnd, 8);
        Check(!ReplayMotionData.Read(legacyInput).Colliders.Recorded,
              "legacy input has no invented moving collider history");
        File.WriteAllBytes(Path.Combine(folder, "input-v1.isinput"), legacyInput);
        motion.Colliders.Recorded = true;
        for (int k = 0; k < 4; k++)
        {
            var track = new ReplayColliderTrack { Group = k == 0   ? 1
                                                          : k == 2 ? 3
                                                                   : 2,
                                                  Owner = k == 0   ? 0
                                                          : k == 2 ? map.Selected[0]
                                                                   : 1,
                                                  Kind = k + 1, Name = "Actual shape " + k };
            motion.Colliders.Tracks.Add(track);
            for (int i = 0; i < 3; i++)
            {
                var matrix = new Matrix4x4 { m00 = 1, m11 = 1, m22 = 1, m33 = 1, m03 = k * 2 };
                var shape = new ReplayColliderShape { Center = new Vector3(.1f, .2f, .3f),
                                                      Size = new Vector3(1, 2, 3),
                                                      Radius = .35f,
                                                      Height = i == 1 ? 1.1f : 1.8f,
                                                      Direction = 1,
                                                      Matrix = matrix };
                motion.Colliders.Add(track, new ReplayColliderFrame {
                    Seconds = i * .2, State = (uint)(k == 2 && i == 2 ? 2 : 3), Shape = shape
                });
            }
        }
        Check(System.Runtime.InteropServices.Marshal.SizeOf(typeof(InputPad)) == 12, "XInput gamepad ABI");
        var devices =
            new InputDeviceFrame { Seconds = 0,
                                   Frame = 0,
                                   Tick = 0,
                                   State = 1 | 4 | 512,
                                   Flags = 257,
                                   Mouse = 5,
                                   K0 = 1UL << 32,
                                   K2 = 1UL << 34,
                                   Sprint = 1,
                                   MoveX = .5f,
                                   MoveY = 1,
                                   LookX = .75f,
                                   LookY = -.25f,
                                   JoyX = .2f,
                                   JoyY = -.1f,
                                   P0 = new InputPad { Buttons = 4096, LeftTrigger = 12, RightTrigger = 240,
                                                       LX = -32768, LY = 32767, RX = 1234, RY = -5678 } };
        motion.ObserveDevices(devices);
        devices.Seconds = .01;
        motion.ObserveDevices(devices);
        Check(motion.Devices.Count == 1 && motion.DeviceObservedThrough == .01,
              "unchanged observations advance coverage without duplicate samples");
        devices.Seconds = .2;
        devices.Frame = 4;
        devices.Tick = 8;
        devices.K0 = 0;
        motion.ObserveDevices(devices);
        Check(motion.Devices.Count == 2, "full keyboard transitions retained");
        var capped = new ReplayMotionData();
        capped.ObserveDevices(devices);
        capped.DeviceDropped = 1;
        devices.Seconds = .3;
        capped.ObserveDevices(devices);
        Check(
            capped.DeviceObservedThrough == .2 && capped.Devices.Count == 1 && capped.DeviceDropped == 2,
            "device capacity loss freezes reliable coverage, even when keys return to the last observed state");
        motion.Visuals.Recorded = true;
        int texture = motion.Visuals.Asset("T|Original wall|256|256"),
            spriteId = motion.Visuals.Asset("S|Monster frame|T|Monster|128|128|0|0|128|128");
        var visualOwner = motion.Visuals.Owner(2, 1, 1, "0:Monster");
        for (int kind = 1; kind <= 4; kind++)
        {
            var track = motion.Visuals.Track(visualOwner, kind, "Pose " + kind);
            for (int i = 0; i < 3; i++)
                motion.Visuals.Add(track, new ReplayVisualFrame {
                    Seconds = motion.Seconds * i / 2, Position = new Vector3(i, 2, 3), Rotation = identity,
                    Scale = new Vector3(1, 1, 1), Color = new Color(.2f, .3f, .4f, 1), Flags = 1,
                    Asset = kind == 3 ? spriteId : 0, Texture = kind == 2 ? texture : 0, Intensity = 2,
                    Range = 15, Angle = 60, InnerAngle = 30
                });
        }
        var visualClone = ReplayMotionData.Read(Encode(motion));
        Check(visualClone.Visuals.Recorded && visualClone.Visuals.Count == 12 &&
                  visualClone.Visuals.TrackCount == 4 &&
                  visualClone.Visuals.Owners[0].Tracks[3].Frames[2].Range == 15,
              "original pose/sprite/material/light observations roundtrip");
        Check(visualClone.Visuals.Owners[0].Tracks[0].At(-1) == -1 &&
                  visualClone.Visuals.Owners[0].Tracks[0].At(motion.Seconds / 2) == 1,
              "visual boundary seek is absolute and exact");
        var visualTrack = motion.Visuals.Owners[0].Tracks[0];
        var goodVisual = visualTrack.Frames[0];
        var badVisual = goodVisual;
        badVisual.Rotation = new Quaternion(0, 0, 0, 0);
        visualTrack.Frames[0] = badVisual;
        Reject(Encode(motion));
        visualTrack.Frames[0] = goodVisual;
        var duplicateOwner = motion.Visuals.Owner(2, 1, 1, "duplicate");
        Reject(Encode(motion));
        motion.Visuals.Owners.Remove(duplicateOwner);
        var lightTrack = visualOwner.Tracks[3];
        badVisual = lightTrack.Frames[0];
        var lightGood = badVisual;
        badVisual.Range = -1;
        lightTrack.Frames[0] = badVisual;
        Reject(Encode(motion));
        lightTrack.Frames[0] = lightGood;
        var visualMethods = new[] { "CopyRenderers", "CopyLight", "CopyTransform" };
        int componentCopies = 0;
        foreach (var name in visualMethods)
        {
            var il = (List<CodeInstruction>)typeof(ManagedTests)
                         .GetMethod("MethodInstructions", BindingFlags.Static | BindingFlags.NonPublic)
                         .Invoke(null, new object[] { typeof(ReplayNativeGraphics)
                                                          .GetMethod(name, BindingFlags.Instance |
                                                                               BindingFlags.NonPublic) });
            foreach (var instruction in il)
            {
                var method = instruction.operand as MethodInfo;
                if (method == null)
                    continue;
                Check(!(method.DeclaringType == typeof(UnityEngine.Object) && method.Name == "Instantiate"),
                      "no live actor prefab can be cloned into native replay");
                if (method.DeclaringType == typeof(GameObject) && method.Name == "AddComponent" &&
                    method.IsGenericMethod)
                {
                    var type = method.GetGenericArguments()[0];
                    Check(type == typeof(MeshFilter) || type == typeof(MeshRenderer) ||
                              type == typeof(SkinnedMeshRenderer) || type == typeof(SpriteRenderer) ||
                              type == typeof(Light),
                          "native proxies only create visual components");
                    componentCopies++;
                }
            }
        }
        Check(componentCopies == 5, "all five visual component copy paths audited in actual built DLL");
        Console.WriteLine(
            "Original graphics: pose/sprite/material/light format, exact seeks, corruption rejection and actual DLL visual-only component audit: PASS");
        map.Review.Motion = motion.Info;
        byte[] bytes = Encode(motion);
        int visualLength;
        using (var vs = new MemoryStream())
        {
            using (var vw = new BinaryWriter(vs, System.Text.Encoding.UTF8, true)) motion.Visuals.Write(vw);
            visualLength = (int)vs.Length;
        }
        int v3body = bytes.Length - 8 - visualLength - ExperienceLength(motion) - CoverageLength(motion);
        var v3 = new byte[v3body + 8];
        Array.Copy(bytes, v3, v3body);
        Array.Copy(BitConverter.GetBytes(3), 0, v3, 4, 4);
        Array.Copy(BitConverter.GetBytes(PracticeMapData.Checksum(v3, v3body)), 0, v3, v3body, 8);
        Check(ReplayMotionData.Read(v3).Devices.Count == 2 && !ReplayMotionData.Read(v3).Visuals.Recorded,
              "v3 has devices without invented visual history");
        File.WriteAllBytes(Path.Combine(folder, "input-v3.isinput"), v3);
        int v2body = bytes.Length - 8 - 16 - motion.Devices.Count * InputDeviceFrame.WireSize - visualLength -
                     ExperienceLength(motion) - CoverageLength(motion);
        byte[] v2 = new byte[v2body + 8];
        Array.Copy(bytes, v2, v2body);
        Array.Copy(BitConverter.GetBytes(2), 0, v2, 4, 4);
        Array.Copy(BitConverter.GetBytes(PracticeMapData.Checksum(v2, v2body)), 0, v2, v2body, 8);
        Check(ReplayMotionData.Read(v2).Devices.Count == 0,
              "v2 remains readable without fabricated native devices");
        File.WriteAllBytes(Path.Combine(folder, "input-v2.isinput"), v2);
        var decoded = ReplayMotionData.Read(bytes);
        Check(decoded.Devices.Count == 2 && decoded.Devices[0].K2 == (1UL << 34) &&
                  decoded.Devices[0].P0.RY == -5678 && decoded.Devices[0].P0.RightTrigger == 240 &&
                  decoded.Devices[0].LookX == .75f && decoded.Devices[0].MoveY == 1 &&
                  decoded.DeviceObservedThrough == .2,
              "device v3 keyboard, pad, axes and coverage round trip");
        Check(decoded.Matches(map) && decoded.Frames.Count == 21 && decoded.Ticks.Count == 41 &&
                  decoded.Events.Count == 4,
              "cross-stream identity and counts");
        Check(decoded.Frames[0].LampIntensity == 2.5f && decoded.Frames[0].LampAngle == 62 &&
                  decoded.Frames[0].LampPosition.y == 2 && decoded.Frames[4].MouseX == 1 &&
                  decoded.Frames[0].Fov == 75 && decoded.Frames[0].Horizontal == .5f &&
                  decoded.Events[0].Down == 0,
              "camera axes, actual FOV, smoothed movement and initially-held key preserved");
        Check((decoded.Events[1].State & 8) == 0 && (decoded.Events[2].State & 8) != 0,
              "radar request and actual acceptance are distinct");
        Check(decoded.Colliders.Recorded && decoded.Colliders.Count == 12 &&
                  decoded.Colliders.Tracks.Count == 4 &&
                  decoded.Colliders.Tracks[3].Samples[1].Shape.Height == 1.1f &&
                  decoded.Colliders.Tracks[1].Samples[0].Shape.Matrix.m03 == 2 &&
                  decoded.Colliders.Tracks[2].Samples[2].State == 2,
              "actual collider types, transforms, dimension and active-state history preserved");
        ReplayColliderFrame observed;
        Check(!decoded.Colliders.Tracks[2].At(.4, out observed) &&
                  decoded.Colliders.Tracks[0].At(.2, out observed) &&
                  !decoded.Colliders.Tracks[0].At(1.1, out observed),
              "held observations never span stale gaps / disabled states");
        var goodShape = motion.Colliders.Tracks[0].Samples[0];
        var invalid = goodShape;
        var badShape = invalid.Shape;
        badShape.Matrix.m03 = float.NaN;
        invalid.Shape = badShape;
        motion.Colliders.Tracks[0].Samples[0] = invalid;
        Reject(Encode(motion));
        motion.Colliders.Tracks[0].Samples[0] = goodShape;
        motion.Colliders.Count++;
        Reject(Encode(motion));
        motion.Colliders.Count--;
        motion.Colliders.Flags = 8;
        Reject(Encode(motion));
        motion.Colliders.Flags = 0;
        File.WriteAllBytes(Path.Combine(folder, "input-wire.isinput"), bytes);
        File.WriteAllBytes(Path.Combine(folder, "input-map.isrun"), map.Encode());
        foreach (int n in new[] { 0, 1, 63, 100, bytes.Length - 1 })
        {
            var shortFile = new byte[n];
            Array.Copy(bytes, shortFile, n);
            Reject(shortFile);
        }
        var corrupt = (byte[])bytes.Clone();
        corrupt[100] ^= 1;
        Reject(corrupt);
        var first = motion.Frames[0];
        first.CameraRotation = new Quaternion(0, 0, 0, 0);
        motion.Frames[0] = first;
        Reject(Encode(motion));
        first.CameraRotation = identity;
        motion.Frames[0] = first;
        first.Fov = float.NaN;
        motion.Frames[0] = first;
        Reject(Encode(motion));
        first.Fov = 75;
        motion.Frames[0] = first;
        var wrong = motion.Events[2];
        wrong.Up = 0;
        motion.Events[2] = wrong;
        Reject(Encode(motion));
        wrong.Up = 64;
        motion.Events[2] = wrong;
        var late = motion.Frames[3];
        late.Seconds = double.NaN;
        motion.Frames[3] = late;
        Reject(Encode(motion));
        late.Seconds = motion.Seconds * 3 / 20;
        motion.Frames[3] = late;
        map.Seed++;
        Check(!decoded.Matches(map), "same ID cannot attach inputs from another seed");
        map.Seed--;
        map.Review.Motion.Dropped++;
        Check(!decoded.Matches(map), "capture quality cannot be silently replaced");
        map.Review.Motion.Dropped--;
        var instructions = (List<CodeInstruction>)typeof(ManagedTests)
                               .GetMethod("MethodInstructions", BindingFlags.Static | BindingFlags.NonPublic)
                               .Invoke(null, new object[] { typeof(MouseLook).GetMethod(
                                                 "Update", BindingFlags.Instance | BindingFlags.NonPublic) });
        int axis = 0, keys = 0, buttons = 0;
        foreach (var i in instructions)
        {
            var m = i.operand as MethodBase;
            if (i.opcode == OpCodes.Call && m != null && m.DeclaringType == typeof(Input))
            {
                if (m.Name == "GetAxis")
                    axis++;
                if (m.Name == "GetKeyDown" && m.GetParameters()[0].ParameterType == typeof(string))
                    keys++;
                if (m.Name == "GetButtonDown")
                    buttons++;
            }
        }
        int observedAxis = 0, observedKeys = 0, observedButtons = 0;
        var rewritten = new List<CodeInstruction>(ReplayInputCapture.ObserveUpdate(instructions));
        foreach (var i in rewritten)
        {
            var m = i.operand as MethodBase;
            if (m != null && m.DeclaringType == typeof(ReplayInputCapture))
            {
                if (m.Name == "Axis")
                    observedAxis++;
                if (m.Name == "KeyDown")
                    observedKeys++;
                if (m.Name == "ButtonDown")
                    observedButtons++;
            }
        }
        Check(axis > 3 && keys > 0 && buttons > 0 && observedAxis == axis && observedKeys == keys &&
                  observedButtons == buttons && rewritten.Count == instructions.Count,
              "original Update reads replaced once with unchanged instruction count");
        foreach (var pair in new[] { new[] { "Axis", "GetAxis" }, new[] { "KeyDown", "GetKeyDown" },
                                     new[] { "ButtonDown", "GetButtonDown" } })
        {
            var il = (List<CodeInstruction>)typeof(ManagedTests)
                         .GetMethod("MethodInstructions", BindingFlags.Static | BindingFlags.NonPublic)
                         .Invoke(null, new object[] { typeof(ReplayInputCapture).GetMethod(pair[0]) });
            int reads = 0;
            foreach (var i in il)
            {
                var m = i.operand as MethodBase;
                if (m != null && m.DeclaringType == typeof(Input) && m.Name == pair[1])
                    reads++;
            }
            Check(reads == 1, "pass-through original input read exactly once");
        }
        Check(typeof(ReplayInputCapture).GetMethod("Physics").GetParameters()[0].Name == "__instance",
              "Harmony physics binds actual instance");
        var same = typeof(Replay3D).GetMethod("SameLayout", BindingFlags.Static | BindingFlags.NonPublic);
        Check((bool)same.Invoke(null, new object[] { map, map.Copy() }), "3D checks original layout");
        var other = map.Copy();
        other.Points = (Vector3[])map.Points.Clone();
        other.Points[other.Selected[0]] += new Vector3(1, 0, 0);
        Check(!(bool)same.Invoke(null, new object[] { map, other }), "3D rejects wrong crystal geometry");
        var state = typeof(Replay3D).GetField("state", BindingFlags.Static | BindingFlags.NonPublic);
        state.SetValue(null, 2);
        Check(!Replay3D.AllowPlayerUpdate() && !Replay3D.AllowPlayerTrigger(),
              "replay blocks gameplay input and pickups");
        state.SetValue(null, 0);
        Check(Replay3D.AllowPlayerUpdate() && Replay3D.AllowPlayerTrigger(), "normal gameplay restored");
        var orbit = (ReplayFollowOrbit) typeof(Replay3D)
                        .GetField("followOrbit", BindingFlags.Static | BindingFlags.NonPublic)
                        .GetValue(null);
        orbit.Reset();
        orbit.Zoom(3);
        float zoom = orbit.Radius;
        var configure = typeof(Replay3D).GetMethod("Configure", BindingFlags.Static | BindingFlags.NonPublic);
        configure.Invoke(null, new object[] {
            new TrainerCommands { Replay3dOptions = Replay3DOptions.Mask | Replay3DOptions.Seek,
                                  Replay3dFollowDistance = 12, Replay3dFollowHeight = 5 }
        });
        Check(
            orbit.Radius == zoom &&
                (uint) typeof(Replay3D)
                        .GetField("options", BindingFlags.Static | BindingFlags.NonPublic)
                        .GetValue(null) == (Replay3DOptions.Mask & ~Replay3DOptions.Follow),
            "ordinary GUI commands / seek never reset local wheel zoom; transient seek bit is not a display option");
        orbit.Reset();
        var stage = (List<CodeInstruction>)typeof(ManagedTests)
                        .GetMethod("MethodInstructions", BindingFlags.Static | BindingFlags.NonPublic)
                        .Invoke(null, new object[] { typeof(Replay3D).GetMethod(
                                          "BuildStage", BindingFlags.Static | BindingFlags.NonPublic) });
        int inclusion = -1, apply = -1;
        for (int i = 0; i < stage.Count; i++)
        {
            var call = stage[i].operand as MethodBase;
            if (call == null)
                continue;
            Check(!(call.DeclaringType == typeof(Camera) && call.Name == "set_cullingMask"),
                  "scene traversal cannot widen original camera mask");
            if (call.DeclaringType == typeof(ReplaySceneVisibility) && call.Name == "Included")
                inclusion = i;
            if (call.DeclaringType == typeof(ReplayRendererScope) && call.Name == "Apply")
                apply = i;
        }
        Check(inclusion >= 0 && apply > inclusion,
              "only original gameplay-visible layers reach renderer styling");
        var follow = (List<CodeInstruction>)typeof(ManagedTests)
                         .GetMethod("MethodInstructions", BindingFlags.Static | BindingFlags.NonPublic)
                         .Invoke(null, new object[] { typeof(Replay3D).GetMethod(
                                           "Follow", BindingFlags.Static | BindingFlags.NonPublic) });
        bool focusPose = false;
        foreach (var i in follow)
        {
            var call = i.operand as MethodBase;
            if (call != null && call.DeclaringType == typeof(ReplayFollowOrbit))
            {
                Check(call.Name != "Pose", "following camera cannot add guessed body-height offset");
                focusPose |= call.Name == "PoseFocus";
            }
        }
        Check(focusPose, "following camera uses recorded camera focus");
        Check(typeof(Replay3D).Assembly.GetType("ReplaySurfaceGeometry") == null &&
                  typeof(Replay3D).Assembly.GetType("ReplayHorizontalBatch") == null,
              "7.3.4 GPU readback / split mesh pipeline is removed from actual module");
        var radarTypes = typeof(MouseLook).GetNestedTypes(BindingFlags.Public | BindingFlags.NonPublic);
        var radarNames = new HashSet<string>();
        foreach (var type in radarTypes)
            if (type.Name.StartsWith("<RadarBlink>", StringComparison.Ordinal))
            {
                var il = (List<CodeInstruction>)typeof(ManagedTests)
                             .GetMethod("MethodInstructions", BindingFlags.Static | BindingFlags.NonPublic)
                             .Invoke(null, new object[] { type.GetMethod(
                                               "MoveNext", BindingFlags.Instance | BindingFlags.Public |
                                                               BindingFlags.NonPublic) });
                foreach (var i in il)
                    if (i.opcode == OpCodes.Ldstr)
                        radarNames.Add((string)i.operand);
            }
        foreach (string name in new[] { "Radar", "Enemy2", "Walls", "RadarSphere" })
            Check(radarNames.Contains(name), "actual game maintains separate radar layer catalog: " + name);
        Console.WriteLine(
            "Actual replay IL: original camera layer mask, recorded focus pose and complete 7.3.4 mesh-pipeline rollback; actual game radar layer separation: PASS");
        Check(SharedMemoryProtocol.Version == 25 &&
                  SharedMemoryProtocol.MapName.EndsWith("v25", StringComparison.Ordinal),
              "new view options use a matched protocol channel");
        var undo = new List<int>();
        var scope = new ReplayRestoreScope();
        scope.Add(() => undo.Add(1));
        scope.Add(() =>
                  {
                      undo.Add(2);
                      throw new InvalidOperationException("simulated renderer failure");
                  });
        scope.Add(() => undo.Add(3));
        scope.Dispose();
        scope.Dispose();
        Check(undo.Count == 3 && undo[0] == 3 && undo[1] == 2 && undo[2] == 1 && scope.Error != null,
              "rollback restores every remaining state once after a failure");
        var updates = new HashSet<MethodInfo>(ReplayGamePause.Updates());
        foreach (var type in new[] { typeof(MouseLook), typeof(HideAndSeekIt), typeof(MonsterController),
                                     typeof(Monster3), typeof(GhostScript), typeof(WallAI) })
            Check(updates.Contains(type.GetMethod("Update", BindingFlags.Instance | BindingFlags.NonPublic |
                                                                BindingFlags.Public)),
                  "actual actor Update pause coverage: " + type.Name);
        Check(!updates.Contains(typeof(VolumetricLight)
                                    .GetMethod("Update", BindingFlags.Instance | BindingFlags.NonPublic)) &&
                  !updates.Contains(typeof(VolumetricLightRenderer)
                                        .GetMethod("Update", BindingFlags.Instance | BindingFlags.NonPublic)),
              "original pure-render command clearing and target resizing remain active");
        var routines = new HashSet<MethodInfo>(ReplayGamePause.Coroutines());
        var catalogs = new HashSet<string>();
        int cullers = 0;
        foreach (var type in typeof(HideAndSeekIt).GetNestedTypes(BindingFlags.NonPublic))
            if (type.Name.StartsWith("<Occlude", StringComparison.Ordinal))
            {
                var method = type.GetMethod("MoveNext", BindingFlags.Instance | BindingFlags.NonPublic);
                Check(routines.Contains(method),
                      "actual culling coroutine is frozen without losing its iterator");
                cullers++;
                var il = (List<CodeInstruction>)typeof(ManagedTests)
                             .GetMethod("MethodInstructions", BindingFlags.Static | BindingFlags.NonPublic)
                             .Invoke(null, new object[] { method });
                bool activation = false;
                foreach (var instruction in il)
                {
                    var field = instruction.operand as FieldInfo;
                    if (field != null && field.DeclaringType == typeof(HideAndSeekIt) &&
                        field.FieldType == typeof(GameObject[]))
                        catalogs.Add(field.Name);
                    var call = instruction.operand as MethodBase;
                    if (call != null && call.DeclaringType == typeof(GameObject) && call.Name == "SetActive")
                        activation = true;
                }
                Check(activation, "original culling disables whole game objects, not just renderers");
            }
        Check(cullers == 2 && catalogs.SetEquals(new[] { "AllWalls", "AllProps", "CielingLights" }),
              "only original distance-culling catalogs are revealed");
        bool continuation = false;
        ReplayGamePause.Frozen = true;
        Check(!ReplayGamePause.AllowUpdate() && !ReplayGamePause.AllowCoroutine(ref continuation) &&
                  continuation,
              "frozen game routine keeps yielding without advancing");
        ReplayGamePause.Frozen = false;
        continuation = false;
        Check(ReplayGamePause.AllowUpdate() && ReplayGamePause.AllowCoroutine(ref continuation) &&
                  !continuation,
              "normal gameplay keeps original callback results");
        Console.WriteLine(
            "Actual game IL: complete actor/coroutine pause targets, 2 distance cullers, exact wall/prop/ceiling-light catalogs and reversible pause contract: PASS");
        var path = Path.Combine(folder, "input-store-test");
        if (Directory.Exists(path))
            Directory.Delete(path, true);
        using (var store = new RunStore(path))
            store.QueueReview(motion.Id, map.Encode(), motion, "{\"runId\":" + motion.Id + "}");
        var stored = Path.Combine(path, "RunReviews", "run-" + motion.Id);
        Check(File.Exists(stored + ".isrun") && File.Exists(stored + ".validation.json") &&
                  ReplayMotionData.Read(File.ReadAllBytes(stored + ".isinput")).Matches(map),
              "background writer commits complete companions");
        var stress = new ReplayMotionData { Id = 1, Scene = 3, Seconds = 100, Hash = map.Hash };
        stress.Info.Tool = "7.2.0";
        var timer = Stopwatch.StartNew();
        for (int i = 0; i < 24000; i++)
            stress.Add(new ReplayViewFrame { Seconds = i / 240.0, Frame = (uint)i, Tick = (uint)(i / 5),
                                             CameraRotation = identity, BodyRotation = identity,
                                             LampRotation = identity, Fov = 60 });
        timer.Stop();
        var encode = Stopwatch.StartNew();
        byte[] heavy = Encode(stress);
        encode.Stop();
        Check(heavy.Length < 4 * 1024 * 1024 && ReplayMotionData.Read(heavy).Frames.Count == 24000,
              "100 s / 240 fps camera capture roundtrip stays below 4 MiB");
        Console.WriteLine("Capture model: 24000 frames appended in " +
                          timer.Elapsed.TotalMilliseconds.ToString("F2") + " ms; sequential encoding " +
                          encode.Elapsed.TotalMilliseconds.ToString("F2") + " ms / " + heavy.Length +
                          " bytes (host model, not live Unity overhead)");
        Console.WriteLine(
            "Input recording: real game IL/pass-through, render/physics/event clocks, radar acceptance, held keys, camera data, strict corruption rejection, identity, layout gating and complete async persistence: PASS");
        return 0;
    }
}
