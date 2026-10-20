using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Reflection;
using System.Text;
using UnityEngine;
using UnityEngine.SceneManagement;

// Inspection runs on Unity's thread, only after an explicit trainer opt-in.
// Game fields are read, never written; arbitrary getters/methods are not called.
public sealed class LiveInspector : IDisposable
{
    private sealed class Node {public GameObject Object;public Renderer Renderer;public InspectorObjectRow Row;public Bounds Bounds;public bool HasBounds;}
    private sealed class ScanFrame {public Transform Transform;public int Next;public bool Read;}
    private sealed class Slot {public InspectorValue Value;public Func<string> Read;}
    private sealed class Watch {public Slot Slot;public string Previous;}
    private struct Segment {public Vector3 A,B;}
    private sealed class MeshWire {public Mesh Source;public Segment[] Lines;public int Frame;}
    private static LiveInspector instance;
    public static bool Active {get{return instance!=null&&instance.active;}}
    public static bool Drawing {get{return Active&&(instance.selected!=null||instance.hoverObject!=null)&&(Application.isFocused||MenuCursor.OwnsInput)&&!Replay3D.Reserved&&!(SpeedrunManager.Instance!=null&&SpeedrunManager.Instance.Restricted);}}
    private readonly int pid;private readonly uint session;private InspectorChannel channel;
    private readonly Dictionary<int,Node> nodes=new Dictionary<int,Node>();private readonly List<InspectorObjectRow> rows=new List<InspectorObjectRow>();
    private readonly Stack<ScanFrame> scan=new Stack<ScanFrame>();private readonly List<Slot> slots=new List<Slot>();private readonly List<Watch> watches=new List<Watch>();
    private readonly List<InspectorChange> changes=new List<InspectorChange>();private readonly List<InspectorPickHit> hits=new List<InspectorPickHit>();private readonly RaycastHit[] rayHits=new RaycastHit[512];
    private readonly Dictionary<int,MeshWire> meshes=new Dictionary<int,MeshWire>();
    private GameObject selected;private Collider[] selectedColliders=new Collider[0];private Renderer selectedRenderer;private MouseLook player;
    private int scene=-1,lastSelected,changesDropped,changeBytes,catalogBytes,ackAction;private uint epoch,catalogRevision,detailRevision,ack,options=12;private bool ackSuccess;
    private bool active,scanning,truncated,dirty,hasA,hasB,selectionGone,ownsFreecam;private double nextDetail,nextOpen;private Vector3 pointA,pointB;private string status="Choose an object",pickSource="";
    private double detailMs,scanMs,nextHover,lastPointerAt;private int summaryCount;private bool detailsLoaded;
    private InspectorPointer pointer;private InspectorHover hover=new InspectorHover();private Collider hoverCollider;private GameObject hoverObject;
    private readonly Vector3[] outlineWorld=new Vector3[8],outlineView=new Vector3[8];private uint hoverRevision;
    private uint geometryRevision;private bool catalogBuilt,queryPartial;private readonly InspectorVisualPicking visuals=new InspectorVisualPicking();
    public LiveInspector(int process,uint ownerSession){pid=process;session=ownerSession;instance=this;}
    public void SceneLoaded(uint nextEpoch){epoch=nextEpoch;scene=-1;ResetSelection();nodes.Clear();rows.Clear();scan.Clear();visuals.Clear();catalogBuilt=false;scanning=false;truncated=false;hasA=hasB=false;active=false;ownsFreecam=false;ack=0;pointer=new InspectorPointer();hover=new InspectorHover();hoverObject=null;hoverCollider=null;}
    private void ResetSelection(){selected=null;selectedRenderer=null;selectedColliders=new Collider[0];slots.Clear();watches.Clear();changes.Clear();meshes.Clear();lastSelected=0;hits.Clear();pickSource="";changesDropped=changeBytes=0;selectionGone=false;summaryCount=0;detailsLoaded=false;dirty=true;}
    private Camera View {get{var c=FreeCam.SpectatorCamera;return c!=null?c:PlayerContext.View(player);}}
    public void Tick(TrainerCommands commands,bool connected,MouseLook mouse,uint sceneEpoch){
        player=mouse;bool permitted=InspectorPolicy.Permitted(connected,commands.RequestsInspector,commands.Restricted,SpeedrunManager.Instance!=null&&SpeedrunManager.Instance.Restricted,Replay3D.Reserved);
        if(!permitted){if(active){active=false;if(ownsFreecam){FreeCam.DisableActive();ownsFreecam=false;}ResetSelection();nodes.Clear();rows.Clear();scan.Clear();visuals.Clear();catalogBuilt=false;scanning=false;truncated=false;hasA=hasB=false;pointer.Active=false;hoverObject=null;hoverCollider=null;PublishHover(new InspectorHover());status="Inspector closed or blocked by clean Speedrun / Replay";PublishDetail();}if(channel!=null)channel.Flush();return;}
        if(!Application.isFocused&&!MenuCursor.OwnsInput)return;
        double now=Time.realtimeSinceStartupAsDouble;
        if(channel==null&&now>=nextOpen){nextOpen=now+1;try{channel=new InspectorChannel(pid,session);}catch{channel=null;return;}}
        if(channel==null)return;
        int current=SceneManager.GetActiveScene().buildIndex;
        if(scene!=current||epoch!=sceneEpoch){SceneLoaded(sceneEpoch);scene=current;}
        if(!active){active=true;status="Picker ready";if(SpeedrunManager.Instance!=null)SpeedrunManager.Instance.MarkAssisted(RunAids.LiveInspector);}
        InspectorCommand command;InspectorPointer nextPointer;bool read=channel.Read(out command,out nextPointer);if(read&&nextPointer.Valid&&nextPointer.Epoch==epoch){if(pointer.Serial!=nextPointer.Serial)lastPointerAt=now;pointer=nextPointer;}
        if(read&&command.Serial!=0&&command.Serial!=ack){
            ack=command.Serial;ackAction=command.Action>=0&&command.Action<=9?command.Action:0;ackSuccess=false;
            if(!command.Valid||command.Epoch!=epoch)status="Stale or invalid inspector command rejected";
            else{bool newBounds=(options&4)==0&&(command.Flags&4)!=0;options=command.Flags;if(newBounds&&!scanning&&!catalogBuilt)BeginScan();ackSuccess=Handle(command);}dirty=true;
        }
        if(pointer.Active&&(options&4)!=0){if(!scanning&&!catalogBuilt)BeginScan();visuals.Tick(Time.frameCount);}
        if(scanning)ScanSlice();
        EnsureDetails();UpdateHover(now);Observe();
        if(geometryRevision!=SceneGeometry.CaptureRevision){geometryRevision=SceneGeometry.CaptureRevision;dirty=true;}
        if(dirty||now>=nextDetail){nextDetail=now+.2;PublishDetail();dirty=false;}
        channel.Flush();
    }
    private bool Handle(InspectorCommand c){
        switch(c.Action){
            case 0:return true;
            case 1:BeginScan();return true;
            case 2:Node n;if(nodes.TryGetValue(c.Object,out n)&&n.Object!=null){SelectRelated(n.Object);return true;}if(selected!=null&&selected.transform.parent!=null&&selected.transform.parent.gameObject.GetInstanceID()==c.Object){Select(selected.transform.parent.gameObject);return true;}status="Object is gone or absent from this catalog; refresh the list";return false;
            case 3:return CommitPick(c);
            case 4:return ToggleWatch(c.Key);
            case 5:if(selected!=null){Bounds b=selectedRenderer!=null&&selectedRenderer.enabled&&selected.activeInHierarchy?selectedRenderer.bounds:new Bounds(selected.transform.position,Vector3.one);bool wasFreecam=FreeCam.FreeCamEnabled;if(FreeCam.Focus(player,b)){ownsFreecam|=!wasFreecam;status="Selection framed in spectator camera";return true;}}status="Spectator camera unavailable";return false;
            case 6:watches.Clear();status="Watches cleared";return true;
            case 7:return Pick(c.X,c.Y,1);
            case 8:return Pick(c.X,c.Y,2);
            case 9:ResetSelection();status="Selection cleared";return true;
        }
        return false;
    }
    private void BeginScan(){
        nodes.Clear();rows.Clear();scan.Clear();visuals.Clear();catalogBuilt=false;scanning=true;truncated=false;scanMs=0;catalogBytes=0;status="Building object catalog";
        var roots=new List<GameObject>();
        for(int i=0;i<SceneManager.sceneCount;i++){var s=SceneManager.GetSceneAt(i);if(s.isLoaded)roots.AddRange(s.GetRootGameObjects());}
        // Include persistent game objects as well; helper scripts are not read.
        if(PlayerContext.Observed!=null){var p=PlayerContext.Observed.gameObject.scene;bool listed=false;for(int i=0;i<SceneManager.sceneCount;i++)listed|=SceneManager.GetSceneAt(i).handle==p.handle;if(!listed&&p.IsValid()&&p.isLoaded)roots.AddRange(p.GetRootGameObjects());}
        for(int i=roots.Count-1;i>=0;i--)if(roots[i]!=null)scan.Push(new ScanFrame{Transform=roots[i].transform});dirty=true;
    }
    private void ScanSlice(){
        long start=Stopwatch.GetTimestamp();int steps=0;
        while(scan.Count>0&&steps++<256){
            if(steps>1&&(Stopwatch.GetTimestamp()-start)*1000.0/Stopwatch.Frequency>.7)break;
            var f=scan.Peek();if(f.Transform==null){scan.Pop();continue;}
            if(!f.Read){
                f.Read=true;var obj=f.Transform.gameObject;int id=obj.GetInstanceID();
                Node existing;if(!nodes.TryGetValue(id,out existing)||existing.Row==null){
                    if(rows.Count==InspectorWire.MaxObjects){truncated=true;scan.Clear();break;}
                    var renderers=obj.GetComponents<Renderer>();var r=renderers.Length>0?renderers[0]:null;var c=obj.GetComponent<Collider>();var p=f.Transform.parent;
                    string name=obj.name;if(name.Length>256)name=name.Substring(0,256);name=name.Replace("\0","\\0");int size=24+Encoding.UTF8.GetByteCount(name);
                    if(catalogBytes+size>InspectorChannel.CatalogCapacity-4096){truncated=true;scan.Clear();break;}catalogBytes+=size;
                    var row=new InspectorObjectRow{Id=id,Parent=p==null?0:p.gameObject.GetInstanceID(),Scene=obj.scene.buildIndex,Layer=obj.layer,Flags=(obj.activeSelf?1u:0u)|(obj.activeInHierarchy?2u:0u)|(c!=null?4u:0u)|(r!=null?8u:0u),Name=name};
                    foreach(var visual in renderers)visuals.Register(obj,visual);
                    rows.Add(row);nodes[id]=new Node{Object=obj,Renderer=r,Row=row,HasBounds=r!=null&&r.enabled&&obj.activeInHierarchy,Bounds=r==null?new Bounds():r.bounds};
                }
            }
            if(f.Next<f.Transform.childCount)scan.Push(new ScanFrame{Transform=f.Transform.GetChild(f.Next++)});else scan.Pop();
        }
        scanMs+=(Stopwatch.GetTimestamp()-start)*1000.0/Stopwatch.Frequency;
        if(scan.Count==0){scanning=false;catalogBuilt=true;status=truncated?"Catalog limit reached; list is partial":"Object catalog ready";PublishCatalog();dirty=true;}
    }
    private void PublishCatalog(){if(channel!=null)channel.QueueCatalog(InspectorWire.Encode(1,pid,session,scene,epoch,++catalogRevision,w=>{InspectorWire.Text(w,SceneManager.GetActiveScene().name,512);w.Write(truncated);w.Write(rows.Count);foreach(var row in rows)InspectorWire.Object(w,row);}));}
    private static string PathOf(Transform t){var names=new List<string>();for(int i=0;t!=null&&i<64;i++,t=t.parent)names.Add(t.name+"["+t.GetSiblingIndex().ToString(CultureInfo.InvariantCulture)+"]");names.Reverse();return string.Join("/",names.ToArray());}
    private void Add(int component,string group,string name,string source,Func<string> read,uint flags=1){
        if(slots.Count==InspectorWire.MaxValues)return;
        slots.Add(new Slot{Value=new InspectorValue{Key=InspectorWire.Key(component,source),Component=component,Flags=flags,Group=group,Name=name,Source=source},Read=read});
    }
    private static string Read(Slot slot){try{return slot.Read()??"null";}catch(Exception e){return "Unavailable ("+e.GetType().Name+")";}}
    private static string Bool(bool value){return value?"true":"false";}
    private void Select(GameObject obj){
        ResetSelection();selected=obj;lastSelected=obj.GetInstanceID();var t=obj.transform;int id=lastSelected;
        Add(id,"Object","Name","GameObject.name",()=>obj.name);Add(id,"Object","Active self","GameObject.activeSelf",()=>Bool(obj.activeSelf));Add(id,"Object","Active in hierarchy","GameObject.activeInHierarchy",()=>Bool(obj.activeInHierarchy));
        Add(id,"Object","Layer","GameObject.layer",()=>obj.layer+" / "+LayerMask.LayerToName(obj.layer));Add(id,"Object","Tag","GameObject.tag",()=>obj.tag);Add(id,"Object","Children","Transform.childCount",()=>t.childCount.ToString(CultureInfo.InvariantCulture));
        Add(id,"Transform","World position","Transform.position",()=>InspectorFields.Vector(t.position));
        Add(id,"Object","View distance","Inspector.viewDistance",()=>View==null?"Unavailable":InspectorFields.Number(Vector3.Distance(View.transform.position,t.position))+" u");
        var colliders=obj.GetComponents<Collider>();int count=Math.Min(InspectorWire.MaxColliders,colliders.Length);selectedColliders=new Collider[count];Array.Copy(colliders,selectedColliders,count);selectedRenderer=obj.GetComponent<Renderer>();
        Add(id,"Object","Primary type","Inspector.primaryType",()=>selectedColliders.Length>0&&selectedColliders[0]!=null?selectedColliders[0].GetType().Name+(selectedColliders[0].isTrigger?" / trigger":" / solid"):selectedRenderer!=null?selectedRenderer.GetType().Name:"GameObject");
        summaryCount=slots.Count;status="Selected "+obj.name+(colliders.Length>count?" / collider limit reached":"");dirty=true;
        // Basic selection does not enumerate materials, game scripts or fields.
    }
    private void EnsureDetails(){
        if(selected==null||(options&16)==0||detailsLoaded)return;detailsLoaded=true;var obj=selected;var t=obj.transform;int id=lastSelected;
        Add(id,"Transform","Local position","Transform.localPosition",()=>InspectorFields.Vector(t.localPosition));
        Add(id,"Transform","World rotation","Transform.eulerAngles",()=>InspectorFields.Vector(t.eulerAngles));Add(id,"Transform","Local rotation","Transform.localEulerAngles",()=>InspectorFields.Vector(t.localEulerAngles));Add(id,"Transform","Local scale","Transform.localScale",()=>InspectorFields.Vector(t.localScale));Add(id,"Transform","World scale","Transform.lossyScale",()=>InspectorFields.Vector(t.lossyScale));
        foreach(var c in selectedColliders)AddCollider(c);
        selectedRenderer=obj.GetComponent<Renderer>();
        if(selectedRenderer!=null){var r=selectedRenderer;int rid=r.GetInstanceID();
            Add(rid,"Renderer","Enabled","Renderer.enabled",()=>Bool(r.enabled));Add(rid,"Renderer","Visible to any camera","Renderer.isVisible",()=>Bool(r.isVisible));Add(rid,"Renderer","Bounds center","Renderer.bounds.center",()=>InspectorFields.Vector(r.bounds.center));Add(rid,"Renderer","Bounds size (not collider)","Renderer.bounds.size",()=>InspectorFields.Vector(r.bounds.size));
            var materials=r.sharedMaterials;for(int k=0;k<Math.Min(8,materials.Length);k++){var material=materials[k];int index=k;Add(rid,"Renderer","Material "+index,"Renderer.sharedMaterials["+index+"]",()=>material==null?"null":material.name+" / "+(material.shader==null?"no shader":material.shader.name));}
            Mesh mesh=null;var filter=obj.GetComponent<MeshFilter>();if(filter!=null)mesh=filter.sharedMesh;var skin=r as SkinnedMeshRenderer;if(skin!=null)mesh=skin.sharedMesh;
            if(mesh!=null){var m=mesh;Add(rid,"Renderer","Shared mesh","MeshFilter/SkinnedMeshRenderer.sharedMesh",()=>m.name+" / "+m.vertexCount+" vertices / readable: "+Bool(m.isReadable));}
        }
        var components=obj.GetComponents<MonoBehaviour>();int taken=0;
        foreach(var component in components){if(taken++==64)break;if(component==null||component.GetType().Assembly!=typeof(MouseLook).Assembly)continue;var target=component;int cid=target.GetInstanceID();string group=target.GetType().Name+" #"+cid;
            Add(cid,group,"Component enabled","Behaviour.enabled",()=>Bool(target.enabled));
            foreach(var field in InspectorFields.Readable(target.GetType())){var f=field;Add(cid,group,f.Name,f.DeclaringType.FullName+"."+f.Name,()=>InspectorFields.Describe(f.GetValue(target)),f.IsPrivate?2u:0u);}
        }
        if(slots.Count==InspectorWire.MaxValues)status="Selected "+obj.name+" / field limit reached";dirty=true;
    }
    private void AddCollider(Collider c){
        int id=c.GetInstanceID();string group=c.GetType().Name+" #"+id;
        Add(id,group,"Enabled","Collider.enabled",()=>Bool(c.enabled));Add(id,group,"Trigger","Collider.isTrigger",()=>Bool(c.isTrigger));
        Add(id,group,"Player layer collision","Physics.GetIgnoreLayerCollision",()=>player==null||player.controller==null?"Player unavailable":Bool(!Physics.GetIgnoreLayerCollision(player.controller.gameObject.layer,c.gameObject.layer)));
        Add(id,group,"Player pair collision","Physics.GetIgnoreCollision",()=>player==null||player.controller==null?"Player unavailable":player.controller==c?"Same collider":Bool(!Physics.GetIgnoreCollision(player.controller,c)));
        Add(id,group,"Attached rigidbody","Collider.attachedRigidbody",()=>c.attachedRigidbody==null?"None (static)":c.attachedRigidbody.name+" / kinematic: "+Bool(c.attachedRigidbody.isKinematic));
        var box=c as BoxCollider;var sphere=c as SphereCollider;var capsule=c as CapsuleCollider;var cc=c as CharacterController;var mesh=c as MeshCollider;
        if(box!=null){Add(id,group,"Local center","BoxCollider.center",()=>InspectorFields.Vector(box.center));Add(id,group,"Local size","BoxCollider.size",()=>InspectorFields.Vector(box.size));}
        if(sphere!=null){Add(id,group,"Local center","SphereCollider.center",()=>InspectorFields.Vector(sphere.center));Add(id,group,"Local radius","SphereCollider.radius",()=>InspectorFields.Number(sphere.radius));}
        if(capsule!=null){Add(id,group,"Local center","CapsuleCollider.center",()=>InspectorFields.Vector(capsule.center));Add(id,group,"Local radius / height","CapsuleCollider.dimensions",()=>InspectorFields.Number(capsule.radius)+" / "+InspectorFields.Number(capsule.height));Add(id,group,"Axis (0 X / 1 Y / 2 Z)","CapsuleCollider.direction",()=>capsule.direction.ToString(CultureInfo.InvariantCulture));}
        if(cc!=null){Add(id,group,"Center","CharacterController.center",()=>InspectorFields.Vector(cc.center));Add(id,group,"Radius / serialized height","CharacterController.dimensions",()=>InspectorFields.Number(cc.radius)+" / "+InspectorFields.Number(cc.height));Add(id,group,"Grounded","CharacterController.isGrounded",()=>Bool(cc.isGrounded));Add(id,group,"Step / slope / skin","CharacterController.clearance",()=>InspectorFields.Number(cc.stepOffset)+" / "+InspectorFields.Number(cc.slopeLimit)+" / "+InspectorFields.Number(cc.skinWidth));}
        if(sphere!=null||capsule!=null||cc!=null)Add(id,group,"World radius / effective height","ReplayColliderShape.Capture",()=>{ReplayColliderShape shape;return ReplayColliderShape.Capture(c,out shape)?InspectorFields.Number(shape.Radius)+" / "+InspectorFields.Number(shape.Height):"Unsupported";});
        if(mesh!=null){Add(id,group,"Shared mesh","MeshCollider.sharedMesh",()=>mesh.sharedMesh==null?"null":mesh.sharedMesh.name+" / readable: "+Bool(mesh.sharedMesh.isReadable));Add(id,group,"Convex","MeshCollider.convex",()=>Bool(mesh.convex));PrepareMesh(mesh);Add(id,group,"Actual mesh wire","Inspector.readableMeshWire",()=>{MeshWire wire;if(mesh.convex)return "Cooked convex hull is not exposed; no source-mesh or bounds substitute";if(meshes.TryGetValue(id,out wire)){if(wire.Source!=mesh.sharedMesh)return "Shared mesh changed; reselect to refresh";return wire.Lines.Length+" source edges / selection frame "+wire.Frame+" (8192 total budget; reselect for topology changes)";}return "Unavailable or beyond the readable mesh budget; no bounds substitute";});}
        Add(id,group,"Suite capture decision","SceneGeometry.captureReason",()=>{var trace=SceneGeometry.Inspect(c);return trace==null?"Not present in the last static capture":trace.Reason;});
    }
    private void PrepareMesh(MeshCollider collider){
        int used=0;foreach(var item in meshes.Values)used+=item.Lines.Length;int limit=8192-used;if(limit<=0||collider.convex)return;
        var mesh=collider.sharedMesh;if(mesh==null||!mesh.isReadable||mesh.vertexCount>60000||mesh.subMeshCount>128)return;
        ulong indexCount=0;for(int k=0;k<mesh.subMeshCount;k++)indexCount+=mesh.GetIndexCount(k);if(indexCount>180000)return;
        var vertices=mesh.vertices;var triangles=mesh.triangles;if(triangles.Length>180000)return;
        var edges=new HashSet<ulong>();var output=new List<Segment>();
        for(int i=0;i+2<triangles.Length&&output.Count<limit;i+=3)for(int k=0;k<3;k++){int a=triangles[i+k],b=triangles[i+(k+1)%3];if(a<0||b<0||a>=vertices.Length||b>=vertices.Length)continue;uint lo=(uint)Math.Min(a,b),hi=(uint)Math.Max(a,b);if(edges.Add(((ulong)lo<<32)|hi)&&output.Count<limit)output.Add(new Segment{A=vertices[a],B=vertices[b]});}
        meshes[collider.GetInstanceID()]=new MeshWire{Source=mesh,Lines=output.ToArray(),Frame=Time.frameCount};
    }
    private bool ToggleWatch(ulong key){
        int old=watches.FindIndex(x=>x.Slot.Value.Key==key);if(old>=0){watches.RemoveAt(old);status="Watch removed";return true;}
        if(watches.Count==8){status="Eight watch limit reached";return false;}var slot=slots.Find(x=>x.Value.Key==key);if(slot==null){status="Field no longer belongs to this selection";return false;}
        watches.Add(new Watch{Slot=slot,Previous=Read(slot)});status="Watching "+slot.Value.Name;return true;
    }
    private void Observe(){
        if(lastSelected!=0&&selected==null&&!selectionGone){selectionGone=true;status="Selected object was destroyed";slots.Clear();watches.Clear();selectedColliders=new Collider[0];meshes.Clear();dirty=true;return;}
        foreach(var watch in watches){string value=Read(watch.Slot);if(value==watch.Previous)continue;var change=new InspectorChange{Key=watch.Slot.Value.Key,Frame=Time.frameCount,Seconds=Time.realtimeSinceStartupAsDouble,Before=watch.Previous,After=value};int size=ChangeBytes(change);
            while(changes.Count>0&&(changes.Count==InspectorWire.MaxChanges||changeBytes+size>180000)){changeBytes-=ChangeBytes(changes[0]);changes.RemoveAt(0);changesDropped++;}
            changes.Add(change);changeBytes+=size;watch.Previous=value;}
    }
    private static int ChangeBytes(InspectorChange c){return 32+Encoding.UTF8.GetByteCount(c.Before)+Encoding.UTF8.GetByteCount(c.After);}
    private bool Watched(ulong key){foreach(var watch in watches)if(watch.Slot.Value.Key==key)return true;return false;}
    private static string HitSource(int kind){return kind==1?"Actual collider / camera ray":kind==3?"Readable mesh triangles / camera ray":kind==4?"Sprite triangles / camera ray":"Renderer bounds estimate / no collider required";}
    private bool Query(float x,float y,int endpoint,out Ray ray,out GameObject target,out Vector3 hitPoint,out Collider collider,out float nearest,out string source,List<InspectorPickHit> found=null,int preferred=0){
        ray=new Ray();target=null;hitPoint=Vector3.zero;collider=null;nearest=float.PositiveInfinity;source="";queryPartial=false;var camera=View;Vector3 uv;
        if(camera==null){source="No live scene camera";return false;}
        if(!InspectorPicking.Viewport(x,y,camera.rect,out uv)){source="Outside the scene camera viewport";return false;}
        ray=camera.ViewportPointToRay(uv);Transform excluded=player!=null&&player.controller!=null&&FreeCam.SpectatorCamera==null?player.controller.transform:null;var choices=found??new List<InspectorPickHit>();choices.Clear();
        if(endpoint!=0||(options&32)==0){
            int count=Physics.RaycastNonAlloc(ray,rayHits,10000,~0,endpoint==0&&(options&8)!=0?QueryTriggerInteraction.Collide:QueryTriggerInteraction.Ignore);
            if(count==rayHits.Length){source="Physics hit limit reached; narrow the view or use Visuals only";return false;}
            for(int i=0;i<count;i++){var hit=rayHits[i];var c=hit.collider;if(c==null)continue;if(excluded!=null&&(c.transform==excluded||c.transform.IsChildOf(excluded)))continue;
                var obj=c.gameObject;choices.Add(new InspectorPickHit{Id=obj.GetInstanceID(),Kind=1,Distance=hit.distance,Name=obj.name,Type=c.GetType().Name,Target=obj,Collider=c,Point=hit.point});}
        }
        if(endpoint==0&&(options&4)!=0)queryPartial=visuals.Collect(ray,camera,excluded,choices);
        queryPartial|=InspectorPickOrder.Normalize(choices);
        InspectorPickHit chosen=InspectorPickOrder.Resolve(choices,preferred);
        if(chosen==null){source=preferred!=0?"Chosen target moved; choose another layer":scanning?"Preparing visual targets...":"No target here; use the object list or refresh the scene";return false;}
        target=chosen.Target;collider=chosen.Collider;hitPoint=chosen.Point;nearest=chosen.Distance;source=HitSource(chosen.Kind);return true;
    }
    private void SelectRelated(GameObject obj){
        var previous=new List<InspectorPickHit>();foreach(var hit in hits)if(hit.Id==obj.GetInstanceID()){previous.AddRange(hits);break;}
        Select(obj);if(previous.Count>0){hits.AddRange(previous);foreach(var hit in previous)if(hit.Id==lastSelected){pickSource=HitSource(hit.Kind);break;}}else pickSource="Object catalog / object ID";
    }
    private bool Pick(float x,float y,int endpoint){
        Ray ray;GameObject target;Vector3 point;Collider collider;float distance;string source;var found=new List<InspectorPickHit>();
        if(!Query(x,y,endpoint,out ray,out target,out point,out collider,out distance,out source,found)){status=source;return false;}
        return ApplyPick(target,point,endpoint,found,source);
    }
    private bool ApplyPick(GameObject target,Vector3 point,int endpoint,List<InspectorPickHit> found,string source){
        if(endpoint==1){pointA=point;hasA=true;status="Probe start observed on the live surface";}
        else if(endpoint==2){pointB=point;hasB=true;status="Probe end observed on the live surface";}
        else{Select(target);pickSource=source;if(!nodes.ContainsKey(lastSelected))nodes[lastSelected]=new Node{Object=target};for(int i=0;i<Math.Min(32,found.Count);i++)hits.Add(found[i]);status="Object selected";}return true;
    }
    private bool CommitPick(InspectorCommand c){
        // Commit only the highlighted observation, never a newly sampled mouse
        // position or an unrelated stale object. Ray and target are shared.
        if(!InspectorPicking.Matches((uint)c.Key,c.Object,hover.Serial,hover.Object)||hoverObject==null||!hover.Active||Time.realtimeSinceStartupAsDouble-lastPointerAt>.4){status="Hover changed; point and select again";return false;}
        Ray ray;GameObject target;Vector3 point;Collider collider;float distance;string source;var found=new List<InspectorPickHit>();
        if(!Query(hover.X,hover.Y,0,out ray,out target,out point,out collider,out distance,out source,found,c.Object)||target.GetInstanceID()!=c.Object){status="The highlighted surface changed; point and select again";return false;}
        return ApplyPick(target,point,0,found,source);
    }
    private void UpdateHover(double now){
        bool wanted=pointer.Valid&&pointer.Epoch==epoch&&pointer.Active&&now-lastPointerAt<.5;
        if(!wanted){if(hover.Active||hover.Serial!=pointer.Serial){hoverObject=null;hoverCollider=null;PublishHover(new InspectorHover{Serial=pointer.Serial});}return;}
        if(now<nextHover)return;nextHover=now+1.0/30;
        Ray ray;GameObject target;Vector3 point;Collider collider;float distance;string source;var found=new List<InspectorPickHit>();
        bool hit=Query(pointer.X,pointer.Y,pointer.Action==3?0:pointer.Action==7?1:2,out ray,out target,out point,out collider,out distance,out source,found,pointer.Action==3?pointer.Preferred:0);var camera=View;
        var h=new InspectorHover{Serial=pointer.Serial,Active=true,Object=hit?target.GetInstanceID():0,Kind=0,X=pointer.X,Y=pointer.Y,Distance=hit?distance:0,Name=hit?target.name:"",Type="",Status=source,CameraId=camera==null?0:camera.GetInstanceID(),Camera=camera==null?"":camera.name+(camera.targetTexture!=null?" / render texture":" / direct"),Width=pointer.Width,Height=pointer.Height,Viewport=camera==null?new Rect():camera.rect,Partial=queryPartial||truncated,Scanning=scanning};
        foreach(var item in found){h.Targets.Add(new InspectorPickTarget{Id=item.Id,Kind=item.Kind,Distance=item.Distance,Name=item.Name,Type=item.Type});if(item.Id==h.Object){h.Kind=item.Kind;h.Type=item.Type;}}
        hoverObject=hit?target:null;hoverCollider=collider;
        if(hit&&camera!=null){var renderer=target.GetComponent<Renderer>();Bounds bounds=renderer!=null&&renderer.enabled?renderer.bounds:collider!=null?collider.bounds:new Bounds(target.transform.position,Vector3.one*.2f);ProjectOutline(camera,bounds,h);}
        PublishHover(h);
    }
    private void PublishHover(InspectorHover value){hover=value;if(channel!=null)channel.QueueHover(InspectorWire.Encode(3,pid,session,scene,epoch,++hoverRevision,w=>InspectorHover.Write(w,value)));}
    private void ProjectOutline(Camera camera,Bounds bounds,InspectorHover h){
        var lo=bounds.min;var hi=bounds.max;float near=Mathf.Max(.001f,camera.nearClipPlane);float left=float.PositiveInfinity,bottom=left,right=float.NegativeInfinity,top=right;
        Action<Vector3> include=p=>{left=Mathf.Min(left,p.x);right=Mathf.Max(right,p.x);bottom=Mathf.Min(bottom,p.y);top=Mathf.Max(top,p.y);};
        for(int i=0;i<8;i++){var world=new Vector3((i&1)==0?lo.x:hi.x,(i&2)==0?lo.y:hi.y,(i&4)==0?lo.z:hi.z);outlineWorld[i]=world;outlineView[i]=camera.WorldToViewportPoint(world);if(outlineView[i].z>=near)include(outlineView[i]);}
        for(int i=0;i<8;i++)for(int axis=0;axis<3;axis++){int j=i^(1<<axis);if(j<i)continue;float a=outlineView[i].z,b=outlineView[j].z;if((a<near)==(b<near))continue;float t=(near-a)/(b-a);include(camera.WorldToViewportPoint(Vector3.LerpUnclamped(outlineWorld[i],outlineWorld[j],t)));}
        if(!InspectorPicking.Finite(left)||!InspectorPicking.Finite(right)||!InspectorPicking.Finite(top)||!InspectorPicking.Finite(bottom)||right<left||top<bottom)return;var r=camera.rect;left=r.x+left*r.width;right=r.x+right*r.width;bottom=r.y+bottom*r.height;top=r.y+top*r.height;
        if(right<0||left>1||top<0||bottom>1)return;h.Left=Mathf.Clamp01(left);h.Right=Mathf.Clamp01(right);h.Top=1-Mathf.Clamp01(top);h.Bottom=1-Mathf.Clamp01(bottom);h.Outline=h.Right>h.Left&&h.Bottom>h.Top;
    }
    private void PublishDetail(){
        if(channel==null)return;long started=Stopwatch.GetTimestamp();var camera=active?View:null;
        var published=new List<Slot>();for(int i=0;i<slots.Count;i++){var slot=slots[i];if(i>=summaryCount&&(options&16)==0&&!Watched(slot.Value.Key))continue;published.Add(slot);slot.Value.Value=Read(slot);slot.Value.Flags=(slot.Value.Flags&~4u)|(Watched(slot.Value.Key)?4u:0u);}
        var bytes=InspectorWire.Encode(2,pid,session,scene,epoch,++detailRevision,w=>{
            w.Write(active);w.Write(ack);w.Write(ackAction);w.Write(ackSuccess);w.Write(lastSelected);InspectorWire.Text(w,status);InspectorWire.Text(w,selected==null?"":selected.name,768);InspectorWire.Text(w,selected==null?"":PathOf(selected.transform),4096);InspectorWire.Text(w,pickSource,1024);
            w.Write(scanning);w.Write(truncated);w.Write(rows.Count);w.Write(scanMs);w.Write(detailMs);w.Write(options);w.Write(changesDropped);long droppedOffset=w.BaseStream.Position;w.Write(0);w.Write(SceneGeometry.CaptureRevision);
            InspectorWire.Vector(w,camera==null?Vector3.zero:camera.transform.position);InspectorWire.Vector(w,camera==null?Vector3.zero:camera.transform.eulerAngles);w.Write(camera==null?0:camera.fieldOfView);
            long countOffset=w.BaseStream.Position;w.Write(0);int valueCount=0;foreach(var slot in published){long before=w.BaseStream.Position;InspectorWire.Value(w,slot.Value);if(w.BaseStream.Position-countOffset>600000){w.BaseStream.Position=before;w.BaseStream.SetLength(before);break;}valueCount++;}
            long afterValues=w.BaseStream.Position;w.BaseStream.Position=countOffset;w.Write(valueCount);w.BaseStream.Position=droppedOffset;w.Write(published.Count-valueCount);w.BaseStream.Position=afterValues;
            w.Write(changes.Count);foreach(var change in changes)InspectorWire.Change(w,change);
            w.Write(hits.Count);foreach(var hit in hits){w.Write(hit.Id);w.Write(hit.Kind);w.Write(hit.Distance);InspectorWire.Text(w,hit.Name,768);}
            w.Write(hasA);InspectorWire.Vector(w,pointA);w.Write(hasB);InspectorWire.Vector(w,pointB);
            w.Write((options&16)!=0?selectedColliders.Length:0);int totalPoints=0,totalParts=0;
            if((options&16)!=0)foreach(var c in selectedColliders){
                var trace=c==null?null:SceneGeometry.Inspect(c);w.Write(c==null?0:c.GetInstanceID());InspectorWire.Text(w,c==null?"Destroyed":c.GetType().Name,512);
                w.Write(c==null?0u:(c.enabled?1u:0u)|(c.gameObject.activeInHierarchy?2u:0u)|(c.isTrigger?4u:0u));
                InspectorWire.Text(w,trace==null?"Not present in the last static capture":trace.Reason,2048);w.Write(trace==null?-1:trace.Frame);w.Write(trace==null?0:trace.Seconds);
                foreach(float f in new[]{trace==null?0:trace.Low,trace==null?0:trace.High,trace==null?0:trace.Floor,trace==null?0:trace.Radius,trace==null?0:trace.Height})w.Write(f);
                w.Write(trace!=null&&trace.Grounded);w.Write(trace!=null&&trace.InactiveAccepted);
                var parts=new List<MapFootprint>();if(trace!=null&&(options&2)!=0)foreach(var p in trace.Parts)if(totalParts<InspectorWire.MaxParts&&totalPoints+p.Points.Length<=InspectorWire.MaxPoints){parts.Add(p);totalParts++;totalPoints+=p.Points.Length;}
                w.Write(parts.Count);w.Write(trace!=null&&(options&2)!=0&&parts.Count<trace.Parts.Count);foreach(var p in parts){w.Write(p.Flags);w.Write(p.Points.Length);foreach(var v in p.Points)InspectorWire.Vector(w,v);}
            }
        });channel.QueueDetail(bytes);detailMs=(Stopwatch.GetTimestamp()-started)*1000.0/Stopwatch.Frequency;
    }
    public static void Draw(ColliderWireGeometry geometry){
        if(!Drawing)return;var self=instance;
        if(self.hoverObject!=null&&self.hover.Active&&self.hoverCollider!=null){ReplayColliderShape hovered;GL.Color(new Color(1,.78f,.3f));if(ReplayColliderShape.Capture(self.hoverCollider,out hovered))geometry.Draw(ReplayColliderShape.Kind(self.hoverCollider),hovered);}
        if((self.options&1)!=0)foreach(var c in self.selectedColliders){if(c==null)continue;GL.Color(c.enabled&&c.gameObject.activeInHierarchy?new Color(.28f,.95f,.86f):new Color(1,.65f,.25f));ReplayColliderShape shape;
            if(ReplayColliderShape.Capture(c,out shape))geometry.Draw(ReplayColliderShape.Kind(c),shape);
            else{MeshWire wire;var mesh=c as MeshCollider;if(mesh!=null&&!mesh.convex&&self.meshes.TryGetValue(c.GetInstanceID(),out wire)&&wire.Source==mesh.sharedMesh)foreach(var line in wire.Lines){GL.Vertex(c.transform.TransformPoint(line.A));GL.Vertex(c.transform.TransformPoint(line.B));}}
        }
        if((self.options&2)!=0){GL.Color(new Color(.86f,.46f,1));int points=0;foreach(var c in self.selectedColliders){var trace=SceneGeometry.Inspect(c);if(trace==null)continue;foreach(var p in trace.Parts){if(points+p.Points.Length>4096)break;points+=p.Points.Length;for(int i=0;i<p.Points.Length;i++){var a=p.Points[i];var b=p.Points[(i+1)%p.Points.Length];a.y=b.y=trace.Floor+.03f;GL.Vertex(a);GL.Vertex(b);}}}}
        GL.Color(new Color(1,.8f,.35f));if(self.selected!=null){var center=self.selected.transform.position;foreach(var axis in new[]{Vector3.right,Vector3.up,Vector3.forward}){GL.Vertex(center-axis*.15f);GL.Vertex(center+axis*.15f);}}
        if(self.hasA){GL.Vertex(self.pointA);GL.Vertex(self.pointA+Vector3.up*.7f);}if(self.hasB){GL.Vertex(self.pointB);GL.Vertex(self.pointB+Vector3.up*.7f);}
    }
    public void Dispose(){active=false;if(ownsFreecam){FreeCam.DisableActive();ownsFreecam=false;}ResetSelection();nodes.Clear();rows.Clear();scan.Clear();visuals.Clear();catalogBuilt=false;if(channel!=null){channel.Dispose();channel=null;}if(instance==this)instance=null;}
}
