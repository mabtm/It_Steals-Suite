using System;
using System.IO;
using System.Text;
using UnityEngine;

// Recovery runs on a sleeping IO worker. Decode only our bounded v7-v13 run
// archive; never deserialize arbitrary objects or touch Unity scene objects.
public static class PracticeMapArchive
{
    private sealed class Reader : IDisposable
    {
        public readonly BinaryReader Input;

        public Reader(byte[] bytes)
        {
            Input = new BinaryReader(new MemoryStream(bytes, false), new UTF8Encoding(false, true));
        }

        public int Count(int maximum)
        {
            int n = Input.ReadInt32();
            if (n < 0 || n > maximum)
                throw new InvalidDataException("Recorded count exceeds limit");
            return n;
        }

        public bool Flag()
        {
            byte b = Input.ReadByte();
            if (b > 1)
                throw new InvalidDataException("Invalid recorded flag");
            return b == 1;
        }

        public float Coordinate()
        {
            float f = Input.ReadSingle();
            if (float.IsNaN(f) || float.IsInfinity(f) || Math.Abs(f) > 100000)
                throw new InvalidDataException("Invalid recorded coordinate");
            return f;
        }

        public string Text(int maximum)
        {
            int n = Count(maximum);
            byte[] b = Input.ReadBytes(n);
            if (b.Length != n)
                throw new EndOfStreamException();
            string s = new UTF8Encoding(false, true).GetString(b);
            if (s.IndexOf('\0') >= 0)
                throw new InvalidDataException("Invalid recorded text");
            return s;
        }

        public int[] Ints(int maximum)
        {
            int n = Count(maximum);
            var a = new int[n];
            for (int i = 0; i < n; i++)
                a[i] = Input.ReadInt32();
            return a;
        }

        public Vector3[] Vectors(int maximum)
        {
            int n = Count(maximum);
            var a = new Vector3[n];
            for (int i = 0; i < n; i++)
                a[i] = new Vector3(Coordinate(), Coordinate(), Coordinate());
            return a;
        }

        public WallOutline[] Outlines()
        {
            int n = Count(20000);
            var a = new WallOutline[n];
            for (int i = 0; i < n; i++)
            {
                bool approximate = Flag();
                var p = Vectors(4);
                if (p.Length != 4)
                    throw new InvalidDataException("Invalid wall contour");
                a[i] = new WallOutline { Approximate = approximate, Corners = p };
            }
            return a;
        }

        public void Dispose()
        {
            Input.Dispose();
        }
    }

    private static bool Finite(double value)
    {
        return !double.IsNaN(value) && !double.IsInfinity(value);
    }

    private static void Selection(System.Collections.Generic.IEnumerable<int> values, int count)
    {
        var seen = new bool[count];
        foreach (int id in values)
        {
            if (id < 0 || id >= count || seen[id])
                throw new InvalidDataException("Invalid recorded selection");
            seen[id] = true;
        }
    }

    private static double Time(BinaryReader b, double duration, ref double previous)
    {
        double at = b.ReadDouble();
        if (!Finite(at) || at < previous || at > duration + .1)
            throw new InvalidDataException("Invalid monster event time");
        previous = at;
        return at;
    }

    public static PracticeMapData Decode(byte[] bytes)
    {
        if (bytes == null || bytes.Length > PracticeMapChannel.Capacity - 64)
            throw new InvalidDataException("Recorded file exceeds limit");
        using (var r = new Reader(bytes))
        {
            var b = r.Input;
            if (b.ReadUInt32() != 0x504D5349u)
                throw new InvalidDataException("Unsupported recorded run format");
            int format = b.ReadInt32();
            if (format < 7 || format > 15)
                throw new InvalidDataException("Unsupported recorded run format");
            var d = new PracticeMapData {
                Pid = b.ReadInt32(),         Scene = b.ReadInt32(),         Epoch = b.ReadInt32(),
                Seed = b.ReadInt32(),        Recipe = b.ReadInt32(),        SourceKind = b.ReadInt32(),
                Spawn = b.ReadInt32(),       Keep = b.ReadInt32(),          Flags = b.ReadInt32(),
                Stage = b.ReadInt32(),       Requested = b.ReadInt32(),     Completed = b.ReadInt32(),
                LiveSpawn = b.ReadInt32(),   LiveSeed = b.ReadInt32(),      LiveRecipe = b.ReadInt32(),
                CapturedUtc = b.ReadInt64(), RequestSerial = b.ReadUInt32()
            };
            d.Hash = r.Text(65);
            d.Message = r.Text(1024);
            d.Spawns = r.Vectors(128);
            d.Points = r.Vectors(1024);
            d.Selected = r.Ints(64);
            d.LiveSelected = r.Ints(64);
            d.Vertices = r.Vectors(30000);
            d.Indices = r.Ints(180000);
            int n = r.Count(20000);
            d.Walls = new Rect[n];
            for (int i = 0; i < n; i++)
            {
                float x0 = r.Coordinate(), z0 = r.Coordinate(), x1 = r.Coordinate(), z1 = r.Coordinate();
                if (x0 > x1 || z0 > z1)
                    throw new InvalidDataException("Invalid recorded wall");
                d.Walls[i] = Rect.MinMaxRect(x0, z0, x1, z1);
            }
            n = r.Count(2000);
            for (int i = 0; i < n; i++)
                d.Samples.Add(new SpawnSelection { Spawn = b.ReadInt32(), Crystals = r.Ints(64) });
            d.NavigationStatus = b.ReadInt32();
            d.NavigationQueries = b.ReadInt32();
            d.NavigationTotal = b.ReadInt32();
            d.NavigationNote = r.Text(1024);
            d.NavigationNodes = r.Vectors(10000);
            d.NavigationEdges = r.Ints(60000);
            d.NavigationAnchors = r.Ints(1152);
            d.RngStatus = b.ReadInt32();
            d.RngInit = b.ReadInt32();
            d.RngRange = b.ReadInt32();
            d.RngAdvance = b.ReadInt32();
            d.RngSite = b.ReadInt32();
            d.RngProbes = b.ReadInt32();
            d.SearchBackend = b.ReadInt32();
            d.RngNote = r.Text(1024);
            d.NavigationModel = b.ReadInt32();
            d.WallOutlines = r.Outlines();
            d.AgentRadius = r.Coordinate();
            d.AgentHeight = r.Coordinate();
            d.AgentStep = r.Coordinate();
            d.AgentSlope = r.Coordinate();
            d.AgentFloor = r.Coordinate();
            d.Obstacles = r.Outlines();
            if (!r.Flag())
                throw new InvalidDataException("Recorded file contains no run");
            var review = new RunReviewData { Id = b.ReadInt64(), Outcome = b.ReadInt32(),
                                             Seconds = b.ReadDouble(), Reduced = r.Flag() };
            d.Review = review;
            if (review.Id <= 0 || review.Outcome < 1 || review.Outcome > 5 || !Finite(review.Seconds) ||
                review.Seconds < 0 || review.Seconds > 315360000)
                throw new InvalidDataException("Invalid recorded run identity");
            n = r.Count(32768);
            double previous = 0;
            for (int i = 0; i < n; i++)
            {
                double seconds = b.ReadDouble();
                if (!Finite(seconds) || seconds < previous || seconds > review.Seconds + .1)
                    throw new InvalidDataException("Invalid recorded time sequence");
                previous = seconds;
                review.Trace.Add(
                    new RunPose { Seconds = seconds,
                                  Position = new Vector3(r.Coordinate(), r.Coordinate(), r.Coordinate()) });
            }
            review.Collected.AddRange(r.Ints(64));
            n = r.Count(64);
            previous = 0;
            for (int i = 0; i < n; i++)
            {
                double seconds = b.ReadDouble();
                if (!Finite(seconds) || seconds < previous || seconds > review.Seconds + .1)
                    throw new InvalidDataException("Invalid pickup time");
                previous = seconds;
                review.PickupSeconds.Add(seconds);
            }
            if (format >= 8)
            {
                review.ValidationChecked = r.Flag();
                review.Valid = r.Flag();
                review.ObservedFromStart = r.Flag();
                review.SeedKnown = r.Flag();
                review.SeedKind = b.ReadInt32();
                review.Aids = b.ReadUInt32();
                review.Warnings = b.ReadUInt32();
                review.ValidityReason = r.Text(1024);
                if (format >= 15)
                {
                    review.Category = (RunCategory)b.ReadByte();
                    review.SelectedSeed = b.ReadInt32();
                    review.RunSeed = b.ReadInt32();
                    review.RawSpawnSeed = r.Flag();
                    review.InitialAids = b.ReadUInt32();
                    if ((byte)review.Category > 2)
                        throw new InvalidDataException("Invalid recorded run category");
                }
                else
                    review.Category = (review.Aids & (uint)(RunAids.Practice | RunAids.FixedSeed)) != 0 ? RunCategory.Practice : RunCategory.RandomSeed;
                if (review.SeedKind < 0 || review.SeedKind > 2 ||
                    review.Valid != RunValidity.Eligible(new RunRecord {
                        ValidationChecked = review.ValidationChecked,
                        ObservedFromStart = review.ObservedFromStart, SeedKnown = review.SeedKnown,
                        SeedType = (SeedKind)review.SeedKind, Aids = (RunAids)review.Aids,
                        Category = review.Category, SelectedSeed = review.SelectedSeed, Seed = review.RunSeed,
                        RawSpawnSeed = review.RawSpawnSeed, InitialAids = (RunAids)review.InitialAids,
                        Warnings = (RunWarnings)review.Warnings
                    }))
                    throw new InvalidDataException("Inconsistent recorded validation");
                review.MonstersReduced = r.Flag();
                n = r.Count(128);
                var ids = new System.Collections.Generic.HashSet<int>();
                int poses = 0, paths = 0;
                for (int i = 0; i < n; i++)
                {
                    var m = new ReplayMonster { Id = b.ReadInt32(), Kind = b.ReadInt32() };
                    if (m.Id <= 0 || !ids.Add(m.Id) || m.Kind < 1 || m.Kind > 4)
                        throw new InvalidDataException("Invalid monster identity");
                    int count = r.Count(32768);
                    poses += count;
                    if (poses > 32768)
                        throw new InvalidDataException("Monster sample limit exceeded");
                    previous = 0;
                    for (int j = 0; j < count; j++)
                    {
                        double at = Time(b, review.Seconds, ref previous);
                        m.Trace.Add(new RunPose { Seconds = at,
                                                  Position = new Vector3(r.Coordinate(), r.Coordinate(),
                                                                         r.Coordinate()),
                                                  Continuous = r.Flag() });
                    }
                    count = r.Count(1024);
                    paths += count;
                    if (paths > 1024)
                        throw new InvalidDataException("Monster path limit exceeded");
                    previous = 0;
                    for (int j = 0; j < count; j++)
                    {
                        double at = Time(b, review.Seconds, ref previous);
                        m.Paths.Add(new ReplayPath { Seconds = at, Corners = r.Vectors(64) });
                    }
                    review.Monsters.Add(m);
                }
                n = r.Count(4096);
                previous = 0;
                for (int i = 0; i < n; i++)
                {
                    double at = Time(b, review.Seconds, ref previous);
                    var p = new Vector3(r.Coordinate(), r.Coordinate(), r.Coordinate());
                    int kind = b.ReadInt32(), actor = b.ReadInt32();
                    if (kind < 1 || kind > 2 || !(format >= 9 && actor == 0) && !ids.Contains(actor))
                        throw new InvalidDataException("Invalid light event");
                    review.Flashes.Add(
                        new ReplayFlash { Seconds = at, Player = p, Kind = kind, Actor = actor });
                }
                if (format >= 10)
                {
                    review.LightSources = b.ReadUInt32();
                    review.LightRejected = r.Count(int.MaxValue);
                    if ((review.LightSources & ~255u) != 0)
                        throw new InvalidDataException("Invalid light observer sources");
                }
            }
            if (format >= 10)
            {
                d.MovementTick = r.Coordinate();
                d.MovementSprintStep = r.Coordinate();
                if (d.MovementTick < 0 || d.MovementTick > 1 || d.MovementSprintStep < 0 ||
                    d.MovementSprintStep > 1 || (d.MovementTick == 0) != (d.MovementSprintStep == 0))
                    throw new InvalidDataException("Invalid captured movement model");
            }
            if (format >= 11)
            {
                d.Movement = MovementSummary.Read(b);
                if (d.Movement != null && (!d.Movement.Matches(d)))
                    throw new InvalidDataException("Movement context differs from map");
            }
            if (format >= 12)
                review.Motion = ReplayMotionInfo.Read(b);
            if (format >= 13)
                MapPhysicalData.Read(b, d, format);
            if (format >= 8)
            {
                if (bytes.Length < 8 || b.ReadUInt64() != PracticeMapData.Checksum(bytes, bytes.Length - 8))
                    throw new InvalidDataException("Recorded checksum mismatch");
            }
            if (b.BaseStream.Position != bytes.Length ||
                Array.IndexOf(SpeedrunManager.LevelIndices, d.Scene) < 0 || d.Pid < 0 || d.Epoch < 0 ||
                d.Flags < 0 || (d.Flags & ~255) != 0 || d.Recipe < 0 || d.Recipe > 2 || d.SourceKind < 0 ||
                d.SourceKind > 2 || d.LiveRecipe < 0 || d.LiveRecipe > 2 || d.Stage != 1 ||
                d.Requested != 0 || d.Completed != 0 || d.Samples.Count != 0 || d.Keep < 0 || d.Keep > 64 ||
                d.Spawn < 0 || d.Spawn >= d.Spawns.Length || d.LiveSpawn < 0 ||
                d.LiveSpawn >= d.Spawns.Length || d.Selected.Length != Math.Min(d.Keep, d.Points.Length) ||
                review.PickupSeconds.Count != review.Collected.Count || d.Indices.Length % 3 != 0)
                throw new InvalidDataException("Invalid recorded metadata");
            foreach (int id in d.Indices)
                if (id < 0 || id >= d.Vertices.Length)
                    throw new InvalidDataException("Invalid recorded floor index");
            if (d.NavigationStatus < 0 || d.NavigationStatus > 3 || d.NavigationModel < 0 ||
                d.NavigationModel > 1 || d.NavigationQueries < 0 || d.NavigationQueries > d.NavigationTotal ||
                d.NavigationTotal < 0 || d.NavigationTotal > 1000000 || d.NavigationEdges.Length % 2 != 0 ||
                d.NavigationStatus == 2 && d.NavigationAnchors.Length != d.Spawns.Length + d.Points.Length)
                throw new InvalidDataException("Invalid recorded navigation metadata");
            foreach (int node in d.NavigationEdges)
                if (node < 0 || node >= d.NavigationNodes.Length)
                    throw new InvalidDataException("Invalid recorded navigation edge");
            foreach (int node in d.NavigationAnchors)
                if (node < -1 || node >= d.NavigationNodes.Length)
                    throw new InvalidDataException("Invalid recorded navigation anchor");
            Selection(d.Selected, d.Points.Length);
            Selection(d.LiveSelected, d.Points.Length);
            Selection(review.Collected, d.Points.Length);
            foreach (int id in review.Collected)
                if (Array.IndexOf(d.Selected, id) < 0)
                    throw new InvalidDataException("Pickup outside recorded layout");
            if (d.AgentRadius < 0 || d.AgentRadius > 5 || d.AgentHeight < 0 || d.AgentHeight > 10 ||
                d.AgentStep < 0 || d.AgentStep > 10 || d.AgentSlope < 0 || d.AgentSlope > 90)
                throw new InvalidDataException("Invalid recorded player profile");
            // Navigation is reconstructed from the captured physical scene,
            // never restored with agent settings from another Unity session.
            if (d.NavigationStatus != 2)
            {
                d.NavigationStatus = d.NavigationModel = 0;
                d.NavigationNodes = new Vector3[0];
                d.NavigationEdges = d.NavigationAnchors = new int[0];
            }
            d.Flags &= ~(8 | 128);
            d.SearchBackend = 0;
            return d;
        }
    }
}
