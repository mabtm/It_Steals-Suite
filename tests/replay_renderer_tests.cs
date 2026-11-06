using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.Rendering;

// Execute the production renderer scope/clock with host doubles. This proves
// ownership, restoration and timing contracts, not Unity/D3D rasterization.
namespace UnityEngine.Rendering
{
public enum CameraEvent
{
    BeforeGBuffer,
    BeforeForwardOpaque,
    BeforeForwardAlpha,
    BeforeImageEffects,
    AfterEverything
}

public sealed class CommandBuffer
{
    public string name;
    public bool Released;
    public readonly Dictionary<string, Vector4> Values = new Dictionary<string, Vector4>();

    public void Clear()
    {
        Values.Clear();
    }

    public void SetGlobalVector(string key, Vector4 value)
    {
        Values[key] = value;
    }

    public void Release()
    {
        Released = true;
    }
}

public enum MotionVectorGenerationMode { Camera, Object, ForceNoMotion }

public enum ShadowCastingMode
{
    Off,
    On,
    TwoSided
}

public enum LightProbeUsage
{
    Off,
    BlendProbes
}

public enum ReflectionProbeUsage
{
    Off,
    BlendProbes
}
}

namespace UnityEngine
{
public struct Vector4
{
    public float x, y, z, w;

    public Vector4(float a, float b, float c, float d)
    {
        x = a;
        y = b;
        z = c;
        w = d;
    }
}

public static class Time
{
    public static double timeSinceLevelLoadAsDouble = 99;
}

public struct RenderTextureDescriptor
{
    public int width, height;
    public bool useDynamicScale;
}

public sealed class RenderTexture
{
    public static RenderTexture active;
    public static int Created, Released;
    public int width = 1920, height = 1080, Value, filterMode, wrapMode;
    public bool IsReleased;
    public RenderTextureDescriptor descriptor
    {
        get { return new RenderTextureDescriptor { width = width, height = height, useDynamicScale = true }; }
    }
    public static RenderTexture GetTemporary(RenderTextureDescriptor descriptor)
    {
        if (descriptor.useDynamicScale)
            throw new Exception("export must use exact requested resolution");
        Created++;
        return new RenderTexture { width = descriptor.width, height = descriptor.height };
    }
    public static void ReleaseTemporary(RenderTexture texture)
    {
        texture.IsReleased = true;
        Released++;
    }
}

public static class Canvas
{
    public static Action Update;
    public static int Updates;
    public static void ForceUpdateCanvases()
    {
        Updates++;
        if (Update != null)
            Update();
    }
}

public class MonoBehaviour
{
    public bool enabled = true;
}

public static class Graphics
{
    public static void Blit(RenderTexture source, RenderTexture target)
    {
        target.Value = source.Value;
    }
}

public sealed class Camera
{
    public RenderTexture targetTexture;
    public float aspect = 1, depth;
    public bool enabled = true;
    public GameObject gameObject = new GameObject(true);
    public Action Draw;
    public readonly List<MonoBehaviour> Effects = new List<MonoBehaviour>();
    public T[] GetComponents<T>() where T : MonoBehaviour
    {
        return Effects.ConvertAll(e => (T)e).ToArray();
    }

    public void Render()
    {
        if (Draw != null)
            Draw();
    }

    public readonly Dictionary<CameraEvent, CommandBuffer> Buffers =
        new Dictionary<CameraEvent, CommandBuffer>();

    public void AddCommandBuffer(CameraEvent stage, CommandBuffer buffer)
    {
        Buffers.Add(stage, buffer);
    }

    public void RemoveCommandBuffer(CameraEvent stage, CommandBuffer buffer)
    {
        Buffers.Remove(stage);
    }
}

public sealed class Shader
{
    public string name = "Game/Original";
}

public sealed class Material
{
    public string Name;
    public string name
    {
        get {
            return Name;
        }
    }
    public Shader shader = new Shader();

    public Material(string name)
    {
        Name = name;
    }
}

public sealed class GameObject
{
    public static int Created;
    public bool activeSelf;
    public int Changes;
    public bool activeInHierarchy
    {
        get {
            return activeSelf;
        }
    }

    public GameObject(bool active)
    {
        Created++;
        activeSelf = active;
    }

    public void SetActive(bool active)
    {
        activeSelf = active;
        Changes++;
    }
}

public sealed class MaterialPropertyBlock
{
    internal Dictionary<int, float> values = new Dictionary<int, float>();
    public bool isEmpty
    {
        get {
            return values.Count == 0;
        }
    }

    public void SetFloat(int id, float value)
    {
        values[id] = value;
    }

    public float GetFloat(int id)
    {
        float value;
        return values.TryGetValue(id, out value) ? value : 0;
    }

    internal void Copy(MaterialPropertyBlock source)
    {
        values = source == null ? new Dictionary<int, float>() : new Dictionary<int, float>(source.values);
    }
}

public sealed class SkinnedMeshRenderer
{
    public bool updateWhenOffscreen, forceMatrixRecalculationPerRender, skinnedMotionVectors = true;
    public MotionVectorGenerationMode motionVectorGenerationMode;
    public float Pose;
    private float cached;
    private int lastFrame = -1;
    public float RenderPose(int frame)
    {
        if (frame != lastFrame || forceMatrixRecalculationPerRender) cached = Pose;
        lastFrame = frame;
        return cached;
    }
}

public sealed class Renderer
{
    private Material[] materials;
    private readonly Dictionary<int, MaterialPropertyBlock> blocks =
        new Dictionary<int, MaterialPropertyBlock>();
    public int lightmapIndex = 9, realtimeLightmapIndex = 4;
    public LightProbeUsage lightProbeUsage = LightProbeUsage.BlendProbes;
    public ReflectionProbeUsage reflectionProbeUsage = ReflectionProbeUsage.BlendProbes;
    public bool enabled = true, receiveShadows = true, FailOnce;
    public ShadowCastingMode shadowCastingMode = ShadowCastingMode.TwoSided;
    // Model a renderer with only two material slots in one large static
    // batch. The batch mesh/range must never be expanded or replaced.
    public readonly object SharedBatch;
    public readonly int BatchStart, BatchCount;

    public Renderer(object batch, int start, int count, Material[] original)
    {
        SharedBatch = batch;
        BatchStart = start;
        BatchCount = count;
        materials = original;
    }

    public Material[] sharedMaterials
    {
        get {
            return (Material[])materials.Clone();
        }
        set {
            if (FailOnce)
            {
                FailOnce = false;
                throw new InvalidOperationException("simulated assignment failure");
            }
            materials = (Material[])value.Clone();
        }
    }

    public void GetPropertyBlock(MaterialPropertyBlock block, int slot = -1)
    {
        MaterialPropertyBlock current;
        blocks.TryGetValue(slot, out current);
        block.Copy(current);
    }

    public void SetPropertyBlock(MaterialPropertyBlock block, int slot = -1)
    {
        if (block == null)
            blocks.Remove(slot);
        else
        {
            var copy = new MaterialPropertyBlock();
            copy.Copy(block);
            blocks[slot] = copy;
        }
    }
}
}

namespace UnityEngine.Rendering.PostProcessing
{
public sealed class PostProcessLayer : MonoBehaviour
{
    public int Resets;
    public void ResetHistory() { Resets++; }
}
}

public static class ReplayRendererTests
{
    private static void Check(bool value, string message)
    {
        if (!value)
            throw new Exception(message);
    }

    private static bool Near(double a, double b)
    {
        return Math.Abs(a - b) < 1e-8;
    }

    public static int Main()
    {
        var recordedSkin = new SkinnedMeshRenderer();
        recordedSkin.Pose = 1;
        Check(recordedSkin.RenderPose(20) == 1, "initial arm pose");
        ReplayRendererScope.ConfigureRecordedSkin(recordedSkin);
        foreach (float pose in new[] { 4f, 1f, 4.5f, 1f, 2f, 1f })
        {
            recordedSkin.Pose = pose;
            Check(recordedSkin.RenderPose(20) == pose, "seek / export / restore recomputes skin in one Unity frame");
        }
        Check(!recordedSkin.skinnedMotionVectors && recordedSkin.motionVectorGenerationMode == MotionVectorGenerationMode.ForceNoMotion,
              "manual replay seeks do not use temporal motion from another timestamp");
        var world = new RenderTexture { width = 640, height = 360, Value = 77 };
        var output = new RenderTexture();
        var radar = new RenderTexture { width = 256, height = 256 };
        var prior = new RenderTexture();
        var presented = world;
        RenderTexture temporary = null;
        RenderTexture.active = prior;
        var main = new Camera { targetTexture = world, depth = -1 };
        var arms = new Camera { targetTexture = world, depth = 2 };
        var ui = new Camera { depth = 1 };
        var radarCamera = new Camera { targetTexture = radar, depth = -2 };
        var effect = new UnityEngine.Rendering.PostProcessing.PostProcessLayer();
        var disabledEffect = new UnityEngine.Rendering.PostProcessing.PostProcessLayer { enabled = false };
        main.Effects.Add(effect);
        ui.Effects.Add(disabledEffect);
        var stack = new List<Camera> { arms, ui, radarCamera, arms, null };
        var discovered = ReplayCameraPipeline.Sources(main, new[] { main, arms, ui }, ui, null, radarCamera);
        Check(discovered.Count == 3 && discovered.Contains(arms) && discovered.Contains(radarCamera) &&
                  !discovered.Contains(main),
              "unassigned body-camera field still discovers the actual hand camera, without duplicates");
        Action<RenderTexture> present = replacement => { presented = replacement; };
        Canvas.Update = () =>
        {
            if (main.targetTexture != world)
                Check(main.targetTexture == arms.targetTexture && ui.targetTexture == output &&
                          main.targetTexture.width == output.width && main.targetTexture.height == output.height,
                      "layout sees the complete export-size world/arms/UI rig, never mixed targets");
        };
        int frame = 0;
        main.Draw = () =>
        {
            Check(main.targetTexture == arms.targetTexture && main.targetTexture == presented &&
                      ui.targetTexture == output && main.targetTexture != world,
                  "render callbacks see all targets and display texture bound before the first render");
            Check(main.targetTexture.width == output.width && main.targetTexture.height == output.height &&
                      main.aspect == arms.aspect && arms.aspect == ui.aspect,
                  "world and arms render at exact output dimensions/aspect without upscaling the game texture");
            Check(effect.Resets == frame * 2 + 1, "temporal history reset before every exported sample");
            main.targetTexture.Value = ++frame;
        };
        arms.Draw = () => arms.targetTexture.Value += 1000;
        ui.Draw = () => ui.targetTexture.Value = presented.Value + 10000;
        radarCamera.Draw = () =>
        {
            Check(radarCamera.targetTexture == radar && radar.width == 256 && radarCamera.aspect == 1,
                  "independent radar keeps its original dimensions");
            radar.Value = frame;
        };
        for (int i = 1; i <= 120; i++)
        {
            // Exercise high/low resolution and different output aspect ratios.
            output.width = i % 2 == 0 ? 3840 : 320;
            output.height = i % 2 == 0 ? 2160 : 240;
            arms.depth = i % 3 == 0 ? main.depth : 2;
            ReplayCameraPipeline.Render(main, output, world, stack, present);
            Check(output.Value == i + 11000 && world.Value == 77 && radar.Value == i - 1,
                  "world and hands (including equal-depth cameras) composite in order at output size without altering game-owned texture contents");
            Check(main.targetTexture == world && arms.targetTexture == world && ui.targetTexture == null &&
                      radarCamera.targetTexture == radar && main.aspect == 1 && ui.aspect == 1 &&
                      RenderTexture.active == prior && presented == world && effect.Resets == i * 2 &&
                      disabledEffect.Resets == 0,
                  "offscreen rendering restores cameras/display/layout and invalidates export history before gameplay resumes");
        }
        Check(Canvas.Updates == 240 && RenderTexture.Created == RenderTexture.Released,
              "every sample recalculates and restores canvas layout and releases its temporary scene target");
        Canvas.Update = null;
        arms.enabled = ui.enabled = radarCamera.enabled = false;
        main.Draw = () => main.targetTexture.Value = ++frame;
        ReplayCameraPipeline.Render(main, output, null, stack);
        Check(output.Value == 121, "follow/simplified camera renders directly without the recorded UI");
        arms.enabled = true;
        ui.enabled = true;
        present = replacement => { presented = replacement; if (replacement != world) temporary = replacement; };
        arms.Draw = () =>
        { throw new InvalidOperationException("body render failed"); };
        bool renderFailed = false;
        try
        {
            ReplayCameraPipeline.Render(main, output, world, stack, present);
        }
        catch (InvalidOperationException)
        {
            renderFailed = true;
        }
        Check(renderFailed && main.targetTexture == world && arms.targetTexture == world &&
                  ui.targetTexture == null && presented == world && temporary.IsReleased &&
                  arms.aspect == 1 && RenderTexture.active == prior,
              "render failure restores all targets/consumers and releases the export scene texture");
        var asset = new Material("Eye");
        var instance = new Material("Eye (Instance)");
        var library = new ReplayMaterialLibrary(new[] { instance, asset });
        Check(ReplayMaterialLibrary.Key(asset) == ReplayMaterialLibrary.Key(instance) &&
                  ReferenceEquals(library.Find(ReplayMaterialLibrary.Key(instance)), asset),
              "runtime instances resolve to the original shader/material asset");
        library = new ReplayMaterialLibrary(new[] { asset, instance });
        Check(ReferenceEquals(library.Find(ReplayMaterialLibrary.Key(asset)), asset),
              "material resolution is independent of asset discovery order");
        var otherShader = new Material("Eye");
        otherShader.shader.name = "Game/Hologram";
        library = new ReplayMaterialLibrary(new[] { asset, otherShader });
        Check(ReferenceEquals(library.Find(ReplayMaterialLibrary.Key(otherShader)), otherShader),
              "equal material names with different shaders remain distinct");
        library = new ReplayMaterialLibrary(new[] { asset, new Material("Eye"), instance });
        Check(library.Find(ReplayMaterialLibrary.Key(asset)) == null && library.Find("missing") == null,
              "ambiguous or absent materials never select an arbitrary asset");
        Material originalA = new Material("wall"), originalB = new Material("floor"),
                 flat = new Material("shared flat");
        var batch = new object();
        Renderer a = new Renderer(batch, 31, 2, new[] { originalA, originalB }),
                 b = new Renderer(batch, 90, 1, new[] { originalB });
        var block = new MaterialPropertyBlock();
        block.SetFloat(1, 0);
        block.SetFloat(2, .35f);
        a.SetPropertyBlock(block);
        block.SetFloat(2, .75f);
        a.SetPropertyBlock(block, 1);
        GameObject culled = new GameObject(false), active = new GameObject(true),
                   unused = new GameObject(false);
        int created = GameObject.Created;
        var restore = new ReplayRestoreScope();
        ReplayRendererScope.Reveal(restore, culled);
        ReplayRendererScope.Reveal(restore, culled);
        ReplayRendererScope.Reveal(restore, active);
        ReplayRendererScope.Apply(restore, a, new[] { flat, flat });
        ReplayRendererScope.Apply(restore, b, new[] { flat });
        Check(a.sharedMaterials.Length == 2 && b.sharedMaterials.Length == 1 &&
                  ReferenceEquals(a.sharedMaterials[0], b.sharedMaterials[0]),
              "one shared material without expanding batch submeshes");
        Check(ReferenceEquals(a.SharedBatch, b.SharedBatch) && a.BatchStart == 31 && a.BatchCount == 2 &&
                  b.BatchStart == 90,
              "original batch ownership/ranges retained");
        a.GetPropertyBlock(block);
        Check(block.isEmpty, "global dark/alpha overrides cleared");
        a.GetPropertyBlock(block, 1);
        Check(block.isEmpty, "per-material overrides cleared");
        Check(a.shadowCastingMode == ShadowCastingMode.Off && !a.receiveShadows && culled.activeSelf &&
                  !unused.activeSelf,
              "flat visible scene, no shadows, unrelated inactive variant preserved");
        Check(a.lightmapIndex == -1 && a.realtimeLightmapIndex == -1 &&
                  a.lightProbeUsage == LightProbeUsage.Off &&
                  a.reflectionProbeUsage == ReflectionProbeUsage.Off,
              "original baked darkness / probes disabled only in replay");
        ReplayRendererScope.Hide(restore, b);
        Check(!b.enabled, "original actor renderer can be hidden");
        restore.Dispose();
        restore.Dispose();
        Check(ReferenceEquals(a.sharedMaterials[0], originalA) &&
                  ReferenceEquals(a.sharedMaterials[1], originalB) &&
                  ReferenceEquals(b.sharedMaterials[0], originalB),
              "original materials restored exactly");
        a.GetPropertyBlock(block);
        Check(block.GetFloat(2) == .35f, "global property block restored");
        a.GetPropertyBlock(block, 1);
        Check(block.GetFloat(2) == .75f, "per-slot property block restored");
        Check(a.shadowCastingMode == ShadowCastingMode.TwoSided && a.receiveShadows && b.enabled &&
                  !culled.activeSelf && culled.Changes == 2 && active.Changes == 0 && !unused.activeSelf,
              "visibility and shadow state restored once");
        Check(a.lightmapIndex == 9 && a.realtimeLightmapIndex == 4 &&
                  a.lightProbeUsage == LightProbeUsage.BlendProbes &&
                  a.reflectionProbeUsage == ReflectionProbeUsage.BlendProbes,
              "lightmap and probe state restored exactly");
        Check(GameObject.Created == created, "styling creates no mesh/scene copies");
        var fail = new ReplayRestoreScope();
        a.FailOnce = true;
        bool rejected = false;
        try
        {
            ReplayRendererScope.Apply(fail, a, new[] { flat, flat });
        }
        catch (InvalidOperationException)
        {
            rejected = true;
        }
        fail.Dispose();
        Check(rejected && ReferenceEquals(a.sharedMaterials[0], originalA),
              "partial renderer failure is recoverable");
        var clock = new ReplayPlaybackClock();
        double now = 1000000;
        clock.Set(now, 3, 600, true, 1);
        double elapsed = 0;
        var jitter = new[] { 1.0 / 240, 1.0 / 90, 1.0 / 37, .8, .004, .031 };
        for (int i = 0; i < 2000; i++)
        {
            elapsed += jitter[i % jitter.Length];
            Check(Near(clock.At(now + elapsed), 3 + elapsed),
                  "playback follows elapsed time across uneven frames and stalls");
        }
        double at = clock.At(now + elapsed);
        clock.Set(now + elapsed, at, 600, false, 1);
        Check(Near(clock.At(now + elapsed + 100), at) && !clock.Playing(now + elapsed + 100),
              "pause does not consume time");
        clock.Set(200, 5, 600, true, .5);
        Check(Near(clock.At(202), 6), "slow motion uses anchor");
        clock.Set(202, 6, 600, true, 2);
        Check(Near(clock.At(204), 10), "rate change stays continuous");
        clock.Set(204, 1, 600, true, 1);
        Check(Near(clock.At(204), 1) && Near(clock.At(204.125), 1.125), "seek anchors exact recording time");
        clock.Set(10, 1, 2, true, 8);
        Check(clock.At(100) == 2 && !clock.Playing(100), "end stops exactly");
        clock.Set(double.NaN, double.NaN, double.PositiveInfinity, true, double.NaN);
        Check(clock.At(50) == 0 && !clock.Playing(50), "invalid control values bounded");
        clock.Set(100, 3, 20, true, 2);
        clock.Loop(true, 2, 5);
        Check(Near(clock.At(102), 4) && clock.Playing(1000),
              "loop wraps overshoot at actual rate without pausing");
        clock.Set(200, 2, 20, true, .5);
        clock.Loop(true, 2, 5);
        Check(Near(clock.At(206), 2) && Near(clock.At(206.5), 2.25),
              "exact B returns to A, fractional rate retained");
        clock.Set(300, 4, 20, false, 2);
        Check(Near(clock.At(400), 4) && !clock.Playing(400), "pause does not wrap a loop");
        clock.Loop(true, 3, 3.01);
        clock.Set(500, 4, 20, true, 1);
        Check(Near(clock.At(501), 5), "too-small loop rejected");
        clock.Loop(true, double.NaN, 4);
        Check(Near(clock.At(502), 6), "invalid loop rejected");
        clock.Set(600, 19, 20, true, 8);
        clock.Loop(true, 2, 5);
        Check(clock.At(601) >= 2 && clock.At(601) < 5 && clock.Playing(601),
              "loop can recover a seek beyond B");
        var shaderCamera = new Camera();
        CommandBuffer shaderBuffer;
        using (var shader = new ReplayShaderClock(shaderCamera, new[] { "Unity shader clock / 12.5" }))
        {
            shader.Set(2);
            shaderBuffer = shaderCamera.Buffers[CameraEvent.BeforeForwardAlpha];
            Check(shader.RecordedOrigin && Near(shaderBuffer.Values["_Time"].y, 14.5),
                  "shader time includes recorded scene phase while game time is paused");
            shader.Set(1);
            Check(Near(shaderBuffer.Values["_Time"].y, 13.5), "shader time supports backward seek");
            shader.Set(2);
            Check(Near(shaderBuffer.Values["_Time"].y, 14.5) && shaderBuffer.Values.Count == 3,
                  "repeat exports evaluate exactly the same material phase without accumulating commands");
            Check(Near(shaderCamera.Buffers[CameraEvent.AfterEverything].Values["_Time"].y, 99),
                  "normal scene time is restored after the replay camera");
        }
        Check(shaderCamera.Buffers.Count == 0 && shaderBuffer.Released,
              "replay shader command buffers are detached and released");
        using (var legacy = new ReplayShaderClock(shaderCamera, new[] { "Unity shader clock / NaN" }))
        {
            legacy.Set(2);
            Check(!legacy.RecordedOrigin &&
                      Near(shaderCamera.Buffers[CameraEvent.BeforeForwardAlpha].Values["_Time"].y, 2),
                  "legacy recordings and malformed origins use a finite replay-relative shader clock");
        }
        Console.WriteLine(
            "Scene renderer adapter: original batched ranges/material slots, property blocks, culled geometry vs inactive variants, shared flat materials, no scene copies, complete/idempotent/failure rollback: PASS");
        Console.WriteLine(
            "Monotonic replay clock: 2000 uneven frames/stalls, pause, speed changes, exact seek, end and invalid values: PASS");
        return 0;
    }
}
