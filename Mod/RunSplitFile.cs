using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using UnityEngine;

// Numeric split metadata only. Geometry, trace, monsters, movement and mesh
// arrays are never materialized on Unity's managed heap during an archive scan.
public static class RunSplitFile
{
    private sealed class Reader : IDisposable
    {
        public readonly BinaryReader B;
        public readonly long End;
        public Reader(string path)
        {
            var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.Read, 4096, FileOptions.RandomAccess);
            B = new BinaryReader(stream, Encoding.UTF8);
            End = stream.Length - 8;
            if (End < 100 || stream.Length > PracticeMapChannel.Capacity - 64)
            { B.Dispose(); throw new InvalidDataException("Recorded file exceeds split limit"); }
        }
        public int Count(int max)
        { int n=B.ReadInt32(); if(n<0 || n>max) throw new InvalidDataException("Recorded split count exceeds limit"); return n; }
        public bool Flag()
        { byte value=B.ReadByte(); if(value>1) throw new InvalidDataException("Invalid recorded flag"); return value!=0; }
        public void Skip(long bytes)
        { if(bytes<0 || B.BaseStream.Position>End-bytes) throw new EndOfStreamException(); B.BaseStream.Seek(bytes,SeekOrigin.Current); }
        public string Text(int max, bool keep=false)
        {
            int n=Count(max);
            if(!keep){Skip(n);return null;}
            byte[] bytes=B.ReadBytes(n);if(bytes.Length!=n)throw new EndOfStreamException();
            var result=new UTF8Encoding(false,true).GetString(bytes);
            if(result.IndexOf('\0')>=0)throw new InvalidDataException("Invalid recorded text");return result;
        }
        public Vector3 Vector()
        {
            float x=B.ReadSingle(),y=B.ReadSingle(),z=B.ReadSingle();
            if(!Coordinate(x)||!Coordinate(y)||!Coordinate(z))throw new InvalidDataException("Invalid split position");
            return new Vector3(x,y,z);
        }
        private static bool Coordinate(float n){return !float.IsNaN(n)&&!float.IsInfinity(n)&&Math.Abs(n)<=100000;}
        public void Array(int limit,int stride){Skip((long)Count(limit)*stride);}
        public void Outlines()
        {
            int n=Count(20000);
            for(int i=0;i<n;i++){Flag();if(Count(4)!=4)throw new InvalidDataException("Invalid split contour");Skip(48);}
        }
        public void Verify(byte[] buffer)
        {
            B.BaseStream.Position=End;ulong expected=B.ReadUInt64();B.BaseStream.Position=0;
            ulong h=14695981039346656037UL;long remaining=End;
            unchecked
            {
                while(remaining>0)
                {
                    int n=B.BaseStream.Read(buffer,0,(int)Math.Min(buffer.Length,remaining));
                    if(n==0)throw new EndOfStreamException();
                    for(int i=0;i<n;i++){h^=buffer[i];h*=1099511628211UL;}remaining-=n;
                }
            }
            if(h!=expected)throw new InvalidDataException("Recorded checksum mismatch");
        }
        public void Dispose(){B.Dispose();}
    }
    private static bool Time(double n){return !double.IsNaN(n)&&!double.IsInfinity(n)&&n>=0&&n<=315360000;}
    public static RunSplitReference Read(string path, byte[] checksumBuffer)
    {
        if(checksumBuffer==null||checksumBuffer.Length<4096)throw new ArgumentException("Split scan needs a reusable checksum buffer");
        using(var r=new Reader(path))
        {
            var b=r.B;
            if(b.ReadUInt32()!=0x504D5349u)throw new InvalidDataException("Unsupported split archive");
            int format=b.ReadInt32();
            if(format<15)return null; // Earlier set seeds were Practice, never this category.
            if(format!=15)throw new InvalidDataException("Unsupported split archive version");
            int pid=b.ReadInt32(),scene=b.ReadInt32(),epoch=b.ReadInt32(),seed=b.ReadInt32(),recipe=b.ReadInt32(),source=b.ReadInt32();
            int spawn=b.ReadInt32(),keep=b.ReadInt32(),flags=b.ReadInt32(),stage=b.ReadInt32(),requested=b.ReadInt32(),completed=b.ReadInt32();
            int liveSpawn=b.ReadInt32();b.ReadInt32();int liveRecipe=b.ReadInt32();b.ReadInt64();b.ReadUInt32();
            string hash=r.Text(65,true);r.Text(1024);
            int n=r.Count(128);var spawns=new Vector3[n];for(int i=0;i<n;i++)spawns[i]=r.Vector();
            n=r.Count(1024);var points=new Vector3[n];for(int i=0;i<n;i++)points[i]=r.Vector();
            n=r.Count(64);var selected=new int[n];var selection=new HashSet<int>();
            for(int i=0;i<n;i++)
            { int id=b.ReadInt32();if(id<0||id>=points.Length||!selection.Add(id))throw new InvalidDataException("Invalid split selection");selected[i]=id; }
            r.Array(64,4);r.Array(30000,12);int indices=r.Count(180000);r.Skip(indices*4L);r.Array(20000,16);
            int samples=r.Count(2000);if(samples!=0)throw new InvalidDataException("Run archive contains planner samples");
            int navStatus=b.ReadInt32(),queries=b.ReadInt32(),total=b.ReadInt32();r.Text(1024);
            r.Array(10000,12);r.Array(60000,4);r.Array(1152,4);
            r.Skip(28);r.Text(1024);int navModel=b.ReadInt32();r.Outlines();
            float radius=b.ReadSingle(),height=b.ReadSingle(),step=b.ReadSingle(),slope=b.ReadSingle();b.ReadSingle();r.Outlines();
            if(!r.Flag())throw new InvalidDataException("Archive has no split recording");
            long idRun=b.ReadInt64();int outcome=b.ReadInt32();double seconds=b.ReadDouble();r.Flag();r.Array(32768,20);
            n=r.Count(64);var collected=new int[n];var seen=new HashSet<int>();
            for(int i=0;i<n;i++)
            {int id=b.ReadInt32();if(!selection.Contains(id)||!seen.Add(id))throw new InvalidDataException("Invalid split pickup");collected[i]=id;}
            n=r.Count(64);var times=new double[n];double previous=0;
            for(int i=0;i<n;i++)
            {double at=b.ReadDouble();if(!Time(at)||at<previous||at>seconds+.1)throw new InvalidDataException("Invalid split time");times[i]=at;previous=at;}
            bool checkedRun=r.Flag(),valid=r.Flag(),observed=r.Flag(),known=r.Flag();
            int kind=b.ReadInt32();uint aids=b.ReadUInt32(),warnings=b.ReadUInt32();r.Text(1024);
            int category=b.ReadByte();int selectedSeed=b.ReadInt32(),runSeed=b.ReadInt32();bool raw=r.Flag();uint initial=b.ReadUInt32();
            if(category!=1)return null;
            bool eligible=checkedRun&&observed&&known&&kind==2&&aids==2&&initial==2&&runSeed==selectedSeed&&(warnings&~64u)==0;
            int[] scenes={3,4,5,7,8,14,12,13};
            if(valid!=eligible || idRun<=0 || outcome<1 || outcome>5 || !Time(seconds) || times.Length!=collected.Length ||
                Array.IndexOf(scenes,scene)<0 || pid<0 || epoch<0 || recipe<0 || recipe>2 || source<0 || source>2 ||
                (flags&~255)!=0 || stage!=1 || requested!=0 || completed!=0 || keep<0 || keep>64 ||
                spawn<0 || spawn>=spawns.Length || liveSpawn<0 || liveSpawn>=spawns.Length || liveRecipe<0 || liveRecipe>2 ||
                selected.Length!=Math.Min(keep,points.Length) || indices%3!=0 || navStatus<0 || navStatus>3 ||
                navModel<0 || navModel>1 || queries<0 || total<queries || total>1000000 ||
                !Time(radius)||radius>5 || !Time(height)||height>10 || !Time(step)||step>10 || !Time(slope)||slope>90)
                throw new InvalidDataException("Invalid split recording metadata");
            r.Verify(checksumBuffer);
            var positions=new Vector3[selected.Length];for(int i=0;i<positions.Length;i++)positions[i]=points[selected[i]];
            return new RunSplitReference {Id=idRun,Scene=scene,Seed=seed,Spawn=spawn,Keep=keep,SelectedSeed=selectedSeed,
                PointCount=points.Length,Hash=hash,RawSeed=raw,Valid=valid,Outcome=outcome,Seconds=seconds,
                SpawnPosition=spawns[spawn],Selected=selected,Positions=positions,Collected=collected,PickupSeconds=times};
        }
    }
}
