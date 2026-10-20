using System;
using System.IO;
using System.Text;
using System.Threading;
using System.Diagnostics;

// Float PCM in a separate immutable companion. No game assets or microphone
// data are read. The DSP producer only copies into a bounded preallocated ring.
public static class ReplayAudioFile {
    public const int HeaderBytes=256;public const long MaxBytes=256L*1024*1024;
    public const uint Magic=0x55415349;
    public static ulong Hash(Stream s,long length){ulong h=14695981039346656037UL;var b=new byte[65536];while(length>0){int n=s.Read(b,0,(int)Math.Min(b.Length,length));if(n<=0)throw new EndOfStreamException();unchecked{for(int i=0;i<n;i++){h^=b[i];h*=1099511628211UL;}}length-=n;}return h;}
    public static ReplayAudioInfo Verify(string path,long id,int scene,int seed,string gameHash,ReplayAudioInfo expected=null){using(var s=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read,65536)){if(s.Length<HeaderBytes+8||s.Length>MaxBytes)throw new InvalidDataException("Recorded audio exceeds file limit");var r=new BinaryReader(s,Encoding.UTF8,true);if(r.ReadUInt32()!=Magic||r.ReadInt32()!=1||r.ReadInt64()!=id||r.ReadInt32()!=scene||r.ReadInt32()!=seed)throw new InvalidDataException("Recorded audio identity mismatch");var i=new ReplayAudioInfo{Recorded=true,Rate=r.ReadInt32(),Channels=r.ReadInt32(),Frames=r.ReadInt64(),Flags=r.ReadInt32(),Gaps=r.ReadInt32(),Bytes=s.Length};int n=r.ReadInt32();if(n<0||n>65||Encoding.UTF8.GetString(r.ReadBytes(n))!=gameHash||i.Rate<8000||i.Rate>192000||i.Channels<1||i.Channels>8||i.Frames<1||i.Frames>(MaxBytes-HeaderBytes-8)/(i.Channels*4)||s.Length!=HeaderBytes+i.Frames*i.Channels*4+8||(i.Flags&~31)!=0||i.Gaps<0)throw new InvalidDataException("Invalid recorded audio header");s.Position=s.Length-8;i.Checksum=r.ReadUInt64();s.Position=0;if(Hash(s,s.Length-8)!=i.Checksum)throw new InvalidDataException("Recorded audio checksum mismatch");if(expected!=null&&(!expected.Recorded||expected.Rate!=i.Rate||expected.Channels!=i.Channels||expected.Frames!=i.Frames||expected.Bytes!=i.Bytes||expected.Checksum!=i.Checksum||expected.Flags!=i.Flags||expected.Gaps!=i.Gaps))throw new InvalidDataException("Recorded audio does not match input recording");return i;}}
}
public sealed class ReplayAudioSession {
    internal sealed class Block {public readonly float[] Data;public long First;public int Count,Channels;public float Gain;public Block(int n){Data=new float[n];}}
    private readonly Block[] ring=new Block[32];private int head,tail,done,closing,discard,flags,gaps,accepting=1;
    private long target,closeDeadline;private int observed;
    private readonly int rate,channels;private readonly long id;private readonly int scene,seed;private readonly string hash,temp;
    private readonly Thread writer;private Exception failure;
    private volatile ReplayAudioLane[] lanes=new ReplayAudioLane[0];
    public readonly ReplayAudioInfo Info=new ReplayAudioInfo();
    public long MemoryBytes {get{long bytes=(long)ring.Length*ring[0].Data.Length*4+ring[0].Data.Length*64+65536;foreach(var lane in lanes)bytes+=lane.MemoryBytes;return bytes;}}
    public bool Done {get{return Volatile.Read(ref done)!=0;}}
    public ReplayAudioSession(string directory,long run,int level,int layoutSeed,string gameHash,int sampleRate,int channelCount,int blockFloats=8192){
        if(run<=0||sampleRate<8000||sampleRate>192000||channelCount<1||channelCount>8||blockFloats<channelCount||blockFloats>65536||Encoding.UTF8.GetByteCount(gameHash??"")>65)throw new ArgumentException("Invalid audio capture settings");
        id=run;scene=level;seed=layoutSeed;hash=gameHash??"";rate=sampleRate;channels=channelCount;
        temp=Path.Combine(directory,"run-"+id+".isaudio.capture-"+Guid.NewGuid().ToString("N")+".tmp");
        for(int i=0;i<ring.Length;i++)ring[i]=new Block(blockFloats);
        writer=new Thread(Write){IsBackground=true,Name="ItSteals.ReplayAudio"};writer.Start();
    }
    private void Flag(int value){int before,after;do{before=Volatile.Read(ref flags);after=before|value;}while(Interlocked.CompareExchange(ref flags,after,before)!=before);}
    // Main-thread setup only. Each pass-through source owns its SPSC ring;
    // neither source ordering nor a shared multi-producer queue is assumed.
    public ReplayAudioLane AddBypass(){
        var before=lanes;if(before.Length>=64){Flag(4);return null;}
        var lane=new ReplayAudioLane(this,ring[0].Data.Length);var after=new ReplayAudioLane[before.Length+1];Array.Copy(before,after,before.Length);after[before.Length]=lane;lanes=after;return lane;
    }
    internal bool Accept(long first){return !Done&&Volatile.Read(ref discard)==0&&Volatile.Read(ref accepting)!=0&&(Volatile.Read(ref closing)==0||first<target);}
    public void MissingRoute(){Flag(4);}
    internal void Invalid(){Flag(8);}
    internal void Overrun(){Flag(1);}
    public void Push(float[] samples,int countChannels,long firstFrame){
        if(!Accept(firstFrame))return;
        if(samples==null||countChannels!=channels||samples.Length%channels!=0||samples.Length>ring[0].Data.Length){Flag(8);return;}
        int at=head,next=(at+1)%ring.Length;if(next==Volatile.Read(ref tail)){Flag(1);return;}
        var b=ring[at];Array.Copy(samples,b.Data,samples.Length);b.First=firstFrame;b.Count=samples.Length;Interlocked.Increment(ref observed);Volatile.Write(ref head,next);
    }
    public void End(double seconds){if(double.IsNaN(seconds)||double.IsInfinity(seconds)||seconds<0||seconds>315360000)seconds=0;target=(long)Math.Ceiling(seconds*rate);closeDeadline=Stopwatch.GetTimestamp()+Stopwatch.Frequency/5;Volatile.Write(ref closing,1);}
    public void Discard(){Volatile.Write(ref discard,1);if(Volatile.Read(ref closing)==0)End(0);if(Done)DeleteTemp();}
    private void Header(Stream s,long frames,int status=0,int missing=0){s.Position=0;using(var w=new BinaryWriter(s,Encoding.UTF8,true)){w.Write(ReplayAudioFile.Magic);w.Write(1);w.Write(id);w.Write(scene);w.Write(seed);w.Write(rate);w.Write(channels);w.Write(frames);w.Write(status);w.Write(missing);byte[] bytes=Encoding.UTF8.GetBytes(hash);w.Write(bytes.Length);w.Write(bytes);while(s.Position<ReplayAudioFile.HeaderBytes)w.Write((byte)0);}}
    private void Write(){
        try{
            Directory.CreateDirectory(Path.GetDirectoryName(temp));
            using(var s=new FileStream(temp,FileMode.CreateNew,FileAccess.ReadWrite,FileShare.None,65536)){
                Header(s,0);long position=0,max=(ReplayAudioFile.MaxBytes-ReplayAudioFile.HeaderBytes-8)/(channels*4);var bytes=new byte[ring[0].Data.Length*8*4];var mixed=new float[bytes.Length/4];var zero=new byte[65536];
                while(true){
                    if(Volatile.Read(ref discard)!=0)break;
                    MixBypasses(s,position,bytes,mixed,false);
                    int at=tail;if(at==Volatile.Read(ref head)){
                        // Listener and bypass callbacks can arrive in either
                        // order. Let the final DSP blocks reach their own rings.
                        if(Volatile.Read(ref closing)!=0&&Stopwatch.GetTimestamp()>closeDeadline)break;
                        Thread.Sleep(2);continue;
                    }
                    var b=ring[at];long first=Math.Max(0,b.First),limit=Volatile.Read(ref closing)!=0?Math.Min(target,max):max;
                    long end=Math.Min(limit,b.First+b.Count/channels);
                    if(first>position){long missing=Math.Min(first,limit)-position;if(missing>0){Flag(1);Interlocked.Increment(ref gaps);while(missing>0){int n=(int)Math.Min(zero.Length/(channels*4),missing);s.Write(zero,0,n*channels*4);missing-=n;position+=n;}}}
                    int skip=(int)Math.Max(0,position-b.First)*channels;
                    int take=end>position?(int)((end-position)*channels):0;
                    if(take>0&&skip>=0&&skip+take<=b.Count){
                        for(int k=skip;k<skip+take;k++)if(float.IsNaN(b.Data[k])||float.IsInfinity(b.Data[k])){b.Data[k]=0;Flag(8);}
                        Buffer.BlockCopy(b.Data,skip*4,bytes,0,take*4);s.Write(bytes,0,take*4);position+=take/channels;
                    }
                    Volatile.Write(ref tail,(at+1)%ring.Length);
                    if(position>=max){Flag(2);break;}
                }
                Volatile.Write(ref accepting,0);
                long wanted=Volatile.Read(ref closing)!=0?Math.Min(target,max):position;if(target>max)Flag(2);
                if(Volatile.Read(ref discard)==0&&Volatile.Read(ref observed)>0&&wanted>0){
                    if(position<wanted){Flag(1);Interlocked.Increment(ref gaps);long missing=wanted-position;while(missing>0){int n=(int)Math.Min(zero.Length/(channels*4),missing);s.Write(zero,0,n*channels*4);missing-=n;position+=n;}}
                    if(position>wanted){position=wanted;s.SetLength(ReplayAudioFile.HeaderBytes+position*channels*4);}
                    MixBypasses(s,position,bytes,mixed,true);
                    int bypassGaps=0;foreach(var lane in lanes)bypassGaps+=lane.Dropped;
                    int finalFlags=Volatile.Read(ref flags),finalGaps=Volatile.Read(ref gaps)+bypassGaps;
                    Header(s,position,finalFlags,finalGaps);s.Flush();s.Position=0;ulong sum=ReplayAudioFile.Hash(s,s.Length);s.Position=s.Length;using(var w=new BinaryWriter(s,Encoding.UTF8,true)){w.Write(sum);w.Flush();}
                    Info.Rate=rate;Info.Channels=channels;Info.Frames=position;Info.Bytes=s.Length;Info.Checksum=sum;Info.Flags=finalFlags;Info.Gaps=finalGaps;Info.Recorded=true;
                }else{Info.Flags=4;}
            }
        }catch(Exception e){failure=e;Info.Flags=16;}
        finally{Volatile.Write(ref done,1);if(Volatile.Read(ref discard)!=0||!Info.Recorded)DeleteTemp();}
    }
    private void MixBypasses(FileStream file,long through,byte[] bytes,float[] mixed,bool final){
        foreach(var lane in lanes){
            Block block;
            while(lane.Peek(out block)){
                long end=block.First+block.Count/block.Channels;
                if(!final&&end>through)break;
                long first=Math.Max(lane.Through,Math.Max(0,block.First)),last=Math.Min(through,end);
                if(last>first){
                    int frames=(int)(last-first),count=frames*channels,skip=(int)(first-block.First)*block.Channels;
                    file.Position=ReplayAudioFile.HeaderBytes+first*channels*4;
                    int length=count*4,read=0;while(read<length){int n=file.Read(bytes,read,length-read);if(n==0)throw new EndOfStreamException("Bypass mix before listener PCM");read+=n;}
                    Buffer.BlockCopy(bytes,0,mixed,0,length);
                    for(int frame=0;frame<frames;frame++){
                        int at=skip+frame*block.Channels;float left=block.Data[at],right=block.Channels==1?left:block.Data[at+1];
                        if(float.IsNaN(left)||float.IsInfinity(left)||float.IsNaN(right)||float.IsInfinity(right)){left=right=0;Flag(8);}
                        float gain=block.Gain;
                        // Unity's filter normally supplies the stereo source
                        // bus. A raw mono bus uses equal-power center panning.
                        if(channels==1)mixed[frame]+=(left+right)*.5f*gain;
                        else{if(block.Channels==1)gain*=.7071067811865475f;mixed[frame*channels]+=left*gain;mixed[frame*channels+1]+=right*gain;}
                    }
                    for(int k=0;k<count;k++)if(float.IsNaN(mixed[k])||float.IsInfinity(mixed[k])){mixed[k]=0;Flag(8);}
                    Buffer.BlockCopy(mixed,0,bytes,0,length);file.Position=ReplayAudioFile.HeaderBytes+first*channels*4;file.Write(bytes,0,length);
                }
                lane.Through=Math.Max(lane.Through,last);lane.Pop();
            }
        }
        file.Position=ReplayAudioFile.HeaderBytes+through*channels*4;
    }
    private void DeleteTemp(){try{if(File.Exists(temp))File.Delete(temp);}catch(IOException){}catch(UnauthorizedAccessException){}}
    public bool Commit(string destination){
        if(!writer.Join(10000)){Discard();Info.Recorded=false;Info.Frames=Info.Bytes=0;Info.Flags|=16;return false;}
        if(!Info.Recorded||Volatile.Read(ref discard)!=0){DeleteTemp();return false;}
        if(failure!=null)throw new IOException("Audio capture failed",failure);
        if(File.Exists(destination))throw new IOException("Existing recorded audio preserved");
        Directory.CreateDirectory(Path.GetDirectoryName(destination));File.Move(temp,destination);return true;
    }
}

// A single 2D bypass source, after its source filters and before source volume.
// The audio callback only copies; final gain and summation run on the writer.
public sealed class ReplayAudioLane {
    private readonly ReplayAudioSession owner;private readonly ReplayAudioSession.Block[] ring=new ReplayAudioSession.Block[8];
    private int head,tail,dropped;internal long Through;
    internal ReplayAudioLane(ReplayAudioSession session,int capacity){owner=session;for(int i=0;i<ring.Length;i++)ring[i]=new ReplayAudioSession.Block(capacity);}
    public long MemoryBytes{get{return (long)ring.Length*ring[0].Data.Length*4;}}
    public int Dropped{get{return Volatile.Read(ref dropped);}}
    public void Push(float[] samples,int channels,long first,float gain){
        if(!owner.Accept(first))return;
        if(samples==null||channels<1||channels>2||samples.Length%channels!=0||samples.Length>ring[0].Data.Length||float.IsNaN(gain)||float.IsInfinity(gain)||gain<0){owner.Invalid();return;}
        int at=head,next=(at+1)%ring.Length;if(next==Volatile.Read(ref tail)){owner.Overrun();Interlocked.Increment(ref dropped);return;}
        var block=ring[at];Array.Copy(samples,block.Data,samples.Length);block.First=first;block.Count=samples.Length;block.Channels=channels;block.Gain=gain;Volatile.Write(ref head,next);
    }
    internal bool Peek(out ReplayAudioSession.Block block){int at=tail;if(at==Volatile.Read(ref head)){block=null;return false;}block=ring[at];return true;}
    internal void Pop(){Volatile.Write(ref tail,(tail+1)%ring.Length);}
}
