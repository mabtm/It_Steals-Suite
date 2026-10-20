using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEngine;

// Production capture, overlay, wire tessellation, orbit and trail math executed
// with host scene/GL adapters. These assertions cover submitted geometry and
// source ownership; they are not a Unity/D3D pixel or frame-rate test.
namespace UnityEngine
{
    public struct Vector2 {public float x,y;public Vector2(float a,float b){x=a;y=b;}}
    public struct Vector3
    {
        public float x,y,z;public Vector3(float a,float b,float c){x=a;y=b;z=c;}
        public static Vector3 up{get{return new Vector3(0,1,0);}}public static Vector3 right{get{return new Vector3(1,0,0);}}public static Vector3 forward{get{return new Vector3(0,0,1);}}
        public float sqrMagnitude{get{return x*x+y*y+z*z;}}public float magnitude{get{return (float)Math.Sqrt(sqrMagnitude);}}
        public Vector3 normalized{get{return magnitude>1e-12f?this/magnitude:new Vector3();}}public void Normalize(){this=normalized;}
        public static Vector3 operator +(Vector3 a,Vector3 b){return new Vector3(a.x+b.x,a.y+b.y,a.z+b.z);}public static Vector3 operator -(Vector3 a,Vector3 b){return new Vector3(a.x-b.x,a.y-b.y,a.z-b.z);}public static Vector3 operator -(Vector3 a){return a*-1;}
        public static Vector3 operator *(Vector3 a,float b){return new Vector3(a.x*b,a.y*b,a.z*b);}public static Vector3 operator /(Vector3 a,float b){return a*(1/b);}
        public static bool operator ==(Vector3 a,Vector3 b){return (a-b).sqrMagnitude<1e-12f;}public static bool operator !=(Vector3 a,Vector3 b){return !(a==b);}public override bool Equals(object o){return o is Vector3&&this==(Vector3)o;}public override int GetHashCode(){return x.GetHashCode()^y.GetHashCode()^z.GetHashCode();}
        public static float Dot(Vector3 a,Vector3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}public static Vector3 Cross(Vector3 a,Vector3 b){return new Vector3(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
    }
    public struct Quaternion
    {
        public float x,y,z,w;public static Quaternion Euler(float pitch,float yaw,float roll){double a=pitch*Math.PI/360,b=yaw*Math.PI/360;return new Quaternion{x=(float)(Math.Sin(a)*Math.Cos(b)),y=(float)(Math.Cos(a)*Math.Sin(b)),z=(float)(-Math.Sin(a)*Math.Sin(b)),w=(float)(Math.Cos(a)*Math.Cos(b))};}
        public static Vector3 operator *(Quaternion q,Vector3 v){var u=new Vector3(q.x,q.y,q.z);return v+Vector3.Cross(u,v)*(2*q.w)+Vector3.Cross(u,Vector3.Cross(u,v))*2;}
    }
    public struct Matrix4x4
    {
        public float m00,m01,m02,m03,m10,m11,m12,m13,m20,m21,m22,m23,m33;
        public static Matrix4x4 identity{get{return new Matrix4x4{m00=1,m11=1,m22=1,m33=1};}}
        public float this[int i,int j]{get{if(i==0)return j==0?m00:j==1?m01:j==2?m02:m03;if(i==1)return j==0?m10:j==1?m11:j==2?m12:m13;return j==0?m20:j==1?m21:j==2?m22:m23;}set{if(i==0){if(j==0)m00=value;else if(j==1)m01=value;else if(j==2)m02=value;else m03=value;}else if(i==1){if(j==0)m10=value;else if(j==1)m11=value;else if(j==2)m12=value;else m13=value;}else{if(j==0)m20=value;else if(j==1)m21=value;else if(j==2)m22=value;else m23=value;}}}
        public Vector3 MultiplyVector(Vector3 v){return new Vector3(m00*v.x+m01*v.y+m02*v.z,m10*v.x+m11*v.y+m12*v.z,m20*v.x+m21*v.y+m22*v.z);}public Vector3 MultiplyPoint3x4(Vector3 v){return MultiplyVector(v)+new Vector3(m03,m13,m23);}
        public static Matrix4x4 Rotate(Quaternion q){var x=q*Vector3.right;var y=q*Vector3.up;var z=q*Vector3.forward;return new Matrix4x4{m00=x.x,m10=x.y,m20=x.z,m01=y.x,m11=y.y,m21=y.z,m02=z.x,m12=z.y,m22=z.z,m33=1};}
    }
    public struct Color
    {
        public float r,g,b,a;public Color(float x,float y,float z,float alpha=1){r=x;g=y;b=z;a=alpha;}public static Color white{get{return new Color(1,1,1);}}public static Color clear{get{return new Color(0,0,0,0);}}
        public static Color Lerp(Color a,Color b,float t){t=Mathf.Clamp01(t);return new Color(a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t,a.a+(b.a-a.a)*t);}
    }
    public static class Mathf
    {
        public const float PI=(float)Math.PI;public static float Sin(float x){return (float)Math.Sin(x);}public static float Cos(float x){return (float)Math.Cos(x);}public static float Abs(float x){return Math.Abs(x);}public static float Max(float a,float b){return Math.Max(a,b);}
        public static float Clamp(float x,float a,float b){return Math.Max(a,Math.Min(b,x));}public static int Clamp(int x,int a,int b){return Math.Max(a,Math.Min(b,x));}public static float Clamp01(float x){return Clamp(x,0,1);}
    }
    public sealed class GameObject {public bool activeInHierarchy=true;public int layer;public string tag="Untagged";public Transform transform;public Collider[] Children=new Collider[0];public T[] GetComponentsInChildren<T>(bool inactive)where T:class{return Children as T[];}}
    public class Component
    {
        private static int next;private readonly int id=++next;public string name="source";public GameObject gameObject=new GameObject();public Transform transform;
        public Collider[] Children=new Collider[0];public int GetInstanceID(){return id;}public T[] GetComponentsInChildren<T>(bool inactive)where T:class{return Children as T[];}
    }
    public class Transform:Component
    {
        public bool CompareTag(string value){return gameObject.tag==value;}
        public Transform parent;public bool IsChildOf(Transform root){for(var p=parent;p!=null;p=p.parent)if(p==root)return true;return false;}
        public Vector3 position,lossyScale=new Vector3(1,1,1);public Quaternion rotation=Quaternion.Euler(0,0,0);public Matrix4x4 localToWorldMatrix=Matrix4x4.identity;
        public Vector3 forward{get{return rotation*Vector3.forward;}}public Vector3 up{get{return rotation*Vector3.up;}}public Vector3 TransformPoint(Vector3 v){return localToWorldMatrix.MultiplyPoint3x4(v);}
    }
    public class Collider:Component {public bool enabled=true,isTrigger;public Bounds bounds;public Collider(){transform=new Transform();}}
    public struct Bounds
    {
        public Vector3 center,size;public void Expand(float v){size+=new Vector3(v,v,v);}public bool Contains(Vector3 p){var d=p-center;return Math.Abs(d.x)<=size.x*.5f&&Math.Abs(d.y)<=size.y*.5f&&Math.Abs(d.z)<=size.z*.5f;}
        public bool Intersects(Bounds b){var d=b.center-center;return Math.Abs(d.x)<=(size.x+b.size.x)*.5f&&Math.Abs(d.y)<=(size.y+b.size.y)*.5f&&Math.Abs(d.z)<=(size.z+b.size.z)*.5f;}
    }
    public sealed class MaterialPropertyBlock
    {
        internal Dictionary<string,Color> Colors=new Dictionary<string,Color>();public void Clear(){Colors.Clear();}public void SetColor(string key,Color c){Colors[key]=c;}
        public MaterialPropertyBlock Copy(){return new MaterialPropertyBlock{Colors=new Dictionary<string,Color>(Colors)};}
    }
    public sealed class Renderer:Component
    {
        public Bounds bounds;public Material[] sharedMaterials;public readonly Dictionary<int,MaterialPropertyBlock> Blocks=new Dictionary<int,MaterialPropertyBlock>();
        public void SetPropertyBlock(MaterialPropertyBlock block,int index){if(block==null)Blocks.Remove(index);else Blocks[index]=block.Copy();}
    }
    public struct RaycastHit {public Collider collider;public Vector3 point,normal;}
    public enum QueryTriggerInteraction{Ignore}
    public static class Physics
    {
        public static int Calls;public static RaycastHit[] Results=new RaycastHit[0];public static bool ReverseOnly;
        public static int RaycastNonAlloc(Vector3 from,Vector3 direction,RaycastHit[] output,float distance,int mask,QueryTriggerInteraction triggers){Calls++;if(ReverseOnly&&from.z<0)return 0;int n=Math.Min(output.Length,Results.Length);Array.Copy(Results,output,n);return n;}
    }
    public sealed class BoxCollider:Collider {public Vector3 center,size=new Vector3(1,1,1);}
    public sealed class SphereCollider:Collider {public Vector3 center;public float radius=.3f;}
    public sealed class CapsuleCollider:Collider {public Vector3 center;public float radius=.3f,height=1.8f;public int direction=1;}
    public sealed class CharacterController:Collider {public Vector3 center;public float radius=.3f,height=1.8f;}
    public class MonoBehaviour:Component {public Camera Host;public T GetComponent<T>()where T:class{return Host as T;}public static void Destroy(object o){}}
    public sealed class Camera:Component {public Matrix4x4 projectionMatrix=Matrix4x4.identity,worldToCameraMatrix=Matrix4x4.identity;public float fieldOfView=65,nearClipPlane=.03f;public int pixelHeight=1080;public Camera(){transform=new Transform();}}
    public enum HideFlags{HideAndDontSave}
    public sealed class Shader
    {
        public string name;public bool isSupported=true;public static readonly Dictionary<string,Shader> Available=new Dictionary<string,Shader>();
        public static Shader Find(string n){Shader s;return Available.TryGetValue(n,out s)?s:null;}
    }
    public sealed class Material
    {
        public bool enableInstancing;public int renderQueue;public static int Destroyed;
        public Dictionary<string,int> Ints=new Dictionary<string,int>();public Dictionary<string,string> Tags=new Dictionary<string,string>();public HashSet<string> Keywords=new HashSet<string>();
        public string name="material";
        public Shader shader;public HideFlags hideFlags;public Color color=Color.white;public static Material Active;public static int Passes;
        public Material(Shader s){shader=s;}public Material(Material m){shader=m.shader;color=m.color;enableInstancing=m.enableInstancing;renderQueue=m.renderQueue;Ints=new Dictionary<string,int>(m.Ints);Tags=new Dictionary<string,string>(m.Tags);Keywords=new HashSet<string>(m.Keywords);}
        public bool HasProperty(string n){return n=="_Color"||shader.name=="Standard"||shader.name=="Hidden/Internal-Colored";}public void SetInt(string n,int v){Ints[n]=v;}public void SetColor(string n,Color c){color=c;}public bool SetPass(int n){Active=this;Passes++;return true;}
        public void SetOverrideTag(string name,string value){Tags[name]=value;}public void EnableKeyword(string value){Keywords.Add(value);}public void DisableKeyword(string value){Keywords.Remove(value);}
    }
    public static class Object{public static void Destroy(Material m){Material.Destroyed++;}}
    public static class GL
    {
        public const int LINES=1,TRIANGLES=4;public static Matrix4x4 modelview;public static int DepthClears,ColorClears,Begins,Ends,Mode;private static Color color;
        public struct VertexRecord{public Vector3 Point;public Color Color;public int Mode;}
        public static readonly List<VertexRecord> Vertices=new List<VertexRecord>();public static void Reset(){Vertices.Clear();DepthClears=ColorClears=Begins=Ends=Material.Passes=0;}
        public static void PushMatrix(){}public static void PopMatrix(){}public static void LoadProjectionMatrix(Matrix4x4 m){}public static void Clear(bool depth,bool colors,Color c){if(depth)DepthClears++;if(colors)ColorClears++;}
        public static void Begin(int mode){Mode=mode;Begins++;}public static void End(){Ends++;}public static void Color(Color c){color=c;}public static void Vertex(Vector3 v){Vertices.Add(new VertexRecord{Point=v,Color=Material.Active.shader.name=="Unlit/Color"?Material.Active.color:color,Mode=Mode});}
    }
}
public sealed class MouseLook:Component{public CharacterController controller;public Transform Body;public List<GameObject> gemsList;}
public sealed class GhostScript:Component{public Collider monsterKillTrigger;}
public static class PlayerContext{public static Transform Body(MouseLook p){return p.Body;}}
public sealed class PracticeMapData{public Vector3[] Points;public int[] Selected;}
public struct ReplayViewFrame{public double Seconds;public Vector3 Body;}
public sealed class ReplayMotionData
{
    public ReplayColliderData Colliders=new ReplayColliderData();public readonly List<ReplayViewFrame> Frames=new List<ReplayViewFrame>();
    public static void Text(BinaryWriter w,string v){byte[] b=System.Text.Encoding.UTF8.GetBytes(v);w.Write(b.Length);w.Write(b);}public static string Text(BinaryReader r,int limit){return System.Text.Encoding.UTF8.GetString(r.ReadBytes(r.ReadInt32()));}
}
public static class ReplayVisualTests
{
    static void Check(bool b,string m){if(!b)throw new Exception(m);}static bool Near(float a,float b){return Math.Abs(a-b)<.0001;}
    static void Call(object target,string name){target.GetType().GetMethod(name,BindingFlags.NonPublic|BindingFlags.Instance).Invoke(target,null);}
    static ReplayColliderTrack Track(int group,int kind){var t=new ReplayColliderTrack{Group=group,Kind=kind};for(int i=0;i<4;i++)t.Samples.Add(new ReplayColliderFrame{Seconds=i*.1,State=3,Shape=new ReplayColliderShape{Size=new Vector3(1,1,1),Radius=.3f,Height=1.8f,Direction=1,Matrix=Matrix4x4.identity}});return t;}
    public static int Main()
    {
        var cc=new CharacterController();var pickup=new SphereCollider{isTrigger=true};var body=new Transform{Children=new Collider[]{cc}};
        var crystal=new BoxCollider{isTrigger=true,size=new Vector3(.2f,.4f,.2f)};
        var player=new MouseLook{controller=cc,Body=body,Children=new Collider[]{cc,pickup},gemsList=new List<GameObject>{new GameObject{transform=new Transform{position=new Vector3(.3f,0,0)},Children=new Collider[]{crystal}}}};var captured=new ReplayMotionData();var map=new PracticeMapData{Points=new[]{new Vector3()},Selected=new[]{0}};
        ReplayColliderCapture.Begin(captured,player,map);Check(captured.Colliders.Recorded&&captured.Colliders.Tracks.Count==3&&captured.Colliders.Count==3&&captured.Colliders.Tracks[2].Group==3,"controller, sibling pickup and selected crystal discovered exactly once");
        var monster=new Component();var actorBody=new Transform{Children=new Collider[]{new CapsuleCollider()}};ReplayColliderCapture.BindActor(monster,actorBody,7);ReplayColliderCapture.Camera(.1);
        Check(captured.Colliders.Tracks.Count==4&&captured.Colliders.Tracks[3].Owner==7&&captured.Colliders.Tracks[3].Samples.Count==1,"separate agent body colliders retain actual monster identity");
        cc.height=1.1f;ReplayColliderCapture.Physics(.2);Check(Near(captured.Colliders.Tracks[0].Samples[1].Shape.Height,1.1f),"actual crouch dimensions sampled");cc.enabled=false;ReplayColliderCapture.Physics(.3);ReplayColliderFrame shape;Check(!captured.Colliders.Tracks[0].At(.3,out shape),"disabled physics shape not fabricated as enabled");ReplayColliderCapture.Finish(.4);
        Check(captured.Colliders.Tracks.Count==4&&captured.Colliders.Count>4,"finish keeps recorded sources after releasing capture ownership");
        var orbit=new ReplayFollowOrbit();Vector3 p0,p1;Quaternion q0,q1;var original=Quaternion.Euler(5,35,0);var turn=Quaternion.Euler(-30,190,0);var pos=new Vector3(2,1,3);orbit.Pose(pos,original,out p0,out q0);orbit.Pose(pos,turn,out p1,out q1);
        Check((p0-p1).magnitude<.0001,"recorded player look does not move following orbit");orbit.Rotate(25,10);orbit.Pose(pos,turn,out p1,out q1);Check((p1-p0).magnitude>1&&Near((p1-pos-Vector3.up*1.2f).magnitude,4.3f),"right-drag independently orbits with fixed configured radius");
        var seek=new Vector3(90,3,-50);orbit.Pose(seek,original,out p0,out q0);Check(((p0-seek)-(p1-pos)).magnitude<.0001,"arbitrary seeks/loops preserve orbit");orbit.Rotate(0,-10000);orbit.Pose(pos,original,out p0,out q0);Check((q0*Vector3.forward).y>-.99f,"orbit cannot flip across a pole");orbit.Rotate(0,1);orbit.Pose(pos,original,out p1,out q1);Check((p0-p1).magnitude>.01,"reverse drag responds immediately at angle limit");orbit.Reset();orbit.Pose(pos,turn,out p0,out q0);orbit.Align(original);orbit.Pose(pos,turn,out p1,out q1);Check((p0-p1).magnitude>1,"explicit align and new-session reset use recorded heading once");
        float radius=orbit.Radius;orbit.Zoom(1);Check(orbit.Radius<radius,"wheel up zooms in");orbit.Zoom(-1);Check(Near(orbit.Radius,radius),"wheel down reverses zoom without drift");orbit.Zoom(float.NaN);Check(Near(orbit.Radius,radius),"invalid wheel input ignored");for(int i=0;i<200;i++)orbit.Zoom(5);Check(orbit.Radius==.75f,"near zoom bounded");for(int i=0;i<200;i++)orbit.Zoom(-5);Check(orbit.Radius==20,"far zoom bounded");orbit.Pose(pos,original,out p0,out q0);orbit.Pose(seek,turn,out p1,out q1);Check(Near((p1-seek-Vector3.up*1.2f).magnitude,20)&&((p0-pos)-(p1-seek)).magnitude<.0001,"zoom and orbit survive seeks and recorded turns");
        Vector3 ra=new Vector3(0,0,-1),rb=new Vector3(0,1,2),sa,sb;Check(ReplayWireVisual.Ribbon(ref ra,ref rb,new Vector3(),Vector3.forward,Vector3.up,.03f,65,1080,2.5f,out sa,out sb)&&ra.z>=.02999f&&sa.magnitude>0,"player wire clips near plane without moving its source shape");ra=new Vector3(0,0,-2);rb=new Vector3(1,1,-1);Check(!ReplayWireVisual.Ribbon(ref ra,ref rb,new Vector3(),Vector3.forward,Vector3.up,.03f,65,1080,2.5f,out sa,out sb),"wire behind camera is discarded");
        var red=ReplayTrailVisual.SpeedColor(0);var turquoise=ReplayTrailVisual.SpeedColor(10);Check(red.r>.3&&red.g<.03&&turquoise.g>.9&&turquoise.b>.8,"speed scale has requested dark red / turquoise endpoints");
        for(int i=1;i<=10000;i++){var a=ReplayTrailVisual.SpeedColor((i-1)/1000f);var b=ReplayTrailVisual.SpeedColor(i/1000f);Check(Math.Abs(a.r-b.r)<.002&&Math.Abs(a.g-b.g)<.002&&Math.Abs(a.b-b.b)<.002,"no color steps at gradient stops");}
        var clamped=ReplayTrailVisual.SpeedColor(50);Check(Near(clamped.r,turquoise.r)&&Near(clamped.g,turquoise.g),"10+ u/s remains turquoise");Check(Near(ReplayTrailVisual.HalfWidth(8,65,1080),ReplayTrailVisual.HalfWidth(8,65,2160)),"viewport scaling retains world width at 1080p / 4K");
        var motion=new ReplayMotionData();motion.Colliders.Recorded=true;for(int group=1;group<=3;group++)motion.Colliders.Tracks.Add(Track(group,group));
        for(int i=0;i<=30;i++)motion.Frames.Add(new ReplayViewFrame{Seconds=i*.01,Body=new Vector3(i*.08f,0,0)});
        int coloredTriangles=0;foreach(bool fallback in new[]{false,true}){
            Shader.Available.Clear();Shader.Available["Hidden/Internal-Colored"]=new Shader{name="Hidden/Internal-Colored",isSupported=!fallback};Shader.Available["Unlit/Color"]=new Shader{name="Unlit/Color"};
            var overlay=new Replay3DOverlay{Host=new Camera(),Motion=motion,Options=8|16|32|128|512,Seconds=.3};overlay.Host.transform.position=new Vector3(0,3,-8);Call(overlay,"Awake");Check(overlay.Ready&&overlay.ShaderName==(fallback?"Unlit/Color":"Hidden/Internal-Colored"),"unsupported internal shader falls back to shipped unlit shader");
            GL.Reset();Call(overlay,"OnPostRender");Check(overlay.VisibleColliders==3&&GL.DepthClears==1&&GL.ColorClears==0&&GL.Begins==GL.Ends,"all three actual collider categories submit geometry and xray clears only replay depth");
            int triangles=0;bool playerColor=false,crystalColor=false;foreach(var v in GL.Vertices){if(v.Mode==GL.TRIANGLES)triangles++;if(v.Mode==GL.TRIANGLES&&v.Color.r>.85&&v.Color.b>.9)playerColor=true;if(v.Mode==GL.LINES&&v.Color.g>.9&&v.Color.b>.7)crystalColor=true;Check(!float.IsNaN(v.Point.x)&&!float.IsInfinity(v.Point.y),"finite submitted geometry");}
            Check(triangles>100&&triangles<=360*12+4096*12&&playerColor&&crystalColor&&Material.Passes<=37,"thick bounded triangle ribbon and outlined player wire survive shader fallback");
            if(!fallback)coloredTriangles=triangles;else Check(triangles==coloredTriangles*2,"fixed-cull unlit fallback submits both windings for trail and player strokes");
            overlay.Options=Replay3DOptions.Monster;GL.Reset();Call(overlay,"OnPostRender");Check(overlay.VisibleColliders==1&&GL.DepthClears==0&&GL.Vertices.Count>0,"independent category toggle and normal depth path");overlay.Options=0;GL.Reset();Call(overlay,"OnPostRender");Check(GL.Vertices.Count==0&&GL.DepthClears==0,"disabled overlay performs no render work");
            overlay.Options=Replay3DOptions.Trail;overlay.Motion=new ReplayMotionData();overlay.Motion.Frames.Add(new ReplayViewFrame{Seconds=0});overlay.Motion.Frames.Add(new ReplayViewFrame{Seconds=.5,Body=new Vector3(1,0,0)});overlay.Seconds=.5;GL.Reset();Call(overlay,"OnPostRender");Check(GL.Vertices.Count==0,"camera gaps never receive a trail ribbon");Call(overlay,"OnDestroy");
        }
        VisibilityCases();
        Console.WriteLine("Production replay visual adapters: sources, actual shapes/states, independent orbit and wheel zoom, near-plane clipped outlined player wire, 10000 continuous gradient steps, bounded GL geometry, shader fallback, ceiling/wall visibility and toggles: PASS");return 0;
    }
    static void VisibilityCases()
    {
        var shader=new Shader{name="Standard"};var normal=new Material[20];var transparent=new Material[20];for(int i=0;i<20;i++){normal[i]=new Material(shader){color=new Color(.4f,.5f,.6f)};transparent[i]=ReplayFadeMaterial.Create(shader,normal[i].color);}var hidden=ReplayFadeMaterial.Create(shader,Color.clear);
        Check(!transparent[0].enableInstancing&&transparent[0].Tags["RenderType"]=="Transparent"&&transparent[0].renderQueue==3000&&transparent[0].Ints["_SrcBlend"]==5&&transparent[0].Ints["_DstBlend"]==10&&transparent[0].Ints["_ZWrite"]==0&&transparent[0].Keywords.Contains("_ALPHABLEND_ON")&&!transparent[0].Keywords.Contains("INSTANCING_ON"),"real fade factory selects the supplied non-instanced alpha variant and complete transparent render state");
        var t=new Transform();var bounds=new Bounds{center=new Vector3(0,1.2f,0),size=new Vector3(4,3,.25f)};var collider=new BoxCollider{transform=t,bounds=bounds};collider.gameObject.layer=10;
        var mixed=new Renderer{transform=t,bounds=bounds,sharedMaterials=new[]{normal[0],normal[8]}};var unrelated=new Renderer{transform=new Transform(),bounds=bounds,sharedMaterials=new[]{normal[0]}};var floor=new Renderer{transform=t,bounds=bounds,sharedMaterials=new[]{normal[4]}};
        var v=new ReplaySceneVisibility(new[]{collider},10);v.Add(mixed,mixed.sharedMaterials,new[]{0,8},transparent,hidden);v.Add(unrelated,unrelated.sharedMaterials,new[]{0},transparent,hidden);v.Add(floor,floor.sharedMaterials,new[]{4},transparent,hidden);
        Physics.Calls=0;Physics.ReverseOnly=true;Physics.Results=new[]{new RaycastHit{collider=collider,point=bounds.center,normal=Vector3.forward}};uint options=Replay3DOptions.Follow|Replay3DOptions.FadeWalls|Replay3DOptions.HideCeiling;var camera=new Vector3(0,1.2f,-2);var focus=new Vector3(0,1.2f,2);
        v.Update(0,.05f,camera,focus,options);var owned=mixed.sharedMaterials[0];Check(Physics.Calls==2&&!ReferenceEquals(owned,transparent[0])&&ReferenceEquals(mixed.sharedMaterials[1],hidden)&&Near(owned.color.a,.7f)&&mixed.Blocks.Count==0,"reverse hit uses renderer-owned alpha material; no indexed property block or instancing variant dependency");
        Check(Near(transparent[0].color.a,1)&&Near(normal[0].color.a,1)&&!ReferenceEquals(unrelated.sharedMaterials[0],owned)&&ReferenceEquals(unrelated.sharedMaterials[0],normal[0])&&ReferenceEquals(floor.sharedMaterials[0],normal[4]),"shared prototypes, unrelated objects and floor remain fully opaque");
        int savedQueries=v.Queries,savedRevision=v.Revision;string savedStatus=v.Status;
        using(v.Preserve()){v.Update(40,0,camera,focus,options,true);Check(Near(owned.color.a,.18f),"offscreen capture reaches deterministic wall alpha without advancing time");v.Update(40,0,camera,focus,Replay3DOptions.Free,true);Check(Near(owned.color.a,1)&&ReferenceEquals(mixed.sharedMaterials[1],normal[8]),"capture uses its own ceiling/camera options immediately");}
        Check(Near(owned.color.a,.7f)&&ReferenceEquals(mixed.sharedMaterials[0],owned)&&ReferenceEquals(mixed.sharedMaterials[1],hidden)&&v.Queries==savedQueries&&v.Revision==savedRevision&&v.Status==savedStatus,"capture restores exact live material slots, partial fade, counters and query clock");Physics.Calls=2;
        v.Update(.001,.001f,camera,focus,options);Check(Physics.Calls==2,"occlusion query remains bounded to 20 Hz");v.Update(.06f,.1f,camera,focus,options);Check(Near(owned.color.a,.18f),"real material color reaches target alpha");
        int calls=Physics.Calls;for(int i=0;i<5;i++)v.Update(.2+i*.1,.1f,camera,focus,Replay3DOptions.Free|Replay3DOptions.FadeWalls);Check(Physics.Calls==calls&&ReferenceEquals(mixed.sharedMaterials[0],normal[0])&&ReferenceEquals(mixed.sharedMaterials[1],normal[8])&&Near(owned.color.a,1),"free/recorded camera disables queries and restores opaque material slots");
        v.Update(1,.1f,camera,focus,options);int dead=Material.Destroyed;v.Dispose();int destroyed=Material.Destroyed;v.Dispose();Check(destroyed>dead&&Material.Destroyed==destroyed&&mixed.sharedMaterials.Length==2&&ReferenceEquals(mixed.sharedMaterials[0],normal[0])&&ReferenceEquals(mixed.sharedMaterials[1],normal[8]),"owned fade materials are released once, source material slots restored");
        LayerAndFloorCases(normal,transparent,hidden,bounds,camera,focus,options);
        Console.WriteLine("Replay visibility: non-instanced shader state, actual owned-material alpha, physical normals, floor/prop protection, 20 Hz, reversed hits, original scene slots and idempotent material cleanup: PASS");
    }
    static void LayerAndFloorCases(Material[] normal,Material[] transparent,Material hidden,Bounds bounds,Vector3 camera,Vector3 focus,uint options)
    {
        int gameMask=~((1<<5)|(1<<12)|(1<<13)|(1<<16)|(1<<24));for(int layer=0;layer<32;layer++)Check(ReplaySceneVisibility.Included(gameMask,layer)==((gameMask&(1<<layer))!=0),"replay honors original camera visibility for all 32 layers");Check(!ReplaySceneVisibility.Included(gameMask,-1)&&!ReplaySceneVisibility.Included(gameMask,32),"invalid layers cannot alias existing bits");
        foreach(int radar in new[]{12,13,24})Check(!ReplaySceneVisibility.Included(gameMask,radar),"radar-only duplicate geometry cannot become an opaque overlay in replay");
        var actualHead=new Vector3(20,1.55f,30);var orbit=new ReplayFollowOrbit();Vector3 position;Quaternion rotation;orbit.PoseFocus(actualHead,Quaternion.Euler(0,0,0),out position,out rotation);Check(Near((position-actualHead).magnitude,orbit.Radius)&&((actualHead-position).normalized-rotation*Vector3.forward).magnitude<.0001,"orbit and occlusion share actual recorded camera focus rather than guessed body height");
        const int walls=10;Physics.ReverseOnly=false;var wall=new BoxCollider{bounds=bounds};wall.gameObject.layer=walls;
        var floor=new Renderer{transform=new Transform(),name="Wall",bounds=new Bounds{center=new Vector3(0,-.05f,0),size=new Vector3(100,.1f,100)},sharedMaterials=new[]{normal[0]}};floor.gameObject.layer=walls;
        Check(ReplaySceneVisibility.FallbackRole(floor,walls,1.6f)==1,"7.3.3 regression: shared Wall material/layer cannot override a horizontal floor");
        var clock=new Renderer{transform=new Transform(),name="Wall",bounds=bounds,sharedMaterials=new[]{normal[0]}};clock.transform.parent=new Transform();clock.transform.parent.gameObject.layer=18;
        using(var v=new ReplaySceneVisibility(new[]{wall},walls,1<<18,1<<9,1<<26,0,2,1.6f)){
            Check(v.ProtectedRole(floor)==1&&v.ProtectedRole(clock)==3,"actual floor height and Clock ancestor layer take precedence over material hints");v.Add(floor,floor.sharedMaterials,new[]{0},transparent,hidden,true);v.Add(clock,clock.sharedMaterials,new[]{0},transparent,hidden,true);
            var visual=new Renderer{transform=new Transform(),name="Wall",bounds=bounds,sharedMaterials=new[]{normal[0]}};visual.gameObject.layer=walls;v.Add(visual,visual.sharedMaterials,new[]{0},transparent,hidden,true);
            Physics.Results=new[]{new RaycastHit{collider=wall,point=bounds.center,normal=Vector3.up}};v.Update(0,.1f,camera,focus,options);Check(ReferenceEquals(visual.sharedMaterials[0],normal[0]),"floor/top/ceiling normals do not trigger wall fading");
            Physics.Results=new[]{new RaycastHit{collider=wall,point=bounds.center,normal=new Vector3(float.NaN,0,0)}};v.Update(.1,.1f,camera,focus,options);Check(ReferenceEquals(visual.sharedMaterials[0],normal[0]),"unknown / nonfinite physical normals cannot trigger guessed fades");
            Physics.Results=new[]{new RaycastHit{collider=wall,point=bounds.center,normal=Vector3.forward}};v.Update(.2,.1f,camera,focus,options);Check(visual.sharedMaterials[0].color.a<1&&Near(floor.sharedMaterials[0].color.a,1)&&Near(clock.sharedMaterials[0].color.a,1),"confirmed wall hit fades main wall; same-material floor and white clock stay opaque");
            v.Update(1.3,.1f,camera,focus,options);Check(v.Candidates==1&&v.Hits==2&&v.Matches==2&&v.Fading==1&&v.Status.Contains("walls 1, hits 2, matched 2, faded 1"),"runtime diagnostic counters distinguish selection, physical hits, bindings and material fades");
        }
        for(int mode=0;mode<4;mode++){
            var collider=new BoxCollider{bounds=bounds};var renderer=new Renderer{transform=new Transform(),bounds=bounds,sharedMaterials=new[]{normal[0]}};
            if(mode==0){collider.gameObject.layer=walls;renderer.gameObject.layer=walls;}
            if(mode==1){collider.transform.parent=new Transform();collider.transform.parent.gameObject.tag="Wall";renderer.transform.parent=new Transform();renderer.transform.parent.gameObject.tag="Wall";}
            if(mode==2){collider.gameObject.layer=walls;renderer.sharedMaterials=new[]{new Material(normal[0].shader){name="CementWall"}};}
            if(mode==3){collider.gameObject.layer=walls;renderer.name="WallOvergrown";}
            using(var v=new ReplaySceneVisibility(new[]{collider},walls)){
                bool identified=ReplaySceneVisibility.IsWallVisual(renderer,walls);renderer.sharedMaterials=new[]{normal[0]};v.Add(renderer,renderer.sharedMaterials,new[]{0},transparent,hidden,identified);
                Physics.Results=new RaycastHit[0];v.Update(0,.1f,camera,focus,options);Check(ReferenceEquals(renderer.sharedMaterials[0],normal[0]),"bounds overlap alone never triggers fade");
                Physics.Results=new[]{new RaycastHit{collider=collider,point=new Vector3(100,100,100),normal=Vector3.forward}};v.Update(.1,.1f,camera,focus,options);Check(ReferenceEquals(renderer.sharedMaterials[0],normal[0]),"out-of-renderer physical hit cannot trigger fade");
                Physics.Results=new[]{new RaycastHit{collider=collider,point=bounds.center,normal=Vector3.forward}};v.Update(.2,.1f,camera,focus,options);Check(!ReferenceEquals(renderer.sharedMaterials[0],transparent[0])&&renderer.sharedMaterials[0].color.a<1,"separate main wall roots retain source identity and direct material fade");
            }
        }
        var propCollider=new BoxCollider{bounds=bounds};propCollider.gameObject.layer=18;var propDraw=new Renderer{transform=propCollider.transform,bounds=bounds,sharedMaterials=new[]{normal[0]}};
        using(var v=new ReplaySceneVisibility(new[]{propCollider},walls,1<<18)){v.Add(propDraw,propDraw.sharedMaterials,new[]{0},transparent,hidden);Physics.Results=new[]{new RaycastHit{collider=propCollider,point=bounds.center,normal=Vector3.forward}};v.Update(0,.1f,camera,focus,options);Check(ReferenceEquals(propDraw.sharedMaterials[0],normal[0]),"prop collider is excluded even if its renderer received an incorrect wall hint");}
        Console.WriteLine("Replay policies: original camera mask / radar layers, exact recorded head focus, floor-height / layer precedence, Clock/prop ancestors, hit-normal rejection, source identity and measured diagnostics: PASS");
    }
}
