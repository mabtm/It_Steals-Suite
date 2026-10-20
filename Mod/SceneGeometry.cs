using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.SceneManagement;

public sealed class WallOutline
{
    public Vector3[] Corners;public bool Approximate;
}
// Shared by the seed map and the live minimap. Hide & Seek disables distant
// wall objects; disabled render visibility must not erase the map geometry.
public static class SceneGeometry
{
    private sealed class Entry {public Collider Collider;public WallOutline Outline;public Rect Bounds;public bool Wall,Obstacle;}
    private static int scene=-1;
    private static readonly List<Entry> entries=new List<Entry>();
    private static readonly Dictionary<int,GeometryInspection> inspection=new Dictionary<int,GeometryInspection>();
    public static uint CaptureRevision {get;private set;}
    public static GeometryInspection Inspect(Collider c){GeometryInspection v;return c!=null&&inspection.TryGetValue(c.GetInstanceID(),out v)?v:null;}
    public static void Reset(){scene=-1;entries.Clear();parts=new MapFootprint[0];omitted=0;inspection.Clear();unchecked{CaptureRevision++;}}
    public static Bounds WorldBounds(Matrix4x4 m,Bounds local)
    {
        var lo=local.min;var hi=local.max;var b=new Bounds(m.MultiplyPoint3x4(lo),Vector3.zero);
        for(int k=1;k<8;k++)b.Encapsulate(m.MultiplyPoint3x4(new Vector3((k&1)==0?lo.x:hi.x,(k&2)==0?lo.y:hi.y,(k&4)==0?lo.z:hi.z)));
        return b;
    }
    public static bool IntersectsPlayerHeight(Bounds bounds,float floor,float height,float step=0){return bounds.max.y>floor+Mathf.Max(.12f,step+.05f)&&bounds.min.y<floor+height+.2f;}
    public static WallOutline ProjectBox(Matrix4x4 matrix,Vector3 center,Vector3 size)
    {
        var x=matrix.MultiplyVector(Vector3.right);var z=matrix.MultiplyVector(Vector3.forward);
        bool approximate=Math.Abs(x.y)>.001f||Math.Abs(z.y)>.001f||Math.Abs(Vector3.Dot(x.normalized,z.normalized))>.001f;
        if(approximate){var b=WorldBounds(matrix,new Bounds(center,size));return new WallOutline{Approximate=true,Corners=new[]{new Vector3(b.min.x,0,b.min.z),new Vector3(b.max.x,0,b.min.z),new Vector3(b.max.x,0,b.max.z),new Vector3(b.min.x,0,b.max.z)}};}
        var h=size*.5f;return new WallOutline{Corners=new[]{matrix.MultiplyPoint3x4(center+new Vector3(-h.x,0,-h.z)),matrix.MultiplyPoint3x4(center+new Vector3(h.x,0,-h.z)),matrix.MultiplyPoint3x4(center+new Vector3(h.x,0,h.z)),matrix.MultiplyPoint3x4(center+new Vector3(-h.x,0,h.z))}};
    }
    private static bool SceneObject(Collider c,HashSet<int> culled){
        if(c.gameObject.activeInHierarchy)return true;
        for(var t=c.transform;t!=null;t=t.parent)if(culled.Contains(t.gameObject.GetInstanceID()))return true;
        return false;
    }
    private static MapFootprint[] parts=new MapFootprint[0];private static int omitted;
    public static void Capture(MouseLook player,PracticeMapData data=null,bool force=false)
    {
        int handle=SceneManager.GetActiveScene().handle;
        if(force||scene!=handle){
            entries.Clear();parts=new MapFootprint[0];omitted=0;scene=-1;inspection.Clear();
            var cc=player==null?null:player.controller;if(cc==null)return;
            var profile=new PracticeMapData();PlayerProfile(player,profile);
            float low=profile.AgentFloor+Mathf.Max(.12f,profile.AgentStep+.05f),high=profile.AgentFloor+profile.AgentHeight+.02f;
            var culled=new HashSet<int>();foreach(var actor in UnityEngine.Object.FindObjectsOfType<HideAndSeekIt>())foreach(var obj in ReplayGamePause.CulledGeometry(actor))if(obj.scene.handle==handle)culled.Add(obj.GetInstanceID());
            var output=new List<MapFootprint>();int pointCount=0,layer=LayerMask.NameToLayer("Walls");var timer=player.TimerMode?UnityEngine.Object.FindObjectOfType<Monster3>():null;
            int captureFrame=Time.frameCount;double captureAt=Time.realtimeSinceStartupAsDouble;bool captureGrounded=cc.isGrounded;
            foreach(var root in SceneManager.GetActiveScene().GetRootGameObjects())foreach(var collider in root.GetComponentsInChildren<Collider>(true)){
                var trace=new GeometryInspection{Collider=collider.GetInstanceID(),Frame=captureFrame,Seconds=captureAt,Low=low,High=high,Floor=profile.AgentFloor,Height=profile.AgentHeight,Radius=profile.AgentRadius,Grounded=captureGrounded};
                if(inspection.Count<30000)inspection[trace.Collider]=trace;
                if(collider.isTrigger){trace.Reason="Trigger: not a static movement wall";continue;}
                if(!collider.enabled){trace.Reason="Collider disabled at capture";continue;}
                if(collider is CharacterController){trace.Reason="CharacterController: actor geometry";continue;}
                if(!SceneObject(collider,culled)){trace.Reason="Inactive and absent from the accepted culling catalog";continue;}
                trace.InactiveAccepted=!collider.gameObject.activeInHierarchy;
                if(timer!=null&&timer.cieling!=null&&(collider.transform==timer.cieling.transform||collider.transform.IsChildOf(timer.cieling.transform))){trace.Reason="Moving Timer ceiling";continue;}
                bool objective=false;for(var t=collider.transform;t!=null;t=t.parent)if(t.CompareTag("Point")){objective=true;break;}
                if(objective){trace.Reason="Objective: disappears after collection";continue;}
                if(collider.GetComponentInParent<WallAI>(true)!=null||collider.GetComponentInParent<UnityEngine.AI.NavMeshAgent>(true)!=null||collider.GetComponentInParent<HideAndSeekIt>(true)!=null||collider.transform==cc.transform||collider.transform.IsChildOf(cc.transform)){trace.Reason="Moving actor or player subtree";continue;}
                bool wall=layer>=0&&collider.gameObject.layer==layer;for(var t=collider.transform;!wall&&t!=null;t=t.parent)wall=t.CompareTag("Wall");
                bool obstacle=(collider.attachedRigidbody==null||collider.attachedRigidbody.isKinematic)&&!Physics.GetIgnoreLayerCollision(cc.gameObject.layer,collider.gameObject.layer)&&!Physics.GetIgnoreCollision(cc,collider);
                if(!wall&&!obstacle){trace.Reason="Neither wall appearance nor static player collision";continue;}
                var polygons=new List<Vector3[]>();bool approximate=false;
                var box=collider as BoxCollider;var mesh=collider as MeshCollider;var sphere=collider as SphereCollider;var capsule=collider as CapsuleCollider;
                if(box!=null)polygons.Add(MapPhysicalData.Box(box.transform.localToWorldMatrix,box.center,box.size,low,high));
                else if(mesh!=null&&mesh.sharedMesh!=null){
                    var bounds=WorldBounds(mesh.transform.localToWorldMatrix,mesh.sharedMesh.bounds);if(bounds.max.y<=low||bounds.min.y>=high){trace.Reason="Outside captured player height band";continue;}
                    if(!mesh.sharedMesh.isReadable){trace.Reason="Mesh is not readable; no invented bounds wall";omitted++;continue;}
                    var vertices=mesh.sharedMesh.vertices;var indices=mesh.sharedMesh.triangles;
                    if(indices.Length>180000){trace.Reason="Mesh triangle budget exceeded";omitted++;continue;}
                    for(int i=0;i+2<indices.Length;i+=3)polygons.Add(MapPhysicalData.Triangle(mesh.transform.TransformPoint(vertices[indices[i]]),mesh.transform.TransformPoint(vertices[indices[i+1]]),mesh.transform.TransformPoint(vertices[indices[i+2]]),low,high));
                } else if(sphere!=null){
                    var scale=sphere.transform.lossyScale;float r=sphere.radius*Mathf.Max(Mathf.Abs(scale.x),Mathf.Abs(scale.y),Mathf.Abs(scale.z));var center=sphere.transform.TransformPoint(sphere.center);
                    float dy=center.y<low?low-center.y:center.y>high?center.y-high:0;if(dy>=r){trace.Reason="Outside captured player height band";continue;}
                    r=Mathf.Sqrt(r*r-dy*dy);var points=new List<Vector3>();for(int k=0;k<24;k++){float angle=k*Mathf.PI/12;points.Add(center+new Vector3(Mathf.Cos(angle),0,Mathf.Sin(angle))*(r/Mathf.Cos(Mathf.PI/24)));}polygons.Add(MapPhysicalData.Hull(points));approximate=true;
                } else if(capsule!=null){
                    ReplayColliderShape shape;if(!ReplayColliderShape.Capture(capsule,out shape)){trace.Reason="Unsupported capsule shape";omitted++;continue;}
                    polygons.Add(MapPhysicalData.Capsule(shape,low,high));approximate=true;
                } else {trace.Reason="Unsupported collider shape";omitted++;continue;}
                trace.Reason="No intersection with captured player height band";
                foreach(var polygon in polygons){
                    if(polygon.Length<2)continue;
                    if(output.Count>=20000||pointCount+polygon.Length>60000){trace.Reason="Geometry budget exceeded; partial or omitted";omitted++;continue;}
                    var footprint=new MapFootprint{Flags=(wall?1u:0u)|(obstacle?2u:0u)|(approximate?4u:0u),Points=polygon};
                    output.Add(footprint);trace.Parts.Add(footprint);if(trace.Reason!="Geometry budget exceeded; partial or omitted")trace.Reason="Included in the captured static geometry";pointCount+=polygon.Length;
                    // Legacy telemetry carries quads only. Detailed packets use
                    // every convex part without enclosing doorways in an AABB.
                    if(polygon.Length==4){var b=new Bounds(polygon[0],Vector3.zero);foreach(var p in polygon)b.Encapsulate(p);entries.Add(new Entry{Collider=collider,Wall=wall,Obstacle=obstacle,Outline=new WallOutline{Corners=polygon,Approximate=approximate},Bounds=Rect.MinMaxRect(b.min.x,b.min.z,b.max.x,b.max.z)});}
                }
            }
            parts=output.ToArray();scene=handle;unchecked{CaptureRevision++;}
        }
        if(data!=null){var outlines=new List<WallOutline>();var rects=new List<Rect>();var obstacles=new List<WallOutline>();foreach(var e in entries)if(e.Collider!=null){if(e.Wall){outlines.Add(e.Outline);rects.Add(e.Bounds);}if(e.Obstacle)obstacles.Add(e.Outline);}data.WallOutlines=outlines.ToArray();data.Walls=rects.ToArray();data.Obstacles=obstacles.ToArray();data.PhysicalParts=parts;data.PhysicalCaptured=scene==handle;data.PhysicalOmitted=omitted;PlayerProfile(player,data);}
    }
    public static void PlayerProfile(MouseLook player,PracticeMapData data){if(player!=null&&player.controller!=null){var cc=player.controller;var scale=cc.transform.lossyScale;data.AgentRadius=cc.radius*Mathf.Max(Mathf.Abs(scale.x),Mathf.Abs(scale.z));data.AgentHeight=Mathf.Max(data.AgentRadius*2,cc.height*Mathf.Abs(scale.y));data.AgentStep=cc.stepOffset*Mathf.Abs(scale.y);data.AgentSlope=cc.slopeLimit;data.AgentFloor=cc.transform.TransformPoint(cc.center).y-data.AgentHeight*.5f;
        // Verified MouseLook.FixedUpdate uses 0.14 units per sprint tick;
        // input axes are added without normalization. This is not u/s.
        if(HarmonyPatches.SupportedBuild&&Time.timeScale>0){data.MovementTick=Time.fixedDeltaTime/Time.timeScale;data.MovementSprintStep=.14f;}
    }}
    public static WallOutline[] Current(){var list=new List<WallOutline>();foreach(var e in entries)if(e.Collider!=null&&e.Obstacle)list.Add(e.Outline);return list.ToArray();}
}
