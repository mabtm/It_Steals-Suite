using System;
using UnityEngine;

// A session-local orbit, independent of the actor's subsequent recorded look.
// No collider, raycast, smoothing clock or gameplay transform is involved.
public sealed class ReplayFollowOrbit
{
    private bool ready;private float yaw,pitch=20,radius=4.3f;
    public float Radius {get{return radius;}}
    public ReplayFollowOrbit Copy(){return (ReplayFollowOrbit)MemberwiseClone();}
    public void Restore(ReplayFollowOrbit saved){ready=saved.ready;yaw=saved.yaw;pitch=saved.pitch;radius=saved.radius;}
    public void Reset(){ready=false;yaw=0;pitch=20;radius=4.3f;}
    public void Ensure(Quaternion recorded){if(!ready)Align(recorded);}
    public void Align(Quaternion recorded)
    {
        Vector3 heading=recorded*Vector3.forward;
        yaw=heading.x*heading.x+heading.z*heading.z>.0001f?(float)(Math.Atan2(heading.x,heading.z)*180/Math.PI):0;
        pitch=20;ready=true;
    }
    public void Rotate(float horizontal,float vertical)
    {
        if(float.IsNaN(horizontal)||float.IsInfinity(horizontal)||float.IsNaN(vertical)||float.IsInfinity(vertical))return;
        yaw=(yaw+horizontal*3)%360;pitch=Mathf.Clamp(pitch-vertical*3,-80,80);
    }
    public void Zoom(float wheel)
    {
        if(float.IsNaN(wheel)||float.IsInfinity(wheel))return;
        radius=Mathf.Clamp(radius*(float)Math.Exp(-Mathf.Clamp(wheel,-20,20)*.12f),.75f,20);
    }
    public void Pose(Vector3 body,Quaternion recorded,out Vector3 position,out Quaternion rotation)
    {PoseFocus(body+Vector3.up*1.2f,recorded,out position,out rotation);}
    public void PoseFocus(Vector3 focus,Quaternion recorded,out Vector3 position,out Quaternion rotation)
    {
        Ensure(recorded);
        rotation=Quaternion.Euler(pitch,yaw,0);
        position=focus-rotation*Vector3.forward*radius;
    }
}
