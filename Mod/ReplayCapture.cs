using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;
using UnityEngine.AI;

public struct ReplayFlash
{
    public double Seconds;
    public Vector3 Player;
    public int Kind, Actor;
}

public sealed class ReplaySignal
{
    private bool value;

    public ReplaySignal(bool initial = false)
    {
        value = initial;
    }

    public bool Rise(bool now)
    {
        bool changed = now && !value;
        value = now;
        return changed;
    }

    public void Set(bool now)
    {
        value = now;
    }
}

public sealed class ReplayPath
{
    public double Seconds;
    public Vector3[] Corners;
}

public sealed class ReplayMonster
{
    public int Id, Kind;
    public readonly List<RunPose> Trace = new List<RunPose>();
    public readonly List<ReplayPath> Paths = new List<ReplayPath>();
    private readonly List<RunPose> span = new List<RunPose>(32);

    public bool Sample(double seconds, Vector3 position, bool continuous)
    {
        var point = new RunPose { Seconds = seconds, Position = position, Continuous = continuous };
        bool merge = false;
        if (continuous && Trace.Count >= 2 && Trace[Trace.Count - 1].Continuous)
        {
            var first = Trace[Trace.Count - 2];
            double dt = seconds - first.Seconds;
            if (dt > 0 && dt <= 2 && span.Count < 32)
            {
                merge = true;
                foreach (var sample in span)
                {
                    var predicted = first.Position + (position - first.Position) *
                                                         (float)((sample.Seconds - first.Seconds) / dt);
                    if ((predicted - sample.Position).sqrMagnitude > .0004f)
                    {
                        merge = false;
                        break;
                    }
                }
            }
        }
        if (merge)
        {
            Trace[Trace.Count - 1] = point;
            span.Add(point);
            return false;
        }
        span.Clear();
        if (Trace.Count > 0)
            span.Add(Trace[Trace.Count - 1]);
        span.Add(point);
        Trace.Add(point);
        return true;
    }
}

// Passive observations. No destinations, movement, RNG or player settings are
// changed. Scene discovery is once per second; paths are inspected at 2 Hz.
public static class ReplayCapture
{
    private sealed class Actor
    {
        public Component Source;
        public Transform Body;
        public NavMeshAgent Agent;
        public ReplayMonster Record;
        public double LastObserved = -1;
        public bool TickHit;
        public ReplaySignal Contact = new ReplaySignal(), Stun = new ReplaySignal();
    }

    private static readonly Dictionary<int, Actor> actors = new Dictionary<int, Actor>();
    private static readonly List<Actor> ordered = new List<Actor>();
    private static int pathCursor, pathRemaining;
    private static NavMeshPath sampledPath;
    private static readonly Action<NavMeshAgent, NavMeshPath> copyPath = PathCopier();

    private static Action<NavMeshAgent, NavMeshPath> PathCopier()
    {
        try
        {
            var method = typeof(NavMeshAgent)
                             .GetMethod("CopyPathTo", System.Reflection.BindingFlags.Instance |
                                                          System.Reflection.BindingFlags.NonPublic |
                                                          System.Reflection.BindingFlags.Public);
            return method == null ? null
                                  : (Action<NavMeshAgent, NavMeshPath>)Delegate.CreateDelegate(
                                        typeof(Action<NavMeshAgent, NavMeshPath>), method);
        }
        catch (Exception)
        {
            return null;
        }
    }

    private static RunReviewData current;
    private static double nextDiscovery, nextPose, nextPath, poseInterval = .1, pathInterval = .5;
    private static int totalPoses, totalPaths;
    private static readonly Vector3[] corners = new Vector3[64];
    private static MouseLook recordedPlayer;
    private static ReplaySignal lamp, camera;
    private static int lampFrame = -1, lampPulse = -1, cameraPulse = -1;

    public static void Begin(RunReviewData review, MouseLook player = null)
    {
        current = review;
        recordedPlayer = player;
        lamp = new ReplaySignal(player != null && player.playerFlashlight != null &&
                                player.playerFlashlight.enabled);
        camera = new ReplaySignal(player != null && player.isCameraFlashing);
        lampFrame = lampPulse = cameraPulse = -1;
        actors.Clear();
        ordered.Clear();
        pathCursor = pathRemaining = 0;
        nextDiscovery = nextPose = nextPath = 0;
        poseInterval = .1;
        pathInterval = .5;
        totalPoses = totalPaths = 0;
    }

    public static void End()
    {
        current = null;
        recordedPlayer = null;
        actors.Clear();
        ordered.Clear();
        pathRemaining = 0;
    }

    private static Actor Bind(Component component, int kind)
    {
        if (current == null || component == null)
            return null;
        int key = component.gameObject.GetInstanceID();
        Actor found;
        if (actors.TryGetValue(key, out found))
            return found;
        if (actors.Count >= 128)
        {
            current.MonstersReduced = true;
            return null;
        }
        var record = new ReplayMonster { Id = actors.Count + 1, Kind = kind };
        current.Monsters.Add(record);
        var classic = component as MonsterController;
        var hide = component as HideAndSeekIt;
        var timer = component as WallAI;
        var agent = classic != null ? classic.agent
                    : hide != null  ? hide.agent
                    : timer != null ? timer.agent
                                    : component.GetComponent<NavMeshAgent>();
        found = new Actor { Source = component, Body = agent != null ? agent.transform : component.transform,
                            Agent = agent, Record = record };
        if (classic != null)
        {
            found.Contact.Set(classic.HasBeenSpotted);
            found.Stun.Set(stunnedField != null && (bool)stunnedField.GetValue(classic));
        }
        else if (timer != null)
            found.Contact.Set(timer.isSpotted);
        actors.Add(key, found);
        ordered.Add(found);
        ReplayColliderCapture.BindActor(component, found.Body, record.Id);
        ReplayVisualCapture.BindActor(component, found.Body, record.Id, kind);
        return found;
    }

    public static void Observe(MouseLook player, double seconds)
    {
        if (current == null || player == null)
            return;
        ObservePlayer(player); // Real game signals, independent of pose/path throttling.
        if (seconds >= nextDiscovery)
        {
            nextDiscovery = seconds + 1;
            if (player.HideAndSeekMode)
            {
                foreach (var c in UnityEngine.Object.FindObjectsOfType<HideAndSeekIt>())
                    Bind(c, 2);
            }
            else if (player.GhostMode)
            {
                foreach (var c in UnityEngine.Object.FindObjectsOfType<GhostScript>())
                    Bind(c, 3);
            }
            else if (player.TimerMode)
            {
                foreach (var c in UnityEngine.Object.FindObjectsOfType<WallAI>())
                    Bind(c, 4);
            }
            else if (player.monsterScript != null)
                Bind(player.monsterScript, 1);
        }
        foreach (var actor in ordered)
        {
            var classic = actor.Source as MonsterController;
            var timer = actor.Source as WallAI;
            // HasBeenSpotted is also forced by the endgame/battery logic.
            // Only its actual RayTest transition is a light contact. The
            // private stun state, in contrast, is written only by stun/recovery.
            if (classic != null)
            {
                current.LightSources |= 64;
                if (stunnedField != null && actor.Stun.Rise((bool)stunnedField.GetValue(classic)))
                    Contact(actor, 2);
            }
            else if (timer != null)
            {
                current.LightSources |= 16;
                if (actor.Contact.Rise(timer.isSpotted))
                    Contact(actor, 1);
            }
        }
        bool pose = seconds >= nextPose, path = seconds >= nextPath;
        if (!pose && !path && pathRemaining == 0)
            return;
        if (pose)
        {
            if (totalPoses + actors.Count > 32768)
            {
                totalPoses = 0;
                foreach (var a in actors.Values)
                {
                    var t = a.Record.Trace;
                    if (t.Count > 2)
                    {
                        for (int i = 1; i < t.Count / 2; i++)
                        {
                            var item = t[i * 2];
                            item.Continuous = false;
                            t[i] = item;
                        }
                        int count = t.Count / 2;
                        var last = t[t.Count - 1];
                        last.Continuous = false;
                        t.RemoveRange(count, t.Count - count);
                        t.Add(last);
                    }
                    totalPoses += t.Count;
                }
                poseInterval *= 2;
                current.MonstersReduced = true;
            }
            nextPose = seconds + poseInterval;
        }
        if (path && pathRemaining == 0)
        {
            nextPath = seconds + pathInterval;
            pathCursor = 0;
            pathRemaining = ordered.Count;
        }
        if (pose)
            foreach (var a in ordered)
            {
                if (a.Body == null || !a.Body.gameObject.activeInHierarchy)
                    continue;
                var position = a.Body.position;
                if (!Finite(position))
                {
                    current.MonstersReduced = true;
                    continue;
                }
                bool continuous = a.LastObserved >= 0 && seconds - a.LastObserved <= .3;
                var trace = a.Record.Trace;
                if (trace.Count > 0 && seconds > trace[trace.Count - 1].Seconds &&
                    (position - trace[trace.Count - 1].Position).magnitude /
                            (seconds - trace[trace.Count - 1].Seconds) >
                        40)
                    continuous = false;
                a.LastObserved = seconds;
                if (a.Record.Sample(seconds, position, continuous))
                    totalPoses++;
            }
        // Bound native path-copy work, especially Timer's many enemies.
        for (int work = 0; work < 4 && pathRemaining > 0; work++)
        {
            var a = ordered[pathCursor++];
            pathRemaining--;
            if (a.Body == null || !a.Body.gameObject.activeInHierarchy || a.Agent == null ||
                !a.Agent.enabled || !a.Agent.isOnNavMesh)
                continue;
            NavMeshPath nav;
            if (copyPath != null)
            {
                if (sampledPath == null)
                    sampledPath = new NavMeshPath();
                copyPath(a.Agent, sampledPath);
                nav = sampledPath;
            }
            else
                nav = a.Agent.path;
            int n = nav.GetCornersNonAlloc(corners);
            if (n >= corners.Length)
            {
                current.MonstersReduced = true;
                continue;
            }
            bool finite = true;
            for (int i = 0; i < n; i++)
                finite &= Finite(corners[i]);
            if (!finite)
            {
                current.MonstersReduced = true;
                continue;
            }
            var prior = a.Record.Paths.Count > 0 ? a.Record.Paths[a.Record.Paths.Count - 1].Corners : null;
            bool changed = prior == null || prior.Length != n;
            // The first corner follows the agent. Record topology changes;
            // the replay anchors this segment to the current enemy marker.
            if (!changed)
                for (int i = 1; i < n; i++)
                    if ((prior[i] - corners[i]).sqrMagnitude > .04f)
                    {
                        changed = true;
                        break;
                    }
            if (!changed)
                continue;
            if (totalPaths >= 1024)
            {
                totalPaths = 0;
                foreach (var actor in ordered)
                {
                    var list = actor.Record.Paths;
                    if (list.Count > 2)
                    {
                        for (int i = 1; i < list.Count / 2; i++)
                            list[i] = list[i * 2];
                        int count = list.Count / 2;
                        var last = list[list.Count - 1];
                        list.RemoveRange(count, list.Count - count);
                        list.Add(last);
                    }
                    totalPaths += list.Count;
                }
                pathInterval *= 2;
                current.MonstersReduced = true;
            }
            var points = new Vector3[n];
            Array.Copy(corners, points, n);
            a.Record.Paths.Add(new ReplayPath { Seconds = seconds, Corners = points });
            totalPaths++;
        }
    }

    public static int ActorId(Component source)
    {
        int kind = source is MonsterController ? 1
                   : source is HideAndSeekIt   ? 2
                   : source is GhostScript     ? 3
                   : source is WallAI          ? 4
                                               : 0;
        var actor = kind == 0 ? null : Bind(source, kind);
        return actor == null ? 0 : actor.Record.Id;
    }

    private static bool Finite(Vector3 p)
    {
        return !float.IsNaN(p.x) && !float.IsInfinity(p.x) && !float.IsNaN(p.y) && !float.IsInfinity(p.y) &&
               !float.IsNaN(p.z) && !float.IsInfinity(p.z) && Math.Abs(p.x) <= 100000 &&
               Math.Abs(p.y) <= 100000 && Math.Abs(p.z) <= 100000;
    }

    private static int Pulse(int kind, int actor = 0)
    {
        if (current == null || recordedPlayer == null || SpeedrunManager.Instance == null ||
            SpeedrunManager.Instance.ActiveRunId != current.Id)
            return -1;
        var body = PlayerContext.Body(recordedPlayer);
        if (body == null)
        {
            current.LightRejected++;
            return -1;
        }
        return current.LightEvent(SpeedrunManager.Instance.CurrentSeconds, body.position, kind, actor)
                   ? current.Flashes.Count - 1
                   : -1;
    }

    private static void Contact(Actor actor, int kind)
    {
        if (actor == null || current == null)
            return;
        if (kind == 2 && recordedPlayer != null && recordedPlayer.isCameraFlashing &&
            current.ConfirmLight(cameraPulse, actor.Record.Id))
            return;
        if (kind == 1 && lampFrame == Time.frameCount && current.ConfirmLight(lampPulse, actor.Record.Id))
            return;
        Pulse(kind, actor.Record.Id);
    }

    private static void ObservePlayer(MouseLook player)
    {
        if (current == null || player == null || player != recordedPlayer)
            return;
        current.LightSources |= 32;
        if (lamp.Rise(player.playerFlashlight != null && player.playerFlashlight.enabled))
        {
            lampPulse = Pulse(1);
            lampFrame = Time.frameCount;
        }
        if (camera.Rise(player.isCameraFlashing))
            cameraPulse = Pulse(2);
    }

    public static void AfterPlayerUpdate(MouseLook __instance)
    {
        if (current == null || __instance != recordedPlayer)
            return;
        current.LightSources |= 1;
        ObservePlayer(__instance);
    }

    public static void CameraStarted(MouseLook __instance)
    {
        if (current == null || __instance != recordedPlayer)
            return;
        current.LightSources |= 4;
        if (camera.Rise(true))
            cameraPulse = Pulse(2);
    }

    public static void BeforeHideTick(HideAndSeekIt __instance)
    {
        if (current == null)
            return;
        var a = Bind(__instance, 2);
        if (a != null)
            a.TickHit = false;
    }

    public static void BeforeHideLight(HideAndSeekIt __instance, out float __state)
    {
        __state = __instance.SpottedCounter;
    }

    public static void AfterHideLight(HideAndSeekIt __instance, float __state)
    {
        if (current == null)
            return;
        current.LightSources |= 2;
        var a = Bind(__instance, 2);
        if (a != null && __instance.SpottedCounter > __state)
            a.TickHit = true;
    }

    public static void AfterHideTick(HideAndSeekIt __instance)
    {
        if (current == null)
            return;
        var a = Bind(__instance, 2);
        if (a != null && a.Contact.Rise(a.TickHit))
            Contact(a, 1);
    }

    public static void BeforeClassicLight(MonsterController __instance, out bool __state)
    {
        __state = __instance.HasBeenSpotted;
    }

    public static void AfterClassicLight(MonsterController __instance, bool __state)
    {
        if (current == null)
            return;
        current.LightSources |= 8;
        var a = Bind(__instance, 1);
        if (a != null)
        {
            a.Contact.Set(__instance.HasBeenSpotted);
            if (!__state && __instance.HasBeenSpotted)
                Contact(a, recordedPlayer != null && recordedPlayer.CameraMode ? 2 : 1);
        }
    }

    private static readonly System.Reflection.FieldInfo stunnedField =
        typeof(MonsterController)
            .GetField("IsStunned", System.Reflection.BindingFlags.Instance |
                                       System.Reflection.BindingFlags.NonPublic |
                                       System.Reflection.BindingFlags.Public);

    public static void CameraStunned(MonsterController monster, bool value)
    {
        bool changed = value && !(bool)stunnedField.GetValue(monster);
        stunnedField.SetValue(monster, value);
        ReplayExperienceCapture.StateWritten(monster, value, "MonsterController.IsStunned");
        if (current == null)
            return;
        current.LightSources |= 128;
        var a = Bind(monster, 1);
        if (a != null)
        {
            a.Stun.Set(value);
            if (changed)
                Contact(a, 2);
        }
    }

    public static void TimerSpotted(WallAI monster, bool value)
    {
        bool changed = value && !monster.isSpotted;
        monster.isSpotted = value;
        ReplayExperienceCapture.StateWritten(monster, value, "WallAI.isSpotted");
        if (current == null)
            return;
        current.LightSources |= 16;
        var a = Bind(monster, 4);
        if (a != null)
        {
            a.Contact.Set(value);
            if (changed)
                Contact(a, 1);
        }
    }

    public static void Write(BinaryWriter w, RunReviewData r)
    {
        w.Write(r.ValidationChecked);
        w.Write(r.Valid);
        w.Write(r.ObservedFromStart);
        w.Write(r.SeedKnown);
        w.Write(r.SeedKind);
        w.Write(r.Aids);
        w.Write(r.Warnings);
        var text = System.Text.Encoding.UTF8.GetBytes(r.ValidityReason ?? "");
        w.Write(text.Length);
        w.Write(text);
        w.Write(r.MonstersReduced);
        w.Write(r.Monsters.Count);
        foreach (var m in r.Monsters)
        {
            w.Write(m.Id);
            w.Write(m.Kind);
            w.Write(m.Trace.Count);
            foreach (var p in m.Trace)
            {
                w.Write(p.Seconds);
                Vector(w, p.Position);
                w.Write(p.Continuous);
            }
            w.Write(m.Paths.Count);
            foreach (var p in m.Paths)
            {
                w.Write(p.Seconds);
                w.Write(p.Corners.Length);
                foreach (var c in p.Corners)
                    Vector(w, c);
            }
        }
        w.Write(r.Flashes.Count);
        foreach (var f in r.Flashes)
        {
            w.Write(f.Seconds);
            Vector(w, f.Player);
            w.Write(f.Kind);
            w.Write(f.Actor);
        }
        w.Write(r.LightSources);
        w.Write(r.LightRejected);
    }

    private static void Vector(BinaryWriter w, Vector3 p)
    {
        w.Write(p.x);
        w.Write(p.y);
        w.Write(p.z);
    }
}
