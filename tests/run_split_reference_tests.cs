using System;
using System.Collections.Generic;
using System.Threading;
using System.IO;
using System.Security.Cryptography;
using System.Diagnostics;
namespace UnityEngine
{
    public struct Vector3
    {
        public float x,y,z;
        public Vector3(float a, float b, float c) { x=a; y=b; z=c; }
        public static Vector3 operator -(Vector3 a, Vector3 b) { return new Vector3(a.x-b.x,a.y-b.y,a.z-b.z); }
        public float sqrMagnitude { get { return x*x+y*y+z*z; } }
    }
}
public enum RunCategory { RandomSeed, SetSeed, Practice }
public enum RunOutcome { Running, Completed }
public sealed class RunReviewData
{
    public RunCategory Category;
    public long Id;
    public int SelectedSeed, Outcome;
    public bool RawSpawnSeed, ValidationChecked, Valid;
    public double Seconds;
    public readonly List<int> Collected = new List<int>();
    public readonly List<double> PickupSeconds = new List<double>();
}
public sealed class PracticeMapData
{
    public int Scene=3, Seed=42, Spawn=0, Keep=2;
    public string Hash="catalog";
    public int[] Selected = {0,1};
    public UnityEngine.Vector3[] Points = {new UnityEngine.Vector3(1,0,1),new UnityEngine.Vector3(2,0,2)};
    public UnityEngine.Vector3[] Spawns = {new UnityEngine.Vector3(0,0,0)};
    public RunReviewData Review;
}
public static class PracticeMapChannel { public const int Capacity=8*1024*1024; }
// Archive decoding is covered against real .isrun fixtures by native archive
// tests. This test isolates immutable matching and worker generation ownership.
public static class PracticeMapArchive
{
    public static PracticeMapData Decode(byte[] bytes) { throw new Exception("unexpected disk read"); }
}
public static class RunSplitReferenceTests
{
    private static void Check(bool value, string message) { if (!value) throw new Exception(message); }
    private static RunSplitReference Item(long id, double time, bool valid)
    {
        var map = new PracticeMapData { Review = new RunReviewData {
            Id=id, Outcome=1, Category=RunCategory.SetSeed, SelectedSeed=42, Seconds=time, ValidationChecked=true, Valid=valid } };
        map.Review.Collected.AddRange(new[] {0,1}); map.Review.PickupSeconds.AddRange(new[] {time/2,time});
        return RunSplitReference.Capture(map);
    }
    private static void Await(Func<bool> done)
    {
        var end=DateTime.UtcNow.AddSeconds(5);
        while (!done() && DateTime.UtcNow<end) Thread.Sleep(1);
        Check(done(),"worker completed");
    }
    private static void SectorBookCases()
    {
        string folder = Path.Combine(Path.GetTempPath(), "elite-sectors-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        try
        {
            string path = Path.Combine(folder,"sectors.bin");
            var target = Item(1000,15,true); target.PickupSeconds = new[] {5d,12d};
            var book = new RunSegmentBook(path);
            Check(book.Observe(target) && book.Snapshot(target).Theoretical(target.Collected)==15,"finish sector included");
            var reset = Item(1001,6,true); reset.Outcome=2; reset.Collected=new[] {0}; reset.PickupSeconds=new[] {4d};
            Check(book.Observe(reset) && book.Snapshot(target).Theoretical(target.Collected)==14,"short reset improves first sector without a replay");
            var other = Item(1002,11.5,true); other.Outcome=2; other.PickupSeconds=new[] {6d,11d};
            Check(book.Observe(other) && book.Snapshot(target).Theoretical(target.Collected)==12,"sums segment durations, never cumulative splits");
            var reverse = Item(1003,2,true); reverse.Collected=new[] {1,0}; reverse.PickupSeconds=new[] {1d,2d};
            Check(book.Observe(reverse) && book.Snapshot(target).Theoretical(target.Collected)==12,"route prefixes stay separate");
            Check(book.Snapshot(reverse).Theoretical(reverse.Collected)==2,"zero-second finish is recorded");
            var invalid = Item(1004,1,false); invalid.PickupSeconds=new[] {.1,.2};
            Check(!book.Observe(invalid) && book.Snapshot(target).Theoretical(target.Collected)==12,"invalid attempts cannot poison sector bests");
            var malformed = Item(1005,3,true); malformed.PickupSeconds=new[] {2d,1d};
            Check(!book.Observe(malformed),"non-monotonic split timestamps rejected atomically");
            malformed=Item(1006,3,true); malformed.Collected=new[] {0,0};
            Check(!book.Observe(malformed),"duplicate crystals rejected");
            var spawn = Item(1007,2,true); spawn.SpawnPosition.x=1;
            book.Observe(spawn);
            Check(book.Snapshot(target).Theoretical(target.Collected)==12,"another spawn cannot contribute sectors");
            Check(book.Save(),"atomic sector file saved");
            var loaded=new RunSegmentBook(path);
            Check(loaded.Error==null && loaded.Snapshot(target).Theoretical(target.Collected)==12,"best sectors survive restart");
            reset.PickupSeconds[0]=3;
            loaded.Observe(reset); Check(loaded.Save(),"changed best saved with last-good backup");
            byte[] bytes=File.ReadAllBytes(path); bytes[bytes.Length-1]^=1; File.WriteAllBytes(path,bytes);
            var recovered=new RunSegmentBook(path);
            Check(recovered.Snapshot(target).Theoretical(target.Collected)==12,"corrupt primary recovers last-good backup");
            reset.PickupSeconds[0]=2; recovered.Observe(reset); Check(recovered.Save(),"recovered history can commit without destroying backup");
            Check(new RunSegmentBook(path).Snapshot(target).Theoretical(target.Collected)==10,"recovery retains new sector best");
            bytes=File.ReadAllBytes(path); bytes[4]=2;
            using(var hash=SHA256.Create()) { var sum=hash.ComputeHash(bytes,0,bytes.Length-32); Array.Copy(sum,0,bytes,bytes.Length-32,32); }
            File.WriteAllBytes(path,bytes);
            var future=new RunSegmentBook(path);
            Check(future.Error!=null && !future.Observe(reset) && !future.Save() &&
                Convert.ToBase64String(File.ReadAllBytes(path))==Convert.ToBase64String(bytes),"future format preserved despite an older backup");
            RunSplitArchive.DirectoryPath=Path.Combine(folder,"RunReviews");
            var queued=Item(1008,2,true); queued.Outcome=2; queued.Collected=new[] {0}; queued.PickupSeconds=new[] {1d};
            RunSplitArchive.RecordEnd(queued); RunSplitArchive.Begin(1009);
            var next=Item(1009,0,false);
            RunSplitArchive.Find(next,new RunSplitReference[0]);
            Await(()=>RunSplitArchive.History(1009)!=null);
            Check(RunSplitArchive.History(1009).Profile.Best(new[] {0},1)==1,"immediate restart keeps queued short-reset sectors");
            Check(new RunSegmentBook(Path.Combine(folder,"set-seed-sectors-v1.bin")).Snapshot(next).Best(new[] {0},1)==1,
                  "worker persists short resets independently of replay retention");
        }
        finally { RunSplitArchive.Flush(); Directory.Delete(folder,true); RunSplitArchive.DirectoryPath=null; }
    }
    private static ulong WireChecksum(byte[] bytes,int n)
    {
        ulong hash=14695981039346656037UL;
        unchecked { for(int i=0;i<n;i++){hash^=bytes[i];hash*=1099511628211UL;} }
        return hash;
    }
    private static void SplitFileCases(string fixture)
    {
        var buffer=new byte[65536];
        var summary=RunSplitFile.Read(fixture,buffer);
        Check(summary!=null && summary.Id==6001 && summary.Valid && summary.Seconds==25 && summary.Collected.Length>0,
              "streaming reader agrees with native validated v15 golden recording");
        byte[] original=File.ReadAllBytes(fixture);int vertexOffset;
        using(var input=new BinaryReader(new MemoryStream(original)))
        {
            input.BaseStream.Position=80;
            for(int i=0;i<2;i++){int n=input.ReadInt32();input.BaseStream.Position+=n;}
            for(int i=0;i<4;i++){int n=input.ReadInt32();input.BaseStream.Position+=n*(i<2?12:4);}
            vertexOffset=(int)input.BaseStream.Position;
        }
        int count=BitConverter.ToInt32(original,vertexOffset),extra=(30000-count)*12;
        var large=new byte[original.Length+extra];
        Array.Copy(original,large,vertexOffset);Array.Copy(BitConverter.GetBytes(30000),0,large,vertexOffset,4);
        Array.Copy(original,vertexOffset+4,large,vertexOffset+4,count*12);
        Array.Copy(original,vertexOffset+4+count*12,large,vertexOffset+4+30000*12,original.Length-(vertexOffset+4+count*12));
        Array.Copy(BitConverter.GetBytes(WireChecksum(large,large.Length-8)),0,large,large.Length-8,8);
        string path=Path.Combine(Path.GetTempPath(),"elite-split-file-"+Guid.NewGuid().ToString("N")+".isrun");
        try
        {
            File.WriteAllBytes(path,large);
            var expanded=RunSplitFile.Read(path,buffer);
            Check(summary.Matches(expanded) && expanded.Id==summary.Id,"unused large geometry does not change split identity");
            for(int i=0;i<3;i++)RunSplitFile.Read(path,buffer);
            long start=GC.GetAllocatedBytesForCurrentThread();var watch=Stopwatch.StartNew();
            for(int i=0;i<40;i++)RunSplitFile.Read(path,buffer);
            watch.Stop();long allocated=GC.GetAllocatedBytesForCurrentThread()-start;
            Check(allocated/40<65536,"archive scan allocation bounded independently of floor/trace size");
            Console.WriteLine("Streaming split scan: "+large.Length+" byte replay, "+(allocated/40)+" bytes allocated/read, "+
                (watch.Elapsed.TotalMilliseconds/40).ToString("F3",System.Globalization.CultureInfo.InvariantCulture)+" ms/read; old byte-array read alone allocates >= file size");
            large[large.Length-9]^=1;File.WriteAllBytes(path,large);
            bool rejected=false;try{RunSplitFile.Read(path,buffer);}catch(InvalidDataException){rejected=true;}
            Check(rejected,"full-payload checksum still validates skipped recording channels");
        }
        finally{File.Delete(path);}
    }
    private static void WarmArchiveCases(string fixture)
    {
        string root=Path.Combine(Path.GetTempPath(),"elite-split-warm-"+Guid.NewGuid().ToString("N"));
        string folder=Path.Combine(root,"RunReviews");Directory.CreateDirectory(folder);
        try
        {
            File.Copy(fixture,Path.Combine(folder,"run-6001.isrun"));
            RunSplitArchive.DirectoryPath=folder;RunSplitArchive.WarmUp(folder);
            Await(()=>File.Exists(Path.Combine(root,"set-seed-sectors-v1.bin")));
            var target=RunSplitFile.Read(fixture,new byte[65536]);
            int first=target.Collected[0];double firstAt=target.PickupSeconds[0];
            target.Seconds=0;target.Outcome=0;target.Collected=new int[0];target.PickupSeconds=new double[0];
            for(int i=0;i<12;i++)
            {
                target.Id=6501+i;RunSplitArchive.Begin(target.Id);RunSplitArchive.Find(target,new RunSplitReference[0]);
                Await(()=>RunSplitArchive.State(target.Id)!=1);
                Check(RunSplitArchive.Reference(target.Id)!=null && RunSplitArchive.Reference(target.Id).Id==6001,"warmed archive reference survives rapid restarts");
                Check(RunSplitArchive.History(target.Id).Profile.Best(new[] {first},1)==firstAt,"warm backfill preserves sector history");
            }
            Console.WriteLine("Background warm-up: actual v15 archive backfill, cached matching reference and sector history across 12 restarts: PASS");
        }
        finally{RunSplitArchive.Flush();RunSplitArchive.DirectoryPath=null;Directory.Delete(root,true);}
    }
    public static int Main(string[] args)
    {
        var target=Item(100,0,false); var pb=Item(80,20,true); var replay=Item(90,10,false);
        Check(target.Matches(pb),"identical layout matches");
        pb.SpawnPosition.x=1; Check(!target.Matches(pb),"same seed with another spawn position rejected"); pb.SpawnPosition.x=0;
        pb.Positions[1].z+=.01f; Check(!target.Matches(pb),"different selected crystal position rejected"); pb.Positions[1].z-=.01f;
        pb.RawSeed=true; Check(!target.Matches(pb),"different recipe rejected"); pb.RawSeed=false;
        pb.SelectedSeed=13; Check(!target.Matches(pb),"different selected seed rejected"); pb.SelectedSeed=42;
        pb.Scene=7; Check(!target.Matches(pb),"different mode rejected"); pb.Scene=3;
        Check(RunSplitReference.Better(pb,replay) && !RunSplitReference.Better(replay,pb),"valid PB preferred over faster unverified replay");
        RunSplitArchive.Begin(100);
        RunSplitArchive.Find(target,new[] {replay,pb});
        Await(()=>RunSplitArchive.State(100)!=1);
        Check(RunSplitArchive.Reference(100).Id==80,"worker selects matching PB");
        RunSplitArchive.Begin(101);
        Check(RunSplitArchive.Reference(100)==null && RunSplitArchive.Reference(101)==null,"new run drops old reference");
        var next=Item(101,0,false); replay.Seed++;
        RunSplitArchive.Find(next,new[] {replay});
        Await(()=>RunSplitArchive.State(101)!=1);
        Check(RunSplitArchive.Reference(101)==null,"incompatible recording cannot supply deltas");
        RunSplitArchive.Find(next,new[] {pb}); RunSplitArchive.Begin(102);
        Thread.Sleep(10);
        Check(RunSplitArchive.Reference(102)==null && RunSplitArchive.State(102)==0,"stale background result cannot attach to restarted run");
        var routeTarget=Item(200,0,false); routeTarget.Collected=new[] {1}; routeTarget.PickupSeconds=new[] {0d};
        var fast=Item(201,10,true); var compatible=Item(202,20,true); compatible.Collected=new[] {1,0};
        RunSplitArchive.Begin(200); RunSplitArchive.Find(routeTarget,new[] {fast,compatible});
        Await(()=>RunSplitArchive.State(200)!=1);
        Check(RunSplitArchive.Reference(200).Id==202,"matching observed route is preferred over incompatible faster PB");
        routeTarget.Collected=new[] {0};
        RunSplitArchive.Find(routeTarget,new[] {fast,compatible});
        Await(()=>RunSplitArchive.State(200)!=1);
        Check(RunSplitArchive.Reference(200).Id==201,"same-run route request selects a compatible baseline without another seed");
        SectorBookCases();
        if(args.Length>0){SplitFileCases(args[0]);WarmArchiveCases(args[0]);}
        Console.WriteLine("Set-Seed reference: exact mode/seed/recipe/spawn/layout, valid PB priority, immutable snapshots and restart cancellation: PASS");
        return 0;
    }
}
