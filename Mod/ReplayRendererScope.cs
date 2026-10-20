using System;
using UnityEngine;
using UnityEngine.Rendering;

// Style the existing renderer. Do not copy its MeshFilter: a static-batched
// renderer owns a sub-range of a shared combined mesh, not that whole mesh.
public static class ReplayRendererScope
{
    public static void Apply(ReplayRestoreScope restore,Renderer renderer,Material[] materials)
    {
        var original=renderer.sharedMaterials;var shadow=renderer.shadowCastingMode;bool receive=renderer.receiveShadows;
        int lightmap=renderer.lightmapIndex,realtime=renderer.realtimeLightmapIndex;var probes=renderer.lightProbeUsage;var reflections=renderer.reflectionProbeUsage;
        var block=new MaterialPropertyBlock();renderer.GetPropertyBlock(block);
        var slots=new MaterialPropertyBlock[original.Length];
        for(int i=0;i<slots.Length;i++){var saved=new MaterialPropertyBlock();renderer.GetPropertyBlock(saved,i);if(!saved.isEmpty)slots[i]=saved;}
        restore.Add(()=>{
            if(renderer==null)return;
            renderer.sharedMaterials=original;renderer.shadowCastingMode=shadow;renderer.receiveShadows=receive;
            renderer.lightmapIndex=lightmap;renderer.realtimeLightmapIndex=realtime;renderer.lightProbeUsage=probes;renderer.reflectionProbeUsage=reflections;
            renderer.SetPropertyBlock(block.isEmpty?null:block);
            for(int i=0;i<slots.Length;i++)renderer.SetPropertyBlock(slots[i],i);
        });
        renderer.sharedMaterials=materials;renderer.shadowCastingMode=ShadowCastingMode.Off;renderer.receiveShadows=false;
        renderer.lightmapIndex=-1;renderer.realtimeLightmapIndex=-1;renderer.lightProbeUsage=LightProbeUsage.Off;renderer.reflectionProbeUsage=ReflectionProbeUsage.Off;
        renderer.SetPropertyBlock(null);for(int i=0;i<slots.Length;i++)renderer.SetPropertyBlock(null,i);
    }
    public static void Hide(ReplayRestoreScope restore,Renderer renderer)
    {
        bool enabled=renderer.enabled;restore.Add(()=>{if(renderer!=null)renderer.enabled=enabled;});renderer.enabled=false;
    }
    public static void Reveal(ReplayRestoreScope restore,GameObject obj)
    {
        if(obj==null||obj.activeSelf)return;
        restore.Add(()=>{if(obj!=null)obj.SetActive(false);});obj.SetActive(true);
    }
}
