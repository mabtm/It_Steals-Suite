using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.SceneManagement;

// Reversible, cached shader scope. Keep each existing renderer, material,
// property block and static-batch sub-range; never rebuild a combined mesh.
// Editing the material shader also keeps replay visibility variants and
// animated property blocks attached to the same live material instances.
public sealed class SceneBrightness : IDisposable {
    private sealed class Entry {
        public Material Material;public Shader Original,Unlit;public int Queue;public bool Captured,Instancing,InstanceKeyword;
        public void Apply(){
            if(Material==null)return;
            Original=Material.shader;Queue=Material.renderQueue;Instancing=Material.enableInstancing;InstanceKeyword=Material.IsKeywordEnabled("INSTANCING_ON");Captured=true;
            Material.shader=Unlit;Material.renderQueue=Queue;
            // The supplied unlit shaders have no INSTANCING_ON variant.
            Material.enableInstancing=false;Material.DisableKeyword("INSTANCING_ON");
        }
        public void Restore(){
            if(!Captured||Material==null)return;Captured=false;
            // A newer shader writer owns its changes. Preserve live color,
            // texture, UV and property-block animation throughout the scope.
            if(Material.shader==Unlit){Material.shader=Original;Material.renderQueue=Queue;Material.enableInstancing=Instancing;if(InstanceKeyword)Material.EnableKeyword("INSTANCING_ON");}
        }
    }
    private readonly Dictionary<int,Renderer> renderers=new Dictionary<int,Renderer>();
    private readonly Dictionary<int,Entry> materials=new Dictionary<int,Entry>();
    private readonly Dictionary<string,Shader> shaders=new Dictionary<string,Shader>(StringComparer.Ordinal);
    private readonly List<int> removed=new List<int>();
    private bool active,disposed,fog;private int environmentScene;
    public bool Active {get{return active;}}
    public int Count {get{return renderers.Count;}}
    private Shader Find(string name){Shader shader;if(!shaders.TryGetValue(name,out shader)){shader=Shader.Find(name);if(shader!=null&&!shader.isSupported)shader=null;shaders.Add(name,shader);}return shader;}
    private Shader Unlit(Material material){
        var original=material.shader;if(original==null)return null;
        string name=original.name;
        if(name.IndexOf("Unlit",StringComparison.OrdinalIgnoreCase)>=0||name.StartsWith("Hidden/",StringComparison.Ordinal)||name.StartsWith("Sprites/",StringComparison.Ordinal))return null;
        bool cutout=material.IsKeywordEnabled("_ALPHATEST_ON")||material.HasProperty("_Mode")&&material.GetFloat("_Mode")==1;
        bool transparent=material.renderQueue>=3000||material.GetTag("RenderType",false,"")=="Transparent"||material.HasProperty("_Mode")&&material.GetFloat("_Mode")>=2;
        if(cutout||transparent){
            // Never make foliage, monster effects or hidden ceilings opaque
            // if that variant was stripped from this game's shader build.
            return Find(cutout?"Unlit/Transparent Cutout":"Unlit/Transparent");
        }
        // Preserve the world's original triplanar projection. The matching
        // TOZ unlit shader is present in the supplied game's shader assets.
        if(name.StartsWith("TOZ/Object/TriProj/",StringComparison.Ordinal)){
            int end=name.LastIndexOf('/');var projected=Find(name.Substring(0,end)+"/Unlit");if(projected!=null)return projected;
        }
        return Find(material.HasProperty("_MainTex")&&material.GetTexture("_MainTex")!=null?"Unlit/Texture":"Unlit/Color");
    }
    private void Track(Material material){
        if(material==null)return;int id=material.GetInstanceID();if(materials.ContainsKey(id))return;
        var shader=Unlit(material);if(shader==null)return;
        var entry=new Entry{Material=material,Unlit=shader};materials.Add(id,entry);
        if(active)try{entry.Apply();}catch{try{entry.Restore();}finally{materials.Remove(id);}throw;}
    }
    public void AddMaterials(Material[] value){if(disposed||value==null)return;foreach(var material in value)Track(material);}
    public void Prune(){
        removed.Clear();foreach(var pair in renderers)if(pair.Value==null)removed.Add(pair.Key);foreach(int id in removed)renderers.Remove(id);
        removed.Clear();foreach(var pair in materials)if(pair.Value.Material==null)removed.Add(pair.Key);foreach(int id in removed)materials.Remove(id);
    }
    public void Add(Renderer renderer){
        if(disposed||renderer==null||!(renderer is MeshRenderer)&&!(renderer is SkinnedMeshRenderer))return;
        renderers[renderer.GetInstanceID()]=renderer;AddMaterials(renderer.sharedMaterials);
    }
    public void SetActive(bool enabled){
        if(disposed||enabled==active)return;if(!enabled){Restore();return;}
        // Refresh only on activation; stable playback does no renderer scans,
        // material allocation, shader searches or native property writes.
        foreach(var renderer in renderers.Values)if(renderer!=null)AddMaterials(renderer.sharedMaterials);
        environmentScene=SceneManager.GetActiveScene().handle;fog=RenderSettings.fog;active=true;
        try{RenderSettings.fog=false;foreach(var entry in materials.Values)entry.Apply();}catch{Restore();throw;}
    }
    private void Restore(){
        if(!active)return;active=false;Exception first=null;
        foreach(var entry in materials.Values)try{entry.Restore();}catch(Exception e){if(first==null)first=e;}
        try{if(SceneManager.GetActiveScene().handle==environmentScene&&!RenderSettings.fog)RenderSettings.fog=fog;}
        finally{if(first!=null)throw first;}
    }
    public void Dispose(){if(disposed)return;try{Restore();}finally{disposed=true;renderers.Clear();materials.Clear();shaders.Clear();removed.Clear();}}
}
