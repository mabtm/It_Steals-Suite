using System;
using System.Collections.Generic;
using UnityEngine;

// Cached once per run/actor. Main-thread observation only: no AI, animation
// sampling, material instances, disk IO or scene searches in the sample loop.
public static class ReplayVisualCapture
{
    private sealed class Entry
    {
        public ReplayVisualTrack Track;
        public Transform Pose;
        public Renderer Renderer;
        public Light Light;
        public ReplayVisualFrame Previous;
        public bool Seen;
        public bool World, Culled;
        public double Next;
        public AdaptivePoseSampler Sampler;
    }

    private static ReplayVisualData data;
    private static ReplayCoverage coverage;
    private static ReplayVisualOwner world;
    private static readonly List<Entry> entries = new List<Entry>();
    private static readonly Dictionary<Sprite, int> spriteIds = new Dictionary<Sprite, int>();
    private static readonly Dictionary<Texture, int> textureIds = new Dictionary<Texture, int>();
    private static readonly HashSet<int> owners = new HashSet<int>();
    private static readonly HashSet<Renderer> culling = new HashSet<Renderer>();
    private static int scene;
    private static Transform observedBody;
    private static readonly HashSet<long> ownerIds = new HashSet<long>();
    private static readonly Dictionary<string, ReplayVisualTrack> globalPoses =
        new Dictionary<string, ReplayVisualTrack>();

    public static string Segment(Transform t)
    {
        return t.GetSiblingIndex() + ":" + t.name.Replace("%", "%25").Replace("/", "%2F");
    }

    public static string Path(Transform t, Transform root = null)
    {
        if (t == root)
            return "";
        var parts = new List<string>();
        for (var p = t; p != root; p = p.parent)
        {
            if (p == null)
                return root == null ? string.Join("/", parts.ToArray()) : null;
            parts.Insert(0, Segment(p));
        }
        return string.Join("/", parts.ToArray());
    }

    public static string TextureKey(Texture t)
    {
        return t == null ? "" : "T|" + t.name + "|" + t.width + "|" + t.height;
    }

    public static string SpriteKey(Sprite s)
    {
        if (s == null)
            return "";
        var r = s.rect;
        return "S|" + s.name + "|" + TextureKey(s.texture) + "|" +
               r.x.ToString("R", System.Globalization.CultureInfo.InvariantCulture) + "|" +
               r.y.ToString("R", System.Globalization.CultureInfo.InvariantCulture) + "|" +
               r.width.ToString("R", System.Globalization.CultureInfo.InvariantCulture) + "|" +
               r.height.ToString("R", System.Globalization.CultureInfo.InvariantCulture);
    }

    private static int SpriteAsset(Sprite s)
    {
        if (s == null)
            return 0;
        int id;
        if (!spriteIds.TryGetValue(s, out id))
        {
            id = data.Asset(SpriteKey(s));
            spriteIds[s] = id;
        }
        return id;
    }

    private static int TextureAsset(Texture t)
    {
        if (t == null)
            return 0;
        int id;
        if (!textureIds.TryGetValue(t, out id))
        {
            id = data.Asset(TextureKey(t));
            textureIds[t] = id;
        }
        return id;
    }

    public static bool Dynamic(Transform t)
    {
        var body = observedBody != null ? observedBody : PlayerContext.Body(PlayerContext.Observed);
        if (body != null && (t == body || t.IsChildOf(body)))
            return true;
        return t.GetComponentInParent<MouseLook>(true) != null ||
               t.GetComponentInParent<MonsterController>(true) != null ||
               t.GetComponentInParent<Monster3>(true) != null ||
               t.GetComponentInParent<HideAndSeekIt>(true) != null ||
               t.GetComponentInParent<GhostScript>(true) != null ||
               t.GetComponentInParent<WallAI>(true) != null;
    }

    public static bool Point(Transform t)
    {
        for (var p = t; p != null; p = p.parent)
            if (p.CompareTag("Point"))
                return true;
        return false;
    }

    public static void Begin(ReplayMotionData motion, MouseLook source, PracticeMapData map)
    {
        Stop();
        data = motion.Visuals;
        coverage = motion.Coverage;
        coverage.Start(ReplayCoverage.Visuals);
        data.Recorded = true;
        scene = source.gameObject.scene.handle;
        observedBody = PlayerContext.Body(source);
        world = data.Owner(0, 0, 0, "");
        try
        {
            foreach (var actor in UnityEngine.Object.FindObjectsOfType<HideAndSeekIt>(true))
                if (actor.gameObject.scene.handle == scene)
                    foreach (var obj in ReplayGamePause.CulledGeometry(actor))
                        if (obj != null)
                            foreach (var r in obj.GetComponentsInChildren<Renderer>(true))
                                culling.Add(r);
            foreach (var r in UnityEngine.Object.FindObjectsOfType<Renderer>(true))
                if (r.gameObject.scene.handle == scene && !Dynamic(r.transform) && !Point(r.transform) &&
                    !(r is ParticleSystemRenderer) && !(r is TrailRenderer) && !(r is LineRenderer))
                {
                    BindRenderer(world, r, Path(r.transform), false);
                    if (r.GetComponentInParent<Animator>(true) != null)
                    {
                        var animator = r.GetComponentInParent<Animator>(true);
                        ReplayExperienceCapture.BindAnimator(world, null, animator);
                        for (var t = r.transform; t != null; t = t.parent)
                        {
                            BindPose(world, t, Path(t), true, null);
                            if (t == animator.transform)
                                break;
                        }
                    }
                    var skin = r as SkinnedMeshRenderer;
                    if (skin != null)
                        ReplayExperienceCapture.BindSkin(world, null, skin);
                    if (skin != null)
                        foreach (var bone in skin.bones)
                            if (bone != null)
                                BindPose(world, bone, Path(bone), true, null);
                }
            foreach (var light in UnityEngine.Object.FindObjectsOfType<Light>(true))
                if (light.gameObject.scene.handle == scene && !Dynamic(light.transform))
                    BindLight(world, light, Path(light.transform));
            foreach (var body in UnityEngine.Object.FindObjectsOfType<Rigidbody>(true))
                if (body.gameObject.scene.handle == scene && !Dynamic(body.transform) &&
                    !Point(body.transform) && !body.isKinematic)
                    BindPose(world, body.transform, Path(body.transform), true, null);
            BindOwner(1, 0, 0, PlayerContext.Body(source));
            if (source.gemsList != null)
                foreach (var gem in source.gemsList)
                    if (gem != null)
                    {
                        int id = ReplayColliderCapture.CrystalOwner(map, gem.transform.position);
                        if (id >= 0)
                            BindOwner(3, id, 0, gem.transform);
                    }
            Sample(0, true);
        }
        catch (Exception)
        {
            data.Drop();
        }
    }

    public static void BindActor(Component source, Transform body, int id, int kind)
    {
        if (data == null || data.Full || source == null || body == null)
            return;
        try
        {
            BindOwner(2, id, kind, body);
            // Actor discovery also runs at run time zero, after the initial
            // camera sample. Record its first pose immediately, without a gap.
            Sample(SpeedrunManager.Instance == null ? 0 : SpeedrunManager.Instance.CurrentSeconds);
        }
        catch (Exception)
        {
            data.Drop();
        }
    }

    private static void BindOwner(int group, int id, int kind, Transform root)
    {
        if (root == null || !owners.Add(root.GetInstanceID()) ||
            !ownerIds.Add(((long)group << 32) | (uint)id))
            return;
        var owner = data.Owner(group, id, kind, Path(root));
        if (owner == null)
            return;
        var known = new HashSet<int>();
        BindPose(owner, root, "", true, known);
        foreach (var r in root.GetComponentsInChildren<Renderer>(true))
        {
            if (!(r is MeshRenderer) && !(r is SkinnedMeshRenderer) && !(r is SpriteRenderer))
                continue;
            BindAncestors(owner, r.transform, root, known);
            BindRenderer(owner, r, Path(r.transform, root), true);
            var skin = r as SkinnedMeshRenderer;
            if (skin != null)
                foreach (var bone in skin.bones)
                    if (bone != null && (bone == root || bone.IsChildOf(root)))
                        BindAncestors(owner, bone, root, known);
        }
        foreach (var light in root.GetComponentsInChildren<Light>(true))
            BindLight(owner, light, Path(light.transform, root));
        ReplayExperienceCapture.Bind(owner, root);
    }

    private static void BindAncestors(ReplayVisualOwner owner, Transform t, Transform root,
                                      HashSet<int> known)
    {
        if (t == root || t == null)
            return;
        BindAncestors(owner, t.parent, root, known);
        BindPose(owner, t, Path(t, root), false, known);
    }

    private static void BindPose(ReplayVisualOwner owner, Transform t, string path, bool absolute,
                                 HashSet<int> known)
    {
        if (owner == null || path == null || known != null && !known.Add(t.GetInstanceID()))
            return;
        if (owner.Group == 0 && globalPoses.ContainsKey(path))
            return;
        var track = data.Track(owner, 1, path);
        if (track == null)
            return;
        if (owner.Group == 0)
            globalPoses.Add(path, track);
        entries.Add(new Entry { Track = track, Pose = t, World = absolute });
    }

    private static void BindRenderer(ReplayVisualOwner owner, Renderer r, string path, bool dynamic)
    {
        if (owner == null || path == null)
            return;
        var array = r.GetComponents<Renderer>();
        int slot = Array.IndexOf(array, r);
        var track = data.Track(owner, r is SpriteRenderer ? 3 : 2, path, slot);
        if (track != null)
            entries.Add(
                new Entry { Track = track, Renderer = r, World = !dynamic, Culled = culling.Contains(r) });
    }

    private static void BindLight(ReplayVisualOwner owner, Light light, string path)
    {
        if (owner == null || path == null)
            return;
        int slot = Array.IndexOf(light.GetComponents<Light>(), light);
        var track = data.Track(owner, 4, path, slot);
        if (track != null)
            entries.Add(new Entry { Track = track, Light = light });
    }

    private static bool Same(ReplayVisualFrame a, ReplayVisualFrame b, int kind)
    {
        if (kind == 1)
            return (a.Position - b.Position).sqrMagnitude < 1e-10f &&
                   (a.Scale - b.Scale).sqrMagnitude < 1e-10f &&
                   Math.Abs(Quaternion.Dot(a.Rotation, b.Rotation)) > .9999999f;
        if (a.Flags != b.Flags || a.Asset != b.Asset || a.Texture != b.Texture || a.Color != b.Color)
            return false;
        return kind != 4 ||
               ((a.Position - b.Position).sqrMagnitude < 1e-10f &&
                Math.Abs(Quaternion.Dot(a.Rotation, b.Rotation)) > .9999999f && a.Intensity == b.Intensity &&
                a.Range == b.Range && a.Angle == b.Angle && a.InnerAngle == b.InnerAngle);
    }

    public static void Sample(double at, bool force = false, bool animationBoundary = false)
    {
        if (data == null || data.Full)
            return;
        coverage.Spans[ReplayCoverage.Visuals].Sources = entries.Count;
        coverage.Observe(ReplayCoverage.Visuals, at);
        for (int i = 0; i < entries.Count; i++)
        {
            var e = entries[i];
            if (!force && at < e.Next && !(animationBoundary && e.Track.Kind == 1))
                continue;
            if (e.Track.Kind != 1)
                e.Next = AdaptivePoseSampler.Next(e.Next, at,
                                                  e.World && e.Track.Kind == 2 ? .1
                                                  : e.Track.Kind == 3          ? 1.0 / 120
                                                                               : 1.0 / 60);
            try
            {
                var f = new ReplayVisualFrame { Seconds = at, Rotation = Quaternion.identity,
                                                Scale = Vector3.one, Color = Color.white };
                if (e.Track.Kind == 1)
                {
                    if (e.Pose == null)
                        continue;
                    f.Position = e.World ? e.Pose.position : e.Pose.localPosition;
                    f.Rotation = e.World ? e.Pose.rotation : e.Pose.localRotation;
                    f.Scale = e.World ? e.Pose.lossyScale : e.Pose.localScale;
                }
                else if (e.Track.Kind == 4)
                {
                    var l = e.Light;
                    if (l != null)
                    {
                        f.Flags = l.enabled && l.gameObject.activeInHierarchy ? 1u : 0;
                        f.Position = l.transform.position;
                        f.Rotation = l.transform.rotation;
                        f.Color = l.color;
                        f.Intensity = l.intensity;
                        f.Range = l.range;
                        f.Angle = l.spotAngle;
                        f.InnerAngle = l.innerSpotAngle;
                    }
                }
                else
                {
                    var r = e.Renderer;
                    if (r != null)
                    {
                        f.Flags = r.enabled && (r.gameObject.activeInHierarchy || e.Culled) ? 1u : 0;
                        var sprite = r as SpriteRenderer;
                        if (sprite != null)
                        {
                            f.Flags |= (sprite.flipX ? 2u : 0) | (sprite.flipY ? 4u : 0);
                            f.Color = sprite.color;
                            f.Asset = SpriteAsset(sprite.sprite);
                        }
                        else
                        {
                            var m = r.sharedMaterial;
                            if (m != null)
                            {
                                if (m.HasProperty("_Color"))
                                {
                                    f.Color = m.color;
                                    f.Flags |= 8;
                                }
                                if (m.HasProperty("_MainTex"))
                                {
                                    f.Texture = TextureAsset(m.mainTexture);
                                    f.Flags |= 16;
                                }
                            }
                        }
                    }
                }
                if (e.Track.Kind == 1)
                {
                    if (e.Sampler == null)
                        e.Sampler = new AdaptivePoseSampler(data, e.Track, coverage);
                    e.Sampler.Push(f, force || animationBoundary, ReplayExperienceCapture.HighDetail);
                    e.Next = AdaptivePoseSampler.Next(e.Next, at, e.Sampler.Interval(at));
                    continue;
                }
                int count = e.Track.Frames.Count;
                if ((!force || e.Track.Kind != 1) && count > 0 &&
                    Same(e.Track.Frames[count - 1], f, e.Track.Kind))
                {
                    e.Previous = f;
                    e.Seen = true;
                    continue;
                }
                if (e.Track.Kind == 1 && e.Seen && count > 0 &&
                    e.Track.Frames[count - 1].Seconds < e.Previous.Seconds)
                    data.Add(e.Track, e.Previous);
                data.Add(e.Track, f);
                e.Previous = f;
                e.Seen = true;
            }
            catch (Exception)
            {
                data.Drop();
                coverage.Fail(ReplayCoverage.Visuals);
            }
        }
    }

    public static void Stop()
    {
        data = null;
        coverage = null;
        world = null;
        entries.Clear();
        owners.Clear();
        culling.Clear();
        globalPoses.Clear();
        observedBody = null;
        ownerIds.Clear();
        spriteIds.Clear();
        textureIds.Clear();
    }
}
