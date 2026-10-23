using System;
using System.Collections.Generic;
using System.Reflection.Emit;
using HarmonyLib;
using UnityEngine;

// Game-local observations. Never install system-wide keyboard/mouse hooks.
public static class ReplayInputCapture
{
    private static ReplayMotionData active;
    private static MouseLook player;
    private static int firstFrame;
    private static uint tick, keys, reads, actions;
    private static bool inside;
    private static float horizontal, vertical, mx, my, jx, jy;
    public static ReplayMotionData Current
    {
        get {
            return active;
        }
    }
    private static double Now
    {
        get {
            return SpeedrunManager.Instance == null ? 0 : SpeedrunManager.Instance.CurrentSeconds;
        }
    }
    private static uint Frame
    {
        get {
            return (uint)Math.Max(0, Time.frameCount - firstFrame);
        }
    }
    private static uint State
    {
        get {
            return LiveInputCapture.PlayerState(player);
        }
    }
    public static uint RecordFrame
    {
        get {
            return Frame;
        }
    }
    public static uint RecordTick
    {
        get {
            return tick;
        }
    }

    public static void Begin(PracticeMapData map, MouseLook mouse, RunRecord record)
    {
        Stop(true);
        if (map == null || map.Review == null || mouse == null)
            return;
        player = mouse;
        firstFrame = Time.frameCount;
        tick = keys = reads = actions = 0;
        horizontal = vertical = mx = my = jx = jy = 0;
        var e = record.Environment;
        active = new ReplayMotionData { Id = record.Id, Scene = map.Scene, Seed = map.Seed, Hash = map.Hash };
        active.Info.Tool = e == null ? ItStealsMod.Version : e.ToolVersion;
        active.Info.Game = e == null ? "" : e.GameVersion;
        active.Info.Unity = e == null ? "" : e.UnityVersion;
        map.Review.Motion = active.Info;
        foreach (int channel in new[] { ReplayCoverage.Movement, ReplayCoverage.Camera,
                                        ReplayCoverage.Inputs })
        {
            active.Coverage.Start(channel);
            active.Coverage.Source(channel);
        }
        LiveInputCapture.BeginRun();
        keys = KeyMask();
        active.Add(new ReplayInputEvent { Seconds = 0, Frame = 0, Tick = 0, Keys = keys, State = State });
        ReplayAudioCapture.Begin(active, player);
        ReplayExperienceCapture.Begin(active, player);
        ReplayColliderCapture.Begin(active, player, map);
        ReplayVisualCapture.Begin(active, player, map);
        Camera.onPreCull += CameraFrame;
        Capture(PlayerContext.View(player), 0);
    }

    public static void Stop(bool keepAudioPrepared = false)
    {
        Camera.onPreCull -= CameraFrame;
        inside = false;
        ReplayColliderCapture.Stop();
        ReplayVisualCapture.Stop();
        ReplayExperienceCapture.Stop();
        ReplayAudioCapture.Stop(keepAudioPrepared);
        active = null;
        player = null;
    }

    private static uint KeyMask()
    {
        uint k = 0;
        if (Input.GetKey(KeyCode.W))
            k |= 1;
        if (Input.GetKey(KeyCode.A))
            k |= 2;
        if (Input.GetKey(KeyCode.S))
            k |= 4;
        if (Input.GetKey(KeyCode.D))
            k |= 8;
        if (Input.GetKey(KeyCode.LeftShift))
            k |= 16;
        if (Input.GetKey(KeyCode.RightShift))
            k |= 32;
        if (Input.GetKey(KeyCode.R))
            k |= 64;
        return k;
    }

    public static void BeforeUpdate(MouseLook __instance)
    {
        inside = active != null && __instance == player;
        if (inside)
        {
            reads &= 3;
            actions = 0;
            mx = my = jx = jy = 0;
        }
    }

    public static void AfterUpdate(MouseLook __instance)
    {
        inside = false;
        try
        {
            LiveInputCapture.Observe(__instance);
        }
        catch (Exception)
        {
            LiveInputCapture.Unavailable();
            if (active != null)
                active.Info.Flags |= 2;
        }
        if (active == null || __instance != player)
            return;
        try
        {
            uint next = KeyMask(), state = State;
            if (next != keys || actions != 0 || active.Events.Count == 0 ||
                active.Events[active.Events.Count - 1].State != state)
            {
                active.Add(new ReplayInputEvent { Seconds = Now, Frame = Frame, Tick = tick, Keys = next,
                                                  Down = next & ~keys, Up = keys & ~next, Actions = actions,
                                                  State = state });
                keys = next;
            }
        }
        catch (Exception)
        {
            active.Info.Flags |= 2;
            active.Coverage.Fail(ReplayCoverage.Inputs);
        }
    }

    public static void UpdateFailed(Exception __exception)
    {
        inside = false;
        if (__exception != null && active != null)
            active.Info.Flags |= 2;
    }

    public static float Axis(string name)
    {
        float value = Input.GetAxis(name);
        ObserveAxis(name, value);
        return value;
    }

    public static void ObserveAxis(string name, float value)
    {
        LiveInputCapture.Axis(name, value);
        if (active == null)
            return;
        switch (name)
        {
        case "Horizontal":
            horizontal = value;
            reads |= 1;
            break;
        case "Vertical":
            vertical = value;
            reads |= 2;
            break;
        case "Mouse X":
            if (inside)
            {
                mx = value;
                reads |= 4;
            }
            break;
        case "Mouse Y":
            if (inside)
            {
                my = value;
                reads |= 8;
            }
            break;
        case "Joystick X":
            if (inside)
            {
                jx = value;
                reads |= 16;
            }
            break;
        case "Joystick Y":
            if (inside)
            {
                jy = value;
                reads |= 32;
            }
            break;
        }
    }

    public static bool KeyDown(string name)
    {
        bool value = Input.GetKeyDown(name);
        if (inside && value && name == "r")
            actions |= 1;
        return value;
    }

    public static bool ButtonDown(string name)
    {
        bool value = Input.GetButtonDown(name);
        if (inside && value && name == "Radar")
            actions |= 2;
        return value;
    }

    public static IEnumerable<CodeInstruction> ObserveUpdate(IEnumerable<CodeInstruction> instructions)
    {
        foreach (var i in instructions)
        {
            if (i.opcode == OpCodes.Call)
            {
                foreach (var pair in new[] { new[] { "GetAxis", "Axis" }, new[] { "GetKeyDown", "KeyDown" },
                                             new[] { "GetButtonDown", "ButtonDown" } })
                {
                    var m = typeof(Input).GetMethod(pair[0], new[] { typeof(string) });
                    if (Equals(i.operand, m))
                    {
                        i.operand = typeof(ReplayInputCapture).GetMethod(pair[1]);
                        break;
                    }
                }
            }
            yield return i;
        }
    }

    public static void Physics(MouseLook __instance)
    {
        LiveInputCapture.Physics();
        if (active == null || __instance != player)
            return;
        tick++;
        ReplayColliderCapture.Physics(Now);
        try
        {
            var body = PlayerContext.Body(__instance);
            if (body == null)
            {
                active.Info.Flags |= 4;
                active.Coverage.Fail(ReplayCoverage.Movement);
                return;
            }
            var f = new ReplayPhysicsFrame { Seconds = Now,
                                             Frame = Frame,
                                             Tick = tick,
                                             State = State,
                                             Horizontal = horizontal,
                                             Vertical = vertical,
                                             Position = body.position,
                                             Rotation = body.rotation };
            if (!Finite(f.Position) || !Finite(horizontal) || !Finite(vertical))
            {
                active.Info.Flags |= 2;
                active.Coverage.Fail(ReplayCoverage.Movement);
                return;
            }
            int count = active.Ticks.Count;
            active.Add(f);
            if (active.Ticks.Count > count)
                active.Coverage.Observe(ReplayCoverage.Movement, f.Seconds);
            else
                active.Coverage.Fail(ReplayCoverage.Movement);
        }
        catch (Exception)
        {
            active.Info.Flags |= 2;
            active.Coverage.Fail(ReplayCoverage.Movement);
        }
    }

    private static bool Finite(float f)
    {
        return !float.IsNaN(f) && !float.IsInfinity(f) && Math.Abs(f) <= 100000;
    }

    private static bool Finite(Vector3 p)
    {
        return Finite(p.x) && Finite(p.y) && Finite(p.z);
    }

    private static bool Rotation(Quaternion q)
    {
        double n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        return Finite(q.x) && Finite(q.y) && Finite(q.z) && Finite(q.w) && n >= .98 && n <= 1.02;
    }

    private static void CameraFrame(Camera camera)
    {
        if (active == null || player == null)
            return;
        try
        {
            if (camera == PlayerContext.View(player))
                Capture(camera, Now);
        }
        catch (Exception)
        {
            if (active != null)
            {
                active.Info.Flags |= 2;
                active.Coverage.Fail(ReplayCoverage.Camera);
            }
        }
    }

    public static void CaptureInactiveCamera()
    {
        if (active == null || player == null)
            return;
        try
        {
            var camera = player.mainCamera;
            if (camera != null && (!camera.enabled || !camera.gameObject.activeInHierarchy))
                Capture(camera, Now);
        }
        catch (Exception)
        {
            if (active != null)
            {
                active.Info.Flags |= 2;
                active.Coverage.Fail(ReplayCoverage.Camera);
            }
        }
    }

    private static void Capture(Camera cam, double seconds)
    {
        if (active == null || cam == null || player == null)
            return;
        ReplayColliderCapture.Camera(seconds);
        ReplayAudioCapture.Sync(seconds);
        bool animationCut = ReplayExperienceCapture.Sample(seconds);
        ReplayVisualCapture.Sample(seconds, false, animationCut);
        try
        {
            var body = PlayerContext.Body(player);
            if (body == null)
            {
                active.Coverage.Fail(ReplayCoverage.Camera);
                return;
            }
            var f =
                new ReplayViewFrame { Seconds = seconds,
                                      Frame = Frame,
                                      Tick = tick,
                                      State = State,
                                      Keys = keys,
                                      Reads = reads,
                                      Horizontal = horizontal,
                                      Vertical = vertical,
                                      MouseX = mx,
                                      MouseY = my,
                                      JoystickX = jx,
                                      JoystickY = jy,
                                      Body = body.position,
                                      BodyRotation = body.rotation,
                                      Camera = cam.transform.position,
                                      CameraRotation = cam.transform.rotation,
                                      Fov = cam.fieldOfView,
                                      LampRotation = player.flashlight != null ? player.flashlight.rotation
                                                                               : cam.transform.rotation };
            var light = player.playerFlashlight;
            if (light != null)
            {
                var color = light.color;
                f.LampPosition = light.transform.position;
                f.LampRotation = light.transform.rotation;
                f.LampColor = new Vector3(color.r, color.g, color.b);
                f.LampIntensity = light.intensity;
                f.LampRange = light.range;
                f.LampAngle = light.spotAngle;
                f.LampType = (uint)light.type;
            }
            if (!Finite(f.Body) || !Finite(f.Camera) || !Finite(f.Fov) || f.Fov <= 0 || f.Fov > 179 ||
                !Finite(mx) || !Finite(my) || !Finite(jx) || !Finite(jy) || !Rotation(f.BodyRotation) ||
                !Rotation(f.CameraRotation) || !Rotation(f.LampRotation) || !Finite(f.LampPosition) ||
                !Finite(f.LampColor) || !Finite(f.LampIntensity) || f.LampIntensity < 0 ||
                !Finite(f.LampRange) || f.LampRange < 0 || !Finite(f.LampAngle) || f.LampAngle < 0 ||
                f.LampAngle > 179 || f.LampType > 4)
            {
                active.Info.Flags |= 2;
                active.Coverage.Fail(ReplayCoverage.Camera);
                return;
            }
            int n = active.Frames.Count;
            if (n > 0 && active.Frames[n - 1].Frame == f.Frame && active.Frames[n - 1].Seconds > 0)
            {
                active.Frames[n - 1] = f;
                active.Coverage.Observe(ReplayCoverage.Camera, seconds);
            }
            else
            {
                if (n > 0 && f.Seconds - active.Frames[n - 1].Seconds > .25)
                    active.Info.Flags |= 4;
                active.Add(f);
                if (active.Frames.Count > n)
                    active.Coverage.Observe(ReplayCoverage.Camera, seconds);
                else
                    active.Coverage.Fail(ReplayCoverage.Camera);
            }
        }
        catch (Exception)
        {
            active.Info.Flags |= 2;
            active.Coverage.Fail(ReplayCoverage.Camera);
        }
    }

    public static ReplayMotionData Finish(double seconds)
    {
        if (active == null)
            return null;
        Capture(PlayerContext.View(player), seconds);
        var done = active;
        done.Seconds = seconds;
        ReplayColliderCapture.Finish(seconds);
        ReplayAudioCapture.Sync(seconds);
        ReplayExperienceCapture.Sample(seconds, true);
        ReplayVisualCapture.Sample(seconds, true);
        ReplayAudioCapture.Finish(seconds);
        done.Coverage.Seal(seconds);
        Stop();
        return done;
    }
}
