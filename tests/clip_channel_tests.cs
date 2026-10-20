using System;
using System.IO;
using System.IO.MemoryMappedFiles;
using System.Collections.Generic;
using System.Threading;
namespace UnityEngine {
public struct Vector3 {public float x,y,z;public Vector3(float a,float b,float c){x=a;y=b;z=c;}}
public struct Quaternion {public float x,y,z,w;public Quaternion(float a,float b,float c,float d){x=a;y=b;z=c;w=d;}}
public struct Rect {public Rect(int x,int y,int w,int h){}}
public struct Color32 {public byte r,g,b,a;public Color32(byte x,byte y,byte z,byte v){r=x;g=y;b=z;a=v;}}
public enum HideFlags {HideAndDontSave}
public enum TextureFormat {BGRA32,RGBA32}
public enum RenderTextureFormat {ARGB32}
public static class SystemInfo {public static bool Bgra=true,supportsAsyncGPUReadback=true;public static bool SupportsTextureFormat(TextureFormat f){return Bgra;}}
public class Object {public bool Destroyed;public static void Destroy(Object o){o.Destroyed=true;}}
public sealed class RenderTexture:Object {
 public static RenderTexture active;public HideFlags hideFlags;public int width,height;public byte[] Bytes;
 public RenderTexture(int w,int h,int depth,RenderTextureFormat f){width=w;height=h;Bytes=new byte[w*h*4];}
 public bool Create(){return true;}
 public void Release(){foreach(var j in Rendering.AsyncGPUReadback.Jobs)if(j.Target==this&&!j.Done)throw new Exception("GPU target released before completion");}
}
public sealed class Texture2D:Object {
 public static int FullReads;public HideFlags hideFlags;public byte[] Bytes;readonly TextureFormat format;
 public Texture2D(int w,int h,TextureFormat f,bool mip){Bytes=new byte[w*h*4];format=f;}
 public void SetPixels32(Color32[] pixels){for(int i=0;i<pixels.Length;i++){Bytes[i*4]=pixels[i].r;Bytes[i*4+1]=pixels[i].g;Bytes[i*4+2]=pixels[i].b;Bytes[i*4+3]=pixels[i].a;}}
 public void Apply(bool m,bool n){}
 public void ReadPixels(Rect r,int x,int y,bool m){if(Bytes.Length>16)FullReads++;Bytes=Rendering.AsyncGPUReadback.Encode(RenderTexture.active,format,false);}
 public RawData GetRawTextureData<T>(){return new RawData{Bytes=Bytes};}
}
public static class Graphics {public static void Blit(Texture2D a,RenderTexture b){Buffer.BlockCopy(a.Bytes,0,b.Bytes,0,a.Bytes.Length);}}
public struct RawData {public byte[] Bytes;public int Length{get{return Bytes.Length;}}public void CopyTo(byte[] to){Buffer.BlockCopy(Bytes,0,to,0,Bytes.Length);}}
}
namespace UnityEngine.Rendering {
public sealed class Job {public UnityEngine.RenderTexture Target;public byte[] Bytes;public bool Done,Error,Expired;public int Reads;}
public struct AsyncGPUReadbackRequest {
 public Job Job;public bool done{get{return Job.Done;}}public bool hasError{get{return Job.Error;}}
 public UnityEngine.RawData GetData<T>(){if(!Job.Done||Job.Error||Job.Expired)throw new Exception("GPU data used outside its completion window");Job.Reads++;return new UnityEngine.RawData{Bytes=Job.Bytes};}
}
public static class AsyncGPUReadback {
 public static bool TopDown;public static readonly List<Job> Jobs=new List<Job>();
 public static byte[] Encode(UnityEngine.RenderTexture t,UnityEngine.TextureFormat f,bool flip){var b=new byte[t.Bytes.Length];int stride=t.width*4;for(int y=0;y<t.height;y++)Buffer.BlockCopy(t.Bytes,(flip?t.height-1-y:y)*stride,b,y*stride,stride);if(f==UnityEngine.TextureFormat.BGRA32)for(int i=0;i<b.Length;i+=4){byte r=b[i];b[i]=b[i+2];b[i+2]=r;}return b;}
 public static AsyncGPUReadbackRequest Request(UnityEngine.RenderTexture t,int mip,UnityEngine.TextureFormat f){var j=new Job{Target=t,Bytes=Encode(t,f,TopDown)};Jobs.Add(j);return new AsyncGPUReadbackRequest{Job=j};}
}
}
public static class Replay3D {
 public static bool Reserved=true;public static int Calls;public static uint LastOptions;public static float AudioSpeed,AudioVolume;
 public static void PreviewAudio(double at,float speed,float volume){AudioSpeed=speed;AudioVolume=volume;}
 public static ClipCameraPose CaptureClip(long id,double at,uint options,UnityEngine.RenderTexture target,UnityEngine.Texture2D destination,ClipCameraPose? custom=null){
  if(id!=750||at<0||at>10)throw new Exception("Wrong replay identity");if(destination!=null)throw new Exception("Blocking ReadPixels requested from scene renderer");Calls++;LastOptions=options;
  for(int y=0;y<target.height;y++)for(int x=0;x<target.width;x++){int i=(y*target.width+x)*4;target.Bytes[i]=(byte)Math.Round(at*20);target.Bytes[i+1]=(byte)(y%251);target.Bytes[i+2]=29;target.Bytes[i+3]=255;}
  return custom??new ClipCameraPose{Position=new UnityEngine.Vector3(1,2,3),Rotation=new UnityEngine.Quaternion(0,0,0,1),Fov=60};
 }
}
public static class ClipChannelTests {
 const int Pid=17900;static MemoryMappedViewAccessor v;static uint serial=1;static readonly ClipCameraPose Pose=new ClipCameraPose{Position=new UnityEngine.Vector3(12.5f,-1,9),Rotation=new UnityEngine.Quaternion(0,.70710677f,0,.70710677f),Fov=83};
 static void Check(bool b,string message){if(!b)throw new Exception(message);}
 static int O(int slot){return ReplayClipChannel.Header+slot*ReplayClipChannel.SlotBytes;}
 static void Tick(){ReplayClipChannel.Tick(Pid,true);}
 static uint Request(int slot,long frame,double at=1.25,int width=320,long run=750,uint options=40962u,ClipCameraPose? custom=null){int o=O(slot);uint id=serial++;v.Write(o+20,width);v.Write(o+24,180);v.Write(o+32,at);v.Write(o+40,run);v.Write(o+48,options);v.Write(o+52,1);(custom??Pose).Write(v,o+56);v.Write(o+136,.5f);v.Write(o+140,.75f);v.Write(o+144,frame);v.Write(o+16,0);v.Write(o+8,id);return id;}
 static void Ack(int slot,uint id,long frame,bool good=true){int o=O(slot);Check(v.ReadUInt32(o+12)==id&&v.ReadInt32(o+16)==(good?1:2),"matching slot acknowledgement");Check(v.ReadInt64(o+144)==frame,"frame identity retained");if(good){var pose=ClipCameraPose.Read(v,o+96);Check(pose.Position.x==12.5f&&pose.Fov==83,"exact camera pose roundtrip");Check(v.ReadByte(o+1024)==29&&v.ReadByte(o+1025)==179&&v.ReadByte(o+1026)==(byte)Math.Round(v.ReadDouble(o+32)*20)&&v.ReadByte(o+1027)==255,"verified top-down BGRA conversion");}}
 static UnityEngine.Rendering.Job Last(){var list=UnityEngine.Rendering.AsyncGPUReadback.Jobs;return list[list.Count-1];}
 public static int Main(string[] args){
  string mode=args[0];bool async=mode!="unsupported";UnityEngine.SystemInfo.supportsAsyncGPUReadback=async;UnityEngine.SystemInfo.Bgra=!mode.Contains("rgba");UnityEngine.Rendering.AsyncGPUReadback.TopDown=mode.Contains("top");Tick();
  using(var map=MemoryMappedFile.OpenExisting("Local\\ItSteals.Clip.v4."+Pid))using(v=map.CreateViewAccessor()){
   Check(v.ReadInt32(4)==4&&v.ReadInt32(8)==3&&v.ReadInt32(12)==ReplayClipChannel.SlotBytes,"v4 three-slot layout");
   uint a=Request(0,0),b=Request(1,1,1.5),c=Request(2,2,1.75);Tick();
   if(async){Check(Replay3D.Calls==0,"calibration precedes full-sized work");Last().Done=true;Tick();Check(Replay3D.Calls==2,"at most two renders per game tick");Tick();Check(Replay3D.Calls==3,"three source frames overlap");var jobs=UnityEngine.Rendering.AsyncGPUReadback.Jobs;
    jobs[3].Done=true;Tick();Ack(2,c,2);Check(v.ReadUInt32(O(0)+12)!=a&&v.ReadUInt32(O(1)+12)!=b,"out-of-order completion cannot acknowledge another slot");
    jobs[1].Done=true;var held=new ManualResetEvent(false);var release=new ManualResetEvent(false);var worker=new Thread(()=>{using(var gate=new Mutex(false,"Local\\ItSteals.Clip.v4."+Pid+".Lock")){gate.WaitOne();held.Set();release.WaitOne();gate.ReleaseMutex();}});worker.Start();held.WaitOne();Tick();Check(jobs[1].Reads==1,"completed GPU bytes copied even while publication mutex is busy");jobs[1].Expired=true;release.Set();worker.Join();Tick();Ack(0,a,0);Check(jobs[1].Reads==1,"publication survives native GPU buffer retirement");jobs[2].Done=true;Tick();Ack(1,b,1);
    Check(UnityEngine.Texture2D.FullReads==0,"async scene frames do not call ReadPixels");
    uint old=Request(0,3);Tick();var abandoned=Last();uint current=Request(0,4,2);Tick();Check(Last()==abandoned&&!abandoned.Target.Destroyed,"cancelled slot retained while GPU owns it");abandoned.Done=true;Tick();Check(Last()!=abandoned&&abandoned.Target.Destroyed,"slot is reused only after completion");Last().Done=true;Tick();Ack(0,current,4);Check(v.ReadUInt32(O(0)+12)!=old,"late cancelled frame not published");
    Request(0,5);Tick();var orphan=Last();ReplayClipChannel.Tick(Pid,false);Check(!orphan.Target.Destroyed,"disconnect preserves in-flight GPU resource");orphan.Done=true;ReplayClipChannel.Tick(Pid,false);Check(orphan.Target.Destroyed,"retired resource released after completion");Tick();
    uint fallback=Request(0,6,2.5);Tick();Last().Error=Last().Done=true;Tick();Ack(0,fallback,6);Check(v.ReadInt32(O(0)+152)==0&&UnityEngine.Texture2D.FullReads==1,"GPU error falls back to exact retained source frame");
   }else{Tick();Tick();Ack(0,a,0);Ack(1,b,1);Ack(2,c,2);Check(UnityEngine.Texture2D.FullReads==3,"unsupported hardware uses verified fallback");}
   Check(Replay3D.AudioSpeed==.5f&&Replay3D.AudioVolume==.75f&&Replay3D.LastOptions==40962u,"audio and visual settings preserved");
   int calls=Replay3D.Calls;Tick();Check(calls==Replay3D.Calls,"serial rendered once");var bad=Pose;bad.Fov=float.NaN;uint fail=Request(0,7,custom:bad);Tick();Tick();Ack(0,fail,7,false);Check(Replay3D.Calls==calls,"nonfinite camera rejected before render");
   fail=Request(0,8,width:319);Tick();Tick();Ack(0,fail,8,false);fail=Request(0,9,options:0x80000000u);Tick();Tick();Ack(0,fail,9,false);fail=Request(0,10,run:751);Tick();Tick();Ack(0,fail,10,false);Check(Replay3D.Calls==calls,"bad dimensions, options and replay identity rejected");
  }ReplayClipChannel.Tick(Pid,false);Console.WriteLine("Production v4 channel + GPU adapter / host graphics doubles / "+mode+": ordered identity, 3-slot overlap, pixel format/orientation, backpressure, cancellation, GPU lifetime and fallback: PASS");return 0;
 }
}
