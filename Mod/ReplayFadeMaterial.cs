using UnityEngine;

// The supplied game's Standard shader has alpha-blend variants without
// INSTANCING_ON. Fade materials must not request the missing combination.
public static class ReplayFadeMaterial
{
    private static void Int(Material m,string name,int value){if(m.HasProperty(name))m.SetInt(name,value);}
    public static Material Create(Shader shader,Color color)
    {
        var m=new Material(shader){color=color,enableInstancing=false,hideFlags=HideFlags.HideAndDontSave,renderQueue=3000};
        m.SetOverrideTag("RenderType","Transparent");Int(m,"_Mode",2);Int(m,"_SrcBlend",5);Int(m,"_DstBlend",10);Int(m,"_ZWrite",0);Int(m,"_ZTest",4);Int(m,"_Cull",0);
        m.DisableKeyword("_ALPHATEST_ON");m.DisableKeyword("_ALPHAPREMULTIPLY_ON");m.DisableKeyword("INSTANCING_ON");if(shader.name=="Standard")m.EnableKeyword("_ALPHABLEND_ON");
        Int(m,"_SpecularHighlights",0);Int(m,"_GlossyReflections",0);Int(m,"_Glossiness",0);Int(m,"_Metallic",0);return m;
    }
    public static Material Instance(Material source){return new Material(source){enableInstancing=false,hideFlags=HideFlags.HideAndDontSave};}
    public static Material Native(Material source){var m=Instance(source);m.renderQueue=3000;m.SetOverrideTag("RenderType","Transparent");Int(m,"_Mode",2);Int(m,"_SrcBlend",5);Int(m,"_DstBlend",10);Int(m,"_ZWrite",0);m.DisableKeyword("_ALPHATEST_ON");m.DisableKeyword("_ALPHAPREMULTIPLY_ON");if(m.shader.name=="Standard")m.EnableKeyword("_ALPHABLEND_ON");return m;}
    public static void Alpha(Material material,Color color,float alpha){color.a=alpha;material.color=color;}
}
