using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using UnityEngine;
using UnityEngine.AI;
using UnityEngine.SceneManagement;

public sealed class SeedMapPlanner : IDisposable
{
    public static SeedMapPlanner Instance {get;private set;}
    public static readonly int SpawnSite=unchecked(typeof(MouseLook).GetMethod("Start",BindingFlags.Instance|BindingFlags.NonPublic|BindingFlags.Public).MetadataToken*31);
    private sealed class Catalog
    {
        public PracticeMapData Data;
        public GameObject[] Objects;public PickupRegion[] PickupSpheres;public uint[] PickupMasks;public Collider[] PickupCollectors;
        public MouseLook Player;
        public int AppliedSeed,Forced=-1;
        public bool SeedKnown,NeedsValidation,Geometry;
        public int StartedFrame;
        public double NextGeometry;public int GeometryAttempts;public UnityNavigationCapture Navigation;public double NavigationPublished;
        public UnityRngVerification Rng;public double RngPublished;
    }
    private readonly Dictionary<int,Catalog> catalogs=new Dictionary<int,Catalog>();
    private readonly PracticeMapChannel channel;
    private readonly GameBuildInfo build;
    private readonly int pid;
    private uint epoch;
    private Catalog capturing,current,display,job;
    private MouseLook capturePlayer;
    private bool pendingPublish,requested;
    private int jobSeed,jobRecipe,jobForced;private bool jobSkipNavigation;
    private ulong jobKey;
    private RandomSeedOrder jobOrder;
    private ulong jobCount,jobDone;
    private long jobBudgetTicks;
    private SearchLoadController loadController;
    private double previousGenerationMs;
    public float GenerationMilliseconds {get {return job==null?0:(float)(loadController==null?2:loadController.Milliseconds);}}
    private SpawnSelectionGenerator jobGenerator;
    private SeedSearchBatchWriter jobWriter;
    private int[] jobSelection;
    private long jobGenerationTicks;
    private ulong jobFrames;
    private byte[] pendingBatch;
    private int pendingBatchLength;
    private static readonly Func<int,int,int> UnityRange=UnityEngine.Random.Range;
    private sealed class ExpectedLayout {public int Scene;public Vector3 Spawn;public Vector3[] Crystals,Spawns,Points;}
    private ExpectedLayout expectedLaunch;
    private static bool Allowed {get {return SpeedrunManager.Instance!=null && !SpeedrunManager.Instance.Restricted;}}
    public SeedMapPlanner(int owner,GameBuildInfo gameBuild,uint session)
    {
        pid=owner;build=gameBuild;channel=new PracticeMapChannel(pid,session);Instance=this;
    }
    private static void CancelNavigation(Catalog c)
    {
        if(c.Navigation!=null)c.Navigation.Cancel();c.Navigation=null;c.Data.Flags&=~128;
        if(c.Data.NavigationStatus==1){c.Data.NavigationStatus=0;c.Data.NavigationModel=0;c.Data.NavigationNote="Reload the level to prepare Unity navigation.";}
    }
    public void SceneLoaded(uint sceneEpoch,int scene)
    {
        CloseReview();
        StopJob();if(current!=null){current.Data.Flags&=~8;CancelNavigation(current);}
        epoch=sceneEpoch;capturing=null;capturePlayer=null;current=null;
        if(expectedLaunch!=null&&expectedLaunch.Scene!=scene)expectedLaunch=null;
    }
    public static void BeforeStart(MouseLook __instance)
    {
        MouseLook mouse=__instance;
        var self=Instance;if(self==null || mouse==null) return;
        try
        {
            int scene=SceneManager.GetActiveScene().buildIndex;
            if(Array.IndexOf(SpeedrunManager.LevelIndices,scene)<0) return;
            self.capturePlayer=mouse;
            self.capturing=new Catalog {StartedFrame=Time.frameCount,Data=new PracticeMapData {Pid=self.pid,Scene=scene,Epoch=(int)self.epoch,CapturedUtc=DateTime.UtcNow.Ticks}};
        }
        catch(Exception error)
        {
            self.capturing=null;self.capturePlayer=null;self.expectedLaunch=null;
            MelonLoader.MelonLogger.Warning("Seed Map capture unavailable: "+error.Message);
        }
    }
    // Called instead of the game's own FindGameObjectsWithTag inside Start.
    // Normal runs keep the exact array; explicit Practice replays align it
    // with the captured catalog before the game's crystal deletion begins.
    public static GameObject[] FindTagged(string tag)
    {
        GameObject[] objects=GameObject.FindGameObjectsWithTag(tag);
        var self=Instance;
        if(self==null || self.capturing==null) return objects;
        try
        {
            var c=self.capturing;var d=c.Data;
            if(self.expectedLaunch!=null&&PracticeRandom.Enabled&&(tag=="Spawn"||tag=="Point")) {
                var expected=tag=="Spawn"?self.expectedLaunch.Spawns:self.expectedLaunch.Points;
                var indices=CatalogOrder.Match(expected,Positions(objects,tag=="Spawn"?128:1024));
                if(indices==null){d.Flags|=64;d.Message="Practice catalog changed; replay cannot match the chosen layout.";}
                else {var aligned=new GameObject[objects.Length];for(int i=0;i<aligned.Length;i++)aligned[i]=objects[indices[i]];objects=aligned;}
            }
            if(tag=="Spawn" && d.Spawns.Length==0)
            {
                d.Spawns=Positions(objects,128);
                if(self.capturePlayer.TimerMode) {d.Flags|=32;if(PlayerPrefs.GetInt("HasSpawnedInVaultC",0)==0 && objects.Length>4) c.Forced=4;}
            }
            if(tag=="Point" && c.Objects==null)
            {
                c.Objects=objects;d.Points=Positions(objects,1024);d.Keep=KeepCount(self.capturePlayer);
                c.PickupSpheres=new PickupRegion[objects.Length];c.PickupMasks=new uint[objects.Length];
                // MouseLook lives on Main Camera. Its own enabled trigger is a
                // box in Classic and a capsule in Hide & Seek. The controller
                // on First Person Player is only the movement collision shape.
                c.PickupCollectors=self.capturePlayer.GetComponents<Collider>();
                if(c.PickupCollectors.Length>32)c.PickupCollectors=new Collider[0];
                for(int i=0;i<objects.Length;i++){
                    var sphere=objects[i].GetComponent<SphereCollider>();
                    if(sphere==null||!sphere.enabled||!sphere.CompareTag("Point"))continue;
                    for(int k=0;k<c.PickupCollectors.Length;k++){
                        var sensor=c.PickupCollectors[k];if(!sensor.enabled||!sensor.gameObject.activeInHierarchy||!(sensor is BoxCollider||sensor is CapsuleCollider||sensor is SphereCollider)||!sensor.isTrigger&&!sphere.isTrigger)continue;
                        if(!Physics.GetIgnoreLayerCollision(sensor.gameObject.layer,sphere.gameObject.layer)&&!Physics.GetIgnoreCollision(sensor,sphere))c.PickupMasks[i]|=1u<<k;
                    }
                    var scale=sphere.transform.lossyScale;
                    c.PickupSpheres[i]=new PickupRegion{Verified=c.PickupMasks[i]!=0,Center=sphere.transform.TransformPoint(sphere.center),Radius=sphere.radius*Mathf.Max(Mathf.Abs(scale.x),Mathf.Abs(scale.y),Mathf.Abs(scale.z))};
                }
            }
        }
        catch(Exception e) {self.capturing.Data.Flags|=64;self.capturing.Data.Message="Catalog capture failed: "+e.Message;}
        return objects;
    }
    private static Vector3[] Positions(GameObject[] objects,int limit)
    {
        if(objects.Length>limit) throw new InvalidOperationException("Catalog exceeds supported point limit; preview disabled");
        var points=new Vector3[objects.Length];for(int i=0;i<points.Length;i++) points[i]=objects[i].transform.position;return points;
    }
    private static int KeepCount(MouseLook mouse)
    {
        if(mouse.CameraMode) return mouse.CameraModeBright?18:9;
        if(mouse.HideAndSeekMode) return mouse.HideAndSeekOutside?52:22;
        if(mouse.TimerMode) return 19;if(mouse.GhostMode) return 20;if(mouse.ClassicBonus) return 40;return 14;
    }
    public static void ObserveRng(int site,int applied,int baseSeed,bool forced,bool rawSpawn)
    {
        var self=Instance;if(self==null || self.capturing==null || site!=SpawnSite || self.capturing.SeedKnown) return;
        var c=self.capturing;c.AppliedSeed=applied;c.SeedKnown=true;
        c.Data.LiveSeed=forced?baseSeed:applied;c.Data.LiveRecipe=forced&&!rawSpawn?2:1;
        c.Data.SourceKind=forced?2:(HarmonyPatches.SupportedBuild&&PracticeRandom.SourcesVerified?1:0);
    }
    public static void AfterStart(MouseLook mouse)
    {
        var self=Instance;if(self==null || self.capturing==null || self.capturePlayer!=mouse) return;
        var c=self.capturing;var d=c.Data;c.Player=mouse;
        try
        {
            Vector3 position=(mouse.playerBody!=null?mouse.playerBody:mouse.transform).position;
            for(int i=0;i<d.Spawns.Length;i++) if((position-d.Spawns[i]).sqrMagnitude<.01f) {d.LiveSpawn=i;break;}
            var selected=new List<int>();
            foreach(GameObject gem in mouse.gemsList)
            {
                int index=Array.IndexOf(c.Objects??new GameObject[0],gem);
                if(index<0) {d.Flags|=64;continue;}selected.Add(index);
            }
            d.LiveSelected=selected.ToArray();d.Selected=(int[])d.LiveSelected.Clone();d.Spawn=d.LiveSpawn;
            d.Seed=d.LiveSeed;d.Recipe=0;d.Stage=1;d.Flags|=8;
            if(c.Forced>=0) d.Flags|=16;
            if(self.expectedLaunch!=null)
            {
                var expected=self.expectedLaunch;self.expectedLaunch=null;
                var actual=new Vector3[d.LiveSelected.Length];for(int i=0;i<actual.Length;i++)actual[i]=d.Points[d.LiveSelected[i]];
                if(expected.Scene!=d.Scene||d.LiveSpawn<0||(expected.Spawn-d.Spawns[d.LiveSpawn]).sqrMagnitude>.0004f||!SamePositions(expected.Crystals,actual))
                {d.Flags|=64;d.Message="Reloaded layout differed from the preview catalog. Captured positions are shown; prediction is disabled until the next fresh capture.";}
            }
            c.NeedsValidation=true;self.catalogs[d.Scene]=c;self.current=self.display=c;self.pendingPublish=true;
        }
        catch(Exception e) {d.Flags|=64;d.Stage=3;d.Message="Initial layout unavailable: "+e.Message;self.catalogs[d.Scene]=c;self.current=self.display=c;self.pendingPublish=true;}
        finally {c.Objects=null;self.capturing=null;self.capturePlayer=null;}
    }
    private SpawnSelection Simulate(Catalog c,int seed,int recipe,int forced)
    {
        var state=UnityEngine.Random.state;
        try
        {
            UnityEngine.Random.InitState(recipe==2?PracticeRandom.DeriveSeed(seed,c.Data.Scene,SpawnSite,0):seed);
            return SpawnSelection.Select(c.Data.Spawns.Length,c.Data.Points.Length,c.Data.Keep,forced,UnityEngine.Random.Range);
        }
        finally {UnityEngine.Random.state=state;}
    }
    public static bool SamePositions(Vector3[] expected,Vector3[] actual)
    {
        if(expected==null||actual==null||expected.Length!=actual.Length)return false;
        var used=new bool[actual.Length];
        foreach(Vector3 point in expected)
        {
            int match=-1;for(int i=0;i<actual.Length;i++)if(!used[i]&&(point-actual[i]).sqrMagnitude<=.0004f){match=i;break;}
            if(match<0)return false;used[match]=true;
        }
        return true;
    }
    public void ExpectPracticeLaunch(int scene,int seed,bool rawSpawn)
    {
        expectedLaunch=null;Catalog c;
        if(!Allowed||!catalogs.TryGetValue(scene,out c)||!c.Data.Verified)return;
        int forced=(c.Data.Flags&32)!=0&&PlayerPrefs.GetInt("HasSpawnedInVaultC",0)==0&&c.Data.Spawns.Length>4?4:-1;
        var selected=Simulate(c,seed,rawSpawn?1:2,forced);var points=new Vector3[selected.Crystals.Length];
        for(int i=0;i<points.Length;i++)points[i]=c.Data.Points[selected.Crystals[i]];
        expectedLaunch=new ExpectedLayout {Scene=scene,Spawn=c.Data.Spawns[selected.Spawn],Crystals=points,Spawns=c.Data.Spawns,Points=c.Data.Points};
    }
    private void Validate(Catalog c)
    {
        if(!c.NeedsValidation || !HarmonyPatches.SupportedBuild || !PracticeRandom.SourcesVerified) return;
        c.NeedsValidation=false;
        if(!c.SeedKnown || c.Data.LiveSpawn<0 || c.Data.Points.Length==0 || (c.Data.Flags&64)!=0) return;
        if(SpawnSelection.Matches(Simulate(c,c.AppliedSeed,1,c.Forced),c.Data.LiveSpawn,c.Data.LiveSelected))
        {
            c.Data.Flags|=1;c.Data.Message="Selection model matched the actual game spawn and crystal layout.";
        }
        else {c.Data.Flags|=64;c.Data.Message="Prediction differed from the game; seed previews and sampling are disabled.";}
        c.Data.Hash=build==null?"UNKNOWN":build.Snapshot().GameHash;pendingPublish=true;
    }
    private void CaptureGeometry(Catalog c)
    {
        // Other scene components can initialize walls/navigation after the
        // player's Start. Never freeze an incomplete same-frame capture.
        if(c.Geometry || c!=current || Time.frameCount<=c.StartedFrame || Time.realtimeSinceStartupAsDouble<c.NextGeometry) return;
        c.NextGeometry=Time.realtimeSinceStartupAsDouble+.25;c.GeometryAttempts++;
        // Initial spawn positions can be airborne. Do not freeze a projection
        // taken several units above the standing collision band.
        if(c.Player!=null&&c.Player.controller!=null&&!c.Player.controller.isGrounded&&c.GeometryAttempts<12)return;
        SceneGeometry.PlayerProfile(c.Player,c.Data);
        var nav=NavMesh.CalculateTriangulation();
        if(nav.vertices.Length<=30000 && nav.indices.Length<=180000) {c.Data.Vertices=nav.vertices;c.Data.Indices=nav.indices;}
        else {c.Data.Vertices=new Vector3[0];c.Data.Indices=new int[0];c.Data.Flags|=4;if(!c.Data.Message.Contains("NavMesh exceeds limit"))c.Data.Message+=" NavMesh exceeds limit; routing disabled.";}
        SceneGeometry.Capture(c.Player,c.Data,true);
        if(c.PickupSpheres!=null){
            var regions=new PickupRegion[c.PickupSpheres.Length];
            for(int i=0;i<regions.Length;i++){
                var p=c.PickupSpheres[i];float reach=0;
                if(p.Verified)for(int k=0;k<c.PickupCollectors.Length;k++)if((c.PickupMasks[i]&(1u<<k))!=0)reach=Math.Max(reach,MapPhysicalData.CollectorReach(c.Player,c.PickupCollectors[k],p));
                // Standing, level-camera core: conservative under every yaw.
                // Leave an overlap margin for discrete physics and small bob.
                regions[i]=new PickupRegion{Verified=p.Verified&&reach>.075f,Center=p.Center,Radius=Math.Max(0,reach-.075f)};
            }
            c.Data.PickupRegions=regions;
        }
        // Delayed NavMesh initialization must not permanently disable routing.
        c.Geometry=c.Data.Indices.Length>0 || (c.Data.Flags&4)!=0 || c.GeometryAttempts>=10;pendingPublish=true;
    }
    private void EnsureNavigation(Catalog c)
    {
        if(c!=current||!c.Data.Verified||c.Navigation!=null||c.Data.NavigationStatus==2||c.Data.NavigationStatus==3)return;
        if(c.Data.Spawns.Length+c.Data.Points.Length>UnityNavigationCapture.MaximumCatalog){c.Data.NavigationStatus=3;c.Data.NavigationNote="Catalog exceeds the bounded Unity capture limit; legacy routing remains available.";return;}
        if(c.Data.NavigationStatus!=1){c.Data.NavigationStatus=1;c.Data.NavigationModel=0;c.Data.Flags|=128;c.Data.NavigationQueries=0;c.Data.NavigationTotal=c.Data.Spawns.Length*c.Data.Points.Length+c.Data.Points.Length*(c.Data.Points.Length-1)/2;c.Data.NavigationNote="Waiting for loaded navigation.";pendingPublish=true;}
        if(!c.Geometry&&c.Data.Indices.Length==0)return;
        try {c.Navigation=new UnityNavigationCapture(c.Data,c.Player);pendingPublish=true;}
        catch(Exception error){c.Data.NavigationStatus=3;c.Data.NavigationModel=0;c.Data.Flags&=~128;c.Data.NavigationNodes=new Vector3[0];c.Data.NavigationEdges=c.Data.NavigationAnchors=new int[0];c.Data.NavigationNote=error.Message;if(SpeedrunManager.IsHideSeekScene(c.Data.Scene))MelonLoader.MelonLogger.Warning("Hide & Seek player routing unavailable: "+error.Message);}
    }
    private void VerifyCpuGenerator(Catalog c)
    {
        if(c!=current||!c.Data.Verified||c.Data.RngStatus==2||c.Data.RngStatus==3)return;
        try {
            if(c.Rng==null){c.Data.RngStatus=1;c.Data.RngSite=SpawnSite;c.Rng=new UnityRngVerification(c.Data,SpawnSite);}
            bool finished=c.Rng.Tick(c.Data);double now=Time.realtimeSinceStartupAsDouble;
            if(finished)c.Rng=null;
            if(finished||now-c.RngPublished>=.5){c.RngPublished=now;pendingPublish=true;}
        } catch(Exception error){c.Rng=null;c.Data.RngStatus=3;c.Data.RngNote="Original Unity generation: "+error.Message;pendingPublish=true;}
    }
    public void RefreshGeometryIfIdle()
    {
        if(!Allowed||current==null||job!=null||current.Data.SearchBackend==1)return;
        CancelNavigation(current);current.Data.NavigationStatus=0;current.Data.NavigationModel=0;current.Data.NavigationNodes=new Vector3[0];current.Data.NavigationEdges=current.Data.NavigationAnchors=new int[0];
        SceneGeometry.Reset();current.Geometry=false;current.GeometryAttempts=0;current.NextGeometry=0;current.Data.Flags&=~(2|4);
    }
    public void Request(TrainerCommands commands)
    {
        if(!Allowed || commands.PlannerRequest==0) return;
        if(commands.PlannerRequest==4 && (job??display)!=null)(job??display).Data.RequestSerial=commands.Serial;
        StopJob();
        if(commands.PlannerRequest==4) {if(display!=null){display.Data.Stage=1;display.Data.SearchBackend=0;}pendingPublish=true;return;}
        int level=(int)commands.PlannerLevel;if(level<0 || level>=SpeedrunManager.LevelIndices.Length) return;
        Catalog c;
        if(!catalogs.TryGetValue(SpeedrunManager.LevelIndices[level],out c))
        {
            display=null;pendingPublish=false;
            channel.Queue(new PracticeMapData {Pid=pid,Scene=SpeedrunManager.LevelIndices[level],Epoch=(int)epoch,RequestSerial=commands.Serial,Stage=3,Message="Load this Practice level once to capture its complete spawn catalog."});return;
        }
        display=c;pendingPublish=false;c.Data.RequestSerial=commands.Serial;
        Validate(c);CaptureGeometry(c);if((commands.PlannerOptions&8u)==0)EnsureNavigation(c);
        if(commands.PlannerRequest==2)
        {
            c.Data.SearchBackend=0;
            c.Data.Seed=c.Data.LiveSeed;c.Data.Recipe=0;c.Data.Selected=(int[])c.Data.LiveSelected.Clone();c.Data.Spawn=c.Data.LiveSpawn;
            c.Data.Stage=1;c.Data.Requested=c.Data.Completed=0;c.Data.Samples.Clear();c.Data.Flags&=~8;
            if(c==current)c.Data.Flags|=8;channel.Queue(c.Data);pendingPublish=false;return;
        }
        if(!c.Data.Verified || !HarmonyPatches.SupportedBuild || !PracticeRandom.SourcesVerified)
        {
            c.Data.Stage=3;c.Data.Message="Seed prediction needs a supported build and a successful comparison against the loaded game layout.";channel.Queue(c.Data);return;
        }
        c.Data.Samples.Clear();c.Data.Requested=c.Data.Completed=0;c.Data.SearchBackend=0;
        int recipe=(commands.PlannerOptions&1)!=0?1:2;
        int forced=(c.Data.Flags&32)!=0 && PlayerPrefs.GetInt("HasSpawnedInVaultC",0)==0 && c.Data.Spawns.Length>4?4:-1;
        var order=commands.PlannerRequest==3?new RandomSeedOrder(commands.PlannerSearchKey):null;
        int previewSeed=order==null?commands.PlannerSeed:order.SeedAt(0);
        var selection=Simulate(c,previewSeed,recipe,forced);
        c.Data.Seed=previewSeed;c.Data.Recipe=recipe;c.Data.Spawn=selection.Spawn;c.Data.Selected=selection.Crystals;c.Data.Stage=1;
        c.Data.Flags&=~(8|16);if(forced>=0)c.Data.Flags|=16;
        if(c==current && SpawnSelection.Matches(selection,c.Data.LiveSpawn,c.Data.LiveSelected)) c.Data.Flags|=8;
        if(commands.PlannerRequest==3&&(commands.PlannerOptions&16u)!=0&&c.Data.RngStatus==2)
        {c.Data.SearchBackend=1;c.Data.Stage=2;}
        else if(commands.PlannerRequest==3)
        {
            job=c;jobSkipNavigation=(commands.PlannerOptions&8u)!=0;jobSeed=previewSeed;jobRecipe=recipe;jobForced=forced;jobKey=commands.PlannerSearchKey;jobOrder=order;
            jobCount=Math.Max(1UL,Math.Min(4294967296UL,commands.PlannerSearchCount));jobDone=0;
            uint micros=Math.Max(250u,Math.Min(8000u,commands.PlannerBudgetMicros));
            jobBudgetTicks=Math.Max(1,Stopwatch.Frequency*(long)micros/1000000);
            loadController=new SearchLoadController((commands.PlannerOptions&4u)!=0,micros/1000.0,Time.unscaledDeltaTime*1000.0,Time.realtimeSinceStartupAsDouble);previousGenerationMs=0;
            jobGenerator=new SpawnSelectionGenerator(c.Data.Spawns.Length,c.Data.Points.Length,c.Data.Keep);
            jobSelection=new int[selection.Crystals.Length];jobGenerationTicks=0;jobFrames=0;
            jobWriter=new SeedSearchBatchWriter(new SeedSearchBatch {Pid=pid,Scene=c.Data.Scene,Epoch=c.Data.Epoch,RequestSerial=c.Data.RequestSerial,
                Seed=jobSeed,Recipe=jobRecipe,Total=jobCount,SearchKey=jobKey,Hash=c.Data.Hash},jobSelection.Length);
            pendingBatch=null;c.Data.Stage=2;
        }
        channel.Queue(c.Data);pendingPublish=false;
    }
    private void StopJob()
    {
        if(job==null)return;
        job.Data.Stage=1;pendingPublish=true;job=null;pendingBatch=null;jobGenerator=null;jobOrder=null;jobWriter=null;jobSelection=null;
    }
    public void Fail(Exception error)
    {
        StopJob();var c=display??current;if(c==null)return;
        CancelNavigation(c);c.Data.NavigationStatus=3;c.Data.NavigationModel=0;c.Data.NavigationNodes=new Vector3[0];c.Data.NavigationEdges=c.Data.NavigationAnchors=new int[0];
        c.NeedsValidation=false;c.Geometry=true;c.Data.Flags|=64;c.Data.Stage=3;
        string message=error.Message??"Unknown error";if(message.Length>500)message=message.Substring(0,500);
        c.Data.Message="Practice map failed: "+message;
        c.Data.Vertices=new Vector3[0];c.Data.Indices=new int[0];c.Data.Walls=new Rect[0];c.Data.Samples.Clear();c.Data.Requested=c.Data.Completed=0;
        pendingPublish=false;
        try {channel.Queue(c.Data);channel.Flush();}catch { /* Preserve the trainer if this optional channel is unavailable. */ }
    }
    public void Tick(bool wantMap,bool wantRoutes=false)
    {
        requested=wantMap&&Allowed;
        if(!HarmonyPatches.SupportedBuild||!PracticeRandom.SourcesVerified)
        {
            foreach(Catalog c in catalogs.Values)if(c.Data.Verified)
            {c.Data.Flags&=~1;if(c.Data.SourceKind!=2)c.Data.SourceKind=0;c.Data.Message="RNG/build coverage was lost. Prediction requires a fresh, verified level capture.";pendingPublish=true;}
            foreach(Catalog c in catalogs.Values)CancelNavigation(c);
            StopJob();
        }
        if(!requested)
        {
            if(display!=null&&display.Data.SearchBackend==1){display.Data.SearchBackend=0;display.Data.Stage=1;pendingPublish=true;}
            StopJob();if(pendingPublish && display!=null) {channel.Queue(display.Data);pendingPublish=false;}
            channel.Flush();return;
        }
        if(current!=null) {Validate(current);CaptureGeometry(current);if(wantRoutes&&job==null&&current.Data.SearchBackend==0)VerifyCpuGenerator(current);}
        if(current!=null&&current.Data.Verified&&wantRoutes&&display==current&&(job==null||job==current&&!jobSkipNavigation)){
            EnsureNavigation(current);
            if(current.Navigation!=null&&current.Data.NavigationStatus==1){
                bool finished;
                try {finished=current.Navigation.Tick();}
                catch(Exception error){current.Navigation.Cancel();current.Data.NavigationStatus=3;current.Data.NavigationModel=0;current.Data.NavigationNodes=new Vector3[0];current.Data.NavigationEdges=current.Data.NavigationAnchors=new int[0];current.Data.NavigationNote=error.Message;if(SpeedrunManager.IsHideSeekScene(current.Data.Scene))MelonLoader.MelonLogger.Warning("Hide & Seek player routing unavailable: "+error.Message);finished=true;}
                double now=Time.realtimeSinceStartupAsDouble;
                if(finished)current.Navigation=null;
                if(finished||now-current.NavigationPublished>=.5){current.NavigationPublished=now;pendingPublish=true;}
            }
        }
        // Publish the finished navigation catalog before the first seed batch.
        if(job!=null&&!jobSkipNavigation&&(job.Data.Flags&128)!=0){if(pendingPublish&&display!=null){channel.Queue(display.Data);pendingPublish=false;}channel.Flush();return;}
        if(job!=null&&jobDone==0&&pendingPublish&&display!=null){channel.Queue(display.Data);pendingPublish=false;channel.Flush();return;}
        bool readyForBatch=job!=null&&channel.ReadyForBatch();
        if(job!=null&&loadController!=null) {
            loadController.Observe(Time.unscaledDeltaTime*1000.0,previousGenerationMs,readyForBatch,Time.realtimeSinceStartupAsDouble);
            jobBudgetTicks=Math.Max(1,(long)(Stopwatch.Frequency*loadController.Milliseconds/1000));previousGenerationMs=0;
        }
        if(job!=null&&readyForBatch)
        {
            if(pendingBatch==null)pendingBatch=GenerateBatch();
            if(channel.TryPublishBatch(pendingBatch,pendingBatchLength))
            {
                pendingBatch=null;
                if(jobDone==jobCount){job.Data.Stage=1;job=null;jobGenerator=null;jobOrder=null;jobWriter=null;jobSelection=null;pendingPublish=true;}
            }
        }
        // The final catalog must not overwrite an unacknowledged last batch.
        if(pendingPublish&&display!=null&&job==null&&channel.ReadyForBatch())
        {channel.Queue(display.Data);pendingPublish=false;}
        channel.Flush();
    }
    public static PracticeMapData RunSnapshot(bool geometry=false)
    {
        var self=Instance;if(self==null||self.current==null)return null;
        var c=self.current;self.Validate(c);if(geometry)self.CaptureGeometry(c);
        var data=c.Data.Copy();data.Seed=c.AppliedSeed;data.Recipe=data.LiveRecipe=1;
        data.Movement=MovementCapture.ForMap(data);
        data.Spawn=data.LiveSpawn;data.Selected=(int[])data.LiveSelected.Clone();data.Stage=1;data.Requested=data.Completed=0;
        data.Samples.Clear();
        if(data.NavigationStatus!=2){data.NavigationStatus=0;data.NavigationModel=0;data.NavigationNodes=new Vector3[0];data.NavigationEdges=data.NavigationAnchors=new int[0];}data.Flags&=~128;
        return data;
    }
    private PracticeMapData review;
    public void Review(long id)
    {
        StopJob();
        var saved=RunReviewRecorder.Find(id);if(saved==null)return;
        review=saved.Copy();review.Pid=pid;review.Flags&=~8;review.SearchBackend=0;
        channel.Queue(review);pendingPublish=false;
    }
    public bool TickReview()
    {
        if(review==null)return false;
        if(SpeedrunManager.Instance!=null&&SpeedrunManager.Instance.Running){CloseReview();return false;}
        channel.Flush();return true;
    }
    public void CloseReview(){review=null;if(current!=null){display=current;pendingPublish=true;}}
    private byte[] GenerateBatch()
    {
        var d=job.Data;
        jobWriter.Begin(jobDone);int count=0;
        long started=Stopwatch.GetTimestamp();var saved=UnityEngine.Random.state;
        try
        {
            do
            {
                int seed=jobOrder.SeedAt(unchecked((uint)jobDone));
                UnityEngine.Random.InitState(jobRecipe==2?PracticeRandom.DeriveSeed(seed,d.Scene,SpawnSite,0):seed);
                int spawn=jobGenerator.NextInto(jobForced,UnityRange,jobSelection);
                jobWriter.Add(seed,spawn,jobSelection);jobDone++;count++;
            }
            while(jobDone<jobCount&&count<SeedSearchBatch.Maximum&&Stopwatch.GetTimestamp()-started<jobBudgetTicks);
        }
        finally {UnityEngine.Random.state=saved;}
        long elapsed=Stopwatch.GetTimestamp()-started;jobGenerationTicks+=elapsed;jobFrames++;previousGenerationMs=elapsed*1000.0/Stopwatch.Frequency;
        ulong micros=(ulong)(jobGenerationTicks/(double)Stopwatch.Frequency*1000000);
        return jobWriter.FinishView(micros,jobFrames,out pendingBatchLength);
    }
    public void Dispose() {CloseReview();foreach(Catalog c in catalogs.Values)CancelNavigation(c);job=null;pendingBatch=null;jobGenerator=null;jobOrder=null;jobWriter=null;jobSelection=null;channel.Dispose();if(Instance==this)Instance=null;}
}
