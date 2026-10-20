using System;
public sealed class FrameCostMeter
{
    private readonly double[] samples=new double[128],scratch=new double[128];private int at,count;private double total,next,mean,p95;
    public void Observe(double ms){if(double.IsNaN(ms)||double.IsInfinity(ms)||ms<0)return;if(count==samples.Length)total-=samples[at];else count++;samples[at]=ms;total+=ms;at=(at+1)%samples.Length;}
    public void Snapshot(double now,out float average,out float percentile){if(now>=next){next=now+1;mean=count>0?total/count:0;Array.Copy(samples,scratch,count);Array.Sort(scratch,0,count);p95=count>0?scratch[(count-1)*95/100]:0;}average=(float)mean;percentile=(float)p95;}
}
