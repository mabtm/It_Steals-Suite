using System;
using System.Collections.Generic;
using System.Threading;
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
            Id=id, Category=RunCategory.SetSeed, SelectedSeed=42, Seconds=time, ValidationChecked=true, Valid=valid } };
        map.Review.Collected.AddRange(new[] {0,1}); map.Review.PickupSeconds.AddRange(new[] {time/2,time});
        return RunSplitReference.Capture(map);
    }
    private static void Await(Func<bool> done)
    {
        var end=DateTime.UtcNow.AddSeconds(5);
        while (!done() && DateTime.UtcNow<end) Thread.Sleep(1);
        Check(done(),"worker completed");
    }
    public static int Main()
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
        Console.WriteLine("Set-Seed reference: exact mode/seed/recipe/spawn/layout, valid PB priority, immutable snapshots and restart cancellation: PASS");
        return 0;
    }
}
