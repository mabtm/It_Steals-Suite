using System;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;
using HarmonyLib;
using UnityEngine;

// Inspect the original game's decisions; never repeat a physics query, choose
// a random sound, advance an Animator or change a gameplay field here.
public static class ReplayExperienceCapture
{
    private sealed class Animation
    {
        public Animator Source;
        public SkinnedMeshRenderer Skin;
        public ReplayAnimationTrack Track;
        public ReplayAnimationFrame Last;
        public bool Seen;
        public double Next, DetailUntil;
        public readonly List<AnimatorClipInfo> Clips = new List<AnimatorClipInfo>(4);
    }

    private sealed class Gate
    {
        public int Frame = -1, Count;
        public double Next;
    }

    private static ReplayCoverage coverage;
    private static ReplayExperienceData data;
    private static MouseLook player;
    private static readonly List<Animation> animations = new List<Animation>();
    private static readonly HashSet<Transform> changedAnimators = new HashSet<Transform>();
    private static readonly HashSet<int> animators = new HashSet<int>(), skins = new HashSet<int>();
    private static readonly Dictionary<AnimationClip, int> clips = new Dictionary<AnimationClip, int>();
    private static readonly Dictionary<object, Component> contexts = new Dictionary<object, Component>();
    private static readonly Dictionary<Component, int> actors = new Dictionary<Component, int>();
    private static readonly Dictionary<Component, Dictionary<string, bool>> states =
        new Dictionary<Component, Dictionary<string, bool>>();
    private static readonly Dictionary<Component, Dictionary<string, Gate>> gates =
        new Dictionary<Component, Dictionary<string, Gate>>();
    private static readonly Dictionary<FieldInfo, string> fields = new Dictionary<FieldInfo, string>();
    private static readonly Dictionary<MethodInfo, MethodInfo> audio =
        new Dictionary<MethodInfo, MethodInfo>();
    private static MethodInfo spawn;
    private static readonly MethodInfo ray = typeof(Physics).GetMethod(
        "Raycast", new[] { typeof(Ray), typeof(RaycastHit).MakeByRefType(), typeof(float), typeof(int) });
    private static readonly MethodInfo line =
        typeof(Physics).GetMethod("Linecast", new[] { typeof(Vector3), typeof(Vector3), typeof(int) });
    public static int HookFailures { get; private set; }
    public static string HookError { get; private set; }
    public static int AudioSites { get; private set; }
    public static int ParticleSites { get; private set; }
    public static int QuerySites { get; private set; }
    public static int StateSites { get; private set; }
    private static double detailUntil;
    public static bool HighDetail
    {
        get {
            return Now < detailUntil;
        }
    }
    private static double Now
    {
        get {
            return SpeedrunManager.Instance == null ? 0 : SpeedrunManager.Instance.CurrentSeconds;
        }
    }

    public static void Begin(ReplayMotionData motion, MouseLook source)
    {
        Stop();
        data = motion.Experience;
        data.Recorded = true;
        coverage = motion.Coverage;
        coverage.Start(ReplayCoverage.Animation);
        data.Dropped = HookFailures;
        player = source;
        // Optional metadata in the existing name table; older readers retain it.
        double shaderOrigin = Math.Max(0, Time.timeSinceLevelLoadAsDouble - Now);
        data.Name(ReplayShaderClock.OriginName +
                  shaderOrigin.ToString("R", System.Globalization.CultureInfo.InvariantCulture));
    }

    public static void Stop()
    {
        changedAnimators.Clear();
        detailUntil = 0;
        coverage = null;
        data = null;
        player = null;
        animations.Clear();
        animators.Clear();
        skins.Clear();
        clips.Clear();
        contexts.Clear();
        actors.Clear();
        states.Clear();
        gates.Clear();
    }

    private static int Actor(Component component)
    {
        if (component == null)
            return 0;
        int id;
        if (actors.TryGetValue(component, out id))
            return id;
        id = ReplayCapture.ActorId(component);
        actors.Add(component, id);
        return id;
    }

    private static Component Context(object value)
    {
        var component = value as Component;
        if (component != null)
            return component;
        if (value == null)
            return null;
        if (contexts.TryGetValue(value, out component))
            return component;
        var field = value.GetType().GetField("<>4__this", BindingFlags.Public | BindingFlags.NonPublic |
                                                              BindingFlags.Instance);
        if (field != null)
            component = field.GetValue(value) as Component;
        if (contexts.Count < 2048)
            contexts.Add(value, component);
        return component;
    }

    public static void Bind(ReplayVisualOwner owner, Transform root)
    {
        if (data == null || owner == null || root == null)
            return;
        foreach (var a in root.GetComponentsInChildren<Animator>(true))
            BindAnimator(owner, root, a);
        foreach (var skin in root.GetComponentsInChildren<SkinnedMeshRenderer>(true))
            BindSkin(owner, root, skin);
        if (owner.Group == 2)
            foreach (var type in new[] { typeof(MonsterController), typeof(HideAndSeekIt),
                                         typeof(GhostScript), typeof(WallAI) })
            {
                var component = root.GetComponentInChildren(type, true) as Component;
                if (component == null || states.ContainsKey(component))
                    continue;
                var initial = new Dictionary<string, bool>(StringComparer.Ordinal);
                states.Add(component, initial);
                foreach (var pair in fields)
                    if (pair.Key.DeclaringType.IsInstanceOfType(component))
                    {
                        bool value = (bool)pair.Key.GetValue(component);
                        initial[pair.Value] = value;
                        data.Add(new ReplayObservation { Seconds = Now, Actor = owner.Id, Kind = 4,
                                                         Name = data.Name(pair.Value), Value = value ? 1 : 0,
                                                         Position = component.transform.position });
                    }
            }
    }

    public static void BindSkin(ReplayVisualOwner owner, Transform root, SkinnedMeshRenderer skin)
    {
        if (data == null || owner == null || skin == null || skin.sharedMesh == null ||
            !skins.Add(skin.GetInstanceID()))
            return;
        int count = Math.Min(128, skin.sharedMesh.blendShapeCount),
            slot = Array.IndexOf(skin.GetComponents<SkinnedMeshRenderer>(), skin);
        for (int i = 0; i < count && data.Animations.Count < ReplayExperienceData.MaxTracks; i++)
        {
            var track = new ReplayAnimationTrack { Group = owner.Group,
                                                   Actor = owner.Id,
                                                   Kind = 2,
                                                   Layer = slot,
                                                   Slot = i,
                                                   Path = ReplayVisualCapture.Path(skin.transform, root),
                                                   Name = skin.sharedMesh.GetBlendShapeName(i) };
            data.Animations.Add(track);
            animations.Add(new Animation { Skin = skin, Track = track });
        }
    }

    public static void BindAnimator(ReplayVisualOwner owner, Transform root, Animator a)
    {
        if (data == null || owner == null || a == null || !animators.Add(a.GetInstanceID()))
            return;
        var controller = a.runtimeAnimatorController;
        if (controller == null)
            return;
        foreach (var clip in controller.animationClips)
            if (clip != null && !clips.ContainsKey(clip))
                clips.Add(clip, data.Name("Animation / " + clip.name));
        for (int layer = 0;
             layer < Math.Min(8, a.layerCount) && data.Animations.Count < ReplayExperienceData.MaxTracks;
             layer++)
        {
            var track = new ReplayAnimationTrack { Group = owner.Group,
                                                   Actor = owner.Id,
                                                   Kind = 1,
                                                   Layer = layer,
                                                   Path = ReplayVisualCapture.Path(a.transform, root),
                                                   Name = a.name + " / " + a.GetLayerName(layer) };
            data.Animations.Add(track);
            animations.Add(new Animation { Source = a, Track = track });
        }
    }

    public static bool Sample(double at, bool force = false)
    {
        changedAnimators.Clear();
        if (data == null)
            return false;
        coverage.Spans[ReplayCoverage.Animation].Sources = animations.Count;
        coverage.Observe(ReplayCoverage.Animation, at);
        bool cut = false;
        foreach (var a in animations)
        {
            try
            {
                var f = new ReplayAnimationFrame { Seconds = at };
                if (a.Track.Kind == 2)
                {
                    if (a.Skin == null || !force && at < a.Next)
                        continue;
                    f.Weight = a.Skin.GetBlendShapeWeight(a.Track.Slot);
                    if (a.Seen && at > a.Last.Seconds &&
                        Math.Abs(f.Weight - a.Last.Weight) / (at - a.Last.Seconds) > 25)
                        a.DetailUntil = at + .2;
                    a.Next = AdaptivePoseSampler.Next(a.Next, at,
                                                      HighDetail || at < a.DetailUntil
                                                          ? AdaptivePoseSampler.FastInterval
                                                          : AdaptivePoseSampler.NormalInterval);
                    if (a.Seen && Math.Abs(f.Weight - a.Last.Weight) < .001f && !force)
                        continue;
                }
                else
                {
                    if (a.Source == null || !a.Source.isInitialized)
                        continue;
                    int layer = a.Track.Layer;
                    var current = a.Source.GetCurrentAnimatorStateInfo(layer);
                    bool transition = a.Source.IsInTransition(layer);
                    if (transition)
                        detailUntil = at + .2;
                    f.State = current.fullPathHash;
                    f.Phase = current.normalizedTime;
                    f.Speed = a.Source.speed * current.speed * current.speedMultiplier;
                    f.Weight = layer == 0 ? 1 : a.Source.GetLayerWeight(layer);
                    if (transition)
                    {
                        var next = a.Source.GetNextAnimatorStateInfo(layer);
                        f.Next = next.fullPathHash;
                        f.NextPhase = next.normalizedTime;
                        f.Transition =
                            Mathf.Clamp01(a.Source.GetAnimatorTransitionInfo(layer).normalizedTime);
                        f.Flags |= 1;
                    }
                    bool changed = !a.Seen || f.State != a.Last.State || f.Next != a.Last.Next ||
                                   f.Flags != (a.Last.Flags & 1) || f.Speed != a.Last.Speed ||
                                   f.Weight != a.Last.Weight;
                    if (!force && !changed && at < a.Next)
                        continue;
                    a.Next = at + (transition ? 1.0 / 60 : .1);
                    if (a.Seen && f.State != a.Last.State && !transition && (a.Last.Flags & 1) == 0)
                    {
                        f.Flags |= 2;
                        cut = true;
                        changedAnimators.Add(a.Source.transform);
                        detailUntil = at + .2;
                    }
                    a.Source.GetCurrentAnimatorClipInfo(layer, a.Clips);
                    float best = -1;
                    foreach (var c in a.Clips)
                        if (c.clip != null && c.weight > best)
                        {
                            best = c.weight;
                            int id;
                            if (!clips.TryGetValue(c.clip, out id))
                            {
                                id = data.Name("Animation / " + c.clip.name);
                                clips[c.clip] = id;
                            }
                            f.Clip = id;
                        }
                }
                if (float.IsNaN(f.Phase) || float.IsInfinity(f.Phase) || float.IsNaN(f.Speed) ||
                    float.IsInfinity(f.Speed) || float.IsNaN(f.Weight) || float.IsInfinity(f.Weight))
                {
                    data.Drop();
                    continue;
                }
                int stored = a.Track.Frames.Count;
                data.Add(a.Track, f);
                if (a.Track.Frames.Count == stored)
                    coverage.Fail(ReplayCoverage.Animation);
                a.Last = f;
                a.Seen = true;
            }
            catch (Exception)
            {
                data.Drop();
                coverage.Fail(ReplayCoverage.Animation);
            }
        }
        return cut;
    }

    public static bool PoseBoundary(Transform pose)
    {
        foreach (var root in changedAnimators)
            if (pose == root || pose.IsChildOf(root))
                return true;
        return false;
    }

    public static void StateWritten(object value, bool enabled, string field)
    {
        if (data == null)
            return;
        try
        {
            var c = value as Component;
            if (c == null)
                return;
            Dictionary<string, bool> prior;
            if (!states.TryGetValue(c, out prior))
            {
                prior = new Dictionary<string, bool>(StringComparer.Ordinal);
                states.Add(c, prior);
            }
            bool was;
            bool known = prior.TryGetValue(field, out was);
            if (known && was == enabled)
                return;
            prior[field] = enabled;
            data.Add(new ReplayObservation { Seconds = Now, Actor = Actor(c), Kind = known ? 1 : 4,
                                             Name = data.Name(field), Value = enabled ? 1 : 0,
                                             Position = c.transform.position });
        }
        catch (Exception)
        {
            if (data != null)
                data.Drop();
        }
    }

    private static void Sound(AudioSource source, AudioClip clip, float scale, int kind, string label = null)
    {
        if (data == null || source == null)
            return;
        try
        {
            if (kind == 2 && clip == null)
                clip = source.clip;
            if (kind == 2 && clip == null)
                return;
            string name = kind == 2 ? "Sound / " + clip.name : label ?? "Audio / stop or pause";
            data.Add(new ReplayObservation { Seconds = Now, Kind = kind, Name = data.Name(name),
                                             Value = source.volume * scale,
                                             Position = source.transform.position });
        }
        catch (Exception)
        {
            if (data != null)
                data.Drop();
        }
    }

    public static void Play(AudioSource source)
    {
        ReplayAudioCapture.Track(source);
        source.Play();
        Sound(source, null, 1, 2);
    }

    public static void OneShot(AudioSource source, AudioClip clip)
    {
        ReplayAudioCapture.Track(source);
        source.PlayOneShot(clip);
        Sound(source, clip, 1, 2);
    }

    public static void OneShotScale(AudioSource source, AudioClip clip, float scale)
    {
        ReplayAudioCapture.Track(source);
        source.PlayOneShot(clip, scale);
        Sound(source, clip, scale, 2);
    }

    public static void AudioStop(AudioSource source)
    {
        source.Stop();
        Sound(source, null, 0, 3, "Audio / Stop");
    }

    public static void AudioPause(AudioSource source)
    {
        source.Pause();
        Sound(source, null, 0, 3, "Audio / Pause");
    }

    public static bool Raycast(Ray ray, out RaycastHit hit, float distance, int mask, object context,
                               string site)
    {
        bool result = Physics.Raycast(ray, out hit, distance, mask);
        Observe(context, site, ray.origin,
                result ? hit.point : ray.origin + ray.direction * Math.Min(64, distance), distance, mask,
                (result ? 1u : 0u) | (!result && distance > 64 ? 4u : 0u), result ? hit.transform : null);
        return result;
    }

    public static bool Linecast(Vector3 a, Vector3 b, int mask, object context, string site)
    {
        bool result = Physics.Linecast(a, b, mask);
        Observe(context, site, a, b, Vector3.Distance(a, b), mask, (result ? 1u : 0u) | 2u, null);
        return result;
    }

    private static void Observe(object context, string site, Vector3 origin, Vector3 end, float limit,
                                int mask, uint flags, Transform hit)
    {
        if (data == null)
            return;
        try
        {
            var c = Context(context);
            if (c == null)
                return;
            double at = Now;
            int frame = Time.frameCount;
            // Only the original query runs. Geometry is sampled at 10 Hz;
            // actual decision transitions above are not throttled.
            Dictionary<string, Gate> catalog;
            if (!gates.TryGetValue(c, out catalog))
            {
                if (gates.Count >= 256)
                {
                    data.Drop();
                    return;
                }
                catalog = new Dictionary<string, Gate>(StringComparer.Ordinal);
                gates.Add(c, catalog);
            }
            Gate gate;
            if (!catalog.TryGetValue(site, out gate))
            {
                if (catalog.Count >= 128)
                {
                    data.Drop();
                    return;
                }
                gate = new Gate();
                catalog.Add(site, gate);
            }
            if (gate.Frame != frame)
            {
                if (at < gate.Next)
                    return;
                gate.Frame = frame;
                gate.Next = at + .1;
                gate.Count = 0;
            }
            if (gate.Count++ >= 32)
            {
                data.Drop();
                return;
            }
            int actor = Actor(c);
            if (c is Monster3 && hit != null)
            {
                var target = hit.GetComponent<WallAI>();
                if (target != null)
                {
                    actor = Actor(target);
                    flags |= 8;
                }
            }
            else if (hit != null)
            {
                bool monsterToPlayer = site.IndexOf("MonsterRayTest", StringComparison.Ordinal) >= 0;
                var expected =
                    monsterToPlayer ? (player == null ? null : PlayerContext.Body(player)) : c.transform;
                if (hit == expected)
                    flags |= 8;
            }
            if (!Finite(origin) || !Finite(end) || float.IsNaN(limit) || limit < 0)
            {
                data.Drop();
                return;
            }
            data.Add(new ReplayProbe { Seconds = at, Actor = actor, Site = data.Name(site), Origin = origin,
                                       End = end, Limit = float.IsPositiveInfinity(limit) ? -1 : limit,
                                       Mask = mask, Flags = flags });
        }
        catch (Exception)
        {
            if (data != null)
                data.Drop();
        }
    }

    private static bool Finite(Vector3 p)
    {
        return !(float.IsNaN(p.x) || float.IsInfinity(p.x) || float.IsNaN(p.y) || float.IsInfinity(p.y) ||
                 float.IsNaN(p.z) || float.IsInfinity(p.z)) &&
               Math.Abs(p.x) <= 100000 && Math.Abs(p.y) <= 100000 && Math.Abs(p.z) <= 100000;
    }

    private static Type Root(Type t)
    {
        while (t.DeclaringType != null)
            t = t.DeclaringType;
        return t;
    }

    private static bool Monster(Type type)
    {
        return type == typeof(MonsterController) || type == typeof(HideAndSeekIt) ||
               type == typeof(GhostScript) || type == typeof(WallAI) || type == typeof(Monster3);
    }

    public static void Install(HarmonyLib.Harmony harmony)
    {
        fields.Clear();
        audio.Clear();
        AudioSites = ParticleSites = QuerySites = StateSites = HookFailures = 0;
        HookError = null;
        foreach (var pair in new[] { new[] { "Play", "Play" }, new[] { "Stop", "AudioStop" },
                                     new[] { "Pause", "AudioPause" } })
            audio.Add(typeof(AudioSource).GetMethod(pair[0], Type.EmptyTypes),
                      typeof(ReplayExperienceCapture).GetMethod(pair[1]));
        audio.Add(typeof(AudioSource).GetMethod("PlayOneShot", new[] { typeof(AudioClip) }),
                  typeof(ReplayExperienceCapture).GetMethod("OneShot"));
        audio.Add(typeof(AudioSource).GetMethod("PlayOneShot", new[] { typeof(AudioClip), typeof(float) }),
                  typeof(ReplayExperienceCapture).GetMethod("OneShotScale"));
        foreach (var method in typeof(UnityEngine.Object)
                     .GetMethods(BindingFlags.Static | BindingFlags.Public))
            if (method.Name == "Instantiate" && method.IsGenericMethodDefinition &&
                method.GetParameters().Length == 3 &&
                method.GetParameters()[1].ParameterType == typeof(Vector3) &&
                method.GetParameters()[2].ParameterType == typeof(Quaternion))
                spawn = method.MakeGenericMethod(typeof(GameObject));
        AddFields(typeof(MonsterController), "HasBeenSpotted", "IsStunned", "EnteringRageMode", "goBerserk");
        AddFields(typeof(HideAndSeekIt), "spottingPlayer", "FoundPlayer", "SeeingIt", "SeekingMode",
                  "startChase", "hiding", "running");
        AddFields(typeof(GhostScript), "isHunting", "isRunning", "isChasing", "isHibernating",
                  "monsterHeardSounds", "preparingToChase");
        AddFields(typeof(WallAI), "isSpotted", "HasLineOfSight", "isWithinLight", "isSleeping", "angry",
                  "attackFreeze");
        // One-time IL scan. Iterator MoveNext methods are included, so audio
        // and detection performed by coroutines are covered too.
        foreach (var type in typeof(MouseLook).Assembly.GetTypes())
            foreach (var method in type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic |
                                                   BindingFlags.Instance | BindingFlags.Static |
                                                   BindingFlags.DeclaredOnly))
            {
                if (method.IsAbstract || method.ContainsGenericParameters || method.GetMethodBody() == null)
                    continue;
                try
                {
                    bool relevant = false;
                    foreach (var i in PatchProcessor.GetOriginalInstructions(method))
                    {
                        var call = i.operand as MethodInfo;
                        var field = i.operand as FieldInfo;
                        if (call != null && (audio.ContainsKey(call) || call == spawn) ||
                            Monster(Root(type)) && (call == ray || call == line) ||
                            field != null && i.opcode == OpCodes.Stfld && fields.ContainsKey(field))
                        {
                            relevant = true;
                            break;
                        }
                    }
                    if (relevant)
                        harmony.Patch(method, transpiler: new HarmonyMethod(typeof(ReplayExperienceCapture),
                                                                            "Rewrite"));
                }
                catch (Exception e)
                {
                    HookFailures++;
                    HookError = type.Name + "." + method.Name + ": " + e;
                    try
                    {
                        MelonLoader.MelonLogger.Warning("Replay observation unavailable / " + HookError);
                    }
                    catch (Exception)
                    {
                    }
                }
            }
    }

    private static void AddFields(Type type, params string[] names)
    {
        foreach (string n in names)
        {
            var f = type.GetField(n, BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            if (f != null && f.FieldType == typeof(bool))
                fields.Add(f, type.Name + "." + n);
        }
    }

    private static void Transfer(CodeInstruction from, CodeInstruction first, CodeInstruction last)
    {
        first.labels.AddRange(from.labels);
        from.labels.Clear();
        foreach (var b in from.blocks)
        {
            if (b.blockType == ExceptionBlockType.EndExceptionBlock)
                last.blocks.Add(b);
            else
                first.blocks.Add(b);
        }
        from.blocks.Clear();
    }

    public static IEnumerable<CodeInstruction> Rewrite(IEnumerable<CodeInstruction> instructions,
                                                       ILGenerator generator, MethodBase __originalMethod)
    {
        bool monster = Monster(Root(__originalMethod.DeclaringType));
        int site = 0;
        foreach (var i in instructions)
        {
            site++;
            var method = i.operand as MethodInfo;
            if ((i.opcode == OpCodes.Call || i.opcode == OpCodes.Callvirt) && method != null &&
                method == spawn)
            {
                i.opcode = OpCodes.Call;
                i.operand = typeof(ReplaySceneCapture).GetMethod("Spawn");
                ParticleSites++;
                yield return i;
                continue;
            }
            MethodInfo replacement;
            if ((i.opcode == OpCodes.Call || i.opcode == OpCodes.Callvirt) && method != null &&
                audio.TryGetValue(method, out replacement))
            {
                i.opcode = OpCodes.Call;
                i.operand = replacement;
                AudioSites++;
                yield return i;
                continue;
            }
            if (monster && (i.opcode == OpCodes.Call || i.opcode == OpCodes.Callvirt) &&
                (method == ray || method == line) && !__originalMethod.IsStatic)
            {
                var first = new CodeInstruction(OpCodes.Ldarg_0);
                var last = new CodeInstruction(
                    OpCodes.Call,
                    typeof(ReplayExperienceCapture).GetMethod(method == ray ? "Raycast" : "Linecast"));
                Transfer(i, first, last);
                yield return first;
                yield return new CodeInstruction(OpCodes.Ldstr,
                                                 Root(__originalMethod.DeclaringType).Name + " / " +
                                                     __originalMethod.DeclaringType.Name + "." +
                                                     __originalMethod.Name + " #" + site);
                yield return last;
                QuerySites++;
                continue;
            }
            var field = i.operand as FieldInfo;
            string name;
            if (i.opcode == OpCodes.Stfld && field != null && fields.TryGetValue(field, out name))
            {
                var value = generator.DeclareLocal(typeof(bool));
                var first = new CodeInstruction(OpCodes.Stloc, value);
                var last = new CodeInstruction(OpCodes.Call,
                                               typeof(ReplayExperienceCapture).GetMethod("StateWritten"));
                Transfer(i, first, last);
                yield return first;
                yield return new CodeInstruction(OpCodes.Dup);
                yield return new CodeInstruction(OpCodes.Ldloc, value);
                yield return i;
                yield return new CodeInstruction(OpCodes.Ldloc, value);
                yield return new CodeInstruction(OpCodes.Ldstr, name);
                yield return last;
                StateSites++;
                continue;
            }
            yield return i;
        }
    }
}
