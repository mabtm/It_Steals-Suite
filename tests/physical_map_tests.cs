using System;
using System.IO;
using UnityEngine;
public static class PhysicalMapTests {
    static void Check(bool yes,string name){if(!yes)throw new Exception(name);}
    static double Area(Vector3[] p){double sum=0;for(int i=0;i<p.Length;i++)sum+=p[i].x*(double)p[(i+1)%p.Length].z-p[i].z*(double)p[(i+1)%p.Length].x;return sum*.5;}
    public static int Main(string[] args){
        // Native asset values: Point sphere radius 2 * world scale .5603603.
        float sphere=2*.5603603f;
        Check(Math.Abs(MapPhysicalData.Reach(sphere,1.65f,.6144456f,2,.4f)-(sphere+.4f))<1e-5,"classic trigger/capsule overlap");
        Check(Math.Abs(MapPhysicalData.Reach(sphere,1.65f,0,.8f,.4f)-Math.Sqrt((sphere+.4)*(sphere+.4)-1.25*1.25))<1e-5,"short Hide & Seek capsule uses vertical separation");
        Check(MapPhysicalData.Reach(sphere,8,0,.8f,.4f)==0,"a goal on another floor has no imaginary pickup area");
        Check(Math.Abs(MapPhysicalData.BoxReach(sphere,1.65f,new Vector3(0,1.1f,0),new Vector3(1.123f,.2053f,.995f),.14f)-(.995+Math.Sqrt(sphere*sphere-.3447*.3447)-.14))<1e-5,"actual camera box yields a conservative yaw-independent cylinder, not the controller radius");
        var identity=Matrix4x4.identity;
        var box=MapPhysicalData.Box(identity,Vector3.zero,new Vector3(4,4,2),-.5f,.5f);Check(box.Length==4&&Math.Abs(Area(box)-8)<1e-5,"exact standing-band box footprint");
        Check(MapPhysicalData.Box(identity,Vector3.zero,new Vector3(4,4,2),3,4).Length==0,"ceiling/floor outside the collision band omitted");
        var tilted=identity;tilted.m10=1;
        var clipped=MapPhysicalData.Box(tilted,Vector3.zero,new Vector3(4,2,2),-.1f,.1f);
        Check(clipped.Length==4&&Math.Abs(Area(clipped)-4.4)<1e-5,"tilted collider clipped to actual height instead of its full AABB");
        var triangle=MapPhysicalData.Triangle(new Vector3(0,-1,0),new Vector3(4,1,0),new Vector3(0,1,4),0,1);Check(triangle.Length==4&&Area(triangle)>0,"mesh triangle clipping preserves the convex part");
        var data=new PracticeMapData{Scene=5,Pid=1234,Epoch=1,Spawn=0,LiveSpawn=0,Keep=1,Stage=1,Flags=1,Spawns=new[]{new Vector3(-3,1,0)},Points=new[]{new Vector3(3,1,0)},Selected=new[]{0},LiveSelected=new[]{0},AgentRadius=.4f,AgentHeight=.8f,AgentSlope=45,Review=new RunReviewData{Id=1,Outcome=2,Seconds=.1},PhysicalCaptured=true,PhysicalParts=new[]{new MapFootprint{Flags=3,Points=clipped}},PickupRegions=new[]{new PickupRegion{Verified=true,Center=new Vector3(3,1,0),Radius=1.2f}}};
        byte[] encoded=data.Encode();var copy=PracticeMapArchive.Decode(encoded);Check(copy.PhysicalCaptured&&copy.PhysicalParts.Length==1&&copy.PhysicalParts[0].Points.Length==4&&copy.PickupRegions[0].Verified&&copy.PickupRegions[0].Radius==1.2f,"physical v13 archive roundtrip");
        Directory.CreateDirectory(args[0]);File.WriteAllBytes(Path.Combine(args[0],"physical-map-v13.bin"),encoded);
        var legacy=PracticeMapArchive.Decode(File.ReadAllBytes(args[1]));Check(!legacy.PhysicalCaptured&&legacy.PickupRegions.Length==0,"v12 archive retains center fallback");
        using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream)){
            MapPhysicalData.Write(writer,true,0,new[]{new MapFootprint{Flags=3,Points=clipped}},data.PickupRegions);
            byte[] bad=stream.ToArray();bad[0]=2;bool rejected=false;try{MapPhysicalData.Read(new BinaryReader(new MemoryStream(bad)),data);}catch(InvalidDataException){rejected=true;}Check(rejected,"invalid physical capture flag rejected");
        }
        Console.WriteLine("Collider-band projection, real sphere/capsule reach, v13 cross-language fixture, v12 fallback and malformed flag: PASS");return 0;
    }
}
