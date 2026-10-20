using System;
using UnityEngine;

// Pointer coordinates are normalized in the GAME CLIENT, bottom-left origin.
// Camera.rect is applied once; Screen.width / DPI never enter the ray mapping.
public static class InspectorPicking
{
    public static bool Viewport(float x, float y, Rect viewport, out Vector3 point)
    {
        point = Vector3.zero;
        if (!Finite(x) || !Finite(y) || !Finite(viewport.x) || !Finite(viewport.y) ||
            !Finite(viewport.width) || !Finite(viewport.height) || viewport.width <= 0 ||
            viewport.height <= 0 || x < 0 || x > 1 || y < 0 || y > 1)
            return false;
        float u = (x - viewport.x) / viewport.width, v = (y - viewport.y) / viewport.height;
        if (u < 0 || u > 1 || v < 0 || v > 1)
            return false;
        point = new Vector3(u, v, 0);
        return true;
    }

    public static bool Matches(uint expectedSerial, int expectedObject, uint observedSerial,
                               int observedObject)
    {
        return expectedSerial != 0 && expectedSerial == observedSerial && expectedObject != 0 &&
               expectedObject == observedObject;
    }

    public static bool Finite(Vector3 p)
    {
        return Finite(p.x) && Finite(p.y) && Finite(p.z);
    }

    public static bool RayBounds(Vector3 origin, Vector3 direction, Vector3 low, Vector3 high,
                                 out float distance)
    {
        distance = 0;
        if (!Finite(origin) || !Finite(direction) || !Finite(low) || !Finite(high))
            return false;
        float far = 10000;
        for (int i = 0; i < 3; i++)
        {
            float o = origin[i], d = direction[i], a = low[i], b = high[i];
            if (a > b)
                return false;
            if (Math.Abs(d) < 1e-12f)
            {
                if (o < a || o > b)
                    return false;
                continue;
            }
            float first = (a - o) / d, last = (b - o) / d;
            if (first > last)
            {
                float swap = first;
                first = last;
                last = swap;
            }
            distance = Math.Max(distance, first);
            far = Math.Min(far, last);
            if (distance > far)
                return false;
        }
        return far >= 0 && distance <= 10000;
    }

    public static bool RayTriangles(Vector3 origin, Vector3 direction, Vector3[] vertices, int[] indices,
                                    out float distance)
    {
        distance = float.PositiveInfinity;
        if (!Finite(origin) || !Finite(direction) || vertices == null || indices == null ||
            indices.Length % 3 != 0)
            return false;
        for (int i = 0; i < indices.Length; i += 3)
        {
            int ia = indices[i], ib = indices[i + 1], ic = indices[i + 2];
            if (ia < 0 || ib < 0 || ic < 0 || ia >= vertices.Length || ib >= vertices.Length ||
                ic >= vertices.Length)
                return false;
            var a = vertices[ia];
            var edge1 = vertices[ib] - a;
            var edge2 = vertices[ic] - a;
            var p = Vector3.Cross(direction, edge2);
            double determinant = Vector3.Dot(edge1, p);
            if (Math.Abs(determinant) < 1e-12)
                continue;
            double inv = 1 / determinant;
            var offset = origin - a;
            double u = Vector3.Dot(offset, p) * inv;
            if (u < 0 || u > 1)
                continue;
            var q = Vector3.Cross(offset, edge1);
            double v = Vector3.Dot(direction, q) * inv;
            if (v < 0 || u + v > 1)
                continue;
            double d = Vector3.Dot(edge2, q) * inv;
            if (d >= 0 && d <= 10000 && d < distance)
                distance = (float)d;
        }
        return Finite(distance);
    }

    public static bool Finite(float value)
    {
        return !float.IsNaN(value) && !float.IsInfinity(value);
    }
}

public struct InspectorPointer
{
    public uint Serial, Epoch;
    public bool Active;
    public float X, Y;
    public int Width, Height, Action, Preferred;
    public bool Valid
    {
        get {
            return (Action == 3 || Action == 7 || Action == 8) && Serial != 0 && InspectorPicking.Finite(X) &&
                   InspectorPicking.Finite(Y) && X >= 0 && X <= 1 && Y >= 0 && Y <= 1 && Width > 0 &&
                   Width <= 32768 && Height > 0 && Height <= 32768;
        }
    }
}

public class InspectorPickTarget
{
    public int Id, Kind;
    public float Distance;
    public string Name = "", Type = "";
}

public sealed class InspectorHover
{
    public uint Serial;
    public bool Active, Outline;
    public int Object, Kind, CameraId;
    public float X, Y, Distance, Left, Top, Right, Bottom;
    public string Name = "", Type = "", Status = "", Camera = "";
    public int Width, Height, Action, Preferred;
    public Rect Viewport;
    public bool Partial, Scanning;
    public readonly System.Collections.Generic.List<InspectorPickTarget> Targets =
        new System.Collections.Generic.List<InspectorPickTarget>();

    public static void Write(System.IO.BinaryWriter w, InspectorHover h)
    {
        w.Write(h.Serial);
        w.Write(h.Active);
        w.Write(h.Object);
        w.Write(h.Kind);
        w.Write(h.X);
        w.Write(h.Y);
        w.Write(h.Distance);
        w.Write(h.Outline);
        w.Write(h.Left);
        w.Write(h.Top);
        w.Write(h.Right);
        w.Write(h.Bottom);
        InspectorWire.Text(w, h.Name, 768);
        InspectorWire.Text(w, h.Type, 256);
        InspectorWire.Text(w, h.Status, 1024);
        w.Write(h.CameraId);
        InspectorWire.Text(w, h.Camera, 512);
        w.Write(h.Width);
        w.Write(h.Height);
        w.Write(h.Viewport.x);
        w.Write(h.Viewport.y);
        w.Write(h.Viewport.width);
        w.Write(h.Viewport.height);
        w.Write(h.Partial);
        w.Write(h.Scanning);
        w.Write(h.Targets.Count);
        foreach (var t in h.Targets)
        {
            w.Write(t.Id);
            w.Write(t.Kind);
            w.Write(t.Distance);
            InspectorWire.Text(w, t.Name, 256);
            InspectorWire.Text(w, t.Type, 128);
        }
    }
}
