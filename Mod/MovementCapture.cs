using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection.Emit;
using System.Text;
using System.Threading;
using HarmonyLib;
using UnityEngine;
using UnityEngine.SceneManagement;

// Observe the original input reads and physics movement without changing them.
public static class MovementCapture
{
    public struct Probe {public Vector3 Position;public bool Enabled;}
    private static Dictionary<int,MovementModel> models=new Dictionary<int,MovementModel>(),restored;
    private static readonly MovementWindow window=new MovementWindow();
    private static MovementModel model;private static MouseLook player;private static GameBuildInfo build;private static RunStore store;
    private static float horizontal=float.NaN,vertical=float.NaN;private static bool inside;
    private static double nextSave,nextSummary;private static MovementSummary summary;
    private static volatile bool readOnly,preserveDamaged,restoreFinished;private static volatile string recovery;private static int storeEpoch;
    private static string observationError;
    public static string StorageNote {get{return observationError??(store!=null&&store.LastMovementError!=null?store.LastMovementError:recovery??"");}}
    public static bool PreservingDamaged {get{return preserveDamaged;}}
    public static void Saved(){preserveDamaged=false;}
    public static void Initialize(RunStore data,string directory,GameBuildInfo identity)
    {
        int epoch=Interlocked.Increment(ref storeEpoch);store=data;build=identity;models=new Dictionary<int,MovementModel>();restored=null;model=null;player=null;summary=null;inside=false;readOnly=preserveDamaged=restoreFinished=false;recovery=observationError=null;
        string path=Path.Combine(directory,"movement-v1.bin");
        new Thread(()=>{
            try{
                if(!File.Exists(path)&&!File.Exists(path+".bak"))return;
                try{var loaded=Decode(ReadFile(path));if(epoch==storeEpoch)Volatile.Write(ref restored,loaded);}
                catch(NotSupportedException e){if(epoch==storeEpoch){readOnly=true;recovery=e.Message;}}
                catch(Exception e){if(epoch!=storeEpoch)return;preserveDamaged=File.Exists(path);try{var loaded=Decode(ReadFile(path+".bak"));if(epoch==storeEpoch){Volatile.Write(ref restored,loaded);recovery="Movement data recovered from backup: "+e.Message;}}catch(NotSupportedException error){if(epoch==storeEpoch){readOnly=true;recovery=error.Message;}}catch{if(epoch==storeEpoch)recovery="Movement data unavailable; learning afresh and preserving damaged data.";}}
            }finally{if(epoch==storeEpoch)restoreFinished=true;}
        }){IsBackground=true,Name="ItSteals.MovementRecovery"}.Start();
    }
    private const int ArchiveLimit=196608;
    private static byte[] ReadFile(string path){var info=new FileInfo(path);if(info.Length>ArchiveLimit)throw new InvalidDataException("Movement file exceeds limit");return File.ReadAllBytes(path);}
    public static Dictionary<int,MovementModel> Decode(byte[] bytes)
    {
        if(bytes==null||bytes.Length>ArchiveLimit)throw new InvalidDataException("Movement file exceeds limit");
        using(var r=new BinaryReader(new MemoryStream(bytes,false),new UTF8Encoding(false,true))){
            if(r.ReadUInt32()!=0x564d5349)throw new InvalidDataException("Invalid movement archive");int version=r.ReadInt32();if(version>1)throw new NotSupportedException("Movement data belongs to a newer suite; writes disabled.");if(version!=1)throw new InvalidDataException("Unsupported movement archive");
            int n=r.ReadInt32();if(n!=64)throw new InvalidDataException("Invalid movement build identity");var hash=Encoding.ASCII.GetString(r.ReadBytes(n));if(hash!=GameBuildInfo.SupportedHash)throw new InvalidDataException("Movement data belongs to another game build");
            n=r.ReadInt32();if(n<0||n>8)throw new InvalidDataException("Invalid movement model count");var result=new Dictionary<int,MovementModel>();
            for(int i=0;i<n;i++){var m=MovementModel.Read(r);if(result.ContainsKey(m.Scene))throw new InvalidDataException("Duplicate movement model");result.Add(m.Scene,m);}
            if(bytes.Length<8||r.ReadUInt64()!=PracticeMapData.Checksum(bytes,bytes.Length-8)||r.BaseStream.Position!=bytes.Length)throw new InvalidDataException("Movement checksum mismatch");return result;
        }
    }
    public static byte[] EncodeModels(IEnumerable<MovementModel> source)
    {
        var ordered=new List<MovementModel>(source);ordered.Sort((a,b)=>a.Scene.CompareTo(b.Scene));
        using(var s=new MemoryStream())using(var w=new BinaryWriter(s,Encoding.UTF8,true)){
            w.Write(0x564d5349u);w.Write(1);var hash=Encoding.ASCII.GetBytes(GameBuildInfo.SupportedHash);w.Write(hash.Length);w.Write(hash);w.Write(ordered.Count);foreach(var m in ordered)m.Write(w);w.Flush();w.Write(PracticeMapData.Checksum(s.ToArray()));w.Flush();return s.ToArray();
        }
    }
    private static void Adopt()
    {
        var loaded=Interlocked.Exchange(ref restored,null);if(loaded==null)return;
        foreach(var pair in loaded){MovementModel live;if(models.TryGetValue(pair.Key,out live))live.Merge(pair.Value);else models.Add(pair.Key,pair.Value);}nextSummary=0;
    }
    private static void Bind()
    {
        Adopt();if(player==null||player.controller==null||build==null||!build.Supported)return;
        float tick=Time.timeScale>0?Time.fixedDeltaTime/Time.timeScale:0;var cc=player.controller;var scale=cc.transform.lossyScale;
        float radius=cc.radius*Mathf.Max(Math.Abs(scale.x),Math.Abs(scale.z)),height=Math.Max(radius*2,cc.height*Math.Abs(scale.y));int scene=SceneManager.GetActiveScene().buildIndex;
        if(float.IsNaN(tick)||float.IsInfinity(tick)||tick<=0||tick>1||float.IsNaN(radius)||float.IsInfinity(radius)||radius<=0||radius>5||float.IsNaN(height)||float.IsInfinity(height)||height<=0||height>10||Array.IndexOf(SpeedrunManager.LevelIndices,scene)<0){model=null;window.Reset();return;}
        if(model!=null&&model.Scene==scene&&model.Matches(tick,radius,height))return;
        if(!models.TryGetValue(scene,out model)||!model.Matches(tick,radius,height)){model=new MovementModel(scene,tick,radius,height);models[scene]=model;}
        window.Reset();nextSummary=0;
    }
    public static void Begin(MouseLook mouse){player=mouse;model=null;summary=null;observationError=null;window.Reset();inside=false;Bind();nextSave=Time.realtimeSinceStartupAsDouble+10;}
    private static void ObservationFailed(Exception error){inside=false;window.Reset();observationError="Movement learning paused: "+error.GetType().Name;}
    public static void BeforePhysics(MouseLook __instance,out Probe __state)
    {
        __state=new Probe();inside=false;if(observationError!=null)return;
        try{
            if(__instance!=player||SpeedrunManager.Instance==null||!SpeedrunManager.Instance.MovementLearningAllowed(__instance)){window.Reset();return;}
            Bind();if(model==null)return;var body=PlayerContext.Body(__instance);if(body==null)return;
            __state.Position=body.position;__state.Enabled=true;horizontal=vertical=float.NaN;inside=true;
        }catch(Exception error){__state.Enabled=false;ObservationFailed(error);}
    }
    public static float Axis(string name)
    {
        float value=Input.GetAxis(name);ReplayInputCapture.ObserveAxis(name,value);if(inside){if(name=="Horizontal")horizontal=value;else if(name=="Vertical")vertical=value;}return value;
    }
    public static IEnumerable<CodeInstruction> ObserveAxes(IEnumerable<CodeInstruction> instructions)
    {
        var original=typeof(Input).GetMethod("GetAxis",new[]{typeof(string)});var replacement=typeof(MovementCapture).GetMethod("Axis");
        foreach(var instruction in instructions){if(instruction.opcode==OpCodes.Call&&Equals(instruction.operand,original))instruction.operand=replacement;yield return instruction;}
    }
    public static void AfterPhysics(MouseLook __instance,Probe __state)
    {
        inside=false;if(!__state.Enabled||__instance!=player||model==null)return;
        try{
            var body=PlayerContext.Body(__instance);if(body==null){window.Reset();return;}
            double axes=Math.Sqrt(horizontal*horizontal+vertical*vertical);
            bool eligible=SpeedrunManager.Instance.MovementLearningAllowed(__instance)&&__instance.isSprinting&&__instance.canMove>=.999f&&Math.Abs(Time.timeScale-1)<.000001&&__instance.controller.enabled&&__instance.controller.isGrounded&&!MenuCursor.OwnsInput&&!double.IsNaN(axes)&&!double.IsInfinity(axes);
            window.Observe(model,body.position-__state.Position,model.Tick,axes,eligible);
        }catch(Exception error){ObservationFailed(error);}
    }
    public static void PhysicsFailed(Exception __exception){if(__exception!=null){inside=false;window.Reset();}}
    public static MovementSummary Summary(int scene,bool force=false)
    {
        Adopt();if(build==null||!build.Supported)return null;if(model!=null&&model.Scene==scene){double now=Time.realtimeSinceStartupAsDouble;if(force||summary==null||now>=nextSummary){summary=model.Summary();nextSummary=now+1;}return summary;}
        MovementModel saved;return models.TryGetValue(scene,out saved)?saved.Summary():null;
    }
    public static MovementSummary ForMap(PracticeMapData data,bool force=false){var s=Summary(data.Scene,force);return s!=null&&s.Matches(data)?s:null;}
    public static void Tick(){Adopt();if(player!=null&&Time.realtimeSinceStartupAsDouble>=nextSave){nextSave=Time.realtimeSinceStartupAsDouble+10;Save();}}
    private static void Save(){Adopt();if(store==null||readOnly||!restoreFinished||build==null||!build.Supported||models.Count==0)return;store.QueueMovement(EncodeModels(models.Values));}
    public static void End(){inside=false;nextSummary=0;Save();player=null;window.Reset();}
    public static void WriteSnapshot(SnapshotBuffer data,int scene)
    {
        var s=Summary(scene);if(s!=null){data.Write(SharedLayout.MovementScene,s.Scene);data.Write(SharedLayout.MovementRevision,s.Revision);data.Write(SharedLayout.MovementRejected,s.Rejected);data.Write(SharedLayout.MovementTick,s.Tick);data.Write(SharedLayout.MovementRadius,s.Radius);data.Write(SharedLayout.MovementHeight,s.Height);
            for(int k=0;k<5;k++){data.Write(SharedLayout.MovementCounts+k*4,s.Counts[k]);data.Write(SharedLayout.MovementLengths+k*4,s.Lengths[k]);for(int q=0;q<3;q++)data.Write(SharedLayout.MovementSpeeds+(k*3+q)*4,s.Speeds[k*3+q]);}}
        else data.Write(SharedLayout.MovementScene,-1);
        data.WriteString(SharedLayout.MovementNote,StorageNote,128);
    }
}
