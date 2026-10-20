using System;
using System.IO;
using System.Threading;
using UnityEngine;

// A small reusable cache supplies the DSP callback. Seeking/file IO happens
// on its own worker; the callback performs no IO, locks, allocations or Unity
// API calls. PCM remains in the original recorded listening perspective.
public sealed class ReplayPcmCache : IDisposable {
    private sealed class Window {public readonly float[] Samples;public long First;public int Frames,Readers;public Window(int n){Samples=new float[n];}}
    private readonly ReplayAudioInfo info;private readonly Window[] windows=new Window[3];
    private volatile Window current;private volatile bool stopped;
    private long wanted;private double seek,speed=1,cursor;private int generation,lastGeneration=-1,underruns;
    public volatile string Error;
    public int Underruns {get{return Volatile.Read(ref underruns);}}
    public ReplayPcmCache(string path,ReplayAudioInfo metadata){info=metadata;int frames=info.Rate*4;for(int i=0;i<windows.Length;i++)windows[i]=new Window(frames*info.Channels);new Thread(()=>Read(path)){IsBackground=true,Name="ItSteals.ReplayAudioRead"}.Start();}
    public bool Prepare(double frame){long first=(long)Math.Max(0,Math.Min(info.Frames-1,frame));Interlocked.Exchange(ref wanted,first);var w=current;return w!=null&&first>=w.First&&first+Math.Min(info.Rate/8,info.Frames-first)<=w.First+w.Frames;}
    public void Seek(double frame){Volatile.Write(ref seek,Math.Max(0,Math.Min(info.Frames,frame)));Interlocked.Exchange(ref wanted,(long)Math.Max(0,Math.Min(info.Frames-1,frame)));Interlocked.Increment(ref generation);}
    public void Speed(double value){Volatile.Write(ref speed,Math.Max(.1,Math.Min(8,value)));}
    private void Read(string path){try{using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read,65536)){var bytes=new byte[windows[0].Samples.Length*4];while(!stopped){long frame=Interlocked.Read(ref wanted);var old=current;if(old!=null&&frame>=old.First&&frame+Math.Min(info.Rate/2,info.Frames-frame)<=old.First+old.Frames){Thread.Sleep(5);continue;}Window target=null;foreach(var w in windows)if(w!=old&&Volatile.Read(ref w.Readers)==0){target=w;break;}if(target==null){Thread.Sleep(2);continue;}long first=Math.Max(0,frame-info.Rate/4);int frames=(int)Math.Min(target.Samples.Length/info.Channels,info.Frames-first),length=frames*info.Channels*4;stream.Position=ReplayAudioFile.HeaderBytes+first*info.Channels*4;int read=0;while(read<length){int n=stream.Read(bytes,read,length-read);if(n==0)throw new EndOfStreamException();read+=n;}Buffer.BlockCopy(bytes,0,target.Samples,0,length);target.First=first;target.Frames=frames;current=target;}}}catch(Exception e){Error="Recorded audio read failed: "+e.Message;}}
    public void Render(float[] output){
        Array.Clear(output,0,output.Length);if(stopped)return;
        int version=Volatile.Read(ref generation);if(version!=lastGeneration){cursor=Volatile.Read(ref seek);lastGeneration=version;}
        Window w=current;if(w==null){Interlocked.Increment(ref underruns);return;}Interlocked.Increment(ref w.Readers);
        try{if(w!=current){Interlocked.Increment(ref underruns);return;}double step=Volatile.Read(ref speed);bool gap=false;int frames=output.Length/info.Channels;
            for(int i=0;i<frames;i++){
                long at=(long)Math.Floor(cursor);float blend=(float)(cursor-at);if(at>=0&&at<info.Frames){long local=at-w.First;if(local<0||local>=w.Frames){gap=true;}else{long next=Math.Min(local+1,w.Frames-1);for(int ch=0;ch<info.Channels;ch++){float a=w.Samples[(int)local*info.Channels+ch],b=w.Samples[(int)next*info.Channels+ch],value=a+(b-a)*blend;output[i*info.Channels+ch]=float.IsNaN(value)||float.IsInfinity(value)?0:value;}}}cursor+=step;
            }
            Interlocked.Exchange(ref wanted,(long)Math.Max(0,Math.Min(info.Frames-1,cursor)));if(gap)Interlocked.Increment(ref underruns);
        }finally{Interlocked.Decrement(ref w.Readers);}
    }
    public void Position(int unused){} // Unity's streaming cursor is not the run clock.
    public void Dispose(){stopped=true;}
}
public sealed class ReplayAudioPlayback : IDisposable {
    private readonly ReplayAudioInfo info;private readonly ReplayPcmCache reader;private AudioSource source;private AudioClip clip;
    private bool live,disposed;private double last=-1;
    public bool HasProblem {get{return reader.Error!=null||reader.Underruns>0;}}
    public string Status {get{return reader.Error??(reader.Underruns>0?"Recorded audio / buffered underruns: "+reader.Underruns:"Recorded game mix / "+info.Rate+" Hz / "+info.Channels+" ch");}}
    public ReplayAudioPlayback(GameObject parent,string path,ReplayAudioInfo metadata){info=metadata;reader=new ReplayPcmCache(path,info);try{source=parent.AddComponent<AudioSource>();source.playOnAwake=false;source.loop=true;source.spatialBlend=0;source.dopplerLevel=0;source.ignoreListenerPause=true;source.ignoreListenerVolume=false;}catch{Dispose();throw;}}
    public void Seek(){last=-1;}
    public void Tick(double seconds,float speed,bool enabled,float volume=1){
        if(disposed||source==null)return;if(reader.Error!=null){if(live)source.Stop();live=false;return;}bool play=enabled&&seconds>=0&&seconds<info.Duration;source.volume=Mathf.Clamp01(volume);reader.Speed(speed);
        bool seek=last<0||seconds<last-.001||Math.Abs(seconds-last)>.3*Math.Max(1,speed);
        if(!play){if(live)source.Stop();live=false;last=seconds;return;}
        if(seek&&live){source.Stop();live=false;}
        if(!live){double frame=seconds*info.Rate;reader.Seek(frame);if(reader.Prepare(frame)){
            // Unity may request stream data during AudioClip.Create. Create
            // only after the first real source window is available.
            try{if(clip==null){clip=AudioClip.Create("Recorded run audio",Math.Max(info.Rate,(int)info.Frames),info.Channels,info.Rate,true,reader.Render,reader.Position);source.clip=clip;}source.Play();live=true;}
            catch(Exception e){reader.Error="Recorded audio playback unavailable: "+e.Message;source.Stop();live=false;}
        }}
        last=seconds;
    }
    public void Dispose(){if(disposed)return;disposed=true;if(source!=null){source.Stop();UnityEngine.Object.Destroy(source);source=null;}reader.Dispose();if(clip!=null){UnityEngine.Object.Destroy(clip);clip=null;}}
}
