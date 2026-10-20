using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using UnityEngine;

// No Unity API is used by this generator. A descriptor is exported only after
// the actual game's InitState, integer Range, state transitions and stock
// selections have all matched. An unknown runtime keeps the Unity fallback.
public struct PortableRandom
{
    public uint X,Y,Z,W;
    public int InitMode,RangeMode,AdvanceOne;
    public void Init(int seed)
    {
        unchecked {
            X=(uint)seed;Y=Expand(X,1);Z=Expand(Y,2);W=Expand(Z,3);
        }
    }
    private uint Expand(uint value,uint index)
    {
        unchecked {return 1812433253u*((InitMode==1||InitMode==3)?value^(value>>30):value)+((InitMode==1||InitMode==2)?index:1u);}
    }
    public uint Next()
    {
        unchecked {uint t=X^(X<<11);X=Y;Y=Z;Z=W;W=W^(W>>19)^t^(t>>8);return W;}
    }
    public int Range(int min,int max)
    {
        uint count=(uint)(max-min);if(count==1&&AdvanceOne==0)return min;
        uint value=Next(),result;
        switch(RangeMode) {
            case 0:result=value%count;break;
            case 1:result=(value&0x7fffffffu)%count;break;
            case 2:result=(uint)(((ulong)value*count)>>32);break;
            case 3:result=(uint)(((ulong)(value>>1)*count)>>31);break;
            case 4:result=((value&0x80000000u)!=0?unchecked(0u-value):value)%count;break;
            default:throw new InvalidOperationException("Unsupported RNG descriptor");
        }
        return min+(int)result;
    }
    public bool SameState(PortableRandom other){return X==other.X&&Y==other.Y&&Z==other.Z&&W==other.W;}
}

public sealed class PortableRngVerifier
{
    public const int RangeProbes=4096,Total=4160;
    private readonly List<PortableRandom> candidates=new List<PortableRandom>();
    private readonly Action<int> init;
    private readonly Func<int,int,int> range;
    private readonly Func<PortableRandom> state;
    private readonly Action<PortableRandom> writeState;
    private readonly int spawns,points,keep,scene,site;
    public int Completed {get;private set;}
    public int Status {get;private set;} // 1 checking, 2 verified, 3 fallback
    public string Note {get;private set;}
    public PortableRandom Descriptor {get;private set;}
    public PortableRngVerifier(int spawnCount,int pointCount,int crystalCount,int sceneIndex,int spawnSite,
        Action<int> initialize,Func<int,int,int> draw,Func<PortableRandom> readState,Action<PortableRandom> restoreState=null)
    {
        spawns=spawnCount;points=pointCount;keep=crystalCount;scene=sceneIndex;site=spawnSite;
        init=initialize;range=draw;state=readState;writeState=restoreState;Status=1;Note="Checking the CPU generator against this Unity runtime.";
        for(int i=0;i<4;i++)for(int r=0;r<5;r++)for(int a=0;a<2;a++)candidates.Add(new PortableRandom {InitMode=i,RangeMode=r,AdvanceOne=a});
    }
    private static int Seed(int probe)
    {
        if(probe==0)return 0;if(probe==1)return -1;if(probe==2)return int.MinValue;if(probe==3)return int.MaxValue;
        return PracticeRandom.DeriveSeed(probe,17,31,unchecked((uint)probe));
    }
    public void Probe()
    {
        if(Status!=1)return;
        try {
            int seed=Seed(Completed);
            if(Completed<RangeProbes) {
                init(seed);var initial=state();int first=range(1,30);int upper=Completed%1024+1;
                // Multiply-high and a dropped-low-bit variant differ only at
                // rare integer boundaries. Force one discriminating state;
                // sampling alone could incorrectly treat them as equivalent.
                bool boundary=Completed==4&&writeState!=null;var forced=new PortableRandom {W=0xaaaaaaabu^(0xaaaaaaabu>>19)};
                if(boundary){writeState(forced);upper=3;}
                int actual=range(0,upper);var after=state();
                for(int i=candidates.Count-1;i>=0;i--) {
                    var test=candidates[i];test.Init(seed);
                    bool valid=test.SameState(initial)&&test.Range(1,30)==first;
                    if(boundary){test.X=forced.X;test.Y=forced.Y;test.Z=forced.Z;test.W=forced.W;}
                    if(!valid||test.Range(0,upper)!=actual||!test.SameState(after))candidates.RemoveAt(i);
                }
                if(candidates.Count==0){Status=3;Note="Unity RNG differs from the CPU model; using the original game-thread generator.";return;}
            } else {
                if(candidates.Count!=1){Status=3;Note="CPU generator is ambiguous; using the original Unity generator.";return;}
                var test=candidates[0];int applied=Completed%2==0?seed:PracticeRandom.DeriveSeed(seed,scene,site,0);
                int forced=Completed%7==0?Math.Min(4,spawns-1):-1;
                init(applied);var expected=SpawnSelection.Select(spawns,points,keep,forced,range);var after=state();
                test.Init(applied);var simulated=new SpawnSelectionGenerator(spawns,points,keep).Next(forced,test.Range);
                // A delegate on a value type boxes it. Verify final state with
                // a separate sequential replay rather than the boxed copy.
                test.Init(applied);test.Range(1,30);test.Range(0,spawns);for(int count=points;count>Math.Min(keep,points);count--)test.Range(0,count);
                if(!SpawnSelection.Matches(simulated,expected.Spawn,expected.Crystals)||!test.SameState(after)) {
                    Status=3;Note="Stock selection differs from the CPU model; using the original Unity generator.";return;
                }
            }
            Completed++;
            if(Completed==Total){Descriptor=candidates[0];Status=2;Note="CPU generator matched 4096 RNG probes and 64 complete stock layouts.";}
        } catch(Exception error){Status=3;Note="CPU verification unavailable: "+error.Message;}
    }
}

public sealed class UnityRngVerification
{
    private readonly FieldInfo[] fields=new FieldInfo[4];
    private readonly PortableRngVerifier verifier;
    public UnityRngVerification(PracticeMapData data,int site)
    {
        for(int i=0;i<4;i++) {
            fields[i]=typeof(UnityEngine.Random.State).GetField("s"+i,BindingFlags.Instance|BindingFlags.NonPublic|BindingFlags.Public);
            if(fields[i]==null||fields[i].FieldType!=typeof(int))throw new NotSupportedException("Unity RNG state layout is unknown");
        }
        verifier=new PortableRngVerifier(data.Spawns.Length,data.Points.Length,data.Keep,data.Scene,site,UnityEngine.Random.InitState,UnityEngine.Random.Range,Read,Write);
    }
    private PortableRandom Read()
    {
        object value=UnityEngine.Random.state;
        return new PortableRandom {X=unchecked((uint)(int)fields[0].GetValue(value)),Y=unchecked((uint)(int)fields[1].GetValue(value)),
            Z=unchecked((uint)(int)fields[2].GetValue(value)),W=unchecked((uint)(int)fields[3].GetValue(value))};
    }
    private void Write(PortableRandom state)
    {
        object value=new UnityEngine.Random.State();uint[] words={state.X,state.Y,state.Z,state.W};
        for(int i=0;i<4;i++)fields[i].SetValue(value,unchecked((int)words[i]));UnityEngine.Random.state=(UnityEngine.Random.State)value;
    }
    public bool Tick(PracticeMapData data)
    {
        long start=Stopwatch.GetTimestamp(),budget=Math.Max(1,Stopwatch.Frequency/2000);var saved=UnityEngine.Random.state;
        try {do {verifier.Probe();}while(verifier.Status==1&&Stopwatch.GetTimestamp()-start<budget);}
        finally {UnityEngine.Random.state=saved;}
        data.RngStatus=verifier.Status;data.RngProbes=verifier.Completed;data.RngNote=verifier.Note;
        if(verifier.Status==2){var rng=verifier.Descriptor;data.RngInit=rng.InitMode;data.RngRange=rng.RangeMode;data.RngAdvance=rng.AdvanceOne;}
        return verifier.Status!=1;
    }
}
