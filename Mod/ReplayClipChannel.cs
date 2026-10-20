using System;
using System.IO.MemoryMappedFiles;
using System.Threading;
using System.Diagnostics;
using System.Collections.Generic;

// Three independent slots. Rendering, GPU transfer, native composition and
// encoding overlap; each result remains bound to serial + run + frame index.
public static class ReplayClipChannel {
    public const int Version=4,Slots=3,Header=1024,SlotBytes=16*1024*1024,Pixels=1024,Capacity=Header+Slots*SlotBytes;
    private sealed class Slot {
        public uint Serial,Served;public long Frame,Run;public double At;public int Width,Height,Offset;
        public ClipCameraPose Pose;public ClipGpuReadback Readback;public string Error;public float RenderMs;
    }
    private static MemoryMappedFile mapping;private static MemoryMappedViewAccessor view;private static Mutex gate;
    private static readonly Slot[] slots={new Slot{Offset=Header},new Slot{Offset=Header+SlotBytes},new Slot{Offset=Header+SlotBytes*2}};
    private static readonly List<ClipGpuReadback> retired=new List<ClipGpuReadback>();
    private static bool Lock(){try{return gate.WaitOne(0);}catch(AbandonedMutexException){return true;}}
    private static void Retire(ClipGpuReadback value){if(value==null)return;if(value.CanDispose)value.Dispose();else retired.Add(value);}
    private static void Cleanup(){for(int i=retired.Count-1;i>=0;i--)if(retired[i].CanDispose){retired[i].Dispose();retired.RemoveAt(i);}}
    private static bool Current(Slot slot){return view.ReadUInt32(slot.Offset+8)==slot.Serial&&view.ReadInt32(slot.Offset+16)==0&&view.ReadInt64(slot.Offset+144)==slot.Frame&&view.ReadInt64(slot.Offset+40)==slot.Run;}
    public static void Tick(int pid,bool connected){
        Cleanup();ClipReadbackCalibration.Tick(false);
        if(!connected||!Replay3D.Reserved){Close();return;}
        try{
            if(mapping==null){
                string name="Local\\ItSteals.Clip.v4."+pid;mapping=MemoryMappedFile.CreateOrOpen(name,Capacity,MemoryMappedFileAccess.ReadWrite);view=mapping.CreateViewAccessor();gate=new Mutex(false,name+".Lock");
                view.Write(0,0);view.Write(4,Version);view.Write(8,Slots);view.Write(12,SlotBytes);foreach(var slot in slots)view.Write(slot.Offset+16,-1);view.Write(0,0x504c4349);
            }
            // Copy every completed request promptly: Unity's native GetData
            // buffer may be retired a frame later. Publishing can wait safely.
            foreach(var slot in slots)if(slot.Serial!=0){
                bool current=true;if(Lock())try{current=Current(slot);}finally{gate.ReleaseMutex();}
                if(!current){if(slot.Readback==null||slot.Readback.CanDispose){Retire(slot.Readback);slot.Readback=null;slot.Serial=0;slot.Error=null;}continue;}
                try{if(slot.Error==null&&!slot.Readback.Poll())continue;}catch(Exception e){slot.Error=e.Message;}
                if(!Lock())continue;
                try{
                    if(Current(slot)){
                        long start=Stopwatch.GetTimestamp();
                        if(slot.Error==null){view.WriteArray(slot.Offset+Pixels,slot.Readback.Pixels,0,slot.Readback.Pixels.Length);slot.Pose.Write(view,slot.Offset+96);}
                        byte[] text=System.Text.Encoding.UTF8.GetBytes(slot.Error??"");int n=Math.Min(510,text.Length);view.WriteArray(slot.Offset+256,text,0,n);view.Write(slot.Offset+256+n,(byte)0);
                        view.Write(slot.Offset+152,slot.Readback==null?0:slot.Readback.Mode);view.Write(slot.Offset+156,slot.RenderMs);view.Write(slot.Offset+160,slot.Readback==null?0:slot.Readback.Milliseconds);
                        view.Write(slot.Offset+164,(float)((Stopwatch.GetTimestamp()-start)*1000.0/Stopwatch.Frequency));view.Write(slot.Offset+16,slot.Error==null?1:2);view.Write(slot.Offset+12,slot.Serial);
                    }
                    slot.Served=slot.Serial;slot.Serial=0;slot.Error=null;
                }finally{gate.ReleaseMutex();}
            }
            bool any=false;
            if(Lock())try{foreach(var slot in slots)if(slot.Serial==0&&view.ReadInt32(slot.Offset+16)==0&&view.ReadUInt32(slot.Offset+8)!=slot.Served){any=true;break;}}finally{gate.ReleaseMutex();}
            if(!any)return;ClipReadbackCalibration.Tick(true);if(!ClipReadbackCalibration.Ready)return;
            int captures=0;
            foreach(var slot in slots){
                if(slot.Serial!=0||captures>=2)continue;
                uint options=0;ClipCameraPose? custom=null;float audioSpeed=0,audioVolume=1;bool requested=false;
                if(!Lock())return;
                try{
                    int at=slot.Offset;uint serial=view.ReadUInt32(at+8);if(serial==0||serial==slot.Served||view.ReadInt32(at+16)!=0)continue;
                    slot.Serial=serial;slot.Frame=view.ReadInt64(at+144);slot.Run=view.ReadInt64(at+40);slot.At=view.ReadDouble(at+32);slot.Width=view.ReadInt32(at+20);slot.Height=view.ReadInt32(at+24);options=view.ReadUInt32(at+48);audioSpeed=view.ReadSingle(at+136);audioVolume=view.ReadSingle(at+140);requested=true;
                    int flag=view.ReadInt32(at+52);if(flag==1)custom=ClipCameraPose.Read(view,at+56);else if(flag!=0)throw new InvalidOperationException("Invalid camera override flag");
                }catch(Exception e){slot.Error=e.Message;}
                finally{gate.ReleaseMutex();}
                if(!requested)continue;captures++;
                try{
                    if(slot.Error!=null)continue;
                    if(slot.Frame<0||slot.Width<320||slot.Width>2560||slot.Height<180||slot.Height>1440||(slot.Width&1)!=0||(slot.Height&1)!=0||double.IsNaN(slot.At)||double.IsInfinity(slot.At)||(options&~196607u)!=0||float.IsNaN(audioSpeed)||float.IsInfinity(audioSpeed)||audioSpeed<0||audioSpeed>8||float.IsNaN(audioVolume)||float.IsInfinity(audioVolume)||audioVolume<0||audioVolume>1)throw new InvalidOperationException("Invalid clip frame request");
                    if(custom.HasValue)custom.Value.Validate();
                    if(slot.Readback==null||slot.Readback.Target.width!=slot.Width||slot.Readback.Target.height!=slot.Height){Retire(slot.Readback);slot.Readback=new ClipGpuReadback(slot.Width,slot.Height);}
                    long start=Stopwatch.GetTimestamp();slot.Pose=Replay3D.CaptureClip(slot.Run,slot.At,options,slot.Readback.Target,null,custom);slot.RenderMs=(float)((Stopwatch.GetTimestamp()-start)*1000.0/Stopwatch.Frequency);
                    slot.Readback.Begin();Replay3D.PreviewAudio(slot.At,audioSpeed,audioVolume);
                }catch(Exception e){slot.Error=e.Message;}
            }
        }catch(Exception){Close();}
    }
    public static void Close(){
        foreach(var slot in slots){Retire(slot.Readback);slot.Readback=null;slot.Serial=slot.Served=0;slot.Error=null;}
        if(view!=null)view.Dispose();if(mapping!=null)mapping.Dispose();if(gate!=null)gate.Dispose();view=null;mapping=null;gate=null;
    }
}
