extern alias suite;
using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using Rng = suite::PortableRandom;
using Seeds = suite::PracticeRandom;
using Selection = suite::SpawnSelection;

// Only the arithmetic needed by the production CatalogOrder matcher. No scene
// or engine behavior is simulated; live Unity loading is a separate test.
namespace UnityEngine
{
    public struct Vector3
    {
        public float x,y,z;
        public Vector3(float x,float y,float z){this.x=x;this.y=y;this.z=z;}
        public float sqrMagnitude {get{return x*x+y*y+z*z;}}
        public static Vector3 operator -(Vector3 a,Vector3 b){return new Vector3(a.x-b.x,a.y-b.y,a.z-b.z);}
    }
}

public static class SeedReproducibilityTests
{
    static void Check(bool value,string why){if(!value)throw new Exception(why);}
    static Selection Generate(int seed,int recipe,int scene,int site,int forced,out Rng final)
    {
        var random=new Rng {InitMode=0,RangeMode=0,AdvanceOne=1};
        random.Init(recipe==1?seed:Seeds.DeriveSeed(seed,scene,site,0));
        var selection=Selection.Select(9,34,18,forced,(min,max)=>random.Range(min,max));
        final=random;return selection;
    }
    static bool Same(Selection a,Selection b)
    {return a.Spawn==b.Spawn && a.Crystals.SequenceEqual(b.Crystals);}
    static UnityEngine.Vector3[] Catalog(int count)
    {return Enumerable.Range(0,count).Select(i=>new UnityEngine.Vector3(i*3+1,i%3,i*7+2)).ToArray();}
    static UnityEngine.Vector3[] Reorder(UnityEngine.Vector3[] source,int[] indices)
    {return indices.Select(i=>source[i]).ToArray();}
    static bool SamePositions(UnityEngine.Vector3[] a,UnityEngine.Vector3[] b,Selection selection)
    {return selection.Crystals.All(i=>(a[i]-b[i]).sqrMagnitude==0);}

    public static int Main(string[] args)
    {
        string fixture=args[0],output=args[1];Directory.CreateDirectory(output);
        int rows=0,site=0;
        using(var reader=new BinaryReader(File.OpenRead(fixture)))
        {
            Check(reader.ReadInt32()==0x52474e50,"frozen cross-language RNG fixture magic");
            rows=reader.ReadInt32();Check(rows==320,"expected historical compatibility vectors");
            for(int row=0;row<rows;row++)
            {
                var rng=new Rng {InitMode=reader.ReadInt32(),RangeMode=reader.ReadInt32(),AdvanceOne=reader.ReadInt32()};
                int seed=reader.ReadInt32();site=reader.ReadInt32();int applied=reader.ReadInt32();
                Check(Seeds.DeriveSeed(seed,12,site,0)==applied,"calculated seed matches historical vector");
                rng.Init(applied);
                var layout=Selection.Select(9,34,18,-1,(min,max)=>rng.Range(min,max));
                Check(layout.Spawn==reader.ReadInt32(),"historical spawn selection");
                foreach(int crystal in layout.Crystals)Check(crystal==reader.ReadInt32(),"historical crystal selection");
            }
            Check(reader.BaseStream.Position==reader.BaseStream.Length,"entire golden fixture consumed");
        }
        int[] scenes={3,4,5,7,8,14,12,13};
        int[] boundaries={0,1,-1,int.MinValue,int.MaxValue,123,-7123,47839471};
        int repeats=0,reorderedDifferences=0,restored=0,timerDifferences=0;
        var spawns=Catalog(9);var points=Catalog(34);
        var otherSpawns=spawns.Reverse().ToArray();var otherPoints=points.Reverse().ToArray();
        var alignedSpawns=Reorder(otherSpawns,CatalogOrder.Match(spawns,otherSpawns));
        var alignedPoints=Reorder(otherPoints,CatalogOrder.Match(points,otherPoints));
        foreach(int scene in scenes)for(int recipe=1;recipe<=2;recipe++)for(int sample=0;sample<208;sample++)
        {
            int seed=sample<boundaries.Length?boundaries[sample]:unchecked(sample*196314165+907633515);
            Rng firstState,secondState;
            var first=Generate(seed,recipe,scene,site,-1,out firstState);
            Generate(~seed,3-recipe,scene,site,4,out secondState);
            var second=Generate(seed,recipe,scene,site,-1,out secondState);
            Check(Same(first,second) && firstState.SameState(secondState),"independent sessions reproduce the entire selection");
            repeats++;
            bool different=(spawns[first.Spawn]-otherSpawns[first.Spawn]).sqrMagnitude!=0 || !SamePositions(points,otherPoints,first);
            if(different)reorderedDifferences++;
            Check((spawns[first.Spawn]-alignedSpawns[first.Spawn]).sqrMagnitude==0 && SamePositions(points,alignedPoints,first),
                "captured-catalog alignment restores physical positions after object enumeration changes");
            restored++;
            if(scene==7)
            {
                var initial=Generate(seed,recipe,scene,site,4,out secondState);
                Check(initial.Spawn==4 && initial.Crystals.SequenceEqual(first.Crystals) && firstState.SameState(secondState),
                    "Timer's first-start override changes spawn but still consumes the stock random draw");
                if(initial.Spawn!=first.Spawn)timerDifferences++;
            }
        }
        Check(reorderedDifferences>0 && timerDifferences>0,"actual setup-sensitive counterexamples observed");
        var changed=(UnityEngine.Vector3[])otherPoints.Clone();changed[0]=new UnityEngine.Vector3(999,999,999);
        Check(CatalogOrder.Match(points,changed)==null,"changed map cannot be silently treated as the same catalog");
        var duplicate=(UnityEngine.Vector3[])otherPoints.Clone();duplicate[0]=duplicate[1];
        Check(CatalogOrder.Match(points,duplicate)==null,"missing/duplicate point cannot claim equivalent layout");
        string report="{\n  \"assemblyVersion\": \""+typeof(Seeds).Assembly.GetName().Version+"\",\n"+
            "  \"historicalVectors\": "+rows+",\n  \"repeatableSelections\": "+repeats+",\n"+
            "  \"unalignedCatalogDifferences\": "+reorderedDifferences+",\n  \"alignedCatalogMatches\": "+restored+",\n"+
            "  \"timerFirstStartDifferences\": "+timerDifferences+",\n"+
            "  \"scope\": \"Production RNG/selection, frozen vectors and catalog logic; not a live multi-PC Unity test\"\n}\n";
        File.WriteAllText(Path.Combine(output,"seed-reproducibility.json"),report);
        Console.WriteLine(report);
        Console.WriteLine("Seed repeatability and historical contract: PASS. Catalog-order and Timer-first-start limitations reproduced; live Unity compatibility is not asserted.");
        return 0;
    }
}
