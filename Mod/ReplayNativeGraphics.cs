using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

// Visual-only actor proxies: no AI, colliders, rigidbodies, audio or animation
// controllers are cloned. Static scene meshes and all their batch ranges stay.
public sealed class ReplayNativeGraphics
{
    private sealed class Proxy
    {
        public ReplayVisualOwner Owner;
        public Transform Source, Root;
        public bool Visible = true;
        public readonly Dictionary<Transform, Transform> Copies = new Dictionary<Transform, Transform>();
        public readonly Dictionary<string, Transform> Paths =
            new Dictionary<string, Transform>(StringComparer.Ordinal);
        public readonly Dictionary<string, Renderer> Renderers =
            new Dictionary<string, Renderer>(StringComparer.Ordinal);
        public readonly Dictionary<string, Light> Lights =
            new Dictionary<string, Light>(StringComparer.Ordinal);
    }

    private sealed class Binding
    {
        public ReplayVisualTrack Track;
        public readonly List<ReplayAnimationTrack> Cuts = new List<ReplayAnimationTrack>();
        public Transform Pose;
        public Renderer Renderer;
        public Light Light;
        public MaterialPropertyBlock Block, OriginalBlock;
        public Color BaseColor, CurrentColor;
        public string BaseTexture = "";
        public bool World, ObservedOnly, Monster, LightOn, ColorOverride, HasPose;
        public int Index = -2;
        public ReplayVisualFrame LastPose;
    }

    private sealed class Morph
    {
        public ReplayAnimationTrack Track;
        public SkinnedMeshRenderer Renderer;
        public readonly List<ReplayAnimationTrack> Cuts = new List<ReplayAnimationTrack>();
    }

    private readonly List<Morph> morphs = new List<Morph>();
    private readonly List<Binding> bindings = new List<Binding>();
    private readonly List<Proxy> proxies = new List<Proxy>();
    private readonly List<Transform> billboards = new List<Transform>();
    private readonly Dictionary<Light, Light> lights = new Dictionary<Light, Light>();
    private readonly Dictionary<string, Texture> textures =
        new Dictionary<string, Texture>(StringComparer.Ordinal);
    private readonly Dictionary<string, Sprite> sprites =
        new Dictionary<string, Sprite>(StringComparer.Ordinal);
    private readonly ReplayMaterialLibrary materials;
    private readonly HashSet<string> missingAssets = new HashSet<string>(StringComparer.Ordinal);
    private readonly Dictionary<string, Transform> world =
        new Dictionary<string, Transform>(StringComparer.Ordinal);
    private readonly ReplayRestoreScope restore;
    private readonly GameObject root;
    private readonly MouseLook player;
    private readonly ReplayMotionData motion;
    private readonly PracticeMapData map;
    private readonly Dictionary<Light, bool> enabledLights;
    private Light lamp;
    private ReplayShaderClock shaderClock;
    private int missing;
    private double last = -1;
    private bool monstersVisible = true;
    private readonly List<Action> renderUpdates = new List<Action>();

    private void RenderUpdate(Component component)
    {
        if (component == null)
            return;
        var method = component.GetType().GetMethod("Update", System.Reflection.BindingFlags.Instance |
                                                                 System.Reflection.BindingFlags.NonPublic);
        if (method != null)
            renderUpdates.Add((Action)Delegate.CreateDelegate(typeof(Action), component, method));
    }

    public void BindView(Camera view)
    {
        RenderUpdate(view.GetComponent<VolumetricLightRenderer>());
        shaderClock = new ReplayShaderClock(view, motion.Experience.Names);
        restore.Add(shaderClock.Dispose);
    }

    public void PrepareRender()
    {
        if (shaderClock != null)
            shaderClock.Set(Math.Max(0, last));
        foreach (var update in renderUpdates)
            update();
    }

    public string Status
    {
        get {
            return "original assets / " + bindings.Count + " visual tracks" +
                   (motion.Visuals.Recorded ? (motion.Visuals.Dropped > 0 ? " / capture reduced" : "")
                                            : " / legacy animation unavailable") +
                   (missing + missingAssets.Count > 0 ? " / unresolved " + (missing + missingAssets.Count)
                                                      : "") +
                   (shaderClock != null && !shaderClock.RecordedOrigin ? " / material phase estimated" : "");
        }
    }

    private static string Key(string path, int slot)
    {
        return path + "#" + slot;
    }

    public ReplayNativeGraphics(GameObject parent, MouseLook mouse, PracticeMapData layout,
                                ReplayMotionData input, ReplayRestoreScope scope,
                                Dictionary<Light, bool> originalLights)
    {
        root = parent;
        player = mouse;
        map = layout;
        motion = input;
        restore = scope;
        enabledLights = originalLights;
        materials = new ReplayMaterialLibrary(Resources.FindObjectsOfTypeAll<Material>());
        int scene = mouse.gameObject.scene.handle;
        foreach (var t in UnityEngine.Object.FindObjectsOfType<Transform>(true))
            if (t.gameObject.scene.handle == scene)
                world[ReplayVisualCapture.Path(t)] = t;
        foreach (var t in Resources.FindObjectsOfTypeAll<Texture>())
        {
            string key = ReplayVisualCapture.TextureKey(t);
            Texture previous;
            if (!textures.TryGetValue(key, out previous))
                textures.Add(key, t);
            else if (previous != t)
                textures[key] = null; // Never guess between ambiguous assets.
        }
        foreach (var s in Resources.FindObjectsOfTypeAll<Sprite>())
        {
            string key = ReplayVisualCapture.SpriteKey(s);
            Sprite previous;
            if (!sprites.TryGetValue(key, out previous))
                sprites.Add(key, s);
            else if (previous != s)
                sprites[key] = null;
        }
        foreach (var pair in originalLights)
        {
            if (pair.Key == null || ReplayVisualCapture.Dynamic(pair.Key.transform))
                continue;
            CopyLight(pair.Key, pair.Value);
        }
        var owners = new List<ReplayVisualOwner>(input.Visuals.Owners);
        if (!input.Visuals.Recorded)
        {
            owners.Add(new ReplayVisualOwner { Group = 1 });
            foreach (var actor in layout.Review.Monsters)
                owners.Add(new ReplayVisualOwner { Group = 2, Id = actor.Id, Kind = actor.Kind });
            foreach (int id in layout.Selected)
                owners.Add(new ReplayVisualOwner { Group = 3, Id = id });
        }
        var used = new HashSet<Transform>();
        foreach (var owner in owners)
        {
            if (owner.Group == 0)
            {
                foreach (var track in owner.Tracks)
                    BindWorld(track);
                continue;
            }
            var source = Source(owner, used);
            if (source == null)
            {
                missing += owner.Tracks.Count;
                continue;
            }
            var proxy = new Proxy { Owner = owner, Source = source };
            proxies.Add(proxy);
            CopyTransform(proxy, source);
            CopyRenderers(proxy);
            foreach (var light in source.GetComponentsInChildren<Light>(true))
            {
                bool on;
                enabledLights.TryGetValue(light, out on);
                var copy = CopyLight(light, on);
                if (copy != light)
                    copy.transform.SetParent(proxy.Root, true);
                proxy.Lights[Key(ReplayVisualCapture.Path(light.transform, source),
                                 Array.IndexOf(light.GetComponents<Light>(), light))] = copy;
                if (light == mouse.playerFlashlight)
                    lamp = copy;
            }
            foreach (var track in owner.Tracks)
                BindProxy(proxy, track);
        }
        if (lamp == null && mouse.playerFlashlight != null)
        {
            bool on;
            enabledLights.TryGetValue(mouse.playerFlashlight, out on);
            lamp = CopyLight(mouse.playerFlashlight, on);
        }
        BindAnimationHistory();
        bindings.Sort((a, b) =>
                      {
                          if (a.Pose == null)
                              return b.Pose == null ? 0 : 1;
                          if (b.Pose == null)
                              return -1;
                          return Depth(a.Pose).CompareTo(Depth(b.Pose));
                      });
        foreach (var b in UnityEngine.Object.FindObjectsOfType<Billboard>(true))
            if (b.gameObject.scene.handle == scene && !ReplayVisualCapture.Dynamic(b.transform) &&
                !ReplayVisualCapture.Point(b.transform))
            {
                var t = b.transform;
                var q = t.localRotation;
                restore.Add(() =>
                            {
                                if (t != null)
                                    t.localRotation = q;
                            });
                billboards.Add(t);
            }
    }

    private static int Depth(Transform t)
    {
        int depth = 0;
        for (; t != null; t = t.parent)
            depth++;
        return depth;
    }

    private Transform Source(ReplayVisualOwner owner, HashSet<Transform> used)
    {
        if (owner.Group == 1)
            return owner.Id == 0 ? PlayerContext.Body(player) : ReplayVisualCapture.Arms(player);
        if (owner.Group == 3)
        {
            if (player.gemsList != null)
                foreach (var gem in player.gemsList)
                    if (gem != null &&
                        ReplayColliderCapture.CrystalOwner(map, gem.transform.position) == owner.Id)
                        return gem.transform;
            return null;
        }
        var candidates = new List<Transform>();
        if (owner.Kind == 1)
            foreach (var c in UnityEngine.Object.FindObjectsOfType<MonsterController>(true))
                candidates.Add(c.agent != null ? c.agent.transform : c.transform);
        else if (owner.Kind == 2)
            foreach (var c in UnityEngine.Object.FindObjectsOfType<HideAndSeekIt>(true))
                candidates.Add(c.agent != null ? c.agent.transform : c.transform);
        else if (owner.Kind == 3)
            foreach (var c in UnityEngine.Object.FindObjectsOfType<GhostScript>(true))
                candidates.Add(c.transform);
        else if (owner.Kind == 4)
            foreach (var c in UnityEngine.Object.FindObjectsOfType<WallAI>(true))
                candidates.Add(c.agent != null ? c.agent.transform : c.transform);
        Transform exact;
        if (owner.Root.Length > 0 && world.TryGetValue(owner.Root, out exact) && candidates.Contains(exact) &&
            !used.Contains(exact))
        {
            used.Add(exact);
            return exact;
        }
        Vector3 first = Vector3.zero;
        foreach (var actor in map.Review.Monsters)
            if (actor.Id == owner.Id && actor.Trace.Count > 0)
            {
                first = actor.Trace[0].Position;
                break;
            }
        Transform best = null;
        float distance = float.MaxValue;
        foreach (var t in candidates)
            if (t != null && !used.Contains(t))
            {
                float d = (t.position - first).sqrMagnitude;
                if (d < distance)
                {
                    best = t;
                    distance = d;
                }
            }
        if (best == null && candidates.Count > 0)
            best = candidates[0];
        if (best != null)
            used.Add(best);
        return best;
    }

    private Transform CopyTransform(Proxy proxy, Transform source)
    {
        Transform copy;
        if (proxy.Copies.TryGetValue(source, out copy))
            return copy;
        var obj = new GameObject(source.name + " / replay visual");
        obj.layer = source.gameObject.layer;
        copy = obj.transform;
        if (source == proxy.Source)
        {
            copy.SetParent(root.transform, false);
            copy.SetPositionAndRotation(source.position, source.rotation);
            copy.localScale = source.lossyScale;
            proxy.Root = copy;
        }
        else
        {
            copy.SetParent(CopyTransform(proxy, source.parent), false);
            copy.localPosition = source.localPosition;
            copy.localRotation = source.localRotation;
            copy.localScale = source.localScale;
        }
        proxy.Copies.Add(source, copy);
        proxy.Paths.Add(ReplayVisualCapture.Path(source, proxy.Source), copy);
        return copy;
    }

    private void CopyRenderers(Proxy proxy)
    {
        foreach (var source in proxy.Source.GetComponentsInChildren<Renderer>(true))
        {
            if (source.isPartOfStaticBatch)
                continue; // Never duplicate a combined static mesh.
            var skin = source as SkinnedMeshRenderer;
            var sprite = source as SpriteRenderer;
            var mesh = source as MeshRenderer;
            if (skin == null && sprite == null && mesh == null)
                continue;
            Transform t = CopyTransform(proxy, source.transform);
            Renderer copy;
            if (skin != null)
            {
                var s = t.gameObject.AddComponent<SkinnedMeshRenderer>();
                s.sharedMesh = skin.sharedMesh;
                s.localBounds = skin.localBounds;
                s.quality = skin.quality;
                ReplayRendererScope.ConfigureRecordedSkin(s);
                var bones = skin.bones;
                for (int i = 0; i < bones.Length; i++)
                    if (bones[i] != null && (bones[i] == proxy.Source || bones[i].IsChildOf(proxy.Source)))
                        bones[i] = CopyTransform(proxy, bones[i]);
                    else
                        bones[i] = null;
                s.bones = bones;
                if (skin.rootBone != null &&
                    (skin.rootBone == proxy.Source || skin.rootBone.IsChildOf(proxy.Source)))
                    s.rootBone = CopyTransform(proxy, skin.rootBone);
                if (s.sharedMesh != null)
                    for (int i = 0; i < s.sharedMesh.blendShapeCount; i++)
                        s.SetBlendShapeWeight(i, skin.GetBlendShapeWeight(i));
                copy = s;
            }
            else if (sprite != null)
            {
                var s = t.gameObject.AddComponent<SpriteRenderer>();
                s.sprite = sprite.sprite;
                s.color = sprite.color;
                s.flipX = sprite.flipX;
                s.flipY = sprite.flipY;
                s.drawMode = sprite.drawMode;
                s.size = sprite.size;
                s.maskInteraction = sprite.maskInteraction;
                s.spriteSortPoint = sprite.spriteSortPoint;
                copy = s;
            }
            else
            {
                var filter = source.GetComponent<MeshFilter>();
                if (filter == null || filter.sharedMesh == null)
                    continue;
                t.gameObject.AddComponent<MeshFilter>().sharedMesh = filter.sharedMesh;
                copy = t.gameObject.AddComponent<MeshRenderer>();
            }
            copy.sharedMaterials = source.sharedMaterials;
            copy.shadowCastingMode = source.shadowCastingMode;
            copy.receiveShadows = source.receiveShadows;
            copy.lightProbeUsage = source.lightProbeUsage;
            copy.reflectionProbeUsage = source.reflectionProbeUsage;
            copy.lightmapIndex = source.lightmapIndex;
            copy.lightmapScaleOffset = source.lightmapScaleOffset;
            copy.sortingLayerID = source.sortingLayerID;
            copy.sortingOrder = source.sortingOrder;
            copy.enabled = !motion.Visuals.Recorded && source.enabled && source.gameObject.activeInHierarchy;
            var block = new MaterialPropertyBlock();
            source.GetPropertyBlock(block);
            copy.SetPropertyBlock(block);
            for (int material = 0; material < source.sharedMaterials.Length; material++)
            {
                source.GetPropertyBlock(block, material);
                if (!block.isEmpty)
                    copy.SetPropertyBlock(block, material);
            }
            int slot = Array.IndexOf(source.GetComponents<Renderer>(), source);
            proxy.Renderers[Key(ReplayVisualCapture.Path(source.transform, proxy.Source), slot)] = copy;
            if (source.GetComponent<Billboard>() != null)
                billboards.Add(t);
        }
    }

    private Light CopyLight(Light source, bool on)
    {
        Light found;
        if (lights.TryGetValue(source, out found))
            return found;
        var volume = source.GetComponent<VolumetricLight>();
        if (volume != null && (motion.Visuals.Recorded || !ReplayVisualCapture.Dynamic(source.transform)))
        {
            // Retain the game's registered light/command-buffer pair. A plain
            // cloned Light cannot produce the original volumetric pass.
            var t = source.transform;
            var position = t.localPosition;
            var rotation = t.localRotation;
            var color = source.color;
            var intensity = source.intensity;
            var range = source.range;
            var angle = source.spotAngle;
            var inner = source.innerSpotAngle;
            var type = source.type;
            restore.Add(() =>
                        {
                            if (source != null)
                            {
                                t.localPosition = position;
                                t.localRotation = rotation;
                                source.color = color;
                                source.intensity = intensity;
                                source.range = range;
                                source.spotAngle = angle;
                                source.innerSpotAngle = inner;
                                source.type = type;
                            }
                        });
            lights.Add(source, source);
            source.enabled = on && source.gameObject.activeInHierarchy;
            RenderUpdate(volume);
            return source;
        }
        var obj = new GameObject(source.name + " / replay light");
        obj.layer = source.gameObject.layer;
        obj.transform.SetParent(root.transform, false);
        obj.transform.SetPositionAndRotation(source.transform.position, source.transform.rotation);
        var l = obj.AddComponent<Light>();
        l.type = source.type;
        l.color = source.color;
        l.intensity = source.intensity;
        l.range = source.range;
        l.spotAngle = source.spotAngle;
        l.innerSpotAngle = source.innerSpotAngle;
        l.cookie = source.cookie;
        l.cookieSize = source.cookieSize;
        l.shadows = source.shadows;
        l.shadowStrength = source.shadowStrength;
        l.shadowBias = source.shadowBias;
        l.shadowNormalBias = source.shadowNormalBias;
        l.shadowNearPlane = source.shadowNearPlane;
        l.shadowResolution = source.shadowResolution;
        l.cullingMask = source.cullingMask;
        l.renderMode = source.renderMode;
        l.bounceIntensity = source.bounceIntensity;
        l.colorTemperature = source.colorTemperature;
        l.useColorTemperature = source.useColorTemperature;
        l.bakingOutput = source.bakingOutput;
        l.enabled = on && source.gameObject.activeInHierarchy;
        lights.Add(source, l);
        return l;
    }

    private void BindWorld(ReplayVisualTrack track)
    {
        Transform t;
        if (!world.TryGetValue(track.Path, out t))
        {
            missing++;
            return;
        }
        if (track.Kind == 1)
        {
            var position = t.position;
            var rotation = t.rotation;
            var scale = t.localScale;
            restore.Add(() =>
                        {
                            if (t != null)
                            {
                                t.SetPositionAndRotation(position, rotation);
                                t.localScale = scale;
                            }
                        });
            bindings.Add(new Binding { Track = track, Pose = t, World = true });
        }
        else if (track.Kind == 4)
        {
            var sources = t.GetComponents<Light>();
            if (track.Slot >= sources.Length)
            {
                missing++;
                return;
            }
            bool on;
            enabledLights.TryGetValue(sources[track.Slot], out on);
            bindings.Add(new Binding { Track = track, Light = CopyLight(sources[track.Slot], on) });
        }
        else
        {
            var sources = t.GetComponents<Renderer>();
            if (track.Slot >= sources.Length)
            {
                missing++;
                return;
            }
            var r = sources[track.Slot];
            bool enabled = r.enabled;
            var originalMaterials = r.sharedMaterials;
            var sprite = r as SpriteRenderer;
            var originalSprite = sprite != null ? sprite.sprite : null;
            var originalColor = sprite != null ? sprite.color : Color.white;
            bool flipX = sprite != null && sprite.flipX, flipY = sprite != null && sprite.flipY;
            var original = new MaterialPropertyBlock();
            r.GetPropertyBlock(original, 0);
            restore.Add(() =>
                        {
                            if (r != null)
                            {
                                r.enabled = enabled;
                                r.sharedMaterials = originalMaterials;
                                r.SetPropertyBlock(original.isEmpty ? null : original, 0);
                                if (sprite != null)
                                {
                                    sprite.sprite = originalSprite;
                                    sprite.color = originalColor;
                                    sprite.flipX = flipX;
                                    sprite.flipY = flipY;
                                }
                            }
                        });
            var bind = new Binding { Track = track, Renderer = r };
            PrepareMaterial(bind);
            bindings.Add(bind);
        }
    }

    private void BindProxy(Proxy proxy, ReplayVisualTrack track)
    {
        var bind = new Binding { Track = track, World = track.Path.Length == 0, ObservedOnly = true,
                                 Monster = proxy.Owner.Group == 2 };
        if (track.Kind == 1)
            proxy.Paths.TryGetValue(track.Path, out bind.Pose);
        else if (track.Kind == 4)
            proxy.Lights.TryGetValue(Key(track.Path, track.Slot), out bind.Light);
        else
        {
            proxy.Renderers.TryGetValue(Key(track.Path, track.Slot), out bind.Renderer);
            PrepareMaterial(bind);
        }
        if (bind.Pose == null && bind.Renderer == null && bind.Light == null)
        {
            missing++;
            return;
        }
        bindings.Add(bind);
    }

    private static void PrepareMaterial(Binding b)
    {
        if (b.Renderer == null)
            return;
        b.Block = new MaterialPropertyBlock();
        b.OriginalBlock = new MaterialPropertyBlock();
        b.Renderer.GetPropertyBlock(b.OriginalBlock, 0);
        var m = b.Renderer.sharedMaterial;
        b.BaseColor = m != null && m.HasProperty("_Color") ? m.color : Color.white;
        b.BaseTexture =
            m != null && m.HasProperty("_MainTex") ? ReplayVisualCapture.TextureKey(m.mainTexture) : "";
    }

    private string Asset(int id)
    {
        return id > 0 && id <= motion.Visuals.Assets.Count ? motion.Visuals.Assets[id - 1] : "";
    }

    private static bool Cut(List<ReplayAnimationTrack> cuts, double a, double b)
    {
        foreach (var t in cuts)
            if (t.Break(a, b))
                return true;
        return false;
    }

    private void BindAnimationHistory()
    {
        foreach (var t in motion.Experience.Animations)
        {
            Proxy proxy = null;
            foreach (var p in proxies)
                if (p.Owner.Group == t.Group && p.Owner.Id == t.Actor)
                {
                    proxy = p;
                    break;
                }
            Transform transform = null;
            if (t.Group == 0)
                world.TryGetValue(t.Path, out transform);
            else if (proxy != null)
                proxy.Paths.TryGetValue(t.Path, out transform);
            if (t.Kind == 1)
            {
                foreach (var b in bindings)
                {
                    if (b.Pose == null)
                        continue;
                    bool applies = false;
                    if (t.Group == 0)
                    {
                        Transform parent;
                        if (world.TryGetValue(t.Path, out parent))
                            applies = b.Pose == parent || b.Pose.IsChildOf(parent);
                    }
                    else if (proxy != null && transform != null)
                        applies = b.Pose == transform || b.Pose.IsChildOf(transform);
                    if (applies)
                        b.Cuts.Add(t);
                }
            }
            else if (t.Kind == 2 && transform != null)
            {
                var array = transform.GetComponents<SkinnedMeshRenderer>();
                if (t.Layer >= array.Length || array[t.Layer].sharedMesh == null ||
                    t.Slot >= array[t.Layer].sharedMesh.blendShapeCount)
                {
                    missing++;
                    continue;
                }
                var renderer = array[t.Layer];
                if (t.Group == 0)
                {
                    int slot = t.Slot;
                    float old = renderer.GetBlendShapeWeight(slot);
                    restore.Add(() =>
                                {
                                    if (renderer != null)
                                        renderer.SetBlendShapeWeight(slot, old);
                                });
                }
                var m = new Morph { Track = t, Renderer = renderer };
                foreach (var a in motion.Experience.Animations)
                    if (a.Kind == 1 && a.Group == t.Group && a.Actor == t.Actor &&
                        (a.Path.Length == 0 || t.Path == a.Path ||
                         t.Path.StartsWith(a.Path + "/", StringComparison.Ordinal)))
                        m.Cuts.Add(a);
                morphs.Add(m);
            }
        }
    }

    private static ReplayVisualFrame PoseAt(ReplayVisualTrack track, int index, double at,
                                            List<ReplayAnimationTrack> cuts)
    {
        var f = track.Frames[index];
        if (index + 1 < track.Frames.Count)
        {
            var n = track.Frames[index + 1];
            double dt = n.Seconds - f.Seconds;
            if (!Cut(cuts, f.Seconds, n.Seconds) && dt > 0 && dt <= .25 &&
                (n.Position - f.Position).magnitude <= Math.Max(4, dt * 40))
            {
                float u = (float)((at - f.Seconds) / dt);
                f.Position = Vector3.Lerp(f.Position, n.Position, u);
                f.Rotation = Quaternion.Slerp(f.Rotation, n.Rotation, u);
                f.Scale = Vector3.Lerp(f.Scale, n.Scale, u);
            }
        }
        return f;
    }

    public void Face(Camera view)
    {
        foreach (var b in billboards)
            if (b != null)
                b.LookAt(view.transform.position);
    }

    public void Apply(double at, ReplayViewFrame camera, Camera view, bool showMonsters)
    {
        // Seeking and export evaluate absolute timestamps, without advancing AI.
        if (last != at)
        {
            foreach (var m in morphs)
            {
                int i = m.Track.At(at);
                if (i < 0 || m.Renderer == null)
                    continue;
                var f = m.Track.Frames[i];
                float weight = f.Weight;
                if (i + 1 < m.Track.Frames.Count)
                {
                    var n = m.Track.Frames[i + 1];
                    double dt = n.Seconds - f.Seconds;
                    if (dt > 0 && dt <= .25 && !Cut(m.Cuts, f.Seconds, n.Seconds))
                        weight = Mathf.Lerp(f.Weight, n.Weight, (float)((at - f.Seconds) / dt));
                }
                if (Math.Abs(m.Renderer.GetBlendShapeWeight(m.Track.Slot) - weight) > .0001f)
                    m.Renderer.SetBlendShapeWeight(m.Track.Slot, weight);
            }
            foreach (var b in bindings)
            {
                int index = b.Track.At(at);
                if (index < 0)
                {
                    if (b.ObservedOnly)
                    {
                        if (b.Renderer != null)
                            b.Renderer.enabled = false;
                        if (b.Light != null)
                            b.Light.enabled = false;
                    }
                    b.Index = index;
                    continue;
                }
                var f = b.Track.Frames[index];
                if (b.Pose != null)
                {
                    f = PoseAt(b.Track, index, at, b.Cuts);
                    bool move = !b.HasPose || f.Position != b.LastPose.Position,
                         turn = !b.HasPose || f.Rotation != b.LastPose.Rotation,
                         scale = !b.HasPose || f.Scale != b.LastPose.Scale;
                    if (b.World)
                    {
                        b.Pose.SetPositionAndRotation(f.Position, f.Rotation);
                        {
                            var parent = b.Pose.parent;
                            var s = parent == null ? Vector3.one : parent.lossyScale;
                            b.Pose.localScale = new Vector3(s.x == 0 ? f.Scale.x : f.Scale.x / s.x,
                                                            s.y == 0 ? f.Scale.y : f.Scale.y / s.y,
                                                            s.z == 0 ? f.Scale.z : f.Scale.z / s.z);
                        }
                    }
                    else
                    {
                        if (move)
                            b.Pose.localPosition = f.Position;
                        if (turn)
                            b.Pose.localRotation = f.Rotation;
                        if (scale)
                            b.Pose.localScale = f.Scale;
                    }
                    b.LastPose = f;
                    b.HasPose = true;
                }
                else if (index != b.Index)
                {
                    if (b.Light != null)
                    {
                        b.Light.transform.SetPositionAndRotation(f.Position, f.Rotation);
                        b.LightOn = (f.Flags & 1) != 0;
                        b.Light.enabled = b.LightOn && (!b.Monster || showMonsters);
                        b.Light.color = f.Color;
                        b.Light.intensity = f.Intensity;
                        b.Light.range = f.Range;
                        b.Light.spotAngle = f.Angle;
                        b.Light.innerSpotAngle = f.InnerAngle;
                    }
                    else if (b.Renderer != null)
                    {
                        b.Renderer.enabled = (f.Flags & 1) != 0;
                        var sprite = b.Renderer as SpriteRenderer;
                        if (sprite != null)
                        {
                            Sprite asset;
                            if (f.Asset == 0)
                                sprite.sprite = null;
                            else if (sprites.TryGetValue(Asset(f.Asset), out asset) && asset != null)
                                sprite.sprite = asset;
                            else
                                missingAssets.Add(Asset(f.Asset));
                            sprite.color = f.Color;
                            sprite.flipX = (f.Flags & 2) != 0;
                            sprite.flipY = (f.Flags & 4) != 0;
                        }
                        else
                        {
                            // Asset was unused on mesh tracks before material capture.
                            // An absent reference keeps legacy replay behavior unchanged.
                            string key = Asset(f.Asset);
                            if (key.StartsWith("M|", StringComparison.Ordinal))
                            {
                                var material = materials.Find(key);
                                if (material != null)
                                {
                                    if (b.Renderer.sharedMaterial != material)
                                        b.Renderer.sharedMaterial = material;
                                    b.BaseColor =
                                        material.HasProperty("_Color") ? material.color : Color.white;
                                    b.BaseTexture = material.HasProperty("_MainTex")
                                                        ? ReplayVisualCapture.TextureKey(material.mainTexture)
                                                        : "";
                                }
                                else
                                    missingAssets.Add(key);
                            }
                            bool before = b.ColorOverride;
                            bool color = (f.Flags & 8) != 0 && f.Color != b.BaseColor;
                            bool texture = (f.Flags & 16) != 0 && Asset(f.Texture) != b.BaseTexture;
                            b.Renderer.SetPropertyBlock(b.OriginalBlock.isEmpty ? null : b.OriginalBlock, 0);
                            b.Renderer.GetPropertyBlock(b.Block, 0);
                            b.ColorOverride = color;
                            b.CurrentColor = f.Color;
                            if (texture)
                            {
                                Texture asset;
                                if (f.Texture == 0)
                                    b.Block.SetTexture("_MainTex", null);
                                else if (textures.TryGetValue(Asset(f.Texture), out asset) && asset != null)
                                    b.Block.SetTexture("_MainTex", asset);
                                else
                                    missingAssets.Add(Asset(f.Texture));
                            }
                            if (color || texture)
                                b.Renderer.SetPropertyBlock(b.Block, 0);
                            else if (before)
                                b.Renderer.SetPropertyBlock(b.OriginalBlock.isEmpty ? null : b.OriginalBlock,
                                                            0);
                        }
                    }
                }
                b.Index = index;
            }
            foreach (var p in proxies)
                if (p.Owner.Group == 2 && (!motion.Visuals.Recorded || p.Owner.Tracks.Count == 0))
                {
                    foreach (var actor in map.Review.Monsters)
                        if (actor.Id == p.Owner.Id)
                        {
                            int i = 0;
                            while (i < actor.Trace.Count && actor.Trace[i].Seconds <= at)
                                i++;
                            i--;
                            if (i < 0)
                            {
                                p.Visible = false;
                                break;
                            }
                            p.Visible = true;
                            var pos = actor.Trace[i].Position;
                            if (i + 1 < actor.Trace.Count && actor.Trace[i + 1].Continuous &&
                                actor.Trace[i + 1].Seconds > actor.Trace[i].Seconds)
                                pos = Vector3.Lerp(
                                    pos, actor.Trace[i + 1].Position,
                                    (float)((at - actor.Trace[i].Seconds) /
                                            (actor.Trace[i + 1].Seconds - actor.Trace[i].Seconds)));
                            p.Root.position = pos;
                            break;
                        }
                }
            if (!motion.Visuals.Recorded)
                foreach (var p in proxies)
                {
                    if (p.Owner.Group == 1 && p.Owner.Id == 0)
                    {
                        p.Root.position = camera.Body;
                        p.Root.rotation = camera.BodyRotation;
                    }
                    else if (p.Owner.Group == 3)
                    {
                        bool picked = false;
                        for (int i = 0; i < map.Review.Collected.Count; i++)
                            if (map.Review.Collected[i] == p.Owner.Id && map.Review.PickupSeconds[i] <= at)
                                picked = true;
                        p.Root.gameObject.SetActive(!picked);
                    }
                }
            foreach (var p in proxies)
                if (p.Owner.Group == 1 && p.Owner.Id == 0)
                {
                    p.Root.SetPositionAndRotation(camera.Body, camera.BodyRotation);
                }
            last = at;
        }
        foreach (var b in bindings)
            if (b.Renderer != null && b.ColorOverride)
            {
                var color = b.CurrentColor;
                var material = b.Renderer.sharedMaterial;
                if (material != null && material.HasProperty("_Color"))
                {
                    float baseline = Math.Abs(b.BaseColor.a) > .00001f ? b.BaseColor.a : 1;
                    color.a *= material.color.a / baseline;
                }
                b.Block.SetColor("_Color", color);
                b.Renderer.SetPropertyBlock(b.Block, 0);
            }
        if (monstersVisible != showMonsters)
        {
            foreach (var b in bindings)
                if (b.Monster && b.Light != null)
                    b.Light.enabled = b.LightOn && showMonsters;
            monstersVisible = showMonsters;
        }
        foreach (var p in proxies)
            if (p.Owner.Group == 2)
                p.Root.gameObject.SetActive(showMonsters && p.Visible);
        Face(view);
        if (lamp != null)
        {
            lamp.transform.SetPositionAndRotation(camera.LampPosition, camera.LampRotation);
            lamp.color = new Color(camera.LampColor.x, camera.LampColor.y, camera.LampColor.z);
            lamp.type = (LightType)camera.LampType;
            lamp.intensity = camera.LampIntensity;
            lamp.range = camera.LampRange;
            lamp.spotAngle = camera.LampAngle;
            lamp.enabled = (camera.State & 16) != 0;
        }
    }
}
