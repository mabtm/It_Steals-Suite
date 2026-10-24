using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Diagnostics;

// Float PCM in a separate immutable companion. No game assets or microphone
// data are read. The DSP producer only copies into a bounded preallocated ring.
public static class ReplayAudioFile
{
    public const int HeaderBytes = 256;
    public const long MaxBytes = 256L * 1024 * 1024;
    public const uint Magic = 0x55415349;

    public static ulong Hash(Stream s, long length)
    {
        ulong h = 14695981039346656037UL;
        var b = new byte[65536];
        while (length > 0)
        {
            int n = s.Read(b, 0, (int)Math.Min(b.Length, length));
            if (n <= 0)
                throw new EndOfStreamException();
            unchecked
            {
                for (int i = 0; i < n; i++)
                {
                    h ^= b[i];
                    h *= 1099511628211UL;
                }
            }
            length -= n;
        }
        return h;
    }

    public static ReplayAudioInfo Verify(string path, long id, int scene, int seed, string gameHash,
                                         ReplayAudioInfo expected = null)
    {
        using (var s = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 65536))
        {
            if (s.Length < HeaderBytes + 8 || s.Length > MaxBytes)
                throw new InvalidDataException("Recorded audio exceeds file limit");
            var r = new BinaryReader(s, Encoding.UTF8, true);
            if (r.ReadUInt32() != Magic || r.ReadInt32() != 1 || r.ReadInt64() != id ||
                r.ReadInt32() != scene || r.ReadInt32() != seed)
                throw new InvalidDataException("Recorded audio identity mismatch");
            var i =
                new ReplayAudioInfo { Recorded = true,        Rate = r.ReadInt32(),  Channels = r.ReadInt32(),
                                      Frames = r.ReadInt64(), Flags = r.ReadInt32(), Gaps = r.ReadInt32(),
                                      Bytes = s.Length };
            int n = r.ReadInt32();
            if (n < 0 || n > 65 || Encoding.UTF8.GetString(r.ReadBytes(n)) != gameHash || i.Rate < 8000 ||
                i.Rate > 192000 || i.Channels < 1 || i.Channels > 8 || i.Frames < 1 ||
                i.Frames > (MaxBytes - HeaderBytes - 8) / (i.Channels * 4) ||
                s.Length != HeaderBytes + i.Frames * i.Channels * 4 + 8 || (i.Flags & ~31) != 0 || i.Gaps < 0)
                throw new InvalidDataException("Invalid recorded audio header");
            s.Position = 128;
            uint diagnosticMagic = r.ReadUInt32();
            if (diagnosticMagic != 0)
            {
                if (diagnosticMagic != 0x47445349u || r.ReadInt32() != 1)
                    throw new InvalidDataException("Invalid audio diagnostics");
                i.Diagnostics = true;
                i.ListenerOverruns = r.ReadInt32();
                i.BypassOverruns = r.ReadInt32();
                i.MissingFrames = r.ReadInt64();
                i.PeakListener = r.ReadInt32();
                i.PeakBypass = r.ReadInt32();
                int details = r.ReadInt32();
                if (i.ListenerOverruns < 0 || i.BypassOverruns < 0 || i.MissingFrames < 0 ||
                    i.MissingFrames > i.Frames || i.PeakListener < 0 || i.PeakListener > 63 ||
                    i.PeakBypass < 0 || i.PeakBypass > 15 || details < 0 || details > 3)
                    throw new InvalidDataException("Invalid audio diagnostics");
                for (int k = 0; k < details; k++)
                {
                    var gap = new ReplayAudioGap { First = r.ReadInt64(), Frames = r.ReadInt64(),
                                                   Reason = r.ReadInt32() };
                    if (r.ReadInt32() != 0 || gap.First < 0 || gap.Frames <= 0 ||
                        gap.First > i.Frames - gap.Frames || gap.Reason < 1 || gap.Reason > 3)
                        throw new InvalidDataException("Invalid audio gap");
                    i.GapDetails.Add(gap);
                }
            }
            s.Position = s.Length - 8;
            i.Checksum = r.ReadUInt64();
            s.Position = 0;
            if (Hash(s, s.Length - 8) != i.Checksum)
                throw new InvalidDataException("Recorded audio checksum mismatch");
            if (expected != null &&
                (!expected.Recorded || expected.Rate != i.Rate || expected.Channels != i.Channels ||
                 expected.Frames != i.Frames || expected.Bytes != i.Bytes ||
                 expected.Checksum != i.Checksum || expected.Flags != i.Flags || expected.Gaps != i.Gaps))
                throw new InvalidDataException("Recorded audio does not match input recording");
            return i;
        }
    }
}

public sealed class ReplayAudioSession
{
    internal sealed class Block
    {
        public readonly float[] Data;
        public long First;
        public int Count, Channels;
        public float Gain;

        public Block(int n)
        {
            Data = new float[n];
        }
    }

    private readonly Block[] ring = new Block[64];
    private int head, tail, done, closing, discard, flags, gaps, accepting = 1;
    private long target, closeDeadline, closeReadyDeadline;
    private int lastBlockFrames;
    private int observed, listenerOverruns, peakListener;
    private long missingFrames;
    private readonly System.Collections.Generic.List<ReplayAudioGap> gapDetails =
        new System.Collections.Generic.List<ReplayAudioGap>();
    private readonly int rate, channels;
    private readonly long id;
    private readonly int scene, seed;
    private readonly string hash, temp;
    private readonly Thread writer;
    private Exception failure;
    private volatile ReplayAudioLane[] lanes = new ReplayAudioLane[0];
    public readonly ReplayAudioInfo Info = new ReplayAudioInfo();
    public long MemoryBytes
    {
        get {
            long bytes = (long)ring.Length * ring[0].Data.Length * 4 + ring[0].Data.Length * 64 + 65536;
            foreach (var lane in lanes)
                bytes += lane.MemoryBytes;
            return bytes;
        }
    }
    public bool Done
    {
        get {
            return Volatile.Read(ref done) != 0;
        }
    }

    public ReplayAudioSession(string directory, long run, int level, int layoutSeed, string gameHash,
                              int sampleRate, int channelCount, int blockFloats = 8192)
    {
        if (run <= 0 || sampleRate < 8000 || sampleRate > 192000 || channelCount < 1 || channelCount > 8 ||
            blockFloats < channelCount || blockFloats > 65536 ||
            Encoding.UTF8.GetByteCount(gameHash ?? "") > 65)
            throw new ArgumentException("Invalid audio capture settings");
        id = run;
        scene = level;
        seed = layoutSeed;
        hash = gameHash ?? "";
        rate = sampleRate;
        channels = channelCount;
        temp = Path.Combine(directory,
                            "run-" + id + ".isaudio.capture-" + Guid.NewGuid().ToString("N") + ".tmp");
        for (int i = 0; i < ring.Length; i++)
            ring[i] = new Block(blockFloats);
        writer = new Thread(Write) { IsBackground = true, Name = "ItSteals.ReplayAudio" };
        writer.Start();
    }

    private void Flag(int value)
    {
        int before, after;
        do
        {
            before = Volatile.Read(ref flags);
            after = before | value;
        } while (Interlocked.CompareExchange(ref flags, after, before) != before);
    }

    // Main-thread setup only. Each pass-through source owns its SPSC ring;
    // neither source ordering nor a shared multi-producer queue is assumed.
    public ReplayAudioLane AddBypass()
    {
        var before = lanes;
        if (before.Length >= 64)
        {
            Flag(4);
            return null;
        }
        var lane = new ReplayAudioLane(this, ring[0].Data.Length);
        var after = new ReplayAudioLane[before.Length + 1];
        Array.Copy(before, after, before.Length);
        after[before.Length] = lane;
        lanes = after;
        return lane;
    }

    internal bool Accept(long first)
    {
        return !Done && Volatile.Read(ref discard) == 0 && Volatile.Read(ref accepting) != 0 &&
               (Volatile.Read(ref closing) == 0 || first < target);
    }

    public void MissingRoute()
    {
        Flag(4);
    }

    internal void Invalid()
    {
        Flag(8);
    }

    internal void Overrun()
    {
        Flag(1);
    }

    public void Push(float[] samples, int countChannels, long firstFrame, int sampleCount = -1)
    {
        if (!Accept(firstFrame))
            return;
        int count = sampleCount < 0 ? (samples == null ? 0 : samples.Length) : sampleCount;
        if (samples == null || countChannels != channels || count < 0 || count > samples.Length ||
            count % channels != 0 || count > ring[0].Data.Length)
        {
            Flag(8);
            return;
        }
        int at = head, next = (at + 1) % ring.Length;
        if (next == Volatile.Read(ref tail))
        {
            Flag(1);
            Interlocked.Increment(ref listenerOverruns);
            return;
        }
        peakListener = Math.Max(peakListener, (next - Volatile.Read(ref tail) + ring.Length) % ring.Length);
        var b = ring[at];
        Array.Copy(samples, b.Data, count);
        b.First = firstFrame;
        b.Count = count;
        Volatile.Write(ref lastBlockFrames, count / channels);
        Interlocked.Increment(ref observed);
        Volatile.Write(ref head, next);
    }

    public void End(double seconds)
    {
        if (double.IsNaN(seconds) || double.IsInfinity(seconds) || seconds < 0 || seconds > 315360000)
            seconds = 0;
        target = (long)Math.Ceiling(seconds * rate);
        long now = Stopwatch.GetTimestamp();
        closeReadyDeadline =
            now + (long)(Stopwatch.Frequency *
                         Math.Min(.2, Math.Max(.02, 2.0 * Volatile.Read(ref lastBlockFrames) / rate)));
        closeDeadline = now + Stopwatch.Frequency;
        Volatile.Write(ref closing, 1);
    }

    public void Discard()
    {
        Volatile.Write(ref discard, 1);
        if (Volatile.Read(ref closing) == 0)
            End(0);
        if (Done)
            DeleteTemp();
    }

    private void Gap(long first, long frames, int reason)
    {
        Flag(1);
        gaps++;
        missingFrames += frames;
        if (gapDetails.Count < 3)
            gapDetails.Add(new ReplayAudioGap { First = first, Frames = frames, Reason = reason });
    }

    private void Header(Stream s, long frames, int status = 0, int missing = 0)
    {
        s.Position = 0;
        using (var w = new BinaryWriter(s, Encoding.UTF8, true))
        {
            w.Write(ReplayAudioFile.Magic);
            w.Write(1);
            w.Write(id);
            w.Write(scene);
            w.Write(seed);
            w.Write(rate);
            w.Write(channels);
            w.Write(frames);
            w.Write(status);
            w.Write(missing);
            byte[] bytes = Encoding.UTF8.GetBytes(hash);
            w.Write(bytes.Length);
            w.Write(bytes);
            while (s.Position < 128)
                w.Write((byte)0);
            w.Write(0x47445349u);
            w.Write(1);
            w.Write(Volatile.Read(ref listenerOverruns));
            int bypass = 0, peak = 0;
            foreach (var lane in lanes)
            {
                bypass += lane.Dropped;
                peak = Math.Max(peak, lane.Peak);
            }
            w.Write(bypass);
            w.Write(missingFrames);
            w.Write(Volatile.Read(ref peakListener));
            w.Write(peak);
            w.Write(gapDetails.Count);
            foreach (var gap in gapDetails)
            {
                w.Write(gap.First);
                w.Write(gap.Frames);
                w.Write(gap.Reason);
                w.Write(0);
            }
            while (s.Position < ReplayAudioFile.HeaderBytes)
                w.Write((byte)0);
        }
    }

    private void Write()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(temp));
            using (var s =
                       new FileStream(temp, FileMode.CreateNew, FileAccess.ReadWrite, FileShare.None, 65536))
            {
                Header(s, 0);
                long position = 0,
                     max = (ReplayAudioFile.MaxBytes - ReplayAudioFile.HeaderBytes - 8) / (channels * 4);
                var bytes = new byte[ring[0].Data.Length * 8 * 4];
                var mixed = new float[bytes.Length / 4];
                var zero = new byte[65536];
                while (true)
                {
                    if (Volatile.Read(ref discard) != 0)
                        break;
                    MixBypasses(s, position, bytes, mixed, false);
                    int at = tail;
                    if (at == Volatile.Read(ref head))
                    {
                        // Listener and bypass callbacks can arrive in either
                        // order. Let the final DSP blocks reach their own rings.
                        if (Volatile.Read(ref closing) != 0 &&
                            (Stopwatch.GetTimestamp() > closeDeadline ||
                             position >= target && Stopwatch.GetTimestamp() > closeReadyDeadline))
                            break;
                        Thread.Sleep(2);
                        continue;
                    }
                    var b = ring[at];
                    long first = Math.Max(0, b.First),
                         limit = Volatile.Read(ref closing) != 0 ? Math.Min(target, max) : max;
                    long end = Math.Min(limit, b.First + b.Count / channels);
                    if (first > position)
                    {
                        long missing = Math.Min(first, limit) - position;
                        if (missing > 0)
                        {
                            Gap(position, missing, position == 0 ? 1 : 2);
                            while (missing > 0)
                            {
                                int n = (int)Math.Min(zero.Length / (channels * 4), missing);
                                s.Write(zero, 0, n * channels * 4);
                                missing -= n;
                                position += n;
                            }
                        }
                    }
                    int skip = (int)Math.Max(0, position - b.First) * channels;
                    int take = end > position ? (int)((end - position) * channels) : 0;
                    if (take > 0 && skip >= 0 && skip + take <= b.Count)
                    {
                        for (int k = skip; k < skip + take; k++)
                            if (float.IsNaN(b.Data[k]) || float.IsInfinity(b.Data[k]))
                            {
                                b.Data[k] = 0;
                                Flag(8);
                            }
                        Buffer.BlockCopy(b.Data, skip * 4, bytes, 0, take * 4);
                        s.Write(bytes, 0, take * 4);
                        position += take / channels;
                    }
                    Volatile.Write(ref tail, (at + 1) % ring.Length);
                    if (position >= max)
                    {
                        Flag(2);
                        break;
                    }
                }
                Volatile.Write(ref accepting, 0);
                long wanted = Volatile.Read(ref closing) != 0 ? Math.Min(target, max) : position;
                if (target > max)
                    Flag(2);
                if (Volatile.Read(ref discard) == 0 && Volatile.Read(ref observed) > 0 && wanted > 0)
                {
                    if (position < wanted)
                    {
                        long missing = wanted - position;
                        Gap(position, missing, 3);
                        while (missing > 0)
                        {
                            int n = (int)Math.Min(zero.Length / (channels * 4), missing);
                            s.Write(zero, 0, n * channels * 4);
                            missing -= n;
                            position += n;
                        }
                    }
                    if (position > wanted)
                    {
                        position = wanted;
                        s.SetLength(ReplayAudioFile.HeaderBytes + position * channels * 4);
                    }
                    MixBypasses(s, position, bytes, mixed, true);
                    int bypassGaps = 0;
                    foreach (var lane in lanes)
                        bypassGaps += lane.Dropped;
                    int finalFlags = Volatile.Read(ref flags),
                        finalGaps = Volatile.Read(ref gaps) + bypassGaps;
                    // A final game tick may end inside a PCM block already written.
                    // Keep diagnostic ranges inside the retained audio timeline too.
                    missingFrames = Math.Min(missingFrames, position);
                    for (int k = gapDetails.Count - 1; k >= 0; k--)
                    {
                        var gap = gapDetails[k];
                        if (gap.First >= position)
                            gapDetails.RemoveAt(k);
                        else
                        {
                            gap.Frames = Math.Min(gap.Frames, position - gap.First);
                            gapDetails[k] = gap;
                        }
                    }
                    Header(s, position, finalFlags, finalGaps);
                    s.Flush();
                    s.Position = 0;
                    ulong sum = ReplayAudioFile.Hash(s, s.Length);
                    s.Position = s.Length;
                    using (var w = new BinaryWriter(s, Encoding.UTF8, true))
                    {
                        w.Write(sum);
                        w.Flush();
                    }
                    Info.Rate = rate;
                    Info.Channels = channels;
                    Info.Frames = position;
                    Info.Bytes = s.Length;
                    Info.Checksum = sum;
                    Info.Flags = finalFlags;
                    Info.Gaps = finalGaps;
                    Info.Recorded = true;
                    Info.Diagnostics = true;
                    Info.ListenerOverruns = listenerOverruns;
                    Info.BypassOverruns = bypassGaps;
                    Info.MissingFrames = missingFrames;
                    Info.PeakListener = peakListener;
                    foreach (var lane in lanes)
                        Info.PeakBypass = Math.Max(Info.PeakBypass, lane.Peak);
                    Info.GapDetails.AddRange(gapDetails);
                }
                else
                {
                    Info.Flags = 4;
                }
            }
        }
        catch (Exception e)
        {
            failure = e;
            Info.Flags = 16;
        }
        finally
        {
            Volatile.Write(ref done, 1);
            if (Volatile.Read(ref discard) != 0 || !Info.Recorded)
                DeleteTemp();
        }
    }

    private void MixBypasses(FileStream file, long through, byte[] bytes, float[] mixed, bool final)
    {
        foreach (var lane in lanes)
        {
            Block block;
            while (lane.Peek(out block))
            {
                long end = block.First + block.Count / block.Channels;
                if (!final && end > through)
                    break;
                long first = Math.Max(lane.Through, Math.Max(0, block.First)), last = Math.Min(through, end);
                if (last > first)
                {
                    int frames = (int)(last - first), count = frames * channels,
                        skip = (int)(first - block.First) * block.Channels;
                    file.Position = ReplayAudioFile.HeaderBytes + first * channels * 4;
                    int length = count * 4, read = 0;
                    while (read < length)
                    {
                        int n = file.Read(bytes, read, length - read);
                        if (n == 0)
                            throw new EndOfStreamException("Bypass mix before listener PCM");
                        read += n;
                    }
                    Buffer.BlockCopy(bytes, 0, mixed, 0, length);
                    for (int frame = 0; frame < frames; frame++)
                    {
                        int at = skip + frame * block.Channels;
                        float left = block.Data[at], right = block.Channels == 1 ? left : block.Data[at + 1];
                        if (float.IsNaN(left) || float.IsInfinity(left) || float.IsNaN(right) ||
                            float.IsInfinity(right))
                        {
                            left = right = 0;
                            Flag(8);
                        }
                        float gain = block.Gain;
                        // Unity's filter normally supplies the stereo source
                        // bus. A raw mono bus uses equal-power center panning.
                        if (channels == 1)
                            mixed[frame] += (left + right) * .5f * gain;
                        else
                        {
                            if (block.Channels == 1)
                                gain *= .7071067811865475f;
                            mixed[frame * channels] += left * gain;
                            mixed[frame * channels + 1] += right * gain;
                        }
                    }
                    for (int k = 0; k < count; k++)
                        if (float.IsNaN(mixed[k]) || float.IsInfinity(mixed[k]))
                        {
                            mixed[k] = 0;
                            Flag(8);
                        }
                    Buffer.BlockCopy(mixed, 0, bytes, 0, length);
                    file.Position = ReplayAudioFile.HeaderBytes + first * channels * 4;
                    file.Write(bytes, 0, length);
                }
                lane.Through = Math.Max(lane.Through, last);
                lane.Pop();
            }
        }
        file.Position = ReplayAudioFile.HeaderBytes + through * channels * 4;
    }

    private void DeleteTemp()
    {
        try
        {
            if (File.Exists(temp))
                File.Delete(temp);
        }
        catch (IOException)
        {
        }
        catch (UnauthorizedAccessException)
        {
        }
    }

    public bool Commit(string destination)
    {
        if (!writer.Join(10000))
        {
            Discard();
            Info.Recorded = false;
            Info.Frames = Info.Bytes = 0;
            Info.Flags |= 16;
            return false;
        }
        if (!Info.Recorded || Volatile.Read(ref discard) != 0)
        {
            DeleteTemp();
            return false;
        }
        if (failure != null)
            throw new IOException("Audio capture failed", failure);
        if (File.Exists(destination))
            throw new IOException("Existing recorded audio preserved");
        Directory.CreateDirectory(Path.GetDirectoryName(destination));
        File.Move(temp, destination);
        return true;
    }
}

// A single 2D bypass source, after its source filters and before source volume.
// The audio callback only copies; final gain and summation run on the writer.
public sealed class ReplayAudioLane
{
    private readonly ReplayAudioSession owner;
    private readonly ReplayAudioSession.Block[] ring = new ReplayAudioSession.Block[16];
    private int head, tail, dropped, peak;
    public int Peak
    {
        get {
            return Volatile.Read(ref peak);
        }
    }
    internal long Through;

    internal ReplayAudioLane(ReplayAudioSession session, int capacity)
    {
        owner = session;
        for (int i = 0; i < ring.Length; i++)
            ring[i] = new ReplayAudioSession.Block(capacity);
    }

    public long MemoryBytes
    {
        get {
            return (long)ring.Length * ring[0].Data.Length * 4;
        }
    }
    public int Dropped
    {
        get {
            return Volatile.Read(ref dropped);
        }
    }

    public void Push(float[] samples, int channels, long first, float gain, int sampleCount = -1)
    {
        if (!owner.Accept(first))
            return;
        int count = sampleCount < 0 ? (samples == null ? 0 : samples.Length) : sampleCount;
        if (samples == null || channels < 1 || channels > 2 || count < 0 || count > samples.Length ||
            count % channels != 0 || count > ring[0].Data.Length || float.IsNaN(gain) ||
            float.IsInfinity(gain) || gain < 0)
        {
            owner.Invalid();
            return;
        }
        int at = head, next = (at + 1) % ring.Length;
        if (next == Volatile.Read(ref tail))
        {
            owner.Overrun();
            Interlocked.Increment(ref dropped);
            return;
        }
        peak = Math.Max(peak, (next - Volatile.Read(ref tail) + ring.Length) % ring.Length);
        var block = ring[at];
        Array.Copy(samples, block.Data, count);
        block.First = first;
        block.Count = count;
        block.Channels = channels;
        block.Gain = gain;
        Volatile.Write(ref head, next);
    }

    internal bool Peek(out ReplayAudioSession.Block block)
    {
        int at = tail;
        if (at == Volatile.Read(ref head))
        {
            block = null;
            return false;
        }
        block = ring[at];
        return true;
    }

    internal void Pop()
    {
        Volatile.Write(ref tail, (tail + 1) % ring.Length);
    }
}
