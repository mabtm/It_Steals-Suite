using System;
using System.Collections.Generic;
using System.Diagnostics;
using UnityEngine;
using UnityEngine.Rendering;

public sealed class InspectorPickHit : InspectorPickTarget
{
    public GameObject Target;public Collider Collider;public Vector3 Point;
}

// Shared, bounded ordering for hover, cycling and the verified click. A visible
// surface precedes collider-only volumes; duplicate components of one object
// remain one choice. Source geometry is preferred over an approximate bound.
public static class InspectorPickOrder
{
    private static int Precision(int kind){return kind==2?0:kind==1?1:2;}
    public static InspectorPickHit Resolve(List<InspectorPickHit> hits,int preferred){foreach(var hit in hits)if(preferred==0||hit.Id==preferred)return hit;return null;}
    public static bool Normalize(List<InspectorPickHit> hits,int maximum=32){
        var unique=new Dictionary<int,InspectorPickHit>();
        foreach(var hit in hits){if(hit==null||hit.Id==0||hit.Kind<1||hit.Kind>4||!InspectorPicking.Finite(hit.Distance)||hit.Distance<0||hit.Distance>10000)continue;InspectorPickHit old;
            if(!unique.TryGetValue(hit.Id,out old))unique[hit.Id]=hit;else if(Precision(hit.Kind)>Precision(old.Kind)||(Precision(hit.Kind)==Precision(old.Kind)&&hit.Distance<old.Distance)){if(hit.Collider==null)hit.Collider=old.Collider;unique[hit.Id]=hit;}else if(old.Collider==null)old.Collider=hit.Collider;}
        hits.Clear();hits.AddRange(unique.Values);hits.Sort((a,b)=>{int visible=(a.Kind==1?1:0).CompareTo(b.Kind==1?1:0);if(visible!=0)return visible;int distance=a.Distance.CompareTo(b.Distance);return distance!=0?distance:a.Id.CompareTo(b.Id);});
        bool partial=hits.Count>maximum;if(partial)hits.RemoveRange(maximum,hits.Count-maximum);return partial;
    }
}

// No proxies/colliders are added to the game. Bounds are a broad phase, then
// readable meshes/simple sprites use their triangles. Unreadable/large/skinned
// geometry explicitly remains a bounds estimate. Textured alpha is not sampled.
public sealed class InspectorVisualPicking
{
    private sealed class Surface {public Vector3[] Vertices;public int[] Indices;public int Kind;public UnityEngine.Object Resource;}
    private sealed class Entry {public GameObject Object;public Renderer Renderer;public int Id,Layer;public Bounds Bounds;public bool Enabled,Checked;public Surface Surface;public Sprite SpriteSource;public SpriteDrawMode SpriteMode;}
    private const int MaxBroad=128,MaxVertices=16384,MaxIndices=24576,MaxCache=8*1024*1024,MaxTriangles=32768;
    private readonly List<Entry> entries=new List<Entry>();
    private readonly Dictionary<string,Surface> surfaces=new Dictionary<string,Surface>();
    private readonly Entry[] broad=new Entry[MaxBroad];private readonly float[] distances=new float[MaxBroad];
    private int refresh,cacheBytes,captures,frame=-1;private bool truncated;public int Count{get{return entries.Count;}}
    public void Clear(){entries.Clear();surfaces.Clear();Array.Clear(broad,0,broad.Length);refresh=cacheBytes=captures=0;frame=-1;truncated=false;}
    public void Register(GameObject obj,Renderer renderer){if(obj==null||renderer==null)return;if(entries.Count>=20000){truncated=true;return;}var e=new Entry{Object=obj,Renderer=renderer,Id=obj.GetInstanceID(),Layer=obj.layer};Refresh(e);entries.Add(e);}
    private static void Refresh(Entry e){if(e.Renderer==null||e.Object==null){e.Enabled=false;return;}e.Enabled=e.Renderer.enabled&&e.Object.activeInHierarchy&&!e.Renderer.forceRenderingOff;e.Layer=e.Object.layer;if(e.Enabled)e.Bounds=e.Renderer.bounds;}
    public void Tick(int nextFrame){
        if(frame!=nextFrame){frame=nextFrame;captures=0;}if(entries.Count==0)return;long start=Stopwatch.GetTimestamp();int count=Math.Min(64,entries.Count);
        for(int i=0;i<count;i++){if(i>0&&(Stopwatch.GetTimestamp()-start)*1000.0/Stopwatch.Frequency>.35)break;if(refresh>=entries.Count)refresh=0;Refresh(entries[refresh++]);}
    }
    private Surface Geometry(Entry entry){
        var animated=entry.Renderer as SpriteRenderer;if(entry.Checked&&animated!=null&&(!object.ReferenceEquals(entry.SpriteSource,animated.sprite)||entry.SpriteMode!=animated.drawMode)){entry.Checked=false;entry.Surface=null;}
        if(entry.Checked)return entry.Surface;entry.Checked=true;
        try{
            var sprite=entry.Renderer as SpriteRenderer;
            if(sprite!=null){
                entry.SpriteSource=sprite.sprite;entry.SpriteMode=sprite.drawMode;if(sprite.sprite==null||sprite.drawMode!=SpriteDrawMode.Simple)return null;string key="S/"+sprite.sprite.GetInstanceID();Surface cached;if(surfaces.TryGetValue(key,out cached)&&object.ReferenceEquals(cached.Resource,sprite.sprite))return entry.Surface=cached;
                if(captures>=1){entry.Checked=false;return null;}captures++;
                var vertices=sprite.sprite.vertices;var indices=sprite.sprite.triangles;if(vertices.Length>MaxVertices||indices.Length>MaxIndices)return null;int bytes=vertices.Length*12+indices.Length*4;if(cacheBytes+bytes>MaxCache||surfaces.Count>=1024)return null;
                var surface=new Surface{Vertices=new Vector3[vertices.Length],Indices=new int[indices.Length],Kind=4,Resource=sprite.sprite};for(int i=0;i<vertices.Length;i++)surface.Vertices[i]=new Vector3(vertices[i].x,vertices[i].y,0);for(int i=0;i<indices.Length;i++)surface.Indices[i]=indices[i];
                if(!Valid(surface))return null;cacheBytes+=bytes;surfaces[key]=surface;return entry.Surface=surface;
            }
            if(!(entry.Renderer is MeshRenderer))return null;var filter=entry.Object.GetComponent<MeshFilter>();var mesh=filter==null?null:filter.sharedMesh;
            if(mesh==null||!mesh.isReadable||mesh.vertexCount>MaxVertices||mesh.subMeshCount>128)return null;
            int first=entry.Renderer.isPartOfStaticBatch?((MeshRenderer)entry.Renderer).subMeshStartIndex:0;int count=Math.Min(entry.Renderer.sharedMaterials.Length,mesh.subMeshCount-first);if(first<0||count<=0)return null;
            string meshKey="M/"+mesh.GetInstanceID()+"/"+first+"/"+count;Surface existing;if(surfaces.TryGetValue(meshKey,out existing)&&object.ReferenceEquals(existing.Resource,mesh))return entry.Surface=existing;
            if(captures>=1){entry.Checked=false;return null;}captures++;
            ulong total=0;for(int i=first;i<first+count;i++){if(mesh.GetTopology(i)!=MeshTopology.Triangles)return null;total+=mesh.GetIndexCount(i);if(total>MaxIndices)return null;}
            int size=mesh.vertexCount*12+(int)total*4;if(cacheBytes+size>MaxCache||surfaces.Count>=1024)return null;
            var result=new Surface{Vertices=mesh.vertices,Indices=new int[(int)total],Kind=3,Resource=mesh};int at=0;for(int i=first;i<first+count;i++){var part=mesh.GetTriangles(i);Array.Copy(part,0,result.Indices,at,part.Length);at+=part.Length;}
            if(!Valid(result))return null;cacheBytes+=size;surfaces[meshKey]=result;return entry.Surface=result;
        }catch{return null;}
    }
    private static bool Valid(Surface s){if(s.Indices.Length%3!=0)return false;foreach(var p in s.Vertices)if(!InspectorPicking.Finite(p))return false;foreach(int index in s.Indices)if(index<0||index>=s.Vertices.Length)return false;return true;}
    public bool Collect(Ray ray,Camera camera,Transform excluded,List<InspectorPickHit> hits){
        int count=0;bool partial=truncated;
        // This pass only touches cached managed data; engine properties are read
        // for the small broad-phase set, not for every scene object at 30 Hz.
        foreach(var entry in entries){float near;if(!entry.Enabled||!InspectorPicking.RayBounds(ray.origin,ray.direction,entry.Bounds.min,entry.Bounds.max,out near))continue;
            int at=count;while(at>0&&distances[at-1]>near)at--;if(at>=MaxBroad){partial=true;continue;}
            int end=Math.Min(count,MaxBroad-1);for(int i=end;i>at;i--){broad[i]=broad[i-1];distances[i]=distances[i-1];}broad[at]=entry;distances[at]=near;if(count<MaxBroad)count++;else partial=true;
        }
        int budget=MaxTriangles;
        for(int i=0;i<count;i++){
            var e=broad[i];broad[i]=null;if(e.Renderer==null||e.Object==null)continue;Refresh(e);if(!e.Enabled||(camera.cullingMask&(1<<e.Layer))==0)continue;
            var t=e.Object.transform;if(excluded!=null&&(t==excluded||t.IsChildOf(excluded)))continue;
            float distance;if(!InspectorPicking.RayBounds(ray.origin,ray.direction,e.Bounds.min,e.Bounds.max,out distance))continue;
            var surface=Geometry(e);int kind=2;
            if(surface!=null&&surface.Indices.Length/3<=budget){
                budget-=surface.Indices.Length/3;var matrix=e.Renderer.worldToLocalMatrix;var origin=matrix.MultiplyPoint3x4(ray.origin);var direction=matrix.MultiplyVector(ray.direction);var sprite=e.Renderer as SpriteRenderer;
                if(sprite!=null){if(sprite.flipX){origin.x=-origin.x;direction.x=-direction.x;}if(sprite.flipY){origin.y=-origin.y;direction.y=-direction.y;}}
                if(!InspectorPicking.RayTriangles(origin,direction,surface.Vertices,surface.Indices,out distance))continue;kind=surface.Kind;
            }
            hits.Add(new InspectorPickHit{Id=e.Id,Kind=kind,Distance=distance,Target=e.Object,Point=ray.GetPoint(distance),Name=e.Object.name,Type=e.Renderer.GetType().Name});
        }
        return partial;
    }
}
