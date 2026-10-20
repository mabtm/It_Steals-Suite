using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Text;
using System.Threading;
using UnityEngine;

public sealed class InspectorObjectRow
{
    public int Id,Parent,Scene,Layer;public uint Flags;public string Name="";
}
public sealed class InspectorValue
{
    public ulong Key;public int Component;public uint Flags;
    public string Group="",Name="",Source="",Value="";
}
public sealed class InspectorChange
{
    public ulong Key;public int Frame;public double Seconds;public string Before="",After="";
}
public struct InspectorCommand
{
    public uint Serial,Epoch,Flags;public int Action,Object;public float X,Y;public ulong Key;
    public bool Valid {get{return Action>=0&&Action<=9&&(Flags&~63u)==0&&!float.IsNaN(X)&&!float.IsNaN(Y)&&!float.IsInfinity(X)&&!float.IsInfinity(Y)&&X>=0&&X<=1&&Y>=0&&Y<=1;}}
}
public static class InspectorPolicy
{
    public static bool Permitted(bool connected,bool requested,bool commandRestricted,bool speedrunRestricted,bool replayReserved){return connected&&requested&&!commandRestricted&&!speedrunRestricted&&!replayReserved;}
}
public static class InspectorWire
{
    public const uint Magic=0x504e5349;public const int Version=3;
    public const int MaxObjects=20000,MaxValues=512,MaxChanges=128,MaxColliders=32,MaxParts=512,MaxPoints=4096;
    public static void Text(BinaryWriter w,string s,int max=2048){
        s=(s??"").Replace("\0","\\0");if(s.Length>max/3)s=s.Substring(0,max/3);var b=Encoding.UTF8.GetBytes(s);w.Write(b.Length);w.Write(b);
    }
    public static void Vector(BinaryWriter w,Vector3 p){w.Write(p.x);w.Write(p.y);w.Write(p.z);}
    public static byte[] Encode(int kind,int pid,uint session,int scene,uint epoch,uint revision,Action<BinaryWriter> body){
        return Encode(kind,pid,session,scene,epoch,revision,DateTime.UtcNow.Ticks,Time.realtimeSinceStartupAsDouble,Time.frameCount,body);
    }
    public static byte[] Encode(int kind,int pid,uint session,int scene,uint epoch,uint revision,long utc,double seconds,int frame,Action<BinaryWriter> body){
        using(var m=new MemoryStream())using(var w=new BinaryWriter(m,Encoding.UTF8,true)){
            w.Write(Magic);w.Write(Version);w.Write(kind);w.Write(pid);w.Write(session);w.Write(scene);w.Write(epoch);w.Write(revision);w.Write(utc);w.Write(seconds);w.Write(frame);
            body(w);w.Flush();w.Write(PracticeMapData.Checksum(m.ToArray()));w.Flush();return m.ToArray();
        }
    }
    public static void Object(BinaryWriter w,InspectorObjectRow o){w.Write(o.Id);w.Write(o.Parent);w.Write(o.Scene);w.Write(o.Layer);w.Write(o.Flags);Text(w,o.Name,768);}
    public static void Value(BinaryWriter w,InspectorValue v){w.Write(v.Key);w.Write(v.Component);w.Write(v.Flags);Text(w,v.Group,512);Text(w,v.Name,512);Text(w,v.Source,1024);Text(w,v.Value,2048);}
    public static void Change(BinaryWriter w,InspectorChange c){w.Write(c.Key);w.Write(c.Frame);w.Write(c.Seconds);Text(w,c.Before,2048);Text(w,c.After,2048);}
    public static ulong Key(int component,string name){return PracticeMapData.Checksum(Encoding.UTF8.GetBytes(component.ToString(System.Globalization.CultureInfo.InvariantCulture)+"/"+name));}
}

// Two independently revised publications keep the static hierarchy out of the
// 5-Hz selected-object stream. Both directions use a nonblocking mutex; Unity
// never waits for the trainer. Commands require the live main-channel opt-in.
public sealed class InspectorChannel : IDisposable
{
    public const int CatalogOffset=256,CatalogCapacity=4194304,DetailOffset=CatalogOffset+CatalogCapacity,DetailCapacity=1048576,HoverOffset=DetailOffset+DetailCapacity,HoverCapacity=16384,Capacity=HoverOffset+HoverCapacity;
    private MemoryMappedFile map;private MemoryMappedViewAccessor view;private Mutex gate;private uint catalogRevision,detailRevision,hoverRevision;
    private readonly uint session;private byte[] catalog,detail,hover;
    public InspectorChannel(int pid,uint ownerSession){
        session=ownerSession;string name="ItStealsInspector.v3."+pid;
        try{
            gate=new Mutex(false,name+".Lock");map=MemoryMappedFile.CreateOrOpen(name,Capacity);view=map.CreateViewAccessor(0,Capacity);
            bool acquired;try{acquired=gate.WaitOne(0);}catch(AbandonedMutexException){acquired=true;}
            if(!acquired)throw new IOException("Inspector channel is busy; retrying.");
            try{for(int i=0;i<256;i+=4)view.Write(i,0u);view.Write(0,InspectorWire.Magic);view.Write(4,3u);view.Write(8,session);view.Write(12,pid);}finally{gate.ReleaseMutex();}
        }catch{Dispose();throw;}
    }
    private bool Enter(){if(gate==null)return false;try{return gate.WaitOne(0);}catch(AbandonedMutexException){return true;}}
    public bool Read(out InspectorCommand c,out InspectorPointer p){
        c=new InspectorCommand();p=new InspectorPointer();if(!Enter())return false;
        try{
            if(view.ReadUInt32(8)!=session)return false;
            c.Serial=view.ReadUInt32(64);c.Action=view.ReadInt32(68);c.Object=view.ReadInt32(72);c.Flags=view.ReadUInt32(76);c.X=view.ReadSingle(80);c.Y=view.ReadSingle(84);c.Epoch=view.ReadUInt32(88);c.Key=view.ReadUInt64(96);p.Serial=view.ReadUInt32(128);p.Epoch=view.ReadUInt32(132);p.Active=view.ReadUInt32(136)==1;p.X=view.ReadSingle(140);p.Y=view.ReadSingle(144);p.Width=view.ReadInt32(148);p.Height=view.ReadInt32(152);p.Action=view.ReadInt32(156);p.Preferred=view.ReadInt32(160);return true;
        }finally{gate.ReleaseMutex();}
    }
    public void QueueCatalog(byte[] b){if(b==null||b.Length>CatalogCapacity)throw new InvalidDataException("Inspector catalog exceeds its bounded channel.");catalog=b;}
    public void QueueDetail(byte[] b){if(b==null||b.Length>DetailCapacity)throw new InvalidDataException("Inspector details exceed their bounded channel.");detail=b;}
    public void QueueHover(byte[] b){if(b==null||b.Length>HoverCapacity)throw new InvalidDataException("Inspector hover exceeds its bounded channel.");hover=b;}
    public void Flush(){
        if((catalog==null&&detail==null&&hover==null)||!Enter())return;
        try{
            if(catalog!=null){view.Write(32,0);view.WriteArray(CatalogOffset,catalog,0,catalog.Length);view.Write(36,unchecked(++catalogRevision));view.Write(32,catalog.Length);catalog=null;}
            if(detail!=null){view.Write(40,0);view.WriteArray(DetailOffset,detail,0,detail.Length);view.Write(44,unchecked(++detailRevision));view.Write(40,detail.Length);detail=null;}
            if(hover!=null){view.Write(48,0);view.WriteArray(HoverOffset,hover,0,hover.Length);view.Write(52,unchecked(++hoverRevision));view.Write(48,hover.Length);hover=null;}
        }finally{gate.ReleaseMutex();}
    }
    public void Dispose(){catalog=detail=hover=null;if(view!=null){view.Dispose();view=null;}if(map!=null){map.Dispose();map=null;}if(gate!=null){gate.Dispose();gate=null;}}
}
