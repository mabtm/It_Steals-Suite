using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.SceneManagement;

// State-contract doubles. GPU shader output still needs the running game.
namespace UnityEngine.SceneManagement
{
public struct Scene
{
    public int handle;
}

public static class SceneManager
{
    public static int Handle = 1;

    public static Scene GetActiveScene()
    {
        return new Scene { handle = Handle };
    }
}
}

namespace UnityEngine
{
public class Texture
{
}

public class Shader
{
    public string name;
    public bool isSupported = true;
    public static int Searches;
    public static readonly Dictionary<string, Shader> Catalog = new Dictionary<string, Shader>();

    public static Shader Find(string name)
    {
        Searches++;
        Shader shader;
        return Catalog.TryGetValue(name, out shader) ? shader : null;
    }
}

public static class RenderSettings
{
    public static bool fog = true;
    public static readonly object Ambient = new object();
}

public class Material
{
    static int next;
    readonly int id = ++next;

    public int GetInstanceID()
    {
        return id;
    }

    Shader current;
    public int Writes, renderQueue = 2000;
    public bool FailApply, FailRestore, Cutout, enableInstancing, InstanceKeyword;
    public float Mode;
    public Shader shader
    {
        get {
            return current;
        }
        set {
            if (FailApply && value != null && value.name.Contains("Unlit"))
            {
                FailApply = false;
                throw new InvalidOperationException("apply failure");
            }
            if (FailRestore && value != null && value.name == "Standard")
            {
                FailRestore = false;
                throw new InvalidOperationException("restore failure");
            }
            current = value;
            renderQueue = 2000;
            Writes++;
        }
    }

    public Material(Shader s)
    {
        current = s;
    }

    public readonly Texture Texture = new Texture();
    public object UV = new object(), Color = new object();
    public bool Textured = true;

    public bool HasProperty(string name)
    {
        return name == "_MainTex" || name == "_Mode";
    }

    public Texture GetTexture(string name)
    {
        return Textured ? Texture : null;
    }

    public bool IsKeywordEnabled(string name)
    {
        return name == "INSTANCING_ON" ? InstanceKeyword : Cutout;
    }

    public void EnableKeyword(string name)
    {
        InstanceKeyword = true;
    }

    public void DisableKeyword(string name)
    {
        InstanceKeyword = false;
    }

    public float GetFloat(string name)
    {
        return Mode;
    }

    public string GetTag(string name, bool fallback, string value)
    {
        return Mode >= 2 ? "Transparent" : "Opaque";
    }
}

public class Renderer
{
    static int next;
    readonly int id = ++next;

    public int GetInstanceID()
    {
        return id;
    }

    public Material[] sharedMaterials;
    public readonly object PropertyBlock = new object();
    public readonly int BatchStart = 24, BatchCount = 17;
    public int lightmapIndex = 9, realtimeLightmapIndex = 4;
}

public sealed class MeshRenderer : Renderer
{
}

public sealed class SkinnedMeshRenderer : Renderer
{
}

public sealed class SpriteRenderer : Renderer
{
}
}

public static class FullbrightTests
{
    static void Check(bool yes, string why)
    {
        if (!yes)
            throw new Exception(why);
    }

    static Shader Register(string name)
    {
        var shader = new Shader { name = name };
        Shader.Catalog[name] = shader;
        return shader;
    }

    public static int Main()
    {
        var standard = Register("Standard");
        var texture = Register("Unlit/Texture");
        var color = Register("Unlit/Color");
        var toz = Register("TOZ/Object/TriProj/World/StandardBumped");
        var projected = Register("TOZ/Object/TriProj/World/Unlit");
        var external = Register("Other shader");
        var wall = new Material(toz) { renderQueue = 2010 };
        var body = new Material(standard);
        var plain =
            new Material(standard) { Textured = false, enableInstancing = true, InstanceKeyword = true };
        var transparent = new Material(standard) { Mode = 2, renderQueue = 3000 };
        var cutout = new Material(standard) { Cutout = true, Mode = 1 };
        var a = new MeshRenderer { sharedMaterials = new[] { wall, null, body, transparent, cutout } };
        var b = new SkinnedMeshRenderer { sharedMaterials = new[] { body, plain } };
        var sprite = new SpriteRenderer { sharedMaterials = new[] { new Material(standard) } };
        var materials = a.sharedMaterials;
        var block = a.PropertyBlock;
        var uv = wall.UV;
        var tint = wall.Color;
        var scope = new SceneBrightness();
        scope.Add(a);
        scope.Add(b);
        scope.Add(a);
        scope.Add(sprite);
        Check(scope.Count == 2, "only distinct mesh and skin renderers");
        scope.SetActive(true);
        Check(wall.shader == projected && body.shader == texture && plain.shader == color,
              "matching triplanar, textured and solid unlit shaders");
        Check(!plain.enableInstancing && !plain.InstanceKeyword,
              "stripped unlit instancing variants never requested");
        Check(wall.renderQueue == 2010 && transparent.shader == standard && cutout.shader == standard,
              "queues retained; stripped alpha variants never turn opaque");
        Check(!RenderSettings.fog && ReferenceEquals(materials, a.sharedMaterials) &&
                  ReferenceEquals(block, a.PropertyBlock) && ReferenceEquals(uv, wall.UV) &&
                  ReferenceEquals(tint, wall.Color) && a.lightmapIndex == 9 && a.realtimeLightmapIndex == 4 &&
                  a.BatchStart == 24 && a.BatchCount == 17,
              "resources, texture mapping, lighting and static batch ranges preserved");
        int writes = wall.Writes + body.Writes + plain.Writes, searches = Shader.Searches;
        for (int i = 0; i < 10000; i++)
            scope.SetActive(true);
        Check(writes == wall.Writes + body.Writes + plain.Writes && searches == Shader.Searches,
              "10000 stable frames: no writes/searches/material creation");
        var late = new Material(standard);
        scope.AddMaterials(new[] { late, body });
        Check(late.shader == texture, "late replay material inherits unlit exactly once");
        var liveTint = new object();
        wall.Color = liveTint;
        scope.SetActive(false);
        Check(wall.shader == toz && wall.renderQueue == 2010 && body.shader == standard &&
                  late.shader == standard && RenderSettings.fog && ReferenceEquals(wall.Color, liveTint),
              "original shaders restored without discarding live properties");
        Check(plain.enableInstancing && plain.InstanceKeyword, "original GPU instancing settings restored");
        scope.SetActive(true);
        body.shader = external;
        scope.SetActive(false);
        Check(body.shader == external, "newer shader writer retained");
        scope.Dispose();
        scope.Dispose();
        var alpha = Register("Unlit/Transparent");
        var alphaScope = new SceneBrightness();
        alphaScope.AddMaterials(new[] { transparent });
        alphaScope.SetActive(true);
        Check(transparent.shader == alpha && transparent.renderQueue == 3000,
              "existing alpha variant uses correct shader and queue");
        alphaScope.Dispose();
        Check(transparent.shader == standard && transparent.renderQueue == 3000, "alpha restore");
        var first = new Material(standard);
        var failed = new Material(standard) { FailApply = true };
        var rollback = new SceneBrightness();
        rollback.AddMaterials(new[] { first, failed });
        bool rejected = false;
        try
        {
            rollback.SetActive(true);
        }
        catch (InvalidOperationException)
        {
            rejected = true;
        }
        Check(rejected && !rollback.Active && first.shader == standard && failed.shader == standard &&
                  RenderSettings.fog,
              "partial activation rolls back every material and fog");
        rollback.Dispose();
        var broken = new Material(standard);
        var remaining = new Material(standard);
        var cleanup = new SceneBrightness();
        cleanup.AddMaterials(new[] { broken, remaining });
        cleanup.SetActive(true);
        broken.FailRestore = true;
        rejected = false;
        try
        {
            cleanup.Dispose();
        }
        catch (InvalidOperationException)
        {
            rejected = true;
        }
        Check(rejected && remaining.shader == standard && RenderSettings.fog,
              "restore error cannot skip other materials or environment");
        cleanup.Dispose();
        var persistent = new Material(standard);
        var sceneScope = new SceneBrightness();
        sceneScope.AddMaterials(new[] { persistent });
        sceneScope.SetActive(true);
        SceneManager.Handle = 2;
        RenderSettings.fog = false;
        sceneScope.Dispose();
        Check(persistent.shader == standard && !RenderSettings.fog,
              "old scope cannot overwrite a new scene environment");
        Console.WriteLine(
            "Fullbright production shader scope: triplanar/textured/solid unlit, alpha fallback, original textures/UVs/property blocks/static batches, zero stable-frame work, late materials, shader/queue restoration, newer writers, activation rollback and scene ownership: PASS");
        return 0;
    }
}
