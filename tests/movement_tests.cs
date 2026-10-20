using System;
using System.IO;
using System.Reflection;
using System.Reflection.Emit;
using System.Collections.Generic;
using System.Threading;
using UnityEngine;
using HarmonyLib;
public static class MovementManagedTests
{
    static void Check(bool v,string message){if(!v)throw new Exception(message);}
    static MovementModel Model(int scene=5){return new MovementModel(scene,.02f,.35f,1.8f);}
    static void Step(MovementWindow w,MovementModel m,float angle,float axis=1.41421356f,float rise=0,bool ok=true){float length=.14f*axis;w.Observe(m,new Vector3(length*(float)Math.Cos(angle),rise,length*(float)Math.Sin(angle)),.02,axis,ok);}
    static void RejectArchive(byte[] raw){bool rejected=false;try{MovementCapture.Decode(raw);}catch(InvalidDataException){rejected=true;}catch(IOException){rejected=true;}catch(NotSupportedException){rejected=true;}Check(rejected,"invalid archive accepted");}
    static void Until(Func<bool> done){DateTime end=DateTime.UtcNow.AddSeconds(5);while(!done()){if(DateTime.UtcNow>end)throw new Exception("storage timed out");Thread.Sleep(5);}}
    public static int Main(string[] args){
        var m=Model();var w=new MovementWindow();
        for(int i=0;i<900;i++)Step(w,m,0);
        var s=m.Summary();Check(s.Ready&&s.Counts[0]>100&&s.Counts[4]==1&&Math.Abs(s.Speeds[1]-Math.Sqrt(2)*7)<.026,"original unnormalized fixed-step diagonal speed");
        var axial=Model();w.Reset();for(int i=0;i<180;i++)Step(w,axial,0,1);Check(axial.Summary().Counts[0]==0,"axial seven u/s must not lower optimal diagonal pace");
        var bend=Model();w.Reset();for(int i=0;i<120;i++)Step(w,bend,i*.06f,1.22f);Check(bend.Summary().Counts[1]>10&&bend.Summary().Speeds[4]<s.Speeds[1],"measured bends preserve input smoothing losses");
        var sharp=Model();w.Reset();double integrated=0;for(int i=0;i<120;i++){Step(w,sharp,i*.2f,1.16f);integrated+=.14*1.16;}var q=sharp.Summary();Check(q.Counts[2]>10&&Math.Abs(q.Lengths[2]-.14*1.16*6)<.0001,"tight turns integrate per-tick distance rather than a shortened chord");
        var slope=Model();w.Reset();for(int i=0;i<120;i++)Step(w,slope,0,1.4f,.04f);Check(slope.Summary().Counts[3]>10,"slope class learns 3D travel");
        int before=m.Summary().Counts[0];w.Reset();w.Observe(m,new Vector3(100,0,0),.02,1.4,true);w.Observe(m,new Vector3(.1f,0,0),.02,1.4,false);w.Observe(m,new Vector3(float.NaN,0,0),.02,1.4,true);w.Observe(m,new Vector3(.2f,0,0),double.NaN,1.4,true);w.Observe(m,new Vector3(.001f,0,0),.02,1.4,true);Check(m.Summary().Counts[0]==before&&m.Summary().Rejected>=4,"teleports, collision blocking, NaN and ineligible movement do not train pace");
        w.Reset();for(int n=0;n<30;n++){for(int i=0;i<6;i++)Step(w,m,0,1.35f);w.Observe(m,Vector3.zero,.02,0,true);}Check(m.Summary().Counts[4]>=30,"stops reset acceleration windows");
        for(int i=0;i<60000;i++)m.Add(i%5,8.1+(i%80)/40.0,1);var bounded=m.Summary();for(int k=0;k<5;k++)Check(bounded.Counts[k]<=MovementModel.Maximum,"bounded histogram");
        var all=new List<MovementModel>();foreach(int scene in SpeedrunManager.LevelIndices){var model=Model(scene);for(int i=0;i<300;i++)model.Add(i%5,8.1+i%50*.025,1);all.Add(model);}var raw=MovementCapture.EncodeModels(all);Check(raw.Length<196608&&MovementCapture.Decode(raw).Count==8,"all eight bounded profiles serialize and checksum");
        var copy=MovementCapture.Decode(MovementCapture.EncodeModels(new[]{m}))[5];Check(copy.Summary().Counts[0]==bounded.Counts[0]&&copy.Summary().Speeds[1]==bounded.Speeds[1],"histogram survives exact restart");copy.Merge(m);Check(copy.Summary().Counts[0]<=20000,"compatible restored/live observations merge within bounds");copy.Merge(Model(13));Check(copy.Summary().Scene==5,"mode isolation");
        for(int i=0;i<10;i++){var damaged=(byte[])raw.Clone();damaged[1000+i*137]^=128;RejectArchive(damaged);}RejectArchive(new byte[196609]);RejectArchive(new byte[3]);
        var future=(byte[])raw.Clone();Array.Copy(BitConverter.GetBytes(2),0,future,4,4);RejectArchive(future);
        var il=(List<CodeInstruction>)typeof(ManagedTests).GetMethod("MethodInstructions",BindingFlags.NonPublic|BindingFlags.Static).Invoke(null,new object[]{typeof(MouseLook).GetMethod("FixedUpdate",BindingFlags.Instance|BindingFlags.NonPublic)});
        var getAxis=typeof(Input).GetMethod("GetAxis",new[]{typeof(string)});int original=0,rewritten=0;
        foreach(var i in il)if(i.opcode==OpCodes.Call&&Equals(i.operand,getAxis))original++;
        foreach(var i in MovementCapture.ObserveAxes(il))if(i.opcode==OpCodes.Call&&Equals(i.operand,typeof(MovementCapture).GetMethod("Axis")))rewritten++;
        Check(original==3&&rewritten==3,"observe each original GetAxis call once, keeping game input unchanged");
        Check(typeof(MovementCapture).GetMethod("PhysicsFailed").ReturnType==typeof(void),"finalizer never suppresses original game exceptions");
        var draw=(List<CodeInstruction>)typeof(ManagedTests).GetMethod("MethodInstructions",BindingFlags.NonPublic|BindingFlags.Static).Invoke(null,new object[]{typeof(HitboxDrawer).GetMethod("DrawCollider",BindingFlags.NonPublic|BindingFlags.Instance)});foreach(var i in draw){var call=i.operand as MethodInfo;Check(call==null||call.Name!="get_bounds","approximate bounds must never replace collider shape");}
        string root=Path.Combine(args[0],"movement-managed-697");if(Directory.Exists(root))Directory.Delete(root,true);Directory.CreateDirectory(root);
        using(var store=new RunStore(root)){store.QueueMovement(raw);Until(()=>!store.SavePending&&File.Exists(Path.Combine(root,"movement-v1.bin")));Check(File.ReadAllBytes(Path.Combine(root,"movement-v1.bin")).Length==raw.Length,"existing sleeping writer persists observations");store.QueueMovement(MovementCapture.EncodeModels(new[]{m}));Until(()=>!store.SavePending);Check(File.Exists(Path.Combine(root,"movement-v1.bin.bak"))&&MovementCapture.Decode(File.ReadAllBytes(Path.Combine(root,"movement-v1.bin.bak"))).Count==8,"atomic replacement retains backup");}
        var build=new GameBuildInfo("/workspace/scratch/7083248ce79b/analysis/Managed/Assembly-CSharp.dll","9","2021.2.19f1");Until(()=>build.Supported);var recovery=Path.Combine(root,"recovery");Directory.CreateDirectory(recovery);File.WriteAllBytes(Path.Combine(recovery,"movement-v1.bin"),new byte[]{1,2,3});File.WriteAllBytes(Path.Combine(recovery,"movement-v1.bin.bak"),raw);
        using(var store=new RunStore(recovery)){MovementCapture.Initialize(store,recovery,build);Until(()=> (bool)typeof(MovementCapture).GetField("restoreFinished",BindingFlags.NonPublic|BindingFlags.Static).GetValue(null));Check(MovementCapture.Summary(5)!=null&&MovementCapture.PreservingDamaged,"damaged file restores compatible backup and preserves original");MovementCapture.End();Until(()=>!store.SavePending);Check(Directory.GetFiles(recovery,"*.corrupt-*").Length==1&&MovementCapture.Decode(File.ReadAllBytes(Path.Combine(recovery,"movement-v1.bin"))).Count==8,"first replacement preserves damaged bytes");}
        var futureDir=Path.Combine(root,"future");Directory.CreateDirectory(futureDir);File.WriteAllBytes(Path.Combine(futureDir,"movement-v1.bin"),future);using(var store=new RunStore(futureDir)){MovementCapture.Initialize(store,futureDir,build);Until(()=> (bool)typeof(MovementCapture).GetField("restoreFinished",BindingFlags.NonPublic|BindingFlags.Static).GetValue(null));MovementCapture.End();Check(File.ReadAllBytes(Path.Combine(futureDir,"movement-v1.bin"))[4]==2&&MovementCapture.StorageNote.Contains("newer"),"future format remains read only");}
        var packet=PracticeMapArchive.Decode(File.ReadAllBytes(Path.Combine(args[0],"review-wire.bin")));packet.Hash=GameBuildInfo.SupportedHash;packet.Movement=m.Summary();packet.MovementTick=.02f;packet.MovementSprintStep=.14f;var wire=packet.Encode();var decoded=PracticeMapArchive.Decode(wire);Check(decoded.Movement!=null&&decoded.Movement.Counts[2]==m.Summary().Counts[2],"v11 cumulative summary roundtrip");File.WriteAllBytes(Path.Combine(args[0],"movement-wire.bin"),wire);
        var summaryBytes=new MemoryStream();using(var writer=new BinaryWriter(summaryBytes,System.Text.Encoding.UTF8,true))MovementSummary.Write(writer,m.Summary());summaryBytes.Position=0;using(var reader=new BinaryReader(summaryBytes))Check(MovementSummary.Read(reader).Speeds[1]==m.Summary().Speeds[1],"summary codec");
        Console.WriteLine("Actual fixed-step input/physics IL, five learned classes, chord/teleport/collision gates, bounded histograms, checksum/mode/build isolation, atomic backup recovery, future read-only preservation and v11 wire: PASS");return 0;
    }
}
