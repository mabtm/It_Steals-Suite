using System.Collections.Generic;

public sealed class GeometryInspection
{
    public int Collider,Frame;public double Seconds;public float Low,High,Floor,Height,Radius;
    public bool Grounded,InactiveAccepted;public string Reason="Not included";
    public readonly List<MapFootprint> Parts=new List<MapFootprint>();
}
