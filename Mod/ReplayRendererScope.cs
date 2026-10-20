using System;
using UnityEngine;
using UnityEngine.Rendering;
using System.Collections.Generic;

// Style the existing renderer. Do not copy its MeshFilter: a static-batched
// renderer owns a sub-range of a shared combined mesh, not that whole mesh.
public static class ReplayRendererScope
{
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
