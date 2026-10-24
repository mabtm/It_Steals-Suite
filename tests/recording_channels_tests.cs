using System;
using System.IO;
using System.Threading;

// Wire/audio contracts run without the Unity runtime. Only the value type and
// text helpers used by animation metadata need adapters here.
namespace UnityEngine
{
public struct Vector3
{
    public float x, y, z;

    public Vector3(float a, float b, float c)
    {
        x = a;
        y = b;
        z = c;
    }
}
}

public static class ReplayMotionData
{
    public static void Text(BinaryWriter w, string value)
    {
        ReplaySceneData.String(w, value);
    }

    public static string Text(BinaryReader r, int max)
    {
        return ReplaySceneData.String(r, max);
    }
}

public static class RecordingChannelsTests
{
    private static void Check(bool yes, string why)
    {
        if (!yes)
            throw new Exception(why);
    }

    private static void Reject(Action action)
    {
        try
        {
            action();
        }
        catch (IOException)
        {
            return;
        }
        catch (InvalidDataException)
        {
            return;
        }
        catch (ArgumentException)
        {
            return;
        }
        throw new Exception("Invalid recording accepted");
    }

    private static byte[] Payload(Action<BinaryWriter> action)
    {
        using (var s = new MemoryStream())
        {
            using (var w = new BinaryWriter(s))
            {
                action(w);
                return s.ToArray();
            }
        }
    }

    private static void Scene()
    {
        var data = new ReplaySceneData { Recorded = true, Through = 1 };
        var track = data.Track(ReplaySceneData.Group, "0:Canvas/0:HUD");
        var a = Payload(w =>
                        {
                            w.Write(1f);
                            w.Write(false);
                        });
        var b = Payload(w =>
                        {
                            w.Write(.25f);
                            w.Write(true);
                        });
        data.Add(track, 0, a, a.Length);
        data.Add(track, .2, a, a.Length);
        data.Add(track, .5, b, b.Length);
        Check(data.Count == 2 && track.At(-.1) == -1 && track.At(.499) == 0 && track.At(.5) == 1 &&
                  track.At(1) == 1,
              "Sparse snapshots retain exact transitions and support backwards seeks");
        byte[] wire = Payload(w => data.Write(w));
        using (var r = new BinaryReader(new MemoryStream(wire)))
        {
            var decoded = ReplaySceneData.Read(r, 1);
            Check(decoded.Recorded && decoded.Count == 2 && decoded.Tracks[0].Frames[1].Values[4] == 1 &&
                      r.BaseStream.Position == wire.Length,
                  "Scene values roundtrip without native engine memory");
        }
        Reject(() => ReplaySceneData.Validate(ReplaySceneData.Group, Payload(w =>
                                                                             {
                                                                                 w.Write(float.NaN);
                                                                                 w.Write(false);
                                                                             })));
        Reject(() => ReplaySceneData.Validate(ReplaySceneData.Group, Payload(w =>
                                                                             {
                                                                                 w.Write(1f);
                                                                                 w.Write((byte)2);
                                                                             })));
        Reject(() => ReplaySceneData.Validate(ReplaySceneData.Group, new byte[100]));
        byte[] bad = (byte[])wire.Clone();
        bad[0] = 2;
        Reject(() => ReplaySceneData.Read(new BinaryReader(new MemoryStream(bad)), 1));
        Reject(() => ReplaySceneData.Read(new BinaryReader(new MemoryStream(wire)), .25));
        data.Add(track, .4, a, a.Length);
        Check(data.Dropped == 1 && data.Count == 2,
              "A backwards capture timestamp cannot overwrite evidence");
        data.Count = ReplaySceneData.MaxFrames;
        data.Add(track, 1, a, a.Length);
        Check(data.Dropped == 2, "Scene storage stays bounded and reports omitted observations");
    }

    private static void Audio(string folder, int id, int start, int second, double seconds, int reason)
    {
        var s = new ReplayAudioSession(folder, id, 3, 123, "hash", 48000, 2);
        var block = new float[960];
        block[0] = .25f;
        s.Push(block, 2, start);
        if (second >= 0)
            s.Push(block, 2, second);
        s.End(seconds);
        string path = Path.Combine(folder, "audio-" + id + ".isaudio");
        Check(s.Commit(path), "Audio committed");
        var decoded = ReplayAudioFile.Verify(path, id, 3, 123, "hash", s.Info);
        Check(decoded.Diagnostics && decoded.ListenerOverruns == 0 && decoded.BypassOverruns == 0,
              "A missing sample boundary is not reported as a queue overrun");
        if (reason == 0)
            Check(decoded.Flags == 0 && decoded.Gaps == 0 && decoded.MissingFrames == 0,
                  "Contiguous PCM has no invented gap");
        else
            Check(decoded.Flags == 1 && decoded.Gaps == 1 && decoded.MissingFrames == 480 &&
                      decoded.GapDetails.Count == 1 && decoded.GapDetails[0].Reason == reason,
                  "Start, interior and finish gaps have exact sample counts and distinct causes");
        var bytes = File.ReadAllBytes(path);
        bytes[150] ^= 1;
        File.WriteAllBytes(path, bytes);
        Reject(() => ReplayAudioFile.Verify(path, id, 3, 123, "hash", s.Info));
    }

    public static int Main()
    {
        Scene();
        string folder =
            Path.Combine(Path.GetTempPath(), "suite-recording-tests-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try
        {
            Audio(folder, 101, 0, 480, .02, 0);
            Audio(folder, 102, 480, -1, .02, 1);
            Audio(folder, 103, 0, 960, .03, 2);
            Audio(folder, 104, 0, -1, .02, 3);
            var s = new ReplayAudioSession(folder, 105, 3, 123, "hash", 48000, 2);
            var block = new float[960];
            for (int i = 0; i < 100000; i++)
                s.Push(block, 2, 0);
            s.End(.01);
            string path = Path.Combine(folder, "overrun.isaudio");
            Check(s.Commit(path), "Bounded overrun evidence committed");
            var info = ReplayAudioFile.Verify(path, 105, 3, 123, "hash", s.Info);
            Check(info.ListenerOverruns > 0 && info.PeakListener == 63 && info.MissingFrames == 0,
                  "Queue saturation is bounded and separately diagnosed without fabricating a timeline gap");
        }
        finally
        {
            Directory.Delete(folder, true);
        }
        Console.WriteLine(
            "Scene wire bounds, sparse seeks, malformed snapshots and exact PCM boundary/queue diagnostics: PASS");
        return 0;
    }
}
