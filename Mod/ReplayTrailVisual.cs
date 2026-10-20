using System;
using UnityEngine;

public static class ReplayTrailVisual
{
    public static Color SpeedColor(float speed)
    {
        float u=Mathf.Clamp01(speed/10),t;Color a,b;
        if(u<.35f){a=new Color(.34f,.015f,.045f);b=new Color(.95f,.12f,.055f);t=u/.35f;}
        else if(u<.7f){a=new Color(.95f,.12f,.055f);b=new Color(1,.73f,.12f);t=(u-.35f)/.35f;}
        else{a=new Color(1,.73f,.12f);b=new Color(.06f,.96f,.87f);t=(u-.7f)/.3f;}
        return Color.Lerp(a,b,t*t*(3-2*t));
    }
    // Six pixels at 1080p, scaled with viewport height and bounded in world
    // space. Unlike GL.LINES this has a real, driver-independent width.
    public static float HalfWidth(float depth,float fov,int pixels)
    {
        float pixelWidth=6*Mathf.Clamp(pixels/1080f,.75f,2);
        return Mathf.Clamp(Math.Max(.03f,depth)*(float)Math.Tan(fov*Math.PI/360)*pixelWidth/Math.Max(1,pixels),.02f,.18f);
    }
    public static bool Side(Vector3 a,Vector3 b,Vector3 camera,Vector3 up,out Vector3 side)
    {
        Vector3 delta=b-a;side=Vector3.Cross(camera-(a+b)*.5f,delta);
        if(delta.sqrMagnitude<.00000001f)return false;
        if(side.sqrMagnitude<.00000001f)side=Vector3.Cross(up,delta);
        if(side.sqrMagnitude<.00000001f)return false;side.Normalize();return true;
    }
}
