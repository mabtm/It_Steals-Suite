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

    // Four DSP blocks cover the run-start boundary without inventing silence.
    // Allocated on the main thread; the DSP thread alone reads/writes history.
    public sealed class History
    {
        private readonly float[][] samples = { new float[65536], new float[65536], new float[65536],
                                               new float[65536] };
        private readonly double[] times = new double[4];
        private readonly int[] counts = new int[4], channels = new int[4];
        private int next, available;

        public void Store(float[] data, int countChannels, double dsp)
        {
            if (data == null || data.Length > 65536 || countChannels < 1 || countChannels > 8)
                return;
            Array.Copy(data, samples[next], data.Length);
            counts[next] = data.Length;
            channels[next] = countChannels;
            times[next] = dsp;
            next = (next + 1) % 4;
            available = Math.Min(4, available + 1);
        }

        public void Replay(Route route, Binding binding)
        {
            for (int n = available; n > 0; n--)
            {
                int i = (next - n + 4) % 4;
                long first = (long)Math.Round((times[i] - binding.Origin) * binding.Rate);
                if (first + counts[i] / channels[i] <= 0)
                    continue;
                if (route.Lane == null)
                    binding.Session.Push(samples[i], channels[i], first, counts[i]);
                else if (route.Gain > 0)
                    route.Lane.Push(samples[i], channels[i], first, route.Gain, counts[i]);
            }
        }
    }

    public volatile Route Current;
    public volatile History Recent;
    private ReplayAudioSession attached;

    private void OnAudioFilterRead(float[] data, int channels)
    {
        double dsp = AudioSettings.dspTime;
        var route = Current;
        var binding = route == null ? null : route.Timeline.Current;
        var history = Recent;
        if (binding != null && !binding.Session.Done)
        {
            if (attached != binding.Session)
            {
                if (history != null)
                    history.Replay(route, binding);
                attached = binding.Session;
            }
            long first = (long)Math.Round((dsp - binding.Origin) * binding.Rate);
            if (route.Lane == null)
                binding.Session.Push(data, channels, first);
            else if (route.Gain > 0)
                route.Lane.Push(data, channels, first, route.Gain);
        }
        if (history != null)
            history.Store(data, channels, dsp);
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
    private static ReplayMotionData recording;
    private static readonly Dictionary<int, AudioSource> enabledTaps = new Dictionary<int, AudioSource>();
    private static readonly HashSet<int> missingSources = new HashSet<int>();
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

    private static bool MakeReadable(AudioSource source)
    {
        if (source.spatialize || source.spatialBlend != 0 || source.panStereo != 0)
            return false;
        if (!source.bypassEffects)
            return true;
        // With no source filters, bypassing source effects and passing through
        // our unchanged PCM are equivalent. Keep listener bypass unchanged.
        foreach (var component in source.GetComponents<Component>())
        {
            if (component == null || component is ReplayAudioTap)
                continue;
            var type = component.GetType();
            if (type.Name.StartsWith("Audio", StringComparison.Ordinal) &&
                type.Name.EndsWith("Filter", StringComparison.Ordinal))
                return false;
            if (type.GetMethod("OnAudioFilterRead", System.Reflection.BindingFlags.Instance |
                                                        System.Reflection.BindingFlags.Public |
                                                        System.Reflection.BindingFlags.NonPublic) != null)
                return false;
        }
        enabledTaps[source.GetInstanceID()] = source;
        source.bypassEffects = false;
        return true;
    }

    private static void Missing(AudioSource source, string reason)
    {
        if (current == null)
            return;
        current.MissingRoute();
        if (recording == null || missingSources.Count >= 4 || !missingSources.Add(source.GetInstanceID()))
            return;
        string name = source.name ?? "Audio source";
        if (name.Length > 48)
            name = name.Substring(0, 48);
        recording.AudioNotes += name + ": " + reason + "\n";
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
            bool audible = !source.mute && source.volume > 0 && (playRequested || source.isPlaying);
            // The supplied GetPoints source is centered 2D, with listener
            // bypass enabled and source effects enabled. A source tap observes
            // that exact PCM. Do not estimate a 3D/HRTF mix or change gameplay
            // listener bypass for routes that cannot be observed faithfully.
            if (!MakeReadable(source))
            {
                if (audible)
                    Missing(source, "listener bypass with a spatial mix or source filter");
                return;
            }
            if (sources.ContainsKey(source.GetInstanceID()))
                return;
            if (source.GetComponent<AudioListener>() != null ||
                source.GetComponents<AudioSource>().Length != 1)
            {
                if (audible)
                    Missing(source, "source shares a listener or another AudioSource");
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
                Missing(source, "source tap setup failed");
        }
    }

    private static AudioListener Listener(MouseLook player)
    {
        AudioListener listener = null;
        var camera = player == null ? null : PlayerContext.View(player);
        if (camera != null)
            listener = camera.GetComponent<AudioListener>();
        if (listener == null || !listener.enabled)
            foreach (var candidate in UnityEngine.Object.FindObjectsOfType<AudioListener>())
                if (candidate.enabled)
                {
                    if (listener != null && listener.enabled)
                        return null;
                    listener = candidate;
                }
        return listener;
    }

    private static ReplayAudioTap Warm(GameObject target)
    {
        var tap = target.GetComponent<ReplayAudioTap>();
        if (tap == null)
            tap = target.AddComponent<ReplayAudioTap>();
        if (tap.Recent == null)
            tap.Recent = new ReplayAudioTap.History();
        return tap;
    }

    public static void Prepare(MouseLook player = null)
    {
        try
        {
            var listener = Listener(player);
            if (listener != null)
                Warm(listener.gameObject);
            foreach (var source in UnityEngine.Object.FindObjectsOfType<AudioSource>(true))
                if (Bypasses(source) && MakeReadable(source) &&
                    source.GetComponent<AudioListener>() == null &&
                    source.GetComponents<AudioSource>().Length == 1)
                    Warm(source.gameObject);
        }
        catch (Exception)
        {
            // A failed warm-up must never interfere with scene loading.
        }
    }

    public static void Begin(ReplayMotionData motion, MouseLook player)
    {
        Stop(true);
        missingSources.Clear();
        recording = motion;
        if (motion == null || player == null)
            return;
        try
        {
            var listener = Listener(player);
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
                else if (source.isPlaying && !source.mute && source.volume > 0)
                    Missing(source, "source route changed to an unobservable mix");
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

    public static void Stop(bool keepPrepared = false)
    {
        if (!keepPrepared)
        {
            foreach (var source in enabledTaps.Values)
                if (source != null && !source.bypassEffects)
                    source.bypassEffects = true;
            enabledTaps.Clear();
        }
        recording = null;
        var session = current;
        current = null;
        clock = null;
        sources.Clear();
        if (session != null)
            session.Discard();
    }
}
