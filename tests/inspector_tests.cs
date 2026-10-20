using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Reflection;
using System.Globalization;
using UnityEngine;

class InspectorTests
{
    enum Mode {Idle=0,Alert=2}
    class Dangerous {public static int Calls;public int Getter {get{Calls++;throw new Exception("getter called");}} public override string ToString(){Calls++;throw new Exception("ToString called");}}
    class Base {public int BaseField=7;}
    class Sample:Base {
#pragma warning disable 0414
        private int secret=42;
#pragma warning restore 0414
        public Mode mode=Mode.Alert;public Dangerous reference=new Dangerous();public int[] values={1,2,3};public static int Static=3;
        public int Getter {get{Dangerous.Calls++;throw new Exception("getter called");}}
    }
    static void Check(bool v,string m){if(!v)throw new Exception(m);}
    static InspectorValue V(int id,string group,string name,string source,string value,uint flags=1){return new InspectorValue{Key=InspectorWire.Key(id,source),Component=id,Group=group,Name=name,Source=source,Value=value,Flags=flags};}
    static readonly InspectorValue[] Values={
        V(101,"Object","Name","GameObject.name","Wall / passage \"A\""),
        V(101,"Object","Active self","GameObject.activeSelf","true"),
        V(101,"Object","Layer","GameObject.layer","10 / Walls"),
        V(101,"Object","Tag","GameObject.tag","Wall"),
        V(101,"Transform","World position","Transform.position","5, 1, 0"),
        V(101,"Transform","World rotation","Transform.eulerAngles","0, 0, 0"),
        V(101,"Transform","World scale","Transform.lossyScale","1, 1, 1"),
        V(301,"BoxCollider #301","Enabled","Collider.enabled","true",5),
        V(301,"BoxCollider #301","Local center","BoxCollider.center","0, 0, 0"),
        V(301,"BoxCollider #301","Local size","BoxCollider.size","2, 2, 8"),
        V(301,"BoxCollider #301","Suite capture decision","SceneGeometry.captureReason","Included in the captured static geometry"),
        V(401,"Renderer","Material 0","Renderer.sharedMaterials[0]","Concrete / Standard"),
        V(401,"Renderer","Bounds size (not collider)","Renderer.bounds.size","2, 2, 8"),
        V(101,"Object","Primary type","Inspector.primaryType","BoxCollider"),V(101,"Object","Active in hierarchy","GameObject.activeInHierarchy","true"),V(101,"Object","View distance","Inspector.viewDistance","3.2 u"),
    };
    static byte[] Catalog(){return InspectorWire.Encode(1,1234,1,5,711,1,638960000000000000L,120.25,14430,w=>{
        InspectorWire.Text(w,"Synthetic Hide & Seek QA scene",512);w.Write(false);w.Write(6);
        foreach(var o in new[]{new InspectorObjectRow{Id=100,Name="Level geometry",Scene=5,Flags=3},new InspectorObjectRow{Id=101,Parent=100,Name="Wall / passage \"A\"",Scene=5,Layer=10,Flags=15},new InspectorObjectRow{Id=102,Parent=100,Name="Wall / passage B",Scene=5,Layer=10,Flags=15},new InspectorObjectRow{Id=103,Parent=101,Name="Surface detail",Scene=5,Flags=11},new InspectorObjectRow{Id=104,Name="Main Camera",Scene=5,Flags=3},new InspectorObjectRow{Id=-105,Parent=100,Name="Culled wall / inactive",Scene=5,Layer=10,Flags=12}})InspectorWire.Object(w,o);
    });}
    static byte[] Detail(bool changed=false,bool active=true,bool endpointChanged=false,int action=2,bool success=true){return InspectorWire.Encode(2,1234,1,5,711,changed?2u:1u,638960000000000000L,changed?121:120.25,changed?14520:14430,w=>{
        w.Write(active);w.Write(12u);w.Write(action);w.Write(success);w.Write(active?101:0);
        InspectorWire.Text(w,active?"Synthetic QA / selected object":"Inspector closed");InspectorWire.Text(w,active?"Wall / passage \"A\"":"",768);InspectorWire.Text(w,"Level geometry[0]/Wall[4]",4096);InspectorWire.Text(w,"Physics ray / actual hit",1024);
        w.Write(false);w.Write(false);w.Write(6);w.Write(.42);w.Write(.08);w.Write(19u);w.Write(0);w.Write(0);w.Write(7u);
        InspectorWire.Vector(w,new Vector3(1,2,3));InspectorWire.Vector(w,new Vector3(10,90,0));w.Write(60f);
        w.Write(active?Values.Length:0);if(active)foreach(var original in Values){var v=new InspectorValue{Key=original.Key,Component=original.Component,Group=original.Group,Name=original.Name,Source=original.Source,Flags=original.Flags,Value=original.Value};if(changed&&v.Source=="Collider.enabled")v.Value="false";InspectorWire.Value(w,v);}
        w.Write(active?2:0);if(active){InspectorWire.Change(w,new InspectorChange{Key=Values[7].Key,Frame=14400,Seconds=120,Before="false",After="true"});InspectorWire.Change(w,new InspectorChange{Key=Values[7].Key,Frame=14424,Seconds=120.2,Before="true",After="false"});}
        w.Write(active?2:0);if(active)foreach(int id in new[]{101,102}){w.Write(id);w.Write(1);w.Write(id==101?3.2f:6.4f);InspectorWire.Text(w,id==101?"Wall / passage A":"Wall / passage B",768);}
        w.Write(active);InspectorWire.Vector(w,new Vector3(endpointChanged?1:0,1,0));w.Write(active);InspectorWire.Vector(w,new Vector3(10,1,0));
        w.Write(active?1:0);if(active){w.Write(301);InspectorWire.Text(w,"BoxCollider",512);w.Write(3u);InspectorWire.Text(w,"Included in the captured static geometry");w.Write(14300);w.Write(119.5);foreach(float f in new[]{.35f,2.02f,0,.4f,2})w.Write(f);w.Write(true);w.Write(false);w.Write(1);w.Write(false);w.Write(3u);w.Write(4);foreach(var v in new[]{new Vector3(4,0,-4),new Vector3(6,0,-4),new Vector3(6,0,4),new Vector3(4,0,4)})InspectorWire.Vector(w,v);}
    });}
    static byte[] Hover(bool active=true,bool layers=false,int preferred=201){
        var h=new InspectorHover{Serial=70,Active=active,Object=active?101:0,Kind=active?1:0,X=.75f,Y=.75f,Distance=active?3.2f:0,Outline=active,Left=.61f,Top=.24f,Right=.81f,Bottom=.85f,Name=active?"Wall / passage A":"",Type=active?"BoxCollider":"",Status=active?"Physics ray / actual hit":"Picker inactive",CameraId=104,Camera="Main Camera / direct",Width=1920,Height=1080,Viewport=new Rect(0,0,1,1)};
        if(active){if(layers){h.Targets.Add(new InspectorPickTarget{Id=202,Kind=4,Distance=3.1f,Name="Graffiti sprite",Type="SpriteRenderer"});h.Targets.Add(new InspectorPickTarget{Id=201,Kind=3,Distance=3.18f,Name="Graffiti / no collider",Type="MeshRenderer"});h.Object=201;h.Kind=3;h.Distance=3.18f;h.Name="Graffiti / no collider";h.Type="MeshRenderer";}h.Targets.Add(new InspectorPickTarget{Id=101,Kind=1,Distance=3.2f,Name="Wall / passage A",Type="BoxCollider"});}
        if(active&&layers){var chosen=h.Targets.Find(t=>t.Id==preferred);h.Object=chosen.Id;h.Kind=chosen.Kind;h.Distance=chosen.Distance;h.Name=chosen.Name;h.Type=chosen.Type;}
        return InspectorWire.Encode(3,1234,1,5,711,1,638960000000000000L,120.25,14430,w=>InspectorHover.Write(w,h));
    }
    static void GeometryTests(){
        float distance;var vertices=new[]{new Vector3(-2,-2,5),new Vector3(2,-2,5),new Vector3(2,2,5),new Vector3(-2,2,5)};var indices=new[]{0,1,2,0,2,3};
        Check(InspectorPicking.RayTriangles(Vector3.zero,Vector3.forward,vertices,indices,out distance)&&Math.Abs(distance-5)<1e-5,"wall triangles");
        var graffiti=vertices.Select(v=>new Vector3(v.x*.25f,v.y*.25f,4.995f)).ToArray();
        Check(InspectorPicking.RayTriangles(Vector3.zero,Vector3.forward,graffiti,indices,out distance)&&Math.Abs(distance-4.995)<1e-5,"collider-free coplanar decal geometry");
        Check(!InspectorPicking.RayTriangles(new Vector3(1.5f,0,0),Vector3.forward,graffiti,indices,out distance),"outside decal, inside wall");
        Check(InspectorPicking.RayBounds(Vector3.zero,Vector3.forward,new Vector3(-.5f,-.5f,5),new Vector3(.5f,.5f,5),out distance)&&distance==5,"zero-thickness visible bounds");
        Check(!InspectorPicking.RayBounds(Vector3.zero,Vector3.right,new Vector3(-.5f,-.5f,5),new Vector3(.5f,.5f,5),out distance),"parallel bounds miss");
        Check(!InspectorPicking.RayTriangles(Vector3.zero,Vector3.forward,vertices,new[]{0,1,999},out distance),"invalid indices rejected");
        Check(!InspectorPicking.RayTriangles(new Vector3(float.NaN,0,0),Vector3.forward,vertices,indices,out distance),"NaN ray rejected");
        // Keep the transformed direction unnormalized: t still denotes world units.
        Check(InspectorPicking.RayTriangles(Vector3.zero,new Vector3(0,0,2),vertices,indices,out distance)&&Math.Abs(distance-2.5f)<1e-5,"nonuniform transform preserves world distance");
        Check(InspectorPicking.RayTriangles(new Vector3(0,0,10),Vector3.back,vertices,indices,out distance)&&distance==5,"double-sided graphics geometry");
        var picks=new System.Collections.Generic.List<InspectorPickHit>{new InspectorPickHit{Id=101,Kind=1,Distance=5},new InspectorPickHit{Id=101,Kind=3,Distance=5.01f},new InspectorPickHit{Id=201,Kind=3,Distance=4.99f},new InspectorPickHit{Id=202,Kind=4,Distance=4.98f},new InspectorPickHit{Id=900,Kind=1,Distance=1}};
        Check(!InspectorPickOrder.Normalize(picks)&&picks.Count==4&&picks[0].Id==202&&picks[1].Id==201&&picks[2].Id==101&&picks[2].Kind==3&&picks[3].Id==900,"visible-first layers / precise duplicate merge");
        Check(InspectorPickOrder.Resolve(picks,101).Kind==3&&InspectorPickOrder.Resolve(picks,999)==null&&InspectorPickOrder.Resolve(picks,0).Id==202,"explicit layer persists / missing target never falls back");
        Check(InspectorPickOrder.Normalize(picks,2)&&picks.Count==2,"bounded layer list");
    }
    public static int Main(string[] args){
        Directory.CreateDirectory(args[0]);GeometryTests();Vector3 viewport;
        Check(InspectorPicking.Viewport(.75f,.75f,new Rect(0,0,1,1),out viewport)&&viewport.x==.75f&&viewport.y==.75f,"full client viewport");
        Check(InspectorPicking.Viewport(.5f,.5f,new Rect(.25f,.25f,.5f,.5f),out viewport)&&viewport.x==.5f&&viewport.y==.5f,"camera sub-viewport mapping");
        Check(!InspectorPicking.Viewport(.1f,.1f,new Rect(.25f,.25f,.5f,.5f),out viewport)&&!InspectorPicking.Viewport(float.NaN,.5f,new Rect(0,0,1,1),out viewport)&&!InspectorPicking.Viewport(.5f,.5f,new Rect(0,0,0,1),out viewport),"invalid/outside viewport rejected");
        Check(InspectorPicking.Matches(70,101,70,101)&&!InspectorPicking.Matches(70,101,71,101)&&!InspectorPicking.Matches(70,101,70,102)&&!InspectorPicking.Matches(0,0,0,0),"commit token / exact target guards");
        var pointer=new InspectorPointer{Serial=70,Epoch=711,Active=true,X=.5f,Y=.75f,Width=1920,Height=1080,Action=3};Check(pointer.Valid,"pointer bounds");pointer.Width=0;Check(!pointer.Valid,"empty viewport rejected");pointer.Width=1920;pointer.X=float.NaN;Check(!pointer.Valid,"pointer NaN rejected");
        File.WriteAllBytes(Path.Combine(args[0],"inspector-hover.bin"),Hover());File.WriteAllBytes(Path.Combine(args[0],"inspector-hover-inactive.bin"),Hover(false));File.WriteAllBytes(Path.Combine(args[0],"inspector-hover-layers.bin"),Hover(true,true));File.WriteAllBytes(Path.Combine(args[0],"inspector-hover-layers-sprite.bin"),Hover(true,true,202));

        Directory.CreateDirectory(args[0]);CultureInfo.CurrentCulture=CultureInfo.GetCultureInfo("de-DE");var sample=new Sample();var fields=InspectorFields.Readable(typeof(Sample),typeof(Sample).Assembly);
        Check(fields.Length==5&&fields.All(f=>!f.IsStatic)&&fields.Any(f=>f.Name=="secret")&&fields.Any(f=>f.Name=="BaseField"),"private and inherited fields, no statics/getters");
        var values=fields.ToDictionary(f=>f.Name,f=>InspectorFields.Describe(f.GetValue(sample)));
        Check(values["secret"]=="42"&&values["mode"]=="Alert (2)"&&values["values"]=="Int32[3] (not expanded)"&&values["reference"]=="Dangerous (not expanded)"&&Dangerous.Calls==0,"no arbitrary getter / ToString invocation");
        Check(InspectorFields.Describe(1.25f)=="1.25"&&InspectorFields.Vector(new Vector3(1.25f,2,3))=="1.25, 2, 3","locale-independent exact primitive representation");
        foreach(int mask in Enumerable.Range(0,32)){bool actual=InspectorPolicy.Permitted((mask&1)!=0,(mask&2)!=0,(mask&4)!=0,(mask&8)!=0,(mask&16)!=0);Check(actual==(mask==3),"independent connection/opt-in/clean Speedrun/replay gates");}
        var command=new InspectorCommand{Action=3,Flags=3,X=.5f,Y=.25f};Check(command.Valid,"valid normalized pick");command.X=float.NaN;Check(!command.Valid,"NaN rejected");command.X=float.PositiveInfinity;Check(!command.Valid,"infinity rejected");command.X=-.01f;Check(!command.Valid,"out-of-viewport rejected");command.X=0;command.Flags=64;Check(!command.Valid,"unknown options rejected");command.Flags=0;command.Action=10;Check(!command.Valid,"unknown action rejected");
        Check(new TrainerCommands{TelemetryOptions=2048}.RequestsInspector&&!new TrainerCommands().RequestsInspector,"main-channel explicit opt-in");
        Check((uint)RunAids.LiveInspector==512,"distinct sticky validation aid");
        using(var m=new MemoryStream())using(var w=new BinaryWriter(m,Encoding.UTF8,true)){InspectorWire.Text(w,new string('é',5000),512);w.Flush();Check(m.Length<=516,"bounded UTF-8 text");}
        File.WriteAllBytes(Path.Combine(args[0],"inspector-catalog.bin"),Catalog());File.WriteAllBytes(Path.Combine(args[0],"inspector-detail.bin"),Detail());File.WriteAllBytes(Path.Combine(args[0],"inspector-changed.bin"),Detail(true));File.WriteAllBytes(Path.Combine(args[0],"inspector-endpoints.bin"),Detail(true,true,true));File.WriteAllBytes(Path.Combine(args[0],"inspector-inactive.bin"),Detail(false,false));File.WriteAllBytes(Path.Combine(args[0],"inspector-focus-ok.bin"),Detail(true,true,false,5,true));File.WriteAllBytes(Path.Combine(args[0],"inspector-focus-failed.bin"),Detail(true,true,false,5,false));
        Console.WriteLine("Inspector: actual SDK compile, collider-free mesh/sprite math, affine distance, thin bounds, layer dedup/order/budgets, safe private/inherited field reads, getter/ToString isolation, culture, independent opt-in/restriction gates, bounded text and C# cross-language fixtures: PASS");return 0;
    }
}
