using System;
using System.Collections.Generic;
using System.IO;

public struct ReplayCoverageGap {public double Begin,End;}
public sealed class ReplayCoverageSpan {
    public bool Started;public int Observations,Errors,Sources,GapCount;
    public double First,Through,MaxGap;
    public readonly List<ReplayCoverageGap> Gaps=new List<ReplayCoverageGap>(16);
    internal void Gap(double first,double last){if(last<=first)return;if(GapCount<int.MaxValue)GapCount++;if(Gaps.Count<16)Gaps.Add(new ReplayCoverageGap{Begin=first,End=last});}
}
// Heartbeats describe successful observation, not sparse on-disk changes. A
// stationary pose/input may have two stored values and still be fully observed.
public sealed class ReplayCoverage {
    public const int Movement=0,Camera=1,Inputs=2,Colliders=3,Visuals=4,Animation=5,Count=6;
    public static readonly double[] Limits={.12,.25,.25,.12,.12,.25};
    public bool Recorded;private bool sealedRecording;
    public readonly ReplayCoverageSpan[] Spans=new ReplayCoverageSpan[Count];
    public ReplayCoverage(){for(int i=0;i<Count;i++)Spans[i]=new ReplayCoverageSpan();}
    public void Start(int channel){if(!sealedRecording){Recorded=true;Spans[channel].Started=true;}}
    public void Source(int channel){if(!sealedRecording&&Spans[channel].Started&&Spans[channel].Sources<8192)Spans[channel].Sources++;}
    public void Fail(int channel){if(!sealedRecording&&Spans[channel].Started&&Spans[channel].Errors<int.MaxValue)Spans[channel].Errors++;}
    public void Observe(int channel,double at){
        if(sealedRecording)return;var span=Spans[channel];if(!span.Started)return;
        if(double.IsNaN(at)||double.IsInfinity(at)||at<0||span.Observations>0&&at<span.Through){Fail(channel);return;}
        double begin=span.Observations==0?0:span.Through,gap=at-begin;span.MaxGap=Math.Max(span.MaxGap,gap);
        if(gap>Limits[channel]+.000001)span.Gap(begin,at);
        if(span.Observations==0)span.First=at;span.Through=at;if(span.Observations<int.MaxValue)span.Observations++;
    }
    public void Seal(double duration){
        if(sealedRecording)return;sealedRecording=true;
        for(int channel=0;channel<Count;channel++){var span=Spans[channel];if(span.Started&&span.Observations>0&&duration-span.Through>Limits[channel]+.000001){span.MaxGap=Math.Max(span.MaxGap,duration-span.Through);span.Gap(span.Through,duration);}}
    }
    public void Write(BinaryWriter w){
        w.Write(Recorded);w.Write(Count);
        foreach(var s in Spans){w.Write(s.Started);w.Write(s.Observations);w.Write(s.Errors);w.Write(s.Sources);w.Write(s.First);w.Write(s.Through);w.Write(s.MaxGap);w.Write(s.GapCount);w.Write(s.Gaps.Count);foreach(var g in s.Gaps){w.Write(g.Begin);w.Write(g.End);}}
    }
    private static bool Flag(BinaryReader r){byte b=r.ReadByte();if(b>1)throw new InvalidDataException("Invalid capture coverage flag");return b!=0;}
    private static int N(BinaryReader r,int max=int.MaxValue){int n=r.ReadInt32();if(n<0||n>max)throw new InvalidDataException("Invalid capture coverage count");return n;}
    private static double Time(BinaryReader r,double duration){double n=r.ReadDouble();if(double.IsNaN(n)||double.IsInfinity(n)||n<0||n>duration+.000001)throw new InvalidDataException("Invalid capture coverage time");return n;}
    public static ReplayCoverage Read(BinaryReader r,double duration){
        var coverage=new ReplayCoverage{Recorded=Flag(r),sealedRecording=true};if(r.ReadInt32()!=Count)throw new InvalidDataException("Unsupported capture coverage channels");
        foreach(var s in coverage.Spans){s.Started=Flag(r);s.Observations=N(r);s.Errors=N(r);s.Sources=N(r,8192);s.First=Time(r,duration);s.Through=Time(r,duration);s.MaxGap=Time(r,duration);s.GapCount=N(r);int n=N(r,16);
            if(s.First>s.Through||n>s.GapCount||!coverage.Recorded&&(s.Started||s.Observations!=0||s.Errors!=0||s.Sources!=0||s.GapCount!=0)||!s.Started&&s.Observations!=0||s.Observations==0&&(s.First!=0||s.Through!=0||s.MaxGap!=0||s.GapCount!=0))throw new InvalidDataException("Inconsistent capture coverage");
            double previous=0;for(int i=0;i<n;i++){double first=Time(r,duration),last=Time(r,duration);if(first<previous||last<=first||last-first>s.MaxGap+.000001)throw new InvalidDataException("Invalid capture coverage interval");previous=last;s.Gaps.Add(new ReplayCoverageGap{Begin=first,End=last});}
        }return coverage;
    }
}
