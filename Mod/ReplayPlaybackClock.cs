using System;

// Playback follows a monotonic clock, not accumulated/clipped render deltas.
// Pausing, rate changes and seeks establish a new anchor; loading never counts.
public sealed class ReplayPlaybackClock
{
    private double anchor,position,duration,rate=1,loopA,loopB;
    private bool running,loop;
    public void Loop(bool enabled,double a,double b)
    {
        loop=enabled&&Finite(a)&&Finite(b)&&a>=0&&b<=duration&&b-a>=.02;
        loopA=loop?a:0;loopB=loop?b:0;
    }
    public void Set(double now,double at,double length,bool play,double speed)
    {
        anchor=Finite(now)?now:0;duration=Finite(length)?Math.Max(0,length):0;
        position=Finite(at)?Math.Max(0,Math.Min(duration,at)):0;
        rate=Finite(speed)?Math.Max(.1,Math.Min(8,speed)):1;
        if(loop&&(loopB>duration||loopB-loopA<.02))loop=false;
        running=play;
    }
    private static bool Finite(double v){return !double.IsNaN(v)&&!double.IsInfinity(v);}
    public double At(double now){double at=position+(running&&Finite(now)?Math.Max(0,now-anchor)*rate:0);if(loop&&running&&at>=loopB)at=loopA+(at-loopB)%(loopB-loopA);return Math.Min(duration,at);}
    public bool Playing(double now){return running&&(loop||At(now)<duration);}
}
