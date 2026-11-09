using System;
using System.IO;
using System.Diagnostics;
namespace UnityEngine.SceneManagement { public static class SceneManager { public static int Loads;public static void LoadScene(string name){Loads++;} } }
public enum RunOutcome { Running, Completed, Reset, Death, Interrupted, Abandoned }
[Flags] public enum RunAids : uint { None=0, FixedSeed=2, Fullbright=8 }
[Flags] public enum RunWarnings : uint { None=0, InvalidTiming=16, ModeInterrupted=32 }
public sealed class RunRecord
{
    public int Scene;public long Id,EndedUtc;public double Seconds;public RunOutcome Outcome;
    public bool ValidationChecked=true,Eligible=true;public RunAids Aids,InitialAids;public RunWarnings Warnings;
}
public sealed class FakeTracker { public RunRecord Active; }
public sealed class FakeStore {public string LastReportError,ReportQueueWarning;}
public static class RunValidity { public static bool Eligible(RunRecord record){return record.Eligible;} }
public sealed partial class SpeedrunManager : IDisposable
{
    public static SpeedrunManager Instance;
    private byte timerState;
    private readonly FakeStore store=new FakeStore();
    private readonly FakeTracker tracker=new FakeTracker();
    private long childId=639000000000000000L;
    private double childStarted;
    public SpeedrunManager(string directory){Instance=this;fullGameStore=new FullGameStore(directory);}
    private void End(RunOutcome outcome)
    {
        if(tracker.Active==null)return;
        double now=FullClock;var record=tracker.Active;record.Seconds=Math.Max(0,now-childStarted);
        record.Outcome=outcome;record.EndedUtc=DateTime.UtcNow.Ticks;
        FullGameAttemptEnded(record,now);tracker.Active=null;timerState=2;
    }
    public void TestLoad()
    {
        if(timerState==1)End(RunOutcome.Reset);
        int scene=FullGameScene;FullGameBeforeReload(scene);FullGameSceneLoaded(scene);
        childStarted=FullClock;
        tracker.Active=new RunRecord {Scene=scene,Id=++childId,Aids=fullGame.Record.Plan.Mode==FullGameMode.SetSeed?RunAids.FixedSeed:RunAids.None};
        timerState=1;FullGamePlayerReady(scene,childStarted);
    }
    public void TestFinish(){End(RunOutcome.Completed);}
    public void TestDeath(bool bad=false){if(bad){tracker.Active.Aids|=RunAids.Fullbright;tracker.Active.Eligible=false;}End(RunOutcome.Death);}
    public void TestScene(int scene){FullGameSceneLoaded(scene);}
    public FullGameRecord TestRecord(){return fullGame.Snapshot(FullClock);}
    public SnapshotBuffer TestSnapshot(){var data=new SnapshotBuffer();WriteFullGame(data);return data;}
    public void Dispose(){StopFullGame(FullGameState.Interrupted);fullGameStore.Dispose();}
}
public static class FullGameCoordinatorTests
{
    private static void Check(bool value,string why){if(!value)throw new Exception(why);}
    private static TrainerCommands Plan()
    {
        return new TrainerCommands {SpeedrunModeActive=1,FullGameMode=2,FullGameCmd=1,FullGameRawMask=0b10101010,
            FullGameSeed0=-11,FullGameSeed1=22,FullGameSeed2=-33,FullGameSeed3=44,FullGameSeed4=55,FullGameSeed5=66,FullGameSeed6=77,FullGameSeed7=88};
    }
    public static int Main(string[] args)
    {
        string root=args[0];Directory.CreateDirectory(root);
        using(var manager=new SpeedrunManager(Path.Combine(root,"campaign")))
        {
            var raw=Plan();var current=manager.ResolveFullGameCommands(raw,true);
            Check(manager.FullGameActive && current.SpeedrunLoadLevelCmd==1 && current.SpeedrunLevelIndex==0 && current.PracticeSeed==-11,"full-game start is Classic with its first seed");
            long id=manager.TestRecord().Id;
            manager.ResolveFullGameCommands(raw,false);Check(manager.TestRecord().Id==id,"heartbeat cannot replay start command");
            for(int i=0;i<8;i++)
            {
                current=manager.ResolveFullGameCommands(raw,false);
                Check(current.SpeedrunLevelIndex==i && current.PracticeSeed==raw.FullGameSeed(i) && current.SetSeed && !current.Practice && current.Restricted,"each stage receives its own frozen SSFG seed");
                Check(((current.PlannerOptions&2)!=0)==((raw.FullGameRawMask&(1u<<i))!=0),"each seed recipe applied before scene RNG");
                Check(manager.TakeFullGameTransition(),"next stage requested without a trainer roundtrip");
                manager.TestLoad();Check(!manager.TakeFullGameTransition(),"load acknowledged once");
                int before=UnityEngine.SceneManagement.SceneManager.Loads;
                SpeedrunManager.LoadCompletionScene("TitleScreen");
                Check(UnityEngine.SceneManagement.SceneManager.Loads==before+1,"normal/unexpected scene requests are not globally suppressed");
                if(i==2){manager.TestDeath();manager.TestLoad();Check(manager.TestRecord().Retries[i]==1,"death retries current level");}
                manager.TestFinish();
                before=UnityEngine.SceneManagement.SceneManager.Loads;
                SpeedrunManager.LoadCompletionScene("ClassicEnding");
                Check(UnityEngine.SceneManagement.SceneManager.Loads==before+(i==7?1:0),"pending campaign transitions suppress vanilla pickup cutscene loads only until the final level");
            }
            var complete=manager.TestRecord();Check(complete.Ranked && complete.Stage==8 && !manager.FullGameActive,"clean campaign completes only after last bonus");
            for(int i=0;i<8;i++)Check(complete.Replays[i]>0,"each completed stage has a replay link");
            var packet=manager.TestSnapshot();Check(packet.Bytes[SharedLayout.FullGameState]==3 && packet.Bytes[SharedLayout.FullGameMode]==2,"authoritative final state reaches IPC");
        }
        using(var manager=new SpeedrunManager(Path.Combine(root,"reset")))
        {
            var raw=Plan();manager.ResolveFullGameCommands(raw,true);
            for(int i=0;i<6;i++){manager.ResolveFullGameCommands(raw,false);manager.TestLoad();manager.TestFinish();}
            manager.ResolveFullGameCommands(raw,false);manager.TestLoad();Check(manager.FullGameScene==12,"second bonus reached");
            long previous=manager.TestRecord().Id;raw.FullGameCmd=2;manager.ResolveFullGameCommands(raw,true);
            Check(manager.FullGameScene==3 && manager.TestRecord().Id!=previous && manager.TestRecord().Seconds==0 && manager.TestRecord().Stage==0,"full reset from Camera Bright returns to a fresh Classic run");
            manager.TestLoad();manager.TestDeath(true);manager.TestLoad();manager.TestFinish();
            Check(!manager.TestRecord().Eligible,"clean retry cannot erase a previous disallowed feature");
            raw.FullGameSeed1++;manager.ResolveFullGameCommands(raw,false);
            Check((manager.TestRecord().Warnings&16384u)!=0,"late seed plan edits detected");
            manager.TestScene(13);Check(!manager.FullGameActive,"unexpected/skipped scene stops campaign");
        }
        using(var manager=new SpeedrunManager(Path.Combine(root,"random")))
        {
            var raw=Plan();raw.FullGameMode=1;var current=manager.ResolveFullGameCommands(raw,true);
            Check(!current.SetSeed && !current.UsesControlledSeed && current.Restricted && current.PracticeSeed==0,"RSFG never inherits a fixed seed from an old SSFG draft");
            raw.SpeedrunModeActive=0;manager.ResolveFullGameCommands(raw,true);Check(!manager.FullGameActive,"disabling speedrun stops aggregate run");
        }
        Console.WriteLine("Actual full-game coordinator: eight per-level seeds/recipes, command ownership, auto advance, retry/death, Camera Bright full reset, sticky checks, skips and Random Seed isolation: PASS");
        return 0;
    }
}
