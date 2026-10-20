using System;
using System.IO;
using System.Collections.Generic;
using UnityEngine;

// Every callback is pass-through: no buffer edits, scene access, disk I/O,
// allocation or locks. Source and listener taps share one published DSP clock.
public sealed class ReplayAudioTap : MonoBehaviour
{
    public sealed class Binding
    {
        public ReplayAudioSession Session;
        public double Origin;
        public int Rate;
    }

    public sealed class Clock
    {
        public volatile Binding Current;
    }

    public sealed class Route
    {
        public Clock Timeline;
        public ReplayAudioLane Lane;
        public float Gain = 1;
    }

    public volatile Route Current;

    private void OnAudioFilterRead(float[] data, int channels)
    {
        var route = Current;
        if (route == null)
            return;
        var b = route.Timeline.Current;
        if (b == null || b.Session.Done)
            return;
        long first = (long)Math.Round((AudioSettings.dspTime - b.Origin) * b.Rate);
        if (route.Lane == null)
            b.Session.Push(data, channels, first);
        else if (route.Gain > 0)
            route.Lane.Push(data, channels, first, route.Gain);
    }
}

public static class ReplayAudioCapture
{
    private sealed class Source
    {
        public AudioSource Audio;
        public ReplayAudioTap Tap;
        public ReplayAudioLane Lane;
        public float Gain;
    }

    private static ReplayAudioSession current;
    private static ReplayAudioTap.Clock clock;
    private static readonly Dictionary<int, Source> sources = new Dictionary<int, Source>();
    public static int BypassSources
    {
        get {
            return sources.Count;
        }
    }

    private static bool Bypasses(AudioSource source)
    {
        return source.bypassListenerEffects && source.outputAudioMixerGroup == null;
    }

    private static bool Readable(AudioSource source)
    {
        return !source.bypassEffects && !source.spatialize && source.spatialBlend == 0 &&
               source.panStereo == 0;
    }

    // Called before original Play/PlayOneShot, including newly created sources.
    // A mixer-routed source is already in the listener mix; never add it twice.
    public static void Track(AudioSource source, bool playRequested = true)
    {
        if (current == null || source == null)
            return;
        try
        {
            if (!Bypasses(source))
                return;
            bool audible = playRequested || source.isPlaying;
            // The supplied GetPoints source is centered 2D, with listener
            // bypass enabled and source effects enabled. A source tap observes
            // that exact PCM. Do not estimate a 3D/HRTF mix or change gameplay
            // bypass settings for routes that cannot be observed faithfully.
            if (!Readable(source))
            {
                if (audible)
                    current.MissingRoute();
                return;
            }
            if (sources.ContainsKey(source.GetInstanceID()))
                return;
            if (source.GetComponent<AudioListener>() != null ||
                source.GetComponents<AudioSource>().Length != 1)
            {
                if (audible)
                    current.MissingRoute();
                return;
            }
            if (!audible && sources.Count >= 64)
                return;
            var lane = current.AddBypass();
            if (lane == null)
                return;
            var tap = source.GetComponent<ReplayAudioTap>();
            if (tap == null)
                tap = source.gameObject.AddComponent<ReplayAudioTap>();
            var entry =
                new Source { Audio = source, Tap = tap, Lane = lane, Gain = source.mute ? 0 : source.volume };
            sources.Add(source.GetInstanceID(), entry);
            tap.Current = new ReplayAudioTap.Route { Timeline = clock, Lane = lane, Gain = entry.Gain };
        }
        catch (Exception)
        {
            if (current != null)
                current.MissingRoute();
        }
    }

    public static void Begin(ReplayMotionData motion, MouseLook player)
    {
        Stop();
        if (motion == null || player == null)
            return;
        try
        {
            AudioListener listener = null;
            var camera = PlayerContext.View(player);
            if (camera != null)
                listener = camera.GetComponent<AudioListener>();
            if (listener == null || !listener.enabled)
                foreach (var l in UnityEngine.Object.FindObjectsOfType<AudioListener>())
                    if (l.enabled)
                    {
                        if (listener != null && listener.enabled)
                        {
                            listener = null;
                            break;
                        }
                        listener = l;
                    }
            if (listener == null)
            {
                motion.Experience.Audio.Flags = 4;
                return;
            }
            int channels;
            switch (AudioSettings.speakerMode)
            {
            case AudioSpeakerMode.Mono:
                channels = 1;
                break;
            case AudioSpeakerMode.Quad:
                channels = 4;
                break;
            case AudioSpeakerMode.Surround:
                channels = 5;
                break;
            case AudioSpeakerMode.Mode5point1:
                channels = 6;
                break;
            case AudioSpeakerMode.Mode7point1:
                channels = 8;
                break;
            default:
                channels = 2;
                break;
            }
            int rate = AudioSettings.outputSampleRate, length, buffers;
            AudioSettings.GetDSPBufferSize(out length, out buffers);
            string folder = Path.Combine(Path.GetFullPath(Path.Combine(Application.dataPath, "..")),
                                         "UserData", "ItStealsPracticeSuite", "RunReviews");
            var session = new ReplayAudioSession(folder, motion.Id, motion.Scene, motion.Seed, motion.Hash,
                                                 rate, channels, Math.Min(65536, Math.Max(8192, length * 8)));
            current = session;
            var tap = listener.GetComponent<ReplayAudioTap>();
            if (tap == null)
                tap = listener.gameObject.AddComponent<ReplayAudioTap>();
            double now = SpeedrunManager.Instance == null ? 0 : SpeedrunManager.Instance.CurrentSeconds;
            clock = new ReplayAudioTap.Clock { Current = new ReplayAudioTap.Binding {
                Origin = AudioSettings.dspTime - now, Rate = rate, Session = session
            } };
            tap.Current = new ReplayAudioTap.Route { Timeline = clock };
            motion.AudioSession = session;
            motion.Experience.Audio = session.Info;
            foreach (var source in UnityEngine.Object.FindObjectsOfType<AudioSource>(true))
                Track(source, false);
        }
        catch (Exception)
        {
            motion.Experience.Audio.Flags = 4;
            Stop();
        }
    }

    public static void Sync(double seconds)
    {
        if (current == null || clock == null)
            return;
        var binding = clock.Current;
        if (binding == null || binding.Session != current)
            return;
        double dsp = AudioSettings.dspTime;
        if (Math.Abs(dsp - binding.Origin - seconds) > .05)
            clock.Current =
                new ReplayAudioTap.Binding { Session = current, Origin = dsp - seconds, Rate = binding.Rate };
        foreach (var entry in sources.Values)
        {
            var source = entry.Audio;
            if (source == null || entry.Tap == null)
                continue;
            float gain = 0;
            if (Bypasses(source))
            {
                if (Readable(source))
                    gain = source.mute ? 0 : source.volume;
                else if (source.isPlaying)
                    current.MissingRoute();
            }
            if (gain != entry.Gain)
            {
                entry.Gain = gain;
                entry.Tap.Current =
                    new ReplayAudioTap.Route { Timeline = clock, Lane = entry.Lane, Gain = gain };
            }
        }
    }

    public static void Finish(double seconds)
    {
        var session = current;
        current = null;
        if (session != null)
            session.End(seconds);
    }

    public static void Stop()
    {
        var session = current;
        current = null;
        clock = null;
        sources.Clear();
        if (session != null)
            session.Discard();
    }
}
