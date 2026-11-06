using System;
using UnityEngine;
using UnityEngine.Rendering;
using System.Collections.Generic;

// Material assets stay in the installed game. Prefer the original asset over
// runtime instances, and refuse ambiguous names instead of choosing at random.
public sealed class ReplayMaterialLibrary
{
    private sealed class Entry
    {
        public Material Asset;
        public bool Original;
    }

    private readonly Dictionary<string, Entry> assets = new Dictionary<string, Entry>(StringComparer.Ordinal);

    public static string Key(Material material)
    {
        if (material == null || material.shader == null)
            return "";
        string name = material.name ?? "";
        while (name.EndsWith(" (Instance)", StringComparison.Ordinal))
            name = name.Substring(0, name.Length - 11);
        return "M|" + name + "|" + material.shader.name;
    }

    public ReplayMaterialLibrary(IEnumerable<Material> materials)
    {
        foreach (var material in materials)
        {
            string key = Key(material);
            if (key.Length == 0)
                continue;
            bool original = !(material.name ?? "").EndsWith(" (Instance)", StringComparison.Ordinal);
            Entry previous;
            if (!assets.TryGetValue(key, out previous) || original && !previous.Original)
                assets[key] = new Entry { Asset = material, Original = original };
            else if (original == previous.Original && previous.Asset != material)
                previous.Asset = null;
        }
    }

    public Material Find(string key)
    {
        Entry found;
        return assets.TryGetValue(key, out found) ? found.Asset : null;
    }
}

// Style the existing renderer. Do not copy its MeshFilter: a static-batched
// renderer owns a sub-range of a shared combined mesh, not that whole mesh.
public static class ReplayRendererScope
{
    public static void ConfigureRecordedSkin(SkinnedMeshRenderer skin)
    {
        // Manual export evaluates and restores multiple absolute poses in one
        // Unity frame. Frame-cached bone matrices / previous-pose vectors are
        // stale in that sequence, even though the transforms are up to date.
        skin.updateWhenOffscreen = true;
        skin.forceMatrixRecalculationPerRender = true;
        skin.skinnedMotionVectors = false;
        skin.motionVectorGenerationMode = MotionVectorGenerationMode.ForceNoMotion;
    }
    public static void Apply(ReplayRestoreScope restore, Renderer renderer, Material[] materials)
    {
        var original = renderer.sharedMaterials;
        var shadow = renderer.shadowCastingMode;
        bool receive = renderer.receiveShadows;
        int lightmap = renderer.lightmapIndex, realtime = renderer.realtimeLightmapIndex;
        var probes = renderer.lightProbeUsage;
        var reflections = renderer.reflectionProbeUsage;
        var block = new MaterialPropertyBlock();
        renderer.GetPropertyBlock(block);
        var slots = new MaterialPropertyBlock[original.Length];
        for (int i = 0; i < slots.Length; i++)
        {
            var saved = new MaterialPropertyBlock();
            renderer.GetPropertyBlock(saved, i);
            if (!saved.isEmpty)
                slots[i] = saved;
        }
        restore.Add(() =>
                    {
                        if (renderer == null)
                            return;
                        renderer.sharedMaterials = original;
                        renderer.shadowCastingMode = shadow;
                        renderer.receiveShadows = receive;
                        renderer.lightmapIndex = lightmap;
                        renderer.realtimeLightmapIndex = realtime;
                        renderer.lightProbeUsage = probes;
                        renderer.reflectionProbeUsage = reflections;
                        renderer.SetPropertyBlock(block.isEmpty ? null : block);
                        for (int i = 0; i < slots.Length; i++)
                            renderer.SetPropertyBlock(slots[i], i);
                    });
        renderer.sharedMaterials = materials;
        renderer.shadowCastingMode = ShadowCastingMode.Off;
        renderer.receiveShadows = false;
        renderer.lightmapIndex = -1;
        renderer.realtimeLightmapIndex = -1;
        renderer.lightProbeUsage = LightProbeUsage.Off;
        renderer.reflectionProbeUsage = ReflectionProbeUsage.Off;
        renderer.SetPropertyBlock(null);
        for (int i = 0; i < slots.Length; i++)
            renderer.SetPropertyBlock(null, i);
    }

    public static void Hide(ReplayRestoreScope restore, Renderer renderer)
    {
        bool enabled = renderer.enabled;
        restore.Add(() =>
                    {
                        if (renderer != null)
                            renderer.enabled = enabled;
                    });
        renderer.enabled = false;
    }

    public static void Reveal(ReplayRestoreScope restore, GameObject obj)
    {
        if (obj == null || obj.activeSelf)
            return;
        restore.Add(() =>
                    {
                        if (obj != null)
                            obj.SetActive(false);
                    });
        obj.SetActive(true);
    }
}

// A failed renderer/camera restore must never prevent restoring game time or
// input. Main-thread-only; rollback is LIFO and safe to invoke repeatedly.
public sealed class ReplayRestoreScope : IDisposable
{
    private readonly List<Action> undo = new List<Action>();
    public string Error { get; private set; }

    public void Add(Action action)
    {
        if (action == null)
            throw new ArgumentNullException("action");
        undo.Add(action);
    }

    public void Dispose()
    {
        for (int i = undo.Count - 1; i >= 0; i--)
            try
            {
                undo[i]();
            }
            catch (Exception e)
            {
                if (Error == null)
                    Error = e.Message;
            }
        undo.Clear();
    }
}

// Unity pauses its built-in shader time with gameplay. Replay cameras instead
// evaluate material effects at the requested recording time, including seeks.
public sealed class ReplayShaderClock : IDisposable
{
    public const string OriginName = "Unity shader clock / ";
    private static readonly CameraEvent[] stages = { CameraEvent.BeforeGBuffer,
                                                     CameraEvent.BeforeForwardOpaque,
                                                     CameraEvent.BeforeForwardAlpha,
                                                     CameraEvent.BeforeImageEffects };
    private readonly Camera camera;
    private readonly CommandBuffer begin = new CommandBuffer { name = "Replay material time" };
    private readonly CommandBuffer end = new CommandBuffer { name = "Restore material time" };
    private readonly double origin;
    private bool disposed;
    public readonly bool RecordedOrigin;

    public ReplayShaderClock(Camera view, IEnumerable<string> names)
    {
        camera = view;
        foreach (string name in names)
        {
            double value;
            if (name.StartsWith(OriginName, StringComparison.Ordinal) &&
                double.TryParse(name.Substring(OriginName.Length), System.Globalization.NumberStyles.Float,
                                System.Globalization.CultureInfo.InvariantCulture, out value) &&
                !double.IsNaN(value) && !double.IsInfinity(value) && value >= 0 && value <= 315360000)
            {
                origin = value;
                RecordedOrigin = true;
                break;
            }
        }
        try
        {
            foreach (var stage in stages)
                camera.AddCommandBuffer(stage, begin);
            camera.AddCommandBuffer(CameraEvent.AfterEverything, end);
            Set(0);
        }
        catch
        {
            Dispose();
            throw;
        }
    }

    private static void Parameters(CommandBuffer buffer, double time)
    {
        buffer.Clear();
        float t = (float)time;
        buffer.SetGlobalVector("_Time", new Vector4(t / 20, t, t * 2, t * 3));
        buffer.SetGlobalVector("_SinTime", new Vector4((float)Math.Sin(time / 8), (float)Math.Sin(time / 4),
                                                       (float)Math.Sin(time / 2), (float)Math.Sin(time)));
        buffer.SetGlobalVector("_CosTime", new Vector4((float)Math.Cos(time / 8), (float)Math.Cos(time / 4),
                                                       (float)Math.Cos(time / 2), (float)Math.Cos(time)));
    }

    public void Set(double seconds)
    {
        if (disposed || double.IsNaN(seconds) || double.IsInfinity(seconds) || seconds < 0)
            return;
        Parameters(begin, origin + seconds);
        Parameters(end, Time.timeSinceLevelLoadAsDouble);
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        if (camera != null)
        {
            foreach (var stage in stages)
                camera.RemoveCommandBuffer(stage, begin);
            camera.RemoveCommandBuffer(CameraEvent.AfterEverything, end);
        }
        begin.Release();
        end.Release();
    }
}

// Preserve the game's world/arms texture and the UI camera that presents it.
public static class ReplayCameraPipeline
{
    public static HashSet<Camera> Sources(Camera main, IEnumerable<Camera> rig, params Camera[] references)
    {
        var result = new HashSet<Camera>(references);
        foreach (var camera in rig)
            result.Add(camera);
        result.Remove(null);
        result.Remove(main);
        return result;
    }

    private sealed class Binding
    {
        public Camera Camera;
        public RenderTexture Target;
        public float Aspect;
    }

    private static void ResetHistory(Camera camera)
    {
        // The installed game's post-processing package owns these temporal
        // buffers. Manual export can seek backwards or render many samples in
        // one Unity frame, so no earlier sample is a valid history frame.
        // Keep this optional package out of the replay file/SDK contract.
        if (camera == null)
            return;
        foreach (var effect in camera.GetComponents<MonoBehaviour>())
            if (effect != null && effect.enabled &&
                effect.GetType().FullName == "UnityEngine.Rendering.PostProcessing.PostProcessLayer")
            {
                var reset = effect.GetType().GetMethod("ResetHistory", Type.EmptyTypes);
                if (reset != null)
                    reset.Invoke(effect, null);
            }
    }

    public static void Render(Camera main, RenderTexture output, RenderTexture scene, List<Camera> cameras,
                              Action<RenderTexture> presentScene = null)
    {
        var active = RenderTexture.active;
        RenderTexture exportScene = null;
        var bindings = new List<Binding>();
        var chain = new List<Camera>();
        var unique = new HashSet<Camera>();
        foreach (var camera in cameras)
            if (camera != null && camera != main && camera.enabled &&
                camera.gameObject.activeInHierarchy && unique.Add(camera))
                chain.Add(camera);
        chain.Add(main);
        chain.Sort((a, b) =>
        {
            if (a == b)
                return 0;
            int order = a.depth.CompareTo(b.depth);
            // The world camera clears the shared target. Cameras at the same
            // depth (notably hands in older captures) must composite after it.
            return order != 0 ? order : a == main ? -1 : b == main ? 1 : 0;
        });
        try
        {
            if (scene != null)
            {
                var descriptor = scene.descriptor;
                descriptor.width = output.width;
                descriptor.height = output.height;
                descriptor.useDynamicScale = false;
                exportScene = RenderTexture.GetTemporary(descriptor);
                exportScene.filterMode = scene.filterMode;
                exportScene.wrapMode = scene.wrapMode;
            }
            // Bind the complete rig before any OnPreCull/OnRenderImage or
            // canvas callback observes its size. In particular the arms and
            // UI must never see a mixture of game and export resolutions.
            foreach (var camera in chain)
            {
                var binding = new Binding { Camera = camera, Target = camera.targetTexture,
                                            Aspect = camera.aspect };
                bindings.Add(binding);
                RenderTexture target = camera == main || scene != null && binding.Target == scene
                                           ? exportScene ?? output
                                           : binding.Target ?? output;
                camera.targetTexture = target;
                camera.aspect = (float)target.width / target.height;
            }
            if (presentScene != null && exportScene != null)
                presentScene(exportScene);
            Canvas.ForceUpdateCanvases();
            foreach (var binding in bindings)
                ResetHistory(binding.Camera);
            // Independent textures (for example the radar) keep their own
            // resolution; only cameras sharing the world texture composite.
            foreach (var binding in bindings)
                if (binding.Camera != main && binding.Target != null && binding.Target != scene)
                    binding.Camera.Render();
            foreach (var binding in bindings)
                if (binding.Camera == main || scene != null && binding.Target == scene)
                    binding.Camera.Render();
            if (exportScene != null)
                Graphics.Blit(exportScene, output);
            foreach (var binding in bindings)
                if (binding.Camera != main && binding.Target == null)
                    binding.Camera.Render();
        }
        finally
        {
            try
            {
                // Restore every participant even when a game render callback
                // fails. The game-owned texture is never resized or released.
                foreach (var binding in bindings)
                {
                    if (binding.Camera != null)
                    {
                        binding.Camera.targetTexture = binding.Target;
                        binding.Camera.aspect = binding.Aspect;
                    }
                }
                if (presentScene != null && exportScene != null)
                    presentScene(scene);
                Canvas.ForceUpdateCanvases();
                foreach (var binding in bindings)
                    ResetHistory(binding.Camera);
            }
            finally
            {
                if (exportScene != null)
                    RenderTexture.ReleaseTemporary(exportScene);
                RenderTexture.active = active;
            }
        }
    }
}
