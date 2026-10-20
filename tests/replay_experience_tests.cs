using System;
using System.IO;
using System.Threading;
using UnityEngine;
public static class ReplayExperienceTests {
    static void Check(bool yes,string message){if(!yes)throw new Exception(message);}
    static void Reject(Action action){bool bad=false;try{action();}catch(IOException){bad=true;}catch(InvalidDataException){bad=true;}Check(bad,"corruption accepted");}
    static byte[] Encode(ReplayMotionData data){using(var s=new MemoryStream()){data.Write(s);return s.ToArray();}}
    public static int Main(string[] args){
        string folder=args[0];Directory.CreateDirectory(folder);var motion=ReplayMotionData.Read(File.ReadAllBytes(args[1]));
        Check(!motion.Experience.Recorded,"legacy channels must remain unavailable");
        var data=motion.Experience;data.Recorded=true;
        int sight=data.Name("HideAndSeekIt.MonsterRayTest / actual query"),state=data.Name("HideAndSeekIt.FoundPlayer"),sound=data.Name("Sound / original clip");
        Check(data.Name("HideAndSeekIt.FoundPlayer")==state,"names deduplicate");
        data.Add(new ReplayProbe{Seconds=0,Actor=1,Site=sight,Mask=-4353,Flags=9,Origin=new Vector3(1,2,3),End=new Vector3(4,5,6),Limit=-1});
        data.Add(new ReplayObservation{Seconds=0,Actor=1,Kind=4,Name=state,Value=0});
        data.Add(new ReplayObservation{Seconds=.01,Actor=1,Kind=1,Name=state,Value=1});
        data.Add(new ReplayObservation{Seconds=.02,Kind=2,Name=sound,Value=.8f});
        var animation=new ReplayAnimationTrack{Group=2,Actor=1,Kind=1,Name="Monster",Path="0:Root"};data.Animations.Add(animation);
        data.Add(animation,new ReplayAnimationFrame{State=1,Clip=sound,Weight=1});data.Add(animation,new ReplayAnimationFrame{Seconds=.01,State=2,Flags=2,Weight=1});
        Check(animation.Break(0,.01)&&!animation.Break(.01,.02),"hard cuts are exact and cannot leak into earlier intervals");
        const int rate=48000;int frames=(int)Math.Ceiling(motion.Seconds*rate);
        string path=Path.Combine(folder,"run-"+motion.Id+".isaudio");
        var session=new ReplayAudioSession(folder,motion.Id,motion.Scene,motion.Seed,motion.Hash,rate,2);
        for(int n=0;n<frames;n+=480){var block=new float[960];for(int k=0;k<480;k++){float value=(n+k)/(float)rate;block[k*2]=value;block[k*2+1]=-value;}session.Push(block,2,n);Thread.Sleep(1);}
        session.End(motion.Seconds);Check(session.Commit(path),"audio companion committed");data.Audio=session.Info;
        var verified=ReplayAudioFile.Verify(path,motion.Id,motion.Scene,motion.Seed,motion.Hash,data.Audio);Check(verified.Flags==0&&verified.Frames==frames,"exact audio duration without invented gaps");
        Reject(()=>ReplayAudioFile.Verify(path,motion.Id+1,motion.Scene,motion.Seed,motion.Hash));
        string badPath=Path.Combine(folder,"corrupt.isaudio");var bytes=File.ReadAllBytes(path);bytes[300]^=1;File.WriteAllBytes(badPath,bytes);Reject(()=>ReplayAudioFile.Verify(badPath,motion.Id,motion.Scene,motion.Seed,motion.Hash));File.Delete(badPath);
        for(int c=0;c<ReplayCoverage.Count;c++){motion.Coverage.Start(c);motion.Coverage.Source(c);for(double at=0;at<motion.Seconds;at+=.01)motion.Coverage.Observe(c,at);motion.Coverage.Observe(c,motion.Seconds);}motion.Coverage.Seal(motion.Seconds);
        bytes=Encode(motion);var clone=ReplayMotionData.Read(bytes);Check(clone.InputFormat==6&&clone.Coverage.Recorded&&clone.Coverage.Spans[0].Through==motion.Seconds&&clone.Experience.Probes.Count==1&&clone.Experience.Events.Count==3&&clone.Experience.Animations[0].Frames[1].State==2&&clone.Experience.Audio.Checksum==verified.Checksum,"format-6 channels and coverage roundtrip");
        File.WriteAllBytes(Path.Combine(folder,"input-v6.isinput"),bytes);File.Copy(args[2],Path.Combine(folder,"input-map.isrun"),true);
        var good=data.Probes[0];var malformed=good;malformed.Flags=16;data.Probes[0]=malformed;Reject(()=>ReplayMotionData.Read(Encode(motion)));data.Probes[0]=good;
        data.Recorded=false;Reject(()=>ReplayMotionData.Read(Encode(motion)));data.Recorded=true;
        using(var cache=new ReplayPcmCache(path,verified)){
            int at=1200;cache.Seek(at);for(int i=0;!cache.Prepare(at)&&i<500;i++)Thread.Sleep(2);Check(cache.Prepare(at),"audio cache prepares off-thread");cache.Speed(1);var block=new float[128];cache.Render(block);Check(Math.Abs(block[0]-at/(float)rate)<1e-6&&Math.Abs(block[126]-(at+63)/(float)rate)<1e-6,"1x audio samples follow absolute run time");cache.Seek(2400);cache.Speed(.5);cache.Render(block);Check(Math.Abs(block[0]-.05f)<1e-6&&Math.Abs(block[126]-2431.5f/rate)<1e-6,"seek and speed use the same sample clock");
        }
        var gap=new ReplayAudioSession(folder,motion.Id+1,motion.Scene,motion.Seed,motion.Hash,rate,2);var gapBlock=new float[960];gapBlock[0]=float.NaN;gap.Push(gapBlock,2,480);gap.End(.02);string gapPath=Path.Combine(folder,"gap.isaudio");Check(gap.Commit(gapPath),"gapped capture saved with evidence");Check((gap.Info.Flags&9)==9&&gap.Info.Gaps>0,"missing samples and invalid floats remain flagged");File.Delete(gapPath);
        var finalizing=new ReplayAudioSession(folder,motion.Id+4,motion.Scene,motion.Seed,motion.Hash,rate,2);var tail=new float[960];finalizing.Push(tail,2,0);finalizing.End(.01);for(int i=0;i<10000;i++)finalizing.Push(tail,2,480+i*480);string finalPath=Path.Combine(folder,"finalizing.isaudio");Check(finalizing.Commit(finalPath),"closing capture committed");var finalInfo=ReplayAudioFile.Verify(finalPath,motion.Id+4,motion.Scene,motion.Seed,motion.Hash,finalizing.Info);Check(finalInfo.Flags==0&&finalInfo.Gaps==0,"post-run callbacks cannot change finalized evidence or invent gaps");File.Delete(finalPath);
        var empty=new ReplayAudioSession(folder,motion.Id+2,motion.Scene,motion.Seed,motion.Hash,rate,2);empty.End(0);Check(!empty.Commit(Path.Combine(folder,"empty.isaudio"))&&!empty.Info.Recorded,"unavailable capture cannot claim recorded audio");
        var discarded=new ReplayAudioSession(folder,motion.Id+3,motion.Scene,motion.Seed,motion.Hash,rate,2);discarded.Push(new float[960],2,0);discarded.Discard();for(int i=0;!discarded.Done&&i<500;i++)Thread.Sleep(2);Check(!discarded.Commit(Path.Combine(folder,"discarded.isaudio")),"discarded run leaves no audio companion");
        // A bypass source may reach its own DSP callback before OR after
        // the listener. Mix only after baseline PCM exists; do not lose it or
        // overwrite it when the listener block arrives later.
        var mix=new ReplayAudioSession(folder,motion.Id+10,motion.Scene,motion.Seed,motion.Hash,rate,2);var left=mix.AddBypass();var right=mix.AddBypass();
        var basis=new float[960];var pickup=new float[960];for(int n=0;n<960;n++){basis[n]=.1f;pickup[n]=n%2==0?.4f:-.2f;}
        left.Push(pickup,2,0,.25f);mix.Push(basis,2,0);Thread.Sleep(15);right.Push(pickup,2,0,.5f);mix.End(.01);
        string mixPath=Path.Combine(folder,"bypass.isaudio");Check(mix.Commit(mixPath),"listener bypass committed");ReplayAudioFile.Verify(mixPath,motion.Id+10,motion.Scene,motion.Seed,motion.Hash,mix.Info);Check(mix.Info.Flags==0&&mix.Info.Gaps==0,"bypass does not invent listener gaps");
        using(var file=new BinaryReader(File.OpenRead(mixPath))){file.BaseStream.Position=ReplayAudioFile.HeaderBytes;for(int n=0;n<960;n++)Check(Math.Abs(file.ReadSingle()-(.1f+(n%2==0?.4f:-.2f)*.75f))<1e-6,"exact source gain and concurrent-source summation");}File.Delete(mixPath);
        var monoMix=new ReplayAudioSession(folder,motion.Id+11,motion.Scene,motion.Seed,motion.Hash,rate,2);var monoLane=monoMix.AddBypass();var monoBlock=new float[480];for(int n=0;n<monoBlock.Length;n++)monoBlock[n]=1;
        monoLane.Push(monoBlock,1,-120,.5f);monoMix.Push(new float[960],2,0);monoMix.End(.01);string monoPath=Path.Combine(folder,"bypass-mono.isaudio");Check(monoMix.Commit(monoPath),"mono pre-roll trimmed");
        using(var file=new BinaryReader(File.OpenRead(monoPath))){file.BaseStream.Position=ReplayAudioFile.HeaderBytes;for(int n=0;n<960;n++)Check(Math.Abs(file.ReadSingle()-(n<720?.35355339f:0))<1e-6,"mono equal-power upmix and exact negative-time trim");}File.Delete(monoPath);
        var partial=new ReplayAudioSession(folder,motion.Id+12,motion.Scene,motion.Seed,motion.Hash,rate,2);partial.MissingRoute();partial.Push(new float[960],2,0);partial.End(.01);string partialPath=Path.Combine(folder,"partial.isaudio");Check(partial.Commit(partialPath)&&partial.Info.Flags==4,"unsupported bypass routes remain explicitly incomplete");File.Delete(partialPath);
        var resync=new ReplayAudioSession(folder,motion.Id+14,motion.Scene,motion.Seed,motion.Hash,rate,2);var resyncLane=resync.AddBypass();resyncLane.Push(pickup,2,0,.25f);resyncLane.Push(pickup,2,0,.25f);resync.Push(basis,2,0);resync.End(.01);string resyncPath=Path.Combine(folder,"bypass-resync.isaudio");Check(resync.Commit(resyncPath),"resynced capture committed");
        using(var file=new BinaryReader(File.OpenRead(resyncPath))){file.BaseStream.Position=ReplayAudioFile.HeaderBytes;for(int n=0;n<960;n++)Check(Math.Abs(file.ReadSingle()-(.1f+(n%2==0?.4f:-.2f)*.25f))<1e-6,"a source block repeated by clock correction is not amplified");}File.Delete(resyncPath);
        var overload=new ReplayAudioSession(folder,motion.Id+15,motion.Scene,motion.Seed,motion.Hash,rate,2);var overloadLane=overload.AddBypass();for(int n=0;n<32;n++)overloadLane.Push(pickup,2,0,.01f);overload.Push(new float[960],2,0);overload.End(.01);string overloadPath=Path.Combine(folder,"bypass-overload.isaudio");Check(overload.Commit(overloadPath)&&(overload.Info.Flags&1)!=0&&overload.Info.Gaps>0,"bounded bypass queue overflow remains visible in immutable metadata");ReplayAudioFile.Verify(overloadPath,motion.Id+15,motion.Scene,motion.Seed,motion.Hash,overload.Info);File.Delete(overloadPath);
        var bounded=new ReplayAudioSession(folder,motion.Id+13,motion.Scene,motion.Seed,motion.Hash,rate,2);for(int n=0;n<64;n++)Check(bounded.AddBypass()!=null,"bounded setup accepts 64 routes");Check(bounded.AddBypass()==null,"extra routes cannot grow memory without bound");bounded.Discard();Check(!bounded.Commit(Path.Combine(folder,"bounded.isaudio")),"discard closes bypass rings");
        File.WriteAllText(Path.Combine(folder,"identity.txt"),motion.Id+"\n"+motion.Scene+"\n"+motion.Seed+"\n"+motion.Hash+"\n"+motion.Seconds.ToString(System.Globalization.CultureInfo.InvariantCulture));
        Console.WriteLine("Experience v6: legacy defaults, actual queries/states, animation cuts, strict parsing, audio identity/checksum, exact samples, seek/rate, gaps, sanitization, bypass ordering/gain/upmix/multi-source mixing, bounded memory and discard: PASS");return 0;
    }
}
