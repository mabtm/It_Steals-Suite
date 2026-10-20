using System;
using System.Collections.Generic;
using System.Reflection;
using UnityEngine;

// Contract doubles around the production Unity adapter. Actual PCM summation
// and on-disk integrity are exercised separately with ReplayAudioSession.
namespace UnityEngine
{
public class Object
{
    public static readonly List<Object> All = new List<Object>();
    public static T[] FindObjectsOfType<T>(bool inactive = false)
        where T : Object
    {
        var a = new List<T>();
        foreach (var o in All)
            if (o is T)
                a.Add((T)o);
        return a.ToArray();
    }
}

public class GameObject : Object
{
    readonly List<Component> components = new List<Component>();
    public T AddComponent<T>()
        where T : Component, new()
    {
        var c = new T { gameObject = this };
        components.Add(c);
        All.Add(c);
        return c;
    }
    public T GetComponent<T>()
        where T : Component
    {
        foreach (var c in components)
            if (c is T)
                return (T)c;
        return null;
    }
    public T[] GetComponents<T>()
        where T : Component
    {
        var a = new List<T>();
        foreach (var c in components)
            if (c is T)
                a.Add((T)c);
        return a.ToArray();
    }
}

public class Component : Object
{
    static int next;
    readonly int id = ++next;
    public GameObject gameObject;

    public int GetInstanceID()
    {
        return id;
    }
    public T GetComponent<T>()
        where T : Component
    {
        return gameObject.GetComponent<T>();
    }
    public T[] GetComponents<T>()
        where T : Component
    {
        return gameObject.GetComponents<T>();
    }
}

public class MonoBehaviour : Component
{
}

public class Camera : Component
{
}

public class AudioListener : Component
{
    public bool enabled = true;
}

public class AudioSource : Component
{
    public bool bypassListenerEffects, bypassEffects, spatialize, mute, isPlaying;
    public object outputAudioMixerGroup;
    public float volume = 1, spatialBlend, panStereo;
}

public static class Application
{
    public static string dataPath = "/game/It Steals_Data";
}

public enum AudioSpeakerMode
{
    Mono,
    Quad,
    Surround,
    Mode5point1,
    Mode7point1,
    Stereo
}

public static class AudioSettings
{
    public static double dspTime = 10;
    public static AudioSpeakerMode speakerMode = AudioSpeakerMode.Stereo;
    public static int outputSampleRate = 48000;

    public static void GetDSPBufferSize(out int length, out int buffers)
    {
        length = 1024;
        buffers = 4;
    }
}
}

public class MouseLook
{
    public Camera View;
}

public static class PlayerContext
{
    public static Camera View(MouseLook player)
    {
        return player.View;
    }
}

public class SpeedrunManager
{
    public static SpeedrunManager Instance = new SpeedrunManager();
    public double CurrentSeconds = 2;
}

public class ReplayAudioInfo
{
    public int Flags;
}

public class ReplayExperienceData
{
    public ReplayAudioInfo Audio = new ReplayAudioInfo();
}

public class ReplayMotionData
{
    public long Id = 1;
    public int Scene = 3, Seed = 42;
    public string Hash = "hash";
    public ReplayAudioSession AudioSession;
    public ReplayExperienceData Experience = new ReplayExperienceData();
}

public class ReplayAudioSession
{
    public readonly ReplayAudioInfo Info = new ReplayAudioInfo();
    public bool Done, Discarded;
    public int MainCalls, LaneCount;
    public long First;
    public double Ended = -1;

    public ReplayAudioSession(string p, long id, int scene, int seed, string hash, int rate, int channels,
                              int size)
    {
    }

    public ReplayAudioLane AddBypass()
    {
        LaneCount++;
        return new ReplayAudioLane();
    }

    public void Push(float[] data, int channels, long first)
    {
        MainCalls++;
        First = first;
    }

    public void MissingRoute()
    {
        Info.Flags |= 4;
    }

    public void End(double at)
    {
        Ended = at;
    }

    public void Discard()
    {
        Done = Discarded = true;
    }
}

public class ReplayAudioLane
{
    public int Calls;
    public float Gain;
    public long First;

    public void Push(float[] data, int channels, long first, float gain)
    {
        Calls++;
        Gain = gain;
        First = first;
    }
}

public static class ReplayAudioCaptureTests
{
    static void Check(bool yes, string message)
    {
        if (!yes)
            throw new Exception(message);
    }

    static AudioSource Source(bool bypass = false)
    {
        var s = new GameObject().AddComponent<AudioSource>();
        s.bypassListenerEffects = bypass;
        return s;
    }

    static void Callback(ReplayAudioTap tap, float[] block)
    {
        typeof(ReplayAudioTap)
            .GetMethod("OnAudioFilterRead", BindingFlags.Instance | BindingFlags.NonPublic)
            .Invoke(tap, new object[] { block, 2 });
    }

    public static int Main()
    {
        var go = new GameObject();
        var camera = go.AddComponent<Camera>();
        go.AddComponent<AudioListener>();
        var player = new MouseLook { View = camera };
        var normal = Source();
        var pickup = Source(true);
        pickup.volume = .253f;
        var spatial = Source(true);
        spatial.spatialBlend = 1;
        var effectsOff = Source(true);
        effectsOff.bypassEffects = true;
        var mixed = Source(true);
        mixed.outputAudioMixerGroup = new object();
        var motion = new ReplayMotionData();
        ReplayAudioCapture.Begin(motion, player);
        var session = motion.AudioSession;
        Check(session != null && ReplayAudioCapture.BypassSources == 1 && session.LaneCount == 1,
              "GetPoints route observed once; normal and mixer buses are never duplicated");
        Check(session.Info.Flags == 0, "unused unsupported sources do not claim missing audible samples");
        ReplayAudioCapture.Track(spatial);
        Check(session.Info.Flags == 4 && spatial.GetComponent<ReplayAudioTap>() == null &&
                  effectsOff.GetComponent<ReplayAudioTap>() == null,
              "unobservable 3D/disabled effects remain explicit and are never approximated");
        Check(normal.GetComponent<ReplayAudioTap>() == null && mixed.GetComponent<ReplayAudioTap>() == null,
              "only the bypass source receives a tap");
        var tap = pickup.GetComponent<ReplayAudioTap>();
        var block = new float[] { .4f, -.2f, .1f, -.1f };
        var copy = (float[])block.Clone();
        Callback(go.GetComponent<ReplayAudioTap>(), block);
        Callback(tap, block);
        var lane = tap.Current.Lane;
        Check(session.MainCalls == 1 && session.First == 96000 && lane.Calls == 1 &&
                  lane.First == session.First && Math.Abs(lane.Gain - .253f) < 1e-7,
              "listener/source callbacks share exact DSP origin and actual source gain");
        for (int n = 0; n < block.Length; n++)
            Check(block[n] == copy[n], "audio callbacks must be pass-through");
        ReplayAudioCapture.Track(pickup);
        Check(session.LaneCount == 1, "repeated original Play hooks do not duplicate capture");
        pickup.mute = true;
        ReplayAudioCapture.Sync(2);
        Callback(tap, block);
        Check(lane.Calls == 1, "muted source cannot leak into replay");
        pickup.mute = false;
        pickup.volume = .5f;
        ReplayAudioCapture.Sync(2);
        Callback(tap, block);
        Check(lane.Calls == 2 && lane.Gain == .5f,
              "live source volume followed without touching game settings");
        pickup.bypassListenerEffects = false;
        ReplayAudioCapture.Sync(2);
        Callback(tap, block);
        Check(lane.Calls == 2, "rerouted source cannot be mixed twice");
        pickup.bypassListenerEffects = true;
        ReplayAudioCapture.Sync(2);
        AudioSettings.dspTime = 12;
        ReplayAudioCapture.Sync(2.25);
        Callback(tap, block);
        Callback(go.GetComponent<ReplayAudioTap>(), block);
        Check(lane.First == 108000 && session.First == 108000,
              "published clock corrections reach every tap atomically");
        var dynamic = Source(true);
        ReplayAudioCapture.Track(dynamic);
        Check(session.LaneCount == 2, "new source tracked before original Play");
        ReplayAudioCapture.Finish(2.25);
        ReplayAudioCapture.Stop();
        Check(session.Ended == 2.25 && !session.Discarded, "finish retains routes for final DSP blocks");
        Callback(tap, block);
        Check(lane.Calls == 4, "last source block still reaches the closing session");
        Check(pickup.volume == .5f && pickup.bypassListenerEffects && !pickup.bypassEffects && !pickup.mute &&
                  spatial.spatialBlend == 1 && effectsOff.bypassEffects &&
                  mixed.outputAudioMixerGroup != null,
              "all original audio settings remain intact");
        Console.WriteLine(
            "Production capture adapter: actual GetPoints bypass configuration, shared DSP timestamps, pass-through PCM, source/mixer deduplication, live mute/gain/routing, closing blocks and untouched game settings: PASS");
        return 0;
    }
}
