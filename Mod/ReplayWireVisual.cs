using System;
using UnityEngine;

// Thicken the projected wire stroke, never the underlying collider dimensions.
public static class ReplayWireVisual
{
    public static bool Ribbon(ref Vector3 a,ref Vector3 b,Vector3 camera,Vector3 forward,Vector3 up,float near,float fov,int height,float pixels,out Vector3 sideA,out Vector3 sideB)
    {
        sideA=sideB=new Vector3();float da=Vector3.Dot(a-camera,forward),db=Vector3.Dot(b-camera,forward);near=Math.Max(.001f,near);
        if(da<near&&db<near)return false;
        if(da<near){a=a+(b-a)*((near-da)/(db-da));da=near;}
        else if(db<near){b=b+(a-b)*((near-db)/(da-db));db=near;}
        Vector3 side;if(!ReplayTrailVisual.Side(a,b,camera,up,out side))return false;
        float factor=(float)Math.Tan(fov*Math.PI/360)*pixels*Mathf.Clamp(height/1080f,.75f,2)/Math.Max(1,height);
        sideA=side*Mathf.Clamp(da*factor,.0001f,.12f);sideB=side*Mathf.Clamp(db*factor,.0001f,.12f);return true;
    }
}
