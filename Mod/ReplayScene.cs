using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEngine;
using UnityEngine.UI;

// These are observations of the game's own components. Sampling never runs a
// game script, advances an emitter or creates material instances.
public static class ReplaySceneCapture
{
    private sealed class Entry
    {
        public Component Source;
        public ReplaySceneTrack Track;
        public ParticleSystem.Particle[] Particles;
        public readonly MemoryStream Buffer = new MemoryStream(256);
        public BinaryWriter Writer;
    }

    private static ReplaySceneData data;
    private static double next;
    private static int serial;
    private static readonly List<Entry> entries = new List<Entry>();
    private static readonly Dictionary<UnityEngine.Object, string> keys =
        new Dictionary<UnityEngine.Object, string>();
    private static readonly Dictionary<string, byte[]> textBytes =
        new Dictionary<string, byte[]>(StringComparer.Ordinal);
    private static readonly System.Text.Encoding utf8 = new System.Text.UTF8Encoding(false, true);

    private static void Text(BinaryWriter w, string value)
    {
        value = value ?? "";
        byte[] bytes;
        if (!textBytes.TryGetValue(value, out bytes))
        {
            bytes = utf8.GetBytes(value);
            if (textBytes.Count == 512)
                textBytes.Clear();
            textBytes.Add(value, bytes);
        }
        w.Write(bytes.Length);
        w.Write(bytes);
    }

    internal static readonly string[] Properties = { "_Cutoff", "_Columns", "_Rows", "_Brightness" };

    private static string Key(UnityEngine.Object asset)
    {
        if (asset == null)
            return "";
        string key;
        if (keys.TryGetValue(asset, out key))
            return key;
        key = asset is Material ? ReplayMaterialLibrary.Key((Material)asset)
              : asset is Sprite ? ReplayVisualCapture.SpriteKey((Sprite)asset)
                                : ReplayVisualCapture.TextureKey((Texture)asset);
        keys.Add(asset, key);
        return key;
    }

    private static void Add(Component source, int kind, Transform root = null, string prefab = "",
                            string spawn = "")
    {
        if (source == null)
            return;
        var track = data.Track(kind, ReplayVisualCapture.Path(source.transform, root), prefab, spawn);
        if (track == null)
            return;
        var entry = new Entry { Source = source, Track = track };
        entry.Writer = new BinaryWriter(entry.Buffer);
        entries.Add(entry);
    }

    public static void Begin(ReplayMotionData motion, MouseLook player)
    {
        Stop();
        data = motion.SceneData;
        data.Recorded = true;
        var main = PlayerContext.View(player);
        // Some shipped scenes leave armsBodyCam unassigned even though a
        // BodyCam exists beneath the main camera. Observe the actual rig too.
        var cameras = ReplayCameraPipeline.Sources(
            main, main == null ? new Camera[0] : main.GetComponentsInChildren<Camera>(true), player.UICamera,
            player.armsBodyCam, player.radarCam);
        foreach (var camera in cameras)
            if (camera != null && camera != PlayerContext.View(player))
            {
                Add(camera, ReplaySceneData.Camera);
                if (camera.targetTexture != null)
                    keys[camera.targetTexture] = "RC|" + ReplayVisualCapture.Path(camera.transform);
            }
        if (ReplayExperienceCapture.ParticleSites == 0)
            data.Drop();
        int scene = player.gameObject.scene.handle;
        foreach (var rect in UnityEngine.Object.FindObjectsOfType<RectTransform>(true))
        {
            if (rect.gameObject.scene.handle != scene || rect.GetComponentInParent<Canvas>(true) == null)
                continue;
            Add(rect, ReplaySceneData.Rect);
            var text = rect.GetComponent<Text>();
            var image = rect.GetComponent<Image>();
            var raw = rect.GetComponent<RawImage>();
            if (text != null)
                Add(text, ReplaySceneData.Text);
            if (image != null)
                Add(image, ReplaySceneData.Image);
            if (raw != null)
                Add(raw, ReplaySceneData.RawImage);
            var group = rect.GetComponent<CanvasGroup>();
            if (group != null)
                Add(group, ReplaySceneData.Group);
        }
        foreach (var ps in UnityEngine.Object.FindObjectsOfType<ParticleSystem>(true))
            if (ps.gameObject.scene.handle == scene)
                Add(ps, ReplaySceneData.Particles);
        Sample(0, true);
    }

    // Called after the original Instantiate, at the same game call site. No
    // scene search, extra RNG call or duplicate particle effect is introduced.
    public static GameObject Spawn(GameObject prefab, Vector3 position, Quaternion rotation)
    {
        var spawned = UnityEngine.Object.Instantiate(prefab, position, rotation);
        if (data != null && spawned != null)
            try
            {
                string id = "effect-" + (++serial);
                foreach (var ps in spawned.GetComponentsInChildren<ParticleSystem>(true))
                    Add(ps, ReplaySceneData.Particles, spawned.transform, prefab.name, id);
            }
            catch (Exception)
            {
                data.Drop();
            }
        return spawned;
    }

    public static void Sample(double at, bool force = false)
    {
        if (data == null || !force && at < next)
            return;
        next = at + 1.0 / 60;
        int dropped = data.Dropped;
        foreach (var e in entries)
            try
            {
                e.Buffer.Position = 0;
                e.Buffer.SetLength(0);
                if (e.Source == null)
                {
                    if (e.Track.Kind != ReplaySceneData.Particles || e.Track.Frames.Count == 0)
                        continue;
                    // Retire a destroyed burst; do not leave its last particles frozen forever.
                    var previous = e.Track.Frames[e.Track.Frames.Count - 1].Values;
                    e.Writer.Write(previous, 0, 52);
                    e.Buffer.Position = 0;
                    e.Writer.Write(0u);
                    e.Buffer.Position = 52;
                    e.Writer.Write(0);
                }
                else
                {
                    var ps = e.Source as ParticleSystem;
                    if (ps != null && ps.particleCount > ReplaySceneData.MaxParticles)
                        data.Drop();
                    int capacity = ps == null ? 0 : Math.Min(ReplaySceneData.MaxParticles, ps.particleCount);
                    if (capacity > 0 && (e.Particles == null || e.Particles.Length < capacity))
                        e.Particles =
                            new ParticleSystem
                                .Particle[Math.Min(ReplaySceneData.MaxParticles, Math.Max(64, capacity * 2))];
                    Write(e.Writer, e.Source, e.Track.Kind, e.Particles);
                }
                data.Add(e.Track, at, e.Buffer.GetBuffer(), checked((int)e.Buffer.Length));
            }
            catch (Exception)
            {
                data.Drop();
            }
        if (data.Dropped == dropped)
            data.Through = at;
    }

    internal static void Vector(BinaryWriter w, Vector3 v)
    {
        w.Write(v.x);
        w.Write(v.y);
        w.Write(v.z);
    }

    internal static Vector3 Vector(BinaryReader r)
    {
        return new Vector3(r.ReadSingle(), r.ReadSingle(), r.ReadSingle());
    }

    internal static void Rotation(BinaryWriter w, Quaternion q)
    {
        w.Write(q.x);
        w.Write(q.y);
        w.Write(q.z);
        w.Write(q.w);
    }

    internal static Quaternion Rotation(BinaryReader r)
    {
        return new Quaternion(r.ReadSingle(), r.ReadSingle(), r.ReadSingle(), r.ReadSingle());
    }

    internal static void Color(BinaryWriter w, Color c)
    {
        w.Write(c.r);
        w.Write(c.g);
        w.Write(c.b);
        w.Write(c.a);
    }

    internal static Color Color(BinaryReader r)
    {
        return new Color(r.ReadSingle(), r.ReadSingle(), r.ReadSingle(), r.ReadSingle());
    }

    internal static void Rect(BinaryWriter w, Rect r)
    {
        w.Write(r.x);
        w.Write(r.y);
        w.Write(r.width);
        w.Write(r.height);
    }

    internal static Rect Rect(BinaryReader r)
    {
        return new Rect(r.ReadSingle(), r.ReadSingle(), r.ReadSingle(), r.ReadSingle());
    }

    internal static void Write(BinaryWriter w, Component component, int kind,
                               ParticleSystem.Particle[] particles = null)
    {
        var t = component.transform;
        if (kind == ReplaySceneData.Camera)
        {
            var c = (Camera)component;
            w.Write(c.enabled);
            Vector(w, t.localPosition);
            Rotation(w, t.localRotation);
            Vector(w, t.localScale);
            w.Write(c.fieldOfView);
            w.Write(c.nearClipPlane);
            w.Write(c.farClipPlane);
            w.Write(c.depth);
            w.Write(c.orthographicSize);
            w.Write(c.aspect);
            w.Write(c.cullingMask);
            w.Write((int)c.clearFlags);
            w.Write(c.orthographic);
            Rect(w, c.rect);
        }
        else if (kind == ReplaySceneData.Rect)
        {
            var r = (RectTransform)component;
            w.Write(t.gameObject.activeSelf);
            w.Write(r.anchorMin.x);
            w.Write(r.anchorMin.y);
            w.Write(r.anchorMax.x);
            w.Write(r.anchorMax.y);
            w.Write(r.pivot.x);
            w.Write(r.pivot.y);
            w.Write(r.sizeDelta.x);
            w.Write(r.sizeDelta.y);
            Vector(w, r.anchoredPosition3D);
            Rotation(w, r.localRotation);
            Vector(w, r.localScale);
        }
        else if (kind == ReplaySceneData.Group)
        {
            var group = (CanvasGroup)component;
            w.Write(group.alpha);
            w.Write(group.ignoreParentGroups);
        }
        else if (kind == ReplaySceneData.Particles)
        {
            var ps = (ParticleSystem)component;
            var renderer = ps.GetComponent<ParticleSystemRenderer>();
            w.Write((t.gameObject.activeSelf ? 1u : 0u) | (renderer != null && renderer.enabled ? 2u : 0u) |
                    (ps.isEmitting ? 4u : 0u));
            w.Write(ps.time);
            w.Write(ps.randomSeed);
            Vector(w, t.position);
            Rotation(w, t.rotation);
            Vector(w, t.lossyScale);
            int count = particles == null ? 0 : ps.GetParticles(particles);
            w.Write(count);
            for (int i = 0; i < count; i++)
            {
                var p = particles[i];
                Vector(w, p.position);
                Vector(w, p.velocity);
                Vector(w, p.rotation3D);
                Vector(w, p.angularVelocity3D);
                Vector(w, p.axisOfRotation);
                Vector(w, p.startSize3D);
                Color32 c = p.startColor;
                w.Write((uint)c.r | (uint)c.g << 8 | (uint)c.b << 16 | (uint)c.a << 24);
                w.Write(Math.Max(0, p.remainingLifetime));
                w.Write(p.startLifetime);
                w.Write(p.randomSeed);
            }
        }
        else
        {
            var g = (Graphic)component;
            w.Write(g.enabled);
            Color(w, g.color);
            Text(w, Key(g.material));
            var m = g.material;
            uint mask = 0;
            for (int i = 0; i < Properties.Length; i++)
                if (m != null && m.HasProperty(Properties[i]))
                    mask |= 1u << i;
            w.Write(mask);
            for (int i = 0; i < Properties.Length; i++)
                w.Write((mask & (1u << i)) != 0 ? m.GetFloat(Properties[i]) : 0);
            var scale = m == null || !m.HasProperty("_MainTex") ? Vector2.one : m.mainTextureScale;
            w.Write(scale.x);
            w.Write(scale.y);
            if (kind == ReplaySceneData.Text)
            {
                var text = (Text)g;
                Text(w, text.text);
                w.Write(text.fontSize);
                w.Write((int)text.fontStyle);
                w.Write((int)text.alignment);
                w.Write(text.lineSpacing);
            }
            else if (kind == ReplaySceneData.Image)
            {
                var image = (Image)g;
                Text(w, Key(image.overrideSprite));
                w.Write(image.fillAmount);
            }
            else
            {
                var raw = (RawImage)g;
                Text(w, Key(raw.texture));
                Rect(w, raw.uvRect);
            }
        }
    }

    public static void Stop()
    {
        foreach (var e in entries)
            e.Writer.Dispose();
        entries.Clear();
        keys.Clear();
        textBytes.Clear();
        data = null;
        next = 0;
        serial = 0;
    }
}

public sealed class ReplayScenePlayback
{
    private sealed class Binding
    {
        public Component Source;
        public ReplaySceneTrack Track;
        public int Index = -2;
        public bool CameraOn;
        public Material Material, OriginalMaterial;
        public string MaterialKey;
        public ParticleSystem.Particle[] Particles;
    }

    // Image.overrideSprite's getter returns the base sprite when no override is
    // set. Preserve the actual nullable field so game sprite changes keep working
    // after playback; this reference is never read from a replay file.
    private static readonly FieldInfo imageOverride =
        typeof(Image).GetField("m_OverrideSprite", BindingFlags.Instance | BindingFlags.NonPublic);

    private readonly List<Binding> bindings = new List<Binding>();
    private readonly List<Camera> cameras = new List<Camera>();
    private readonly List<RawImage> sceneImages = new List<RawImage>();
    private readonly Dictionary<string, Sprite> sprites = new Dictionary<string, Sprite>();
    private readonly Dictionary<string, Texture> textures = new Dictionary<string, Texture>();
    private readonly HashSet<string> missingAssets = new HashSet<string>(StringComparer.Ordinal);

    private void MissingAsset(string key)
    {
        if (key.Length != 0 && missingAssets.Add(key))
            Missing++;
    }

    private readonly ReplayMaterialLibrary materials;
    private readonly ReplayRestoreScope restore;
    public int Missing { get; private set; }

    private static void Asset<T>(Dictionary<string, T> assets, string key, T value)
        where T : UnityEngine.Object
    {
        T previous;
        if (!assets.TryGetValue(key, out previous))
            assets.Add(key, value);
        else if (previous != value)
            assets[key] = null;
    }

    public ReplayScenePlayback(ReplaySceneData data, MouseLook player, ReplayRestoreScope scope,
                               IEnumerable<string> shaderNames)
    {
        restore = scope;
        materials = new ReplayMaterialLibrary(Resources.FindObjectsOfTypeAll<Material>());
        foreach (var s in Resources.FindObjectsOfTypeAll<Sprite>())
            Asset(sprites, ReplayVisualCapture.SpriteKey(s), s);
        foreach (var t in Resources.FindObjectsOfTypeAll<Texture>())
            Asset(textures, ReplayVisualCapture.TextureKey(t), t);
        var world = new Dictionary<string, Transform>();
        foreach (var t in UnityEngine.Object.FindObjectsOfType<Transform>(true))
            if (t.gameObject.scene.handle == player.gameObject.scene.handle)
                world[ReplayVisualCapture.Path(t)] = t;
        foreach (var t in world.Values)
        {
            var image = t.GetComponent<RawImage>();
            if (image != null)
                sceneImages.Add(image);
            var camera = t.GetComponent<Camera>();
            if (camera != null && camera.targetTexture != null)
                Asset(textures, "RC|" + ReplayVisualCapture.Path(t), camera.targetTexture);
        }
        var prefabs = new Dictionary<string, GameObject>();
        foreach (var ps in Resources.FindObjectsOfTypeAll<ParticleSystem>())
        {
            var root = ps.transform.root.gameObject;
            if (!root.scene.IsValid() && Safe(root))
                Asset(prefabs, root.name, root);
        }
        var spawns = new Dictionary<string, GameObject>();
        foreach (var track in data.Tracks)
        {
            Transform t = null;
            if (track.Spawn.Length == 0)
                world.TryGetValue(track.Path, out t);
            else
            {
                GameObject spawned;
                if (!spawns.TryGetValue(track.Spawn, out spawned))
                {
                    GameObject prefab;
                    if (spawns.Count < 128 && prefabs.TryGetValue(track.Prefab, out prefab) && prefab != null)
                    {
                        spawned = UnityEngine.Object.Instantiate(prefab);
                        var saved = spawned;
                        restore.Add(() =>
                                    {
                                        if (saved != null)
                                            UnityEngine.Object.Destroy(saved);
                                    });
                        spawned.SetActive(false);
                    }
                    spawns[track.Spawn] = spawned;
                }
                if (spawned != null)
                    foreach (var child in spawned.GetComponentsInChildren<Transform>(true))
                        if (ReplayVisualCapture.Path(child, spawned.transform) == track.Path)
                        {
                            t = child;
                            break;
                        }
            }
            Component source =
                t == null                                ? null
                : track.Kind == ReplaySceneData.Camera   ? (Component)t.GetComponent<Camera>()
                : track.Kind == ReplaySceneData.Rect     ? t as RectTransform
                : track.Kind == ReplaySceneData.Text     ? (Component)t.GetComponent<Text>()
                : track.Kind == ReplaySceneData.Image    ? (Component)t.GetComponent<Image>()
                : track.Kind == ReplaySceneData.RawImage ? (Component)t.GetComponent<RawImage>()
                : track.Kind == ReplaySceneData.Group    ? (Component)t.GetComponent<CanvasGroup>()
                                                         : t.GetComponent<ParticleSystem>();
            if (source == null)
            {
                Missing++;
                continue;
            }
            var binding = new Binding { Source = source, Track = track };
            bindings.Add(binding);
            Save(binding);
            var camera = source as Camera;
            if (camera != null)
            {
                cameras.Add(camera);
                var clock = new ReplayShaderClock(camera, shaderNames);
                restore.Add(clock.Dispose);
                clocks.Add(clock);
            }
        }
    }

    private readonly List<ReplayShaderClock> clocks = new List<ReplayShaderClock>();

    private static bool Safe(GameObject root)
    {
        // Never execute gameplay or deserialize arbitrary component types from a replay.
        foreach (var c in root.GetComponentsInChildren<Component>(true))
            if (!(c is Transform) && !(c is ParticleSystem) && !(c is ParticleSystemRenderer))
                return false;
        return true;
    }

    private void Save(Binding b)
    {
        var ps = b.Source as ParticleSystem;
        if (ps != null)
        {
            var mainModule = ps.main;
            var stopAction = mainModule.stopAction;
            var state = ps.GetPlaybackState();
            var particles = new ParticleSystem.Particle[ps.particleCount];
            int count = ps.GetParticles(particles);
            var trails = ps.GetTrails();
            bool active = ps.gameObject.activeSelf, automatic = ps.useAutoRandomSeed, playing = ps.isPlaying,
                 paused = ps.isPaused;
            uint seed = ps.randomSeed;
            var renderer = ps.GetComponent<ParticleSystemRenderer>();
            bool enabled = renderer != null && renderer.enabled;
            var p = ps.transform.position;
            var q = ps.transform.rotation;
            var scale = ps.transform.localScale;
            restore.Add(() =>
                        {
                            if (ps == null)
                                return;
                            ps.transform.SetPositionAndRotation(p, q);
                            ps.transform.localScale = scale;
                            ps.gameObject.SetActive(active);
                            ps.randomSeed = seed;
                            ps.useAutoRandomSeed = automatic;
                            ps.SetPlaybackState(state);
                            ps.SetParticles(particles, count);
                            ps.SetTrails(trails);
                            if (renderer != null)
                                renderer.enabled = enabled;
                            if (playing)
                                ps.Play(false);
                            else if (paused)
                                ps.Pause(false);
                            else
                                ps.Stop(false, ParticleSystemStopBehavior.StopEmitting);
                            var originalMain = ps.main;
                            originalMain.stopAction = stopAction;
                        });
            // OrbExplode uses StopAction.Destroy. A replay emitter must survive
            // finishing a burst so backwards seeks can show that burst again.
            mainModule.stopAction = ParticleSystemStopAction.None;
            ps.Pause(false);
            return;
        }
        byte[] saved;
        using (var stream = new MemoryStream())
        {
            using (var w = new BinaryWriter(stream))
            {
                ReplaySceneCapture.Write(w, b.Source, b.Track.Kind);
                saved = stream.ToArray();
            }
        }
        var graphic = b.Source as Graphic;
        var image = b.Source as Image;
        var raw = b.Source as RawImage;
        if (image != null && imageOverride == null)
            throw new InvalidOperationException("Game UI sprite override binding unavailable");
        var originalOverride = image == null ? null : (Sprite)imageOverride.GetValue(image);
        var originalTexture = raw == null ? null : raw.texture;
        var original = graphic == null ? null : graphic.material;
        b.OriginalMaterial = original;
        restore.Add(() =>
                    {
                        try
                        {
                            if (b.Source == null)
                                return;
                            Apply(b, saved, 0);
                            if (graphic != null)
                                graphic.material = original;
                            if (image != null)
                                image.overrideSprite = originalOverride;
                            if (raw != null)
                                raw.texture = originalTexture;
                        }
                        finally
                        {
                            if (b.Material != null)
                                UnityEngine.Object.Destroy(b.Material);
                        }
                    });
    }

    public void Apply(double at, bool originalCamera)
    {
        foreach (var clock in clocks)
            clock.Set(at);
        foreach (var b in bindings)
        {
            if (b.Source == null)
                continue;
            int index = b.Track.At(at);
            if (index >= 0 && (index != b.Index || b.Track.Kind == ReplaySceneData.Particles))
                Apply(b, b.Track.Frames[index].Values,
                      Math.Min(1.0 / 60, Math.Max(0, at - b.Track.Frames[index].Seconds)));
            else if (index < 0 && b.Track.Kind == ReplaySceneData.Particles)
            {
                var ps = (ParticleSystem)b.Source;
                ps.Clear(false);
            }
            b.Index = index;
            var camera = b.Source as Camera;
            if (camera != null)
                camera.enabled = index >= 0 && b.CameraOn && originalCamera;
        }
    }

    private void Apply(Binding b, byte[] values, double advance)
    {
        using (var r = new BinaryReader(new MemoryStream(values, false)))
        {
            int kind = b.Track.Kind;
            var t = b.Source.transform;
            if (kind == ReplaySceneData.Camera)
            {
                var c = (Camera)b.Source;
                b.CameraOn = r.ReadBoolean();
                t.localPosition = ReplaySceneCapture.Vector(r);
                t.localRotation = ReplaySceneCapture.Rotation(r);
                t.localScale = ReplaySceneCapture.Vector(r);
                c.fieldOfView = r.ReadSingle();
                c.nearClipPlane = r.ReadSingle();
                c.farClipPlane = r.ReadSingle();
                c.depth = r.ReadSingle();
                c.orthographicSize = r.ReadSingle();
                c.aspect = r.ReadSingle();
                c.cullingMask = r.ReadInt32();
                c.clearFlags = (CameraClearFlags)r.ReadInt32();
                c.orthographic = r.ReadBoolean();
                c.rect = ReplaySceneCapture.Rect(r);
                c.enabled = b.CameraOn;
            }
            else if (kind == ReplaySceneData.Rect)
            {
                var rect = (RectTransform)b.Source;
                bool active = r.ReadBoolean();
                rect.anchorMin = new Vector2(r.ReadSingle(), r.ReadSingle());
                rect.anchorMax = new Vector2(r.ReadSingle(), r.ReadSingle());
                rect.pivot = new Vector2(r.ReadSingle(), r.ReadSingle());
                rect.sizeDelta = new Vector2(r.ReadSingle(), r.ReadSingle());
                rect.anchoredPosition3D = ReplaySceneCapture.Vector(r);
                rect.localRotation = ReplaySceneCapture.Rotation(r);
                rect.localScale = ReplaySceneCapture.Vector(r);
                if (t.gameObject.activeSelf != active)
                    t.gameObject.SetActive(active);
            }
            else if (kind == ReplaySceneData.Group)
            {
                var group = (CanvasGroup)b.Source;
                group.alpha = r.ReadSingle();
                group.ignoreParentGroups = r.ReadBoolean();
            }
            else if (kind == ReplaySceneData.Particles)
            {
                var ps = (ParticleSystem)b.Source;
                uint flags = r.ReadUInt32();
                float time = r.ReadSingle();
                uint seed = r.ReadUInt32();
                t.SetPositionAndRotation(ReplaySceneCapture.Vector(r), ReplaySceneCapture.Rotation(r));
                var scale = ReplaySceneCapture.Vector(r);
                var parent = t.parent == null ? Vector3.one : t.parent.lossyScale;
                t.localScale = new Vector3(parent.x == 0 ? 0 : scale.x / parent.x,
                                           parent.y == 0 ? 0 : scale.y / parent.y,
                                           parent.z == 0 ? 0 : scale.z / parent.z);
                bool active = (flags & 1) != 0;
                if (ps.gameObject.activeSelf != active)
                    ps.gameObject.SetActive(active);
                ps.Pause(false);
                ps.useAutoRandomSeed = false;
                ps.randomSeed = seed;
                ps.time = time;
                var renderer = ps.GetComponent<ParticleSystemRenderer>();
                if (renderer != null)
                    renderer.enabled = (flags & 2) != 0;
                int count = r.ReadInt32();
                if (b.Particles == null || b.Particles.Length < count)
                    b.Particles = new ParticleSystem.Particle[count];
                for (int i = 0; i < count; i++)
                {
                    var p = new ParticleSystem.Particle();
                    p.position = ReplaySceneCapture.Vector(r);
                    p.velocity = ReplaySceneCapture.Vector(r);
                    p.rotation3D = ReplaySceneCapture.Vector(r);
                    p.angularVelocity3D = ReplaySceneCapture.Vector(r);
                    p.axisOfRotation = ReplaySceneCapture.Vector(r);
                    p.startSize3D = ReplaySceneCapture.Vector(r);
                    uint color = r.ReadUInt32();
                    p.startColor = new Color32((byte)color, (byte)(color >> 8), (byte)(color >> 16),
                                               (byte)(color >> 24));
                    p.remainingLifetime = r.ReadSingle();
                    p.startLifetime = r.ReadSingle();
                    p.randomSeed = r.ReadUInt32();
                    b.Particles[i] = p;
                }
                ps.SetParticles(b.Particles ?? new ParticleSystem.Particle[0], count);
                if (advance > 0 && count > 0)
                {
                    var emission = ps.emission;
                    bool enabled = emission.enabled;
                    try
                    {
                        emission.enabled = false;
                        ps.Simulate((float)advance, false, false, false);
                    }
                    finally
                    {
                        emission.enabled = enabled;
                    }
                }
                ps.Pause(false);
            }
            else
            {
                var g = (Graphic)b.Source;
                g.enabled = r.ReadBoolean();
                g.color = ReplaySceneCapture.Color(r);
                string key = ReplaySceneData.String(r, 1024);
                if (b.MaterialKey != key)
                {
                    var asset =
                        b.OriginalMaterial != null && ReplayMaterialLibrary.Key(b.OriginalMaterial) == key
                            ? b.OriginalMaterial
                            : materials.Find(key);
                    if (asset == null)
                        MissingAsset(key);
                    var old = b.Material;
                    b.Material = asset == null ? null : new Material(asset);
                    b.MaterialKey = key;
                    g.material = b.Material;
                    if (old != null)
                        UnityEngine.Object.Destroy(old);
                }
                uint mask = r.ReadUInt32();
                for (int i = 0; i < ReplaySceneCapture.Properties.Length; i++)
                {
                    float value = r.ReadSingle();
                    if (b.Material != null && (mask & (1u << i)) != 0 &&
                        b.Material.HasProperty(ReplaySceneCapture.Properties[i]))
                        b.Material.SetFloat(ReplaySceneCapture.Properties[i], value);
                }
                var scale = new Vector2(r.ReadSingle(), r.ReadSingle());
                if (b.Material != null && b.Material.HasProperty("_MainTex"))
                    b.Material.mainTextureScale = scale;
                if (kind == ReplaySceneData.Text)
                {
                    var text = (Text)g;
                    text.text = ReplaySceneData.String(r, 16384);
                    text.fontSize = r.ReadInt32();
                    text.fontStyle = (FontStyle)r.ReadInt32();
                    text.alignment = (TextAnchor)r.ReadInt32();
                    text.lineSpacing = r.ReadSingle();
                }
                else if (kind == ReplaySceneData.Image)
                {
                    var image = (Image)g;
                    Sprite sprite;
                    string id = ReplaySceneData.String(r, 1024);
                    sprites.TryGetValue(id, out sprite);
                    if (sprite == null)
                        MissingAsset(id);
                    image.overrideSprite = sprite;
                    image.fillAmount = r.ReadSingle();
                }
                else
                {
                    var raw = (RawImage)g;
                    Texture texture;
                    string id = ReplaySceneData.String(r, 1024);
                    textures.TryGetValue(id, out texture);
                    if (texture == null)
                        MissingAsset(id);
                    raw.texture = texture;
                    raw.uvRect = ReplaySceneCapture.Rect(r);
                }
            }
        }
    }

    public void Render(Camera main, RenderTexture target, RenderTexture scene)
    {
        // Keep the original texture object and every consumer intact between
        // samples; only the captured scene's display images follow this export.
        var images = new List<RawImage>();
        if (scene != null)
            foreach (var image in sceneImages)
                if (image != null && image.texture == scene)
                    images.Add(image);
        ReplayCameraPipeline.Render(main, target, scene, cameras, replacement =>
        {
            foreach (var image in images)
                if (image != null)
                    image.texture = replacement;
        });
    }
}
