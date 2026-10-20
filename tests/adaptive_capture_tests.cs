using System;
using System.IO;
using UnityEngine;
class AdaptiveCaptureTests {
    static void Check(bool v,string message){if(!v)throw new Exception(message);}
    static ReplayVisualFrame Pose(double t,double angle,double x=0){double a=angle*Math.PI/360;return new ReplayVisualFrame{Seconds=t,Position=new Vector3((float)x,0,0),Scale=new Vector3(1,1,1),Rotation=new Quaternion(0,(float)Math.Sin(a),0,(float)Math.Cos(a))};}
    static double Angle(Quaternion a,Quaternion b){double dot=Math.Abs(a.x*(double)b.x+a.y*(double)b.y+a.z*(double)b.z+a.w*(double)b.w);return 2*Math.Acos(Math.Min(1,dot))*180/Math.PI;}
    static ReplayCoverage Health(){var c=new ReplayCoverage();c.Start(ReplayCoverage.Visuals);return c;}
    static void Signal(double frequency){
        var data=new ReplayVisualData();var owner=data.Owner(2,1,1,"");var track=data.Track(owner,1,"bone");var sampler=new AdaptivePoseSampler(data,track,Health());
        const int frames=1200;for(int n=0;n<=frames;n++){double t=n/120.0;sampler.Push(Pose(t,35*Math.Sin(t*2*Math.PI*frequency),.05*Math.Sin(t*2*Math.PI*frequency)),n==frames);}
        Check(track.Frames[track.Frames.Count-1].Seconds==10,"terminal pose is actually flushed");
        double worst=0;int checkedCount=0;
        for(int n=0;n<=frames;n++){double t=n/120.0;var expected=Pose(t,35*Math.Sin(t*2*Math.PI*frequency),.05*Math.Sin(t*2*Math.PI*frequency));int i=track.At(t);Check(i>=0,"pose starts on time");var first=track.Frames[i];Quaternion q=first.Rotation;double x=first.Position.x;
            if(i+1<track.Frames.Count){var next=track.Frames[i+1];double u=(t-first.Seconds)/(next.Seconds-first.Seconds);q=AdaptivePoseSampler.Rotation(first.Rotation,next.Rotation,u);x=first.Position.x+(next.Position.x-first.Position.x)*u;}
            double error=Angle(expected.Rotation,q);worst=Math.Max(worst,error);Check(error<AdaptivePoseSampler.RotationErrorDegrees+.04,"sampled angular reconstruction bound");Check(Math.Abs(x-expected.Position.x)<AdaptivePoseSampler.PositionError+.00001,"sampled positional reconstruction bound");checkedCount++;
        }
        Console.WriteLine("Adaptive signal "+frequency+": "+checkedCount+" checked poses / "+track.Frames.Count+" stored / maximum angular error "+worst.ToString("F5"));
    }
    static byte[] CoverageBytes(ReplayCoverage c){using(var s=new MemoryStream()){using(var w=new BinaryWriter(s,System.Text.Encoding.UTF8,true))c.Write(w);return s.ToArray();}}
    public static int Main(){
        Signal(.5);Signal(4);Signal(12);
        var data=new ReplayVisualData();var track=data.Track(data.Owner(1,0,0,""),1,"");var health=Health();var s=new AdaptivePoseSampler(data,track,health);
        for(int n=0;n<=12000;n++)s.Push(Pose(n/120.0,0));s.Push(Pose(100,0),true);Check(track.Frames.Count==2,"stationary recording stores endpoints, not 12000 repeated poses");
        s.Push(Pose(100.01,0,10));s.Push(Pose(100.02,0,10));s.Push(Pose(100.02,0,10),true);Check(track.Frames[track.Frames.Count-1].Seconds==100.02,"same-time finish flushes pending knots");
        s.Push(Pose(100.03,90,10),true);Check(track.Frames[track.Frames.Count-1].Rotation.y>.7f,"animation boundary keeps exact new pose");
        var rate=new AdaptivePoseSampler(new ReplayVisualData(),new ReplayVisualTrack(),Health());rate.Push(Pose(0,0));rate.Push(Pose(.02,20));Check(rate.Interval(.02)==AdaptivePoseSampler.FastInterval&&rate.Interval(1)==AdaptivePoseSampler.NormalInterval,"detail escalates for actual angular motion and decays");
        var coverage=new ReplayCoverage();coverage.Start(0);coverage.Source(0);for(int i=0;i<=100;i++)coverage.Observe(0,i/100.0);coverage.Seal(1);Check(coverage.Spans[0].GapCount==0,"continuous heartbeat remains complete");
        var partial=new ReplayCoverage();partial.Start(0);partial.Observe(0,0);partial.Observe(0,.5);partial.Seal(1);Check(partial.Spans[0].GapCount==2&&partial.Spans[0].Gaps[1].End==1,"mid-run and terminal gaps are retained");
        var bytes=CoverageBytes(partial);using(var r=new BinaryReader(new MemoryStream(bytes))){var round=ReplayCoverage.Read(r,1);Check(round.Recorded&&round.Spans[0].GapCount==2&&round.Spans[0].Through==.5,"coverage roundtrip keeps actual last observation");}
        bool rejected=false;bytes[0]=2;try{using(var r=new BinaryReader(new MemoryStream(bytes)))ReplayCoverage.Read(r,1);}catch(InvalidDataException){rejected=true;}Check(rejected,"strict coverage flag validation");
        for(int i=0;i<100;i++){partial.Fail(0);partial.Observe(0,2);}Check(partial.Spans[0].Through==.5,"sealed coverage cannot change on late callbacks");
        Console.WriteLine("Adaptive production capture: bounded positional/angular interpolation error, high-frequency motion, constant-pose compression, exact boundaries/endpoints, adaptive detail, strict heartbeat coverage and late-callback isolation: PASS");return 0;
    }
}
