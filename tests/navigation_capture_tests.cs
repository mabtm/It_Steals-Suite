using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.AI;
// Controlled engine contract, original SDK Vector3; no live Unity process.
namespace UnityEngine {
    public class Object {public static readonly List<object> Objects=new List<object>();public static int Searches;public static T[] FindObjectsOfType<T>(){Searches++;var values=new List<T>();foreach(var x in Objects)if(x is T)values.Add((T)x);return values.ToArray();}}
    public class GameObject {public UnityEngine.SceneManagement.Scene scene;public GameObject(int scene){this.scene=new UnityEngine.SceneManagement.Scene{handle=scene};}}
}
namespace UnityEngine.SceneManagement {
    public struct Scene {public int handle;}
    public static class SceneManager {public static Scene GetActiveScene(){return new Scene{handle=13};}}
}
namespace UnityEngine.AI {
    public sealed class NavMeshData {}
    public sealed class NavMeshSurface {public static readonly List<NavMeshSurface> activeSurfaces=new List<NavMeshSurface>();public int agentTypeID;public bool isActiveAndEnabled=true;public NavMeshData navMeshData=new NavMeshData();public GameObject gameObject=new GameObject(13);}
    public sealed class NavMeshAgent {public int agentTypeID;public bool enabled=true;public GameObject gameObject=new GameObject(13);}
    public enum NavMeshPathStatus {PathComplete,PathPartial,PathInvalid}
    public struct NavMeshQueryFilter {public int agentTypeID,areaMask;}
    public struct NavMeshHit {public Vector3 position;}
    public sealed class NavMeshPath {public NavMeshPathStatus status=NavMeshPathStatus.PathComplete;public Vector3 a,b;public int GetCornersNonAlloc(Vector3[] output){output[0]=a;output[1]=b;return 2;}public void ClearCorners(){}}
    public static class NavMesh {
        public const int AllAreas=-1;public static int LoadedType;public static readonly List<int> Queries=new List<int>();
        public static bool SamplePosition(Vector3 point,out NavMeshHit hit,float radius,NavMeshQueryFilter filter){Queries.Add(filter.agentTypeID);hit=new NavMeshHit{position=point};return filter.agentTypeID==LoadedType;}
        public static bool CalculatePath(Vector3 a,Vector3 b,NavMeshQueryFilter filter,NavMeshPath path){Queries.Add(filter.agentTypeID);path.a=a;path.b=b;return filter.agentTypeID==LoadedType;}
    }
}
public sealed class MouseLook {}
public sealed class PracticeMapData {
    public Vector3[] Spawns={new Vector3(0,0,0)},Points={new Vector3(5,0,0),new Vector3(10,0,0)},NavigationNodes;
    public int[] NavigationEdges,NavigationAnchors;public int NavigationModel,NavigationStatus,Flags,NavigationQueries,NavigationTotal;public string NavigationNote;
}
public static class NavigationCaptureTests {
    private static void Check(bool condition,string message){if(!condition)throw new Exception(message);}
    private static void Reset(int type){UnityEngine.Object.Objects.Clear();UnityEngine.Object.Searches=0;NavMeshSurface.activeSurfaces.Clear();NavMesh.Queries.Clear();NavMesh.LoadedType=type;}
    private static void Capture(int type){
        var data=new PracticeMapData();var capture=new UnityNavigationCapture(data);int ticks=0;while(!capture.Tick()&&++ticks<100){}
        Check(data.NavigationStatus==2&&(data.Flags&128)==0,"capture completes without a stuck pending flag");
        Check(data.NavigationAnchors.Length==3&&Array.TrueForAll(data.NavigationAnchors,x=>x>=0)&&data.NavigationEdges.Length>=4,"all anchors and corridors use the loaded navigation data");
        Check(NavMesh.Queries.Count>=6&&NavMesh.Queries.TrueForAll(x=>x==type),"every real production query uses the selected scene surface type");
    }
    public static void Main(){
        // These are the actual supplied Outside surface / spider agent IDs.
        const int outside=-334000983;Reset(outside);
        UnityEngine.Object.Objects.Add(new NavMeshAgent{agentTypeID=0});UnityEngine.Object.Objects.Add(new NavMeshAgent{agentTypeID=outside});
        NavMeshSurface.activeSurfaces.Add(new NavMeshSurface{agentTypeID=0,gameObject=new GameObject(5)});
        NavMeshSurface.activeSurfaces.Add(new NavMeshSurface{agentTypeID=0,isActiveAndEnabled=false});
        NavMeshSurface.activeSurfaces.Add(new NavMeshSurface{agentTypeID=0,navMeshData=null});
        NavMeshSurface.activeSurfaces.Add(new NavMeshSurface{agentTypeID=outside});Capture(outside);Check(UnityEngine.Object.Searches==0,"a loaded surface is preferred to unrelated monster agents");
        Reset(0);NavMeshSurface.activeSurfaces.Add(new NavMeshSurface{agentTypeID=0});UnityEngine.Object.Objects.Add(new NavMeshAgent{agentTypeID=outside});Capture(0);
        Reset(outside);UnityEngine.Object.Objects.Add(new NavMeshAgent{agentTypeID=0,enabled=false});UnityEngine.Object.Objects.Add(new NavMeshAgent{agentTypeID=0,gameObject=new GameObject(5)});UnityEngine.Object.Objects.Add(new NavMeshAgent{agentTypeID=outside});Capture(outside);
        Reset(0);Capture(0);
        Console.WriteLine("Production Unity navigation contract: actual Outside surface type vs first spider, current scene / enabled / loaded surface gates, unchanged regular type, valid fallback, all anchors / corridor queries and pending completion: PASS");
    }
}
