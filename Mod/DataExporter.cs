using System;
using System.Collections.Generic;
using System.Reflection;
using UnityEngine;
using UnityEngine.AI;
using UnityEngine.SceneManagement;

public sealed class DataExporter : MonoBehaviour
{
    private MouseLook mouse;
    private Transform playerTr;
    private MonsterController monsterClassic;
    private GhostScript ghost;
    private HideAndSeekIt hideSeek;
    private Monster3 monsterTimer;
    private WallAI[] timerActors=new WallAI[0];
    private static readonly FieldInfo crouchField = typeof(MouseLook).GetField("isCrouching", BindingFlags.NonPublic | BindingFlags.Instance);
    public static bool Crouching(MouseLook player){return player!=null&&crouchField!=null&&(bool)crouchField.GetValue(player);}
    private readonly List<Rect> wallBoxes = new List<Rect>();
    private WallOutline[] wallOutlines=new WallOutline[0];
    private readonly List<Rect> rectanglesA = new List<Rect>(), rectanglesB = new List<Rect>();
    private Collider[] sceneColliders = new Collider[0];
    private double nextDiscovery;
    private readonly List<Collider> oob = new List<Collider>();
    private readonly Dictionary<GameObject, Collider[]> crystalColliders = new Dictionary<GameObject, Collider[]>();
    private readonly List<GameObject> removed = new List<GameObject>();
    private GameObject cachedMonster;
    private Collider[] monsterColliders = new Collider[0];
    private Collider[] playerColliders = new Collider[0];
    private readonly HashSet<Collider> includedHitboxes=new HashSet<Collider>();
    private string hitboxDetails="";
    private double nextHitboxDetails;
    private readonly Vector3[] pathPoints = new Vector3[20];
    private int pathCount, wallOverflow;
    private int boundScene = -1;
    private double nextScan, nextPath, lastSample;
    private Vector3 previousPosition;
    private float smoothedSpeed;
    private bool mapInitialized;
    private bool scannedGeometry, scannedWalls;
    private GameObject pathMonster;
    private NavMeshAgent pathAgent;
    private float minX, maxX, minZ, maxZ;

    public void Bind(MouseLook player)
    {
        int scene = SceneManager.GetActiveScene().handle;
        if (mouse == player && boundScene == scene) return;
        boundScene = scene;
        mouse = player; playerTr = PlayerContext.Body(player);
        monsterClassic = mouse.monsterScript;
        previousPosition = playerTr.position; lastSample = Time.realtimeSinceStartupAsDouble;
        smoothedSpeed = 0; pathCount = 0; monsterColliders = new Collider[0];
        nextScan = 0; nextPath = 0; mapInitialized = false;
        scannedGeometry = scannedWalls = false; pathMonster = null; pathAgent = null;
        sceneColliders = new Collider[0]; timerActors=new WallAI[0]; nextDiscovery = 0;
        wallBoxes.Clear(); oob.Clear(); crystalColliders.Clear(); cachedMonster = null;
        playerColliders=GetComponentsInChildren<Collider>(true);nextHitboxDetails=0;
    }
    public void ResetMapBounds() { SceneGeometry.Reset();mapInitialized = false; nextScan = nextDiscovery = 0; }
    public void NotifyTeleport() { ResetMapBounds(); if(playerTr != null) previousPosition=playerTr.position;lastSample=Time.realtimeSinceStartupAsDouble;smoothedSpeed=0; }
    private static bool Live(Collider collider)
    {
        return collider != null && collider.enabled && collider.gameObject.activeInHierarchy && collider.gameObject.scene.IsValid() && collider.gameObject.scene.isLoaded;
    }
    private static bool Finite(float value) { return !float.IsNaN(value) && !float.IsInfinity(value); }
    private void Scan(bool geometry, bool includeWalls, bool includeMonster)
    {
        if(includeMonster)
        {
            // Searching the entire scene for enemy types absent from this
            // mode was repeated every second. Discover only the active mode.
            if(mouse.GhostMode){if(ghost==null || !ghost.gameObject.activeInHierarchy)ghost=FindObjectOfType<GhostScript>();}
            else if(mouse.HideAndSeekMode){if(hideSeek==null || !hideSeek.gameObject.activeInHierarchy)hideSeek=FindObjectOfType<HideAndSeekIt>();}
            else if(mouse.TimerMode){if(monsterTimer==null || !monsterTimer.gameObject.activeInHierarchy)monsterTimer=FindObjectOfType<Monster3>();timerActors=FindObjectsOfType<WallAI>();}
            else if(mouse.monsterScript != null)monsterClassic=mouse.monsterScript;
            else if(monsterClassic==null || !monsterClassic.gameObject.activeInHierarchy)monsterClassic=FindObjectOfType<MonsterController>();
        }
        removed.Clear();
        foreach (var pair in crystalColliders) if (pair.Key == null) removed.Add(pair.Key);
        foreach (GameObject key in removed) crystalColliders.Remove(key);
        if (!geometry) { scannedGeometry = scannedWalls = false; return; }
        scannedGeometry = true;
        scannedWalls = includeWalls;
        int wallsLayer = LayerMask.NameToLayer("Walls");
        oob.Clear();
        List<Rect> rectangles=rectanglesA,scratch=rectanglesB;rectangles.Clear();
        double now=Time.realtimeSinceStartupAsDouble;
        if(now>=nextDiscovery) {sceneColliders=FindObjectsOfType<Collider>();playerColliders=GetComponentsInChildren<Collider>(true);nextDiscovery=now+2;}
        // This scan runs at most once per second. Filter out prefab/assets and
        // inactive scene objects; bounds come from actual live colliders.
        foreach (Collider collider in sceneColliders)
        {
            if (!Live(collider)||collider.gameObject.scene.handle!=boundScene) continue;
            if (HitboxDrawer.Enabled && collider.CompareTag("OutOfBounds")) oob.Add(collider);
            if (!includeWalls) continue;
            if(collider.isTrigger)continue;
            if(mouse.TimerMode && monsterTimer!=null && monsterTimer.cieling!=null && (collider.transform==monsterTimer.cieling.transform || collider.transform.IsChildOf(monsterTimer.cieling.transform)))continue;
            if((wallsLayer<0 || collider.gameObject.layer!=wallsLayer) && !(mouse.TimerMode&&collider.CompareTag("Wall")))continue;
            if(mouse.TimerMode&&collider.GetComponentInParent<WallAI>()!=null)continue;
            Bounds bounds = collider.bounds;
            Vector3 a = bounds.min, b = bounds.max;
            if (!Finite(a.x) || !Finite(a.z) || !Finite(b.x) || !Finite(b.z) || b.x <= a.x || b.z <= a.z) continue;
            // Centimeter quantization also matches native edge extraction.
            float x0 = Mathf.Round(a.x * 100f) / 100f, z0 = Mathf.Round(a.z * 100f) / 100f;
            float x1 = Mathf.Round(b.x * 100f) / 100f, z1 = Mathf.Round(b.z * 100f) / 100f;
            if (x1 > x0 && z1 > z0) rectangles.Add(Rect.MinMaxRect(x0,z0,x1,z1));
        }
        if (!includeWalls) return;
        if(mouse.HideAndSeekMode){
            SceneGeometry.Capture(mouse);wallOutlines=SceneGeometry.Current();var outlines=wallOutlines;wallBoxes.Clear();
            foreach(var outline in outlines){float x0=float.PositiveInfinity,z0=x0,x1=float.NegativeInfinity,z1=x1;foreach(var p in outline.Corners){x0=Mathf.Min(x0,p.x);x1=Mathf.Max(x1,p.x);z0=Mathf.Min(z0,p.z);z1=Mathf.Max(z1,p.z);}if(wallBoxes.Count<200)wallBoxes.Add(Rect.MinMaxRect(x0,z0,x1,z1));}
            wallOverflow=Math.Max(0,outlines.Length-200);mapInitialized=false;return;
        }
        for (int pass = 0; pass < 3; pass++)
        {
            Merge(rectangles,false,scratch);var swap=rectangles;rectangles=scratch;scratch=swap;
            Merge(rectangles,true,scratch);swap=rectangles;rectangles=scratch;scratch=swap;
        }
        wallOverflow = Mathf.Max(0, rectangles.Count - 200);
        int count = Mathf.Min(rectangles.Count, 200);
        bool changed = count != wallBoxes.Count;
        if (!changed) for (int i = 0; i < count; i++) if (rectangles[i] != wallBoxes[i]) { changed = true; break; }
        wallBoxes.Clear(); for (int i = 0; i < count; i++) wallBoxes.Add(rectangles[i]);
        if (changed) mapInitialized = false;
    }
    private static void Merge(List<Rect> input, bool horizontal, List<Rect> output)
    {
        input.Sort((a,b) =>
        {
            int c = (horizontal ? a.yMin : a.xMin).CompareTo(horizontal ? b.yMin : b.xMin);
            if (c != 0) return c;
            c = (horizontal ? a.yMax : a.xMax).CompareTo(horizontal ? b.yMax : b.xMax);
            return c != 0 ? c : (horizontal ? a.xMin : a.yMin).CompareTo(horizontal ? b.xMin : b.yMin);
        });
        output.Clear();
        foreach (Rect rect in input)
        {
            if (output.Count > 0)
            {
                Rect old = output[output.Count-1];
                bool aligned = horizontal ? Mathf.Abs(old.yMin-rect.yMin)<0.005f && Mathf.Abs(old.yMax-rect.yMax)<0.005f : Mathf.Abs(old.xMin-rect.xMin)<0.005f && Mathf.Abs(old.xMax-rect.xMax)<0.005f;
                bool adjacent = horizontal ? rect.xMin <= old.xMax+0.005f : rect.yMin <= old.yMax+0.005f;
                if (aligned && adjacent)
                {
                    output[output.Count-1] = Rect.MinMaxRect(Mathf.Min(old.xMin,rect.xMin),Mathf.Min(old.yMin,rect.yMin),Mathf.Max(old.xMax,rect.xMax),Mathf.Max(old.yMax,rect.yMax));
                    continue;
                }
            }
            output.Add(rect);
        }
    }
    private void Include(float x, float z)
    {
        if (!Finite(x) || !Finite(z)) return;
        minX = Mathf.Min(minX,x-2f); maxX = Mathf.Max(maxX,x+2f);
        minZ = Mathf.Min(minZ,z-2f); maxZ = Mathf.Max(maxZ,z+2f);
    }
    public void WriteSnapshot(SnapshotBuffer data, bool includeMap = true, bool includePath = true, bool includeWalls = true,
        bool includeCrystals = true, bool includeMonster = true)
    {
        HitboxDrawer.HitboxCount = 0;
        includedHitboxes.Clear();
        if (mouse == null || playerTr == null) return;
        double now = Time.realtimeSinceStartupAsDouble;
        includeWalls &= includeMap;
        includeCrystals |= HitboxDrawer.Enabled;includeMonster |= HitboxDrawer.Enabled || includePath;
        bool geometry = includeWalls || HitboxDrawer.Enabled;
        if (now >= nextScan || (geometry && !scannedGeometry) || (includeWalls && !scannedWalls)) { Scan(geometry, includeWalls,includeMonster); nextScan = now + 1f; }
        Vector3 pos = playerTr.position;
        data.Write(SharedLayout.PlayerX,pos.x); data.Write(SharedLayout.PlayerY,pos.y); data.Write(SharedLayout.PlayerZ,pos.z); data.Write(SharedLayout.PlayerRotY,playerTr.eulerAngles.y);
        data.Write(SharedLayout.Objectives,mouse.Objectives); data.Write(SharedLayout.TotalObjectives,mouse.TotalObjectives); data.Write(SharedLayout.Speed,mouse.speed);
        data.Write(SharedLayout.CanMove,mouse.canMove); data.Write(SharedLayout.IsSprinting,mouse.isSprinting ? 1 : 0); data.Write(SharedLayout.Health,MouseLook.Health);
        data.Write(SharedLayout.CameraMode,mouse.CameraMode); data.Write(SharedLayout.CameraBright,mouse.CameraModeBright); data.Write(SharedLayout.HideAndSeekMode,mouse.HideAndSeekMode);
        data.Write(SharedLayout.HideAndSeekOutside,mouse.HideAndSeekOutside); data.Write(SharedLayout.TimerMode,mouse.TimerMode); data.Write(SharedLayout.GhostMode,mouse.GhostMode);
        data.Write(SharedLayout.ClassicBonus,mouse.ClassicBonus); data.Write(SharedLayout.IsCrouching,Crouching(mouse));
        if (!mapInitialized) { minX = maxX = pos.x; minZ = maxZ = pos.z; }
        Include(pos.x,pos.z);
        int crystalCount = 0;
        // Keep the player visible even when the scene fills the collider budget.
        if(HitboxDrawer.Enabled)
        {
            AddHitbox(mouse.controller,HitboxDrawer.HitboxType.Player);
            foreach(Collider collider in playerColliders)AddHitbox(collider,collider!=null&&collider.isTrigger?HitboxDrawer.HitboxType.Pickup:HitboxDrawer.HitboxType.Player);
        }
        var gems = mouse.gemsList;
        if (includeCrystals && gems != null)
        {
            for (int i = 0; i < gems.Count; i++)
            {
                GameObject gem = gems[i];
                if (gem == null || !gem.activeInHierarchy) continue;
                Vector3 cp = gem.transform.position; Include(cp.x,cp.z);
                if (crystalCount < 100) { data.Write(SharedLayout.CrystalPositions+crystalCount*8,cp.x); data.Write(SharedLayout.CrystalPositions+4+crystalCount*8,cp.z); crystalCount++; }
                if (!HitboxDrawer.Enabled) continue;
                Collider[] colliders;
                if (!crystalColliders.TryGetValue(gem,out colliders)) { colliders = gem.GetComponentsInChildren<Collider>(true); crystalColliders[gem] = colliders; }
                // Draw real colliders in both modes; basic mode renders their AABB.
                foreach (Collider collider in colliders) AddHitbox(collider,collider!=null && collider.CompareTag("Point")?HitboxDrawer.HitboxType.CrystalPickup:HitboxDrawer.HitboxType.Crystal);
            }
        }
        data.Write(SharedLayout.CrystalCount,crystalCount);
        data.Write(SharedLayout.WallCount,includeWalls ? wallBoxes.Count : 0); data.Write(SharedLayout.WallOverflowCount,includeWalls ? wallOverflow : 0);
        if(includeWalls&&mouse.HideAndSeekMode){var outlines=wallOutlines;int count=Math.Min(200,outlines.Length);data.Write(SharedLayout.WallOutlineCount,count);
            for(int i=0;i<count;i++){data.Write(SharedLayout.WallOutlineApproximate+i,outlines[i].Approximate);for(int j=0;j<4;j++){data.Write(SharedLayout.WallOutlines+(i*8+j*2)*4,outlines[i].Corners[j].x);data.Write(SharedLayout.WallOutlines+(i*8+j*2+1)*4,outlines[i].Corners[j].z);}}}
        for (int i = 0; includeWalls && i < wallBoxes.Count; i++)
        {
            Rect rect = wallBoxes[i]; int offset = SharedLayout.WallBoxes+i*16;
            data.Write(offset,rect.xMin); data.Write(offset+4,rect.yMin); data.Write(offset+8,rect.xMax); data.Write(offset+12,rect.yMax);
            Include(rect.xMin,rect.yMin); Include(rect.xMax,rect.yMax);
        }
            data.Write(SharedLayout.TimerMonsterCount,0);data.Write(SharedLayout.TimerMonsterOverflow,0);
            // Common panel uses the nearest physical actor in Timer mode.
            bool monActive = false;
            float monX = 0f, monZ = 0f;
            byte monSneak = 0, monSpotted = 0;
            int monStrikes = 0, monMaxStrikes = 0;
            float monTimer = 0f;
            GameObject monObj = null;WallAI nearestTimerActor=null;

            if (includeMonster && mouse.HideAndSeekMode && hideSeek != null)
            {
                monActive = true;
                monX = hideSeek.transform.position.x; monZ = hideSeek.transform.position.z;
                monSneak = (byte)(hideSeek.hiding ? 1 : 0);
                monSpotted = (byte)(hideSeek.FoundPlayer ? 1 : 0);
                monStrikes = hideSeek.ChaseCount; monMaxStrikes = 3;
                monTimer = hideSeek.SpottedCounter;
                monObj = hideSeek.gameObject;
            }
            else if (includeMonster && mouse.TimerMode && monsterTimer != null)
            {
                // Monster3 is a controller, not a physical enemy. Export the
                // actual WallAI actors and use the closest one for the panel/path.
                int count=0,total=0;float closest=float.PositiveInfinity;
                foreach(WallAI actor in timerActors)
                {
                    if(actor==null || !actor.gameObject.activeInHierarchy || actor.gameObject.scene.handle!=boundScene)continue;
                    Vector3 at=actor.transform.position;if(!Finite(at.x)||!Finite(at.z))continue;
                    byte state=(byte)(actor.angry?2:actor.isSpotted?1:0);
                    if(count<128) {
                        data.Write(SharedLayout.TimerMonsterPositions+count*8,at.x);data.Write(SharedLayout.TimerMonsterPositions+count*8+4,at.z);
                        data.Write(SharedLayout.TimerMonsterIds+count*4,actor.GetInstanceID());data.Write(SharedLayout.TimerMonsterStates+count,state);count++;
                    }
                    total++;float distance=(at-pos).sqrMagnitude;
                    if(distance<closest){closest=distance;nearestTimerActor=actor;monObj=actor.gameObject;monActive=true;monX=at.x;monZ=at.z;monSneak=(byte)(state==0?1:0);monSpotted=(byte)(state>0?1:0);}
                }
                data.Write(SharedLayout.TimerMonsterCount,count);data.Write(SharedLayout.TimerMonsterOverflow,Math.Max(0,total-count));
                monTimer = monsterTimer.ClockTime;
            }
            else if (includeMonster && mouse.GhostMode && ghost != null)
            {
                monActive = true;
                monX = ghost.transform.position.x; monZ = ghost.transform.position.z;
                monSneak = (byte)(ghost.GhostHidden ? 1 : 0);
                monSpotted = (byte)(ghost.isChasing ? 1 : 0);
                monStrikes = ghost.chaseCount; monMaxStrikes = 5;
                monTimer = ghost.detectionRange;
                monObj = ghost.gameObject;
            }
            else if (includeMonster && monsterClassic != null)
            {
                monActive = true;
                monX = monsterClassic.transform.position.x; monZ = monsterClassic.transform.position.z;
                monSneak = (byte)(monsterClassic.InSneakMode ? 1 : 0);
                monSpotted = (byte)(monsterClassic.HasBeenSpotted ? 1 : 0);
                monStrikes = monsterClassic.Strikes; monMaxStrikes = monsterClassic.maxStrikes;
                monTimer = monsterClassic.Timer;
                monObj = monsterClassic.gameObject;
            }

            data.Write(SharedLayout.MonsterX, monX); data.Write(SharedLayout.MonsterZ, monZ);
            data.Write(SharedLayout.MonsterSneak, monSneak); data.Write(SharedLayout.MonsterSpotted, monSpotted);
            data.Write(SharedLayout.MonsterStrikes, monStrikes); data.Write(SharedLayout.MonsterMaxStrikes, monMaxStrikes);
            data.Write(SharedLayout.MonsterTimer, monTimer); data.Write(SharedLayout.MonsterActive, monActive);

            if (includeMonster && ghost != null)
            {
                data.Write(SharedLayout.PlayerNoise, GhostScript.playerNoise);
                data.Write(SharedLayout.GhostHidden, (byte)(ghost.GhostHidden ? 1 : 0));
                data.Write(SharedLayout.GhostChasing, (byte)(ghost.isChasing ? 1 : 0));
                data.Write(SharedLayout.GhostHunting, (byte)(ghost.isHunting ? 1 : 0));
                data.Write(SharedLayout.GhostRunning, (byte)(ghost.isRunning ? 1 : 0));
                data.Write(SharedLayout.GhostChaseCount, ghost.chaseCount);
                data.Write(SharedLayout.GhostDetectionRange, ghost.detectionRange);
                data.Write(SharedLayout.GhostDefaultSpeed, ghost.defaultSpeed);
            }

            if (mouse.HideAndSeekMode && hideSeek != null)
            {
                data.Write(SharedLayout.ActiveModeId, (byte)1);
                data.Write(SharedLayout.HideSeek_hiding, (byte)(hideSeek.hiding ? 1 : 0));
                data.Write(SharedLayout.HideSeek_running, (byte)(hideSeek.running ? 1 : 0));
                data.Write(SharedLayout.HideSeek_foundPlayer, (byte)(hideSeek.FoundPlayer ? 1 : 0));
                data.Write(SharedLayout.HideSeek_seekingMode, (byte)(hideSeek.SeekingMode ? 1 : 0));
                data.Write(SharedLayout.HideSeek_chaseCount, hideSeek.ChaseCount);
                data.Write(SharedLayout.HideSeek_spottedCounter, hideSeek.SpottedCounter);
                data.Write(SharedLayout.HideSeek_chasingSpeed, hideSeek.ChasingSpeed);
                data.Write(SharedLayout.HideSeek_wallCoolDown, hideSeek.WallCoolDown);
                data.Write(SharedLayout.HideSeek_coolDownSpeed, hideSeek.CoolDownSpeed);
            }
            else if (mouse.TimerMode && monsterTimer != null)
            {
                data.Write(SharedLayout.ActiveModeId, (byte)2);
                data.Write(SharedLayout.Timer_active, (byte)(monsterTimer.active ? 1 : 0));
                data.Write(SharedLayout.Timer_playerIsSafe, (byte)(monsterTimer.playerIsSafe ? 1 : 0));
                data.Write(SharedLayout.Timer_clockTime, monsterTimer.ClockTime);
                data.Write(SharedLayout.Timer_clockStartingTime, monsterTimer.ClockStartingTime);
                data.Write(SharedLayout.Timer_distanceToClosestMonster, monsterTimer.distanceToClosestMonster);
                data.Write(SharedLayout.Timer_activeRadius, monsterTimer.activeRadius);
                data.Write(SharedLayout.Timer_speedMultiplier, monsterTimer.speedMultiplier);
                data.Write(SharedLayout.Timer_alwaysMakesSoundWhenMoving, (byte)(monsterTimer.AlwaysMakesSoundWhenMoving ? 1 : 0));
                data.Write(SharedLayout.Timer_playerIsStaring, (byte)(monsterTimer.playerIsStaring ? 1 : 0));
                data.Write(SharedLayout.Timer_randomEventInVault, (byte)(monsterTimer.randomEventInVault ? 1 : 0));
            }
            else if (mouse.GhostMode && ghost != null) data.Write(SharedLayout.ActiveModeId, (byte)3);
            else data.Write(SharedLayout.ActiveModeId, (byte)0);

        if (monActive) Include(monX,monZ);
        mapInitialized = true;
        data.Write(SharedLayout.MapMinX,minX); data.Write(SharedLayout.MapMaxX,maxX); data.Write(SharedLayout.MapMinZ,minZ); data.Write(SharedLayout.MapMaxZ,maxZ); data.Write(SharedLayout.MapBoundsValid,1);
        data.Write(SharedLayout.LevelSeed,PracticeRandom.Enabled ? PracticeRandom.BaseSeed : -1);
        data.Write(SharedLayout.SeedActive,PracticeRandom.Enabled);
        data.Write(SharedLayout.RngPatchComplete,PracticeRandom.SourcesVerified);
        if (now-lastSample >= 0.05f)
        {
            float dt = (float)(now-lastSample);
            Vector3 delta = pos-previousPosition;
            float raw = new Vector2(delta.x,delta.z).magnitude/dt;
            // Prevent teleports being reported as movement speed.
            smoothedSpeed = raw > 100f ? 0 : Mathf.Lerp(smoothedSpeed,raw,1f-Mathf.Exp(-dt*10f));
            previousPosition = pos; lastSample = now;
        }
        data.Write(SharedLayout.SmoothedSpeed,smoothedSpeed);
        if (!includePath) pathCount = 0;
        else if (now >= nextPath)
        {
            pathCount = 0;
            if (pathMonster != monObj) { pathMonster = monObj; pathAgent = monObj != null ? monObj.GetComponent<NavMeshAgent>() : null; }
            if(nearestTimerActor!=null)pathAgent=nearestTimerActor.agent;
            NavMeshAgent agent = pathAgent;
            if (agent != null && agent.enabled && agent.isOnNavMesh && agent.hasPath)
                pathCount = Mathf.Clamp(agent.path.GetCornersNonAlloc(pathPoints),0,pathPoints.Length);
            nextPath = now+0.1f;
        }
        data.Write(SharedLayout.PathCornerCount,pathCount);
        for (int i = 0; i < pathCount; i++) { data.Write(SharedLayout.PathCorners+i*8,pathPoints[i].x); data.Write(SharedLayout.PathCorners+4+i*8,pathPoints[i].z); }
        data.Write(SharedLayout.FreecamActive,FreeCam.FreeCamEnabled);
        Camera camera = FreeCam.SpectatorCamera;
        if (camera != null)
        {
            Vector3 cp = camera.transform.position;
            data.Write(SharedLayout.FreecamX,cp.x); data.Write(SharedLayout.FreecamY,cp.y); data.Write(SharedLayout.FreecamZ,cp.z);
        }
        if (HitboxDrawer.Enabled)
        {
            if (cachedMonster != monObj)
            {
                cachedMonster = monObj;
                monsterColliders = monObj != null ? monObj.GetComponentsInChildren<Collider>(true) : new Collider[0];
            }
            // Ghost exposes the exact dynamically resized kill collider.
            Collider ghostKill = mouse.GhostMode && ghost != null ? ghost.monsterKillTrigger : null;
            foreach (Collider collider in monsterColliders)
                if (collider != ghostKill) AddHitbox(collider,HitboxDrawer.HitboxType.Monster);
            AddHitbox(ghostKill,HitboxDrawer.HitboxType.KillTrigger);
            foreach (Collider collider in oob) AddHitbox(collider,HitboxDrawer.HitboxType.OutOfBounds);
            if(Time.realtimeSinceStartupAsDouble>=nextHitboxDetails)
            {
                nextHitboxDetails=Time.realtimeSinceStartupAsDouble+1;
                var cc=mouse.controller;
                hitboxDetails=cc==null?"Player controller unavailable":string.Format(System.Globalization.CultureInfo.InvariantCulture,
                    "Player capsule: radius {0:0.###}, height {1:0.###}, center ({2:0.###}, {3:0.###}, {4:0.###}), skin {5:0.###}. Player colliders: {6}. Crystal pickups use actual Point-tagged colliders.",
                    cc.radius,cc.height,cc.center.x,cc.center.y,cc.center.z,cc.skinWidth,playerColliders.Length);
            }
            data.WriteString(SharedLayout.HitboxDetails,hitboxDetails,192);
        }
    }
    private void AddHitbox(Collider collider, HitboxDrawer.HitboxType type)
    {
        if (!Live(collider) || !(collider is BoxCollider || collider is SphereCollider || collider is CapsuleCollider || collider is CharacterController) || HitboxDrawer.HitboxCount >= HitboxDrawer.Hitboxes.Length || !includedHitboxes.Add(collider)) return;
        HitboxDrawer.Hitboxes[HitboxDrawer.HitboxCount++] = new HitboxDrawer.HitboxInfo { collider = collider, type = type };
    }
    private void OnDestroy() { HitboxDrawer.HitboxCount = 0; }
}
