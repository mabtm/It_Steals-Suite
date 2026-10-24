using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

// Version 8 observations for the game's extra cameras, UI and particles.
// Payloads contain public values only: no pointers or native engine structs.
public sealed class ReplaySceneFrame
{
    public double Seconds;
    public byte[] Values;
}

public sealed class ReplaySceneTrack
{
    public int Kind;
    public string Path = "", Prefab = "", Spawn = "";
    public readonly List<ReplaySceneFrame> Frames = new List<ReplaySceneFrame>();

    public int At(double at)
    {
        int lo = 0, hi = Frames.Count;
        while (lo < hi)
        {
            int mid = lo + (hi - lo) / 2;
            if (Frames[mid].Seconds <= at)
                lo = mid + 1;
            else
                hi = mid;
        }
        return lo - 1;
    }
}

public sealed class ReplaySceneData
{
    public const int Camera = 1, Rect = 2, Text = 3, Image = 4, RawImage = 5, Group = 6, Particles = 7;
    public const int MaxTracks = 2048, MaxFrames = 262144, MaxParticles = 8192, MaxPayload = 1024 * 1024,
                     MaxBytes = 32 * 1024 * 1024;
    public bool Recorded;
    public Func<int, bool> ReserveBytes;
    public int Dropped, Count;
    public double Through;
    public int Bytes = 32;
    public readonly List<ReplaySceneTrack> Tracks = new List<ReplaySceneTrack>();
    private static readonly Encoding Utf8 = new UTF8Encoding(false, true);

    public void Drop()
    {
        if (Dropped < int.MaxValue)
            Dropped++;
    }

    public ReplaySceneTrack Track(int kind, string path, string prefab = "", string spawn = "")
    {
        int bytes = 24 + Utf8.GetByteCount(path) + Utf8.GetByteCount(prefab) + Utf8.GetByteCount(spawn);
        if (Tracks.Count == MaxTracks || bytes > 8192 || Bytes + bytes > MaxBytes ||
            ReserveBytes != null && !ReserveBytes(bytes))
        {
            Drop();
            return null;
        }
        var track = new ReplaySceneTrack { Kind = kind, Path = path, Prefab = prefab, Spawn = spawn };
        Tracks.Add(track);
        Bytes += bytes;
        return track;
    }

    public void Add(ReplaySceneTrack track, double at, byte[] values, int length)
    {
        if (track == null)
            return;
        var frames = track.Frames;
        if (frames.Count > 0)
        {
            var previous = frames[frames.Count - 1];
            if (at < previous.Seconds)
            {
                Drop();
                return;
            }
            bool same = previous.Values.Length == length;
            for (int i = 0; same && i < length; i++)
                same = values[i] == previous.Values[i];
            if (same)
                return;
        }
        if (Count == MaxFrames || length > MaxPayload || Bytes + length + 12 > MaxBytes ||
            ReserveBytes != null && !ReserveBytes(length + 12))
        {
            Drop();
            return;
        }
        var copy = new byte[length];
        Buffer.BlockCopy(values, 0, copy, 0, length);
        frames.Add(new ReplaySceneFrame { Seconds = at, Values = copy });
        Bytes += length + 12;
        Count++;
    }

    public static void String(BinaryWriter w, string text)
    {
        byte[] bytes = Utf8.GetBytes(text ?? "");
        w.Write(bytes.Length);
        w.Write(bytes);
    }

    public static string String(BinaryReader r, int max = 4096)
    {
        int length = Number(r, max);
        byte[] bytes = r.ReadBytes(length);
        if (bytes.Length != length)
            throw new EndOfStreamException();
        string text = Utf8.GetString(bytes);
        if (text.IndexOf('\0') >= 0)
            throw new InvalidDataException("Invalid scene text");
        return text;
    }

    public static bool Flag(BinaryReader r)
    {
        byte flag = r.ReadByte();
        if (flag > 1)
            throw new InvalidDataException("Invalid scene flag");
        return flag == 1;
    }

    public static int Number(BinaryReader r, int max)
    {
        int n = r.ReadInt32();
        if (n < 0 || n > max)
            throw new InvalidDataException("Scene recording exceeds limit");
        return n;
    }

    public static float Float(BinaryReader r, float max = 100000)
    {
        float n = r.ReadSingle();
        if (float.IsNaN(n) || float.IsInfinity(n) || Math.Abs(n) > max)
            throw new InvalidDataException("Invalid scene number");
        return n;
    }

    private static void Floats(BinaryReader r, int count)
    {
        for (int i = 0; i < count; i++)
            Float(r);
    }

    private static void Rotation(BinaryReader r)
    {
        double norm = 0;
        for (int i = 0; i < 4; i++)
        {
            float n = Float(r, 1.01f);
            norm += n * (double)n;
        }
        if (norm < .98 || norm > 1.02)
            throw new InvalidDataException("Invalid scene rotation");
    }

    private static void Graphic(BinaryReader r)
    {
        Flag(r);
        Floats(r, 4);
        String(r, 1024);
        if ((r.ReadUInt32() & ~15u) != 0)
            throw new InvalidDataException("Invalid material properties");
        Floats(r, 6);
    }

    public static void Validate(int kind, byte[] bytes)
    {
        using (var stream = new MemoryStream(bytes, false)) using (var r =
                                                                       new BinaryReader(stream, Utf8, true))
        {
            if (kind == Camera)
            {
                Flag(r);
                Floats(r, 3);
                Rotation(r);
                Floats(r, 3);
                float fov = Float(r, 179), near = Float(r), far = Float(r);
                Floats(r, 3);
                r.ReadInt32();
                int clear = Number(r, 4);
                Flag(r);
                Floats(r, 4);
                if (fov < 1 || near <= 0 || far <= near || clear == 0)
                    throw new InvalidDataException("Invalid recorded camera");
            }
            else if (kind == Rect)
            {
                Flag(r);
                Floats(r, 11);
                Rotation(r);
                Floats(r, 3);
            }
            else if (kind == Text)
            {
                Graphic(r);
                String(r, 16384);
                Number(r, 1000);
                Number(r, 3);
                Number(r, 8);
                Float(r);
            }
            else if (kind == Image)
            {
                Graphic(r);
                String(r, 1024);
                float fill = Float(r, 1);
                if (fill < 0)
                    throw new InvalidDataException("Invalid image fill");
            }
            else if (kind == RawImage)
            {
                Graphic(r);
                String(r, 1024);
                Floats(r, 4);
            }
            else if (kind == Group)
            {
                float alpha = Float(r, 1);
                if (alpha < 0)
                    throw new InvalidDataException("Invalid group alpha");
                Flag(r);
            }
            else if (kind == Particles)
            {
                if ((r.ReadUInt32() & ~7u) != 0)
                    throw new InvalidDataException("Invalid particle state");
                float time = Float(r, 315360000);
                if (time < 0)
                    throw new InvalidDataException("Invalid particle time");
                r.ReadUInt32();
                Floats(r, 3);
                Rotation(r);
                Floats(r, 3);
                int count = Number(r, MaxParticles);
                for (int i = 0; i < count; i++)
                {
                    Floats(r, 18);
                    r.ReadUInt32();
                    float life = Float(r), start = Float(r);
                    if (life < 0 || start <= 0 || life > start + .001f)
                        throw new InvalidDataException("Invalid particle lifetime");
                    r.ReadUInt32();
                }
            }
            else
                throw new InvalidDataException("Unsupported scene track");
            if (stream.Position != stream.Length)
                throw new InvalidDataException("Trailing scene values");
        }
    }

    public void Write(BinaryWriter w)
    {
        w.Write(Recorded);
        w.Write(Dropped);
        w.Write(Through);
        w.Write(Tracks.Count);
        foreach (var t in Tracks)
        {
            w.Write(t.Kind);
            String(w, t.Path);
            String(w, t.Prefab);
            String(w, t.Spawn);
            w.Write(t.Frames.Count);
            foreach (var f in t.Frames)
            {
                w.Write(f.Seconds);
                w.Write(f.Values.Length);
                w.Write(f.Values);
            }
        }
    }

    public static ReplaySceneData Read(BinaryReader r, double duration)
    {
        long begin = r.BaseStream.Position;
        var d = new ReplaySceneData { Recorded = Flag(r), Dropped = Number(r, int.MaxValue),
                                      Through = r.ReadDouble() };
        if (double.IsNaN(d.Through) || double.IsInfinity(d.Through) || d.Through < 0 ||
            d.Through > duration + .000001)
            throw new InvalidDataException("Invalid scene coverage");
        int tracks = Number(r, MaxTracks);
        var unique = new HashSet<string>(StringComparer.Ordinal);
        for (int i = 0; i < tracks; i++)
        {
            var t = new ReplaySceneTrack { Kind = Number(r, Particles), Path = String(r, 4096),
                                           Prefab = String(r, 1024), Spawn = String(r, 4096) };
            if (t.Kind == 0 ||
                (!string.IsNullOrEmpty(t.Prefab) || !string.IsNullOrEmpty(t.Spawn)) &&
                    (t.Kind != Particles || string.IsNullOrEmpty(t.Prefab) ||
                     string.IsNullOrEmpty(t.Spawn)) ||
                !unique.Add(t.Kind + "|" + t.Spawn + "|" + t.Path))
                throw new InvalidDataException("Invalid scene track identity");
            int frames = Number(r, MaxFrames - d.Count);
            double previous = -1;
            for (int j = 0; j < frames; j++)
            {
                double at = r.ReadDouble();
                if (double.IsNaN(at) || double.IsInfinity(at) || at < 0 || at < previous ||
                    at > duration + .000001)
                    throw new InvalidDataException("Invalid scene timestamp");
                int length = Number(r, MaxPayload);
                if (r.BaseStream.Position - begin + length > MaxBytes)
                    throw new InvalidDataException("Scene recording exceeds byte limit");
                byte[] values = r.ReadBytes(length);
                if (values.Length != length)
                    throw new EndOfStreamException();
                Validate(t.Kind, values);
                t.Frames.Add(new ReplaySceneFrame { Seconds = at, Values = values });
                previous = at;
            }
            d.Tracks.Add(t);
            d.Count += frames;
        }
        if (!d.Recorded && (tracks != 0 || d.Count != 0 || d.Dropped != 0 || d.Through != 0))
            throw new InvalidDataException("Unexpected scene observations");
        d.Bytes = checked((int)(r.BaseStream.Position - begin));
        if (d.Bytes > MaxBytes)
            throw new InvalidDataException("Scene recording exceeds byte limit");
        return d;
    }
}
