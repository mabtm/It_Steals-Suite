using System;
using System.Collections.Generic;
using System.IO;
using UnityEngine;

public sealed class MapFootprint
{
    // 1 wall appearance, 2 collides with the actual player, 4 tessellated curve, 8 captured floor triangle.
    public uint Flags;
    public Vector3[] Points = new Vector3[0];
}

public struct PickupRegion
{
    public bool Verified;
    public Vector3 Center;
    public float Radius;
}

// Projection of the actual solid intersected with the standing player's height
// band. Convex parts stay separate: a doorway is never replaced by a mesh AABB.
public static class MapPhysicalData
{
    private static readonly int[] Edges = { 0, 1, 0, 2, 0, 4, 1, 3, 1, 5, 2, 3,
                                            2, 6, 3, 7, 4, 5, 4, 6, 5, 7, 6, 7 };

    private static double Cross(Vector3 a, Vector3 b, Vector3 c)
    {
        return (b.x - a.x) * (double)(c.z - a.z) - (b.z - a.z) * (double)(c.x - a.x);
    }

    public static Vector3[] Hull(List<Vector3> source)
    {
        source.Sort((a, b) => a.x == b.x ? a.z.CompareTo(b.z) : a.x.CompareTo(b.x));
        var points = new List<Vector3>();
        foreach (var p in source)
            if (points.Count == 0 || Math.Abs(p.x - points[points.Count - 1].x) > 1e-5 ||
                Math.Abs(p.z - points[points.Count - 1].z) > 1e-5)
                points.Add(new Vector3(p.x, 0, p.z));
        if (points.Count <= 2)
            return points.ToArray();
        var hull = new List<Vector3>();
        foreach (var p in points)
        {
            while (hull.Count >= 2 && Cross(hull[hull.Count - 2], hull[hull.Count - 1], p) <= 1e-8)
                hull.RemoveAt(hull.Count - 1);
            hull.Add(p);
        }
        int lower = hull.Count;
        for (int i = points.Count - 2; i >= 0; i--)
        {
            var p = points[i];
            while (hull.Count > lower && Cross(hull[hull.Count - 2], hull[hull.Count - 1], p) <= 1e-8)
                hull.RemoveAt(hull.Count - 1);
            hull.Add(p);
        }
        hull.RemoveAt(hull.Count - 1);
        return hull.ToArray();
    }

    private static void SliceEdge(List<Vector3> output, Vector3 a, Vector3 b, float y)
    {
        if ((a.y < y && b.y < y) || (a.y > y && b.y > y) || Math.Abs(b.y - a.y) < 1e-9)
            return;
        double u = (y - a.y) / (double)(b.y - a.y);
        if (u >= 0 && u <= 1)
            output.Add(new Vector3((float)(a.x + (b.x - a.x) * u), y, (float)(a.z + (b.z - a.z) * u)));
    }

    public static Vector3[] Box(Matrix4x4 matrix, Vector3 center, Vector3 size, float low, float high)
    {
        var p = new Vector3[8];
        var list = new List<Vector3>(32);
        var half = size * .5f;
        for (int i = 0; i < 8; i++)
        {
            p[i] = matrix.MultiplyPoint3x4(center + new Vector3((i & 1) == 0 ? -half.x : half.x,
                                                                (i & 2) == 0 ? -half.y : half.y,
                                                                (i & 4) == 0 ? -half.z : half.z));
            if (p[i].y >= low && p[i].y <= high)
                list.Add(p[i]);
        }
        for (int i = 0; i < Edges.Length; i += 2)
        {
            SliceEdge(list, p[Edges[i]], p[Edges[i + 1]], low);
            SliceEdge(list, p[Edges[i]], p[Edges[i + 1]], high);
        }
        return Hull(list);
    }

    public static Vector3[] Triangle(Vector3 a, Vector3 b, Vector3 c, float low, float high)
    {
        var list = new List<Vector3>(9);
        foreach (var p in new[] { a, b, c })
            if (p.y >= low && p.y <= high)
                list.Add(p);
        foreach (var edge in new[] { new[] { a, b }, new[] { b, c }, new[] { c, a } })
        {
            SliceEdge(list, edge[0], edge[1], low);
            SliceEdge(list, edge[0], edge[1], high);
        }
        return Hull(list);
    }

    public static Vector3[] Capsule(ReplayColliderShape shape, float low, float high)
    {
        var axis = shape.Direction == 0 ? Vector3.right : shape.Direction == 2 ? Vector3.forward : Vector3.up;
        axis = shape.Matrix.MultiplyVector(axis).normalized;
        var half = axis * Math.Max(0, shape.Height * .5f - shape.Radius);
        var a = shape.Center - half;
        var b = shape.Center + half;
        if (Math.Max(a.y, b.y) + shape.Radius <= low || Math.Min(a.y, b.y) - shape.Radius >= high)
            return new Vector3[0];
        float radius = shape.Radius;
        if (Math.Abs(axis.y) > .9999f)
        {
            float dy = Math.Max(0, Math.Max(low - Math.Max(a.y, b.y), Math.Min(a.y, b.y) - high));
            radius = (float)Math.Sqrt(Math.Max(0, radius * radius - dy * dy));
        }
        var points = new List<Vector3>();
        foreach (var center in new[] { a, b })
            for (int k = 0; k < 24; k++)
            {
                float angle = k * Mathf.PI / 12;
                points.Add(center + new Vector3(Mathf.Cos(angle), 0, Mathf.Sin(angle)) *
                                        (radius / Mathf.Cos(Mathf.PI / 24)));
            }
        return Hull(points);
    }

    private static void Validate(MapFootprint p, int format = 14)
    {
        if (p.Points.Length < 2 || p.Points.Length > 64 || (p.Flags & ~(format >= 14 ? 15u : 7u)) != 0)
            throw new InvalidDataException("Invalid physical footprint");
        if ((p.Flags & 8) != 0 && (p.Flags != 8 || p.Points.Length != 3))
            throw new InvalidDataException("Invalid physical floor triangle");
        foreach (var v in p.Points)
            foreach (float n in new[] { v.x, v.y, v.z })
                if (float.IsNaN(n) || float.IsInfinity(n) || Math.Abs(n) > 100000)
                    throw new InvalidDataException("Invalid physical coordinate");
        if (p.Points.Length == 2)
        {
            if (Math.Abs(p.Points[0].x - p.Points[1].x) + Math.Abs(p.Points[0].z - p.Points[1].z) < 1e-6)
                throw new InvalidDataException("Degenerate physical segment");
            return;
        }
        double sign = 0, area = 0;
        for (int i = 0; i < p.Points.Length; i++)
        {
            var a = p.Points[i];
            var b = p.Points[(i + 1) % p.Points.Length];
            var c = p.Points[(i + 2) % p.Points.Length];
            double cross = Cross(a, b, c);
            if (Math.Abs(cross) > 1e-8)
            {
                if (sign * cross < 0)
                    throw new InvalidDataException("Non-convex physical footprint");
                sign = cross;
            }
            area += a.x * (double)b.z - a.z * (double)b.x;
        }
        if (Math.Abs(area) < 1e-8)
            throw new InvalidDataException("Degenerate physical footprint");
        if (area < 0)
            throw new InvalidDataException("Reversed physical footprint");
    }

    public static float Reach(float sphereRadius, float sphereY, float floor, float height,
                              float playerRadius)
    {
        float lo = floor + playerRadius, hi = floor + Math.Max(height - playerRadius, playerRadius);
        double dy = sphereY<lo ? lo - sphereY : sphereY> hi ? sphereY - hi : 0,
               r = sphereRadius + playerRadius;
        return (float)Math.Sqrt(Math.Max(0, r * r - dy * dy));
    }

    public static float BoxReach(float sphereRadius, float sphereY, Vector3 center, Vector3 half,
                                 float horizontalOffset)
    {
        double dy = Math.Max(0, Math.Abs(sphereY - center.y) - half.y);
        if (dy >= sphereRadius)
            return 0;
        return Math.Max(0, Math.Min(half.x, half.z) +
                               (float)Math.Sqrt(sphereRadius * sphereRadius - dy * dy) - horizontalOffset);
    }

    public static float CollectorReach(MouseLook player, Collider collider, PickupRegion sphere)
    {
        if (player == null || DataExporter.Crouching(player) || collider == null || !collider.enabled ||
            !collider.gameObject.activeInHierarchy)
            return 0;
        var t = collider.transform;
        var parent = t.parent;
        Quaternion rotation = parent == null ? Quaternion.identity : parent.rotation;
        // MouseLook.Update writes localRotation = Euler(xRotation,0,0).
        // Reconstruct a level view without moving/changing any game object.
        if (Math.Abs((rotation * Vector3.up).y) < .999f)
            return 0;
        var matrix = Matrix4x4.TRS(t.position, rotation, t.lossyScale);
        var body = PlayerContext.Body(player);
        if (body == null)
            return 0;
        var box = collider as BoxCollider;
        var capsule = collider as CapsuleCollider;
        var ball = collider as SphereCollider;
        Vector3 center, scale = t.lossyScale;
        float reach;
        scale = new Vector3(Math.Abs(scale.x), Math.Abs(scale.y), Math.Abs(scale.z));
        if (box != null)
        {
            center = matrix.MultiplyPoint3x4(box.center);
            var half = Vector3.Scale(box.size, scale) * .5f;
            reach = BoxReach(sphere.Radius, sphere.Center.y, center, half, 0);
        }
        else if (capsule != null && capsule.direction == 1)
        {
            center = matrix.MultiplyPoint3x4(capsule.center);
            float r = capsule.radius * Math.Max(scale.x, scale.z),
                  height = Math.Max(capsule.height * scale.y, r * 2);
            reach = Reach(sphere.Radius, sphere.Center.y, center.y - height * .5f, height, r);
        }
        else if (ball != null)
        {
            center = matrix.MultiplyPoint3x4(ball.center);
            float r = ball.radius * Math.Max(scale.x, Math.Max(scale.y, scale.z));
            reach = Reach(sphere.Radius, sphere.Center.y, center.y - r, r * 2, r);
        }
        else
            return 0;
        var offset = center - body.position;
        return Math.Max(0, reach - (float)Math.Sqrt(offset.x * offset.x + offset.z * offset.z));
    }

    public static void Write(BinaryWriter w, bool captured, int omitted, MapFootprint[] footprints,
                             PickupRegion[] pickups)
    {
        if (omitted < 0 || footprints.Length > 20000 || !captured && footprints.Length > 0)
            throw new InvalidDataException("Invalid physical capture");
        w.Write(captured);
        w.Write(omitted);
        w.Write(footprints.Length);
        int points = 0;
        foreach (var p in footprints)
        {
            Validate(p);
            points += p.Points.Length;
            if (points > 60000)
                throw new InvalidDataException("Physical footprint limit exceeded");
            w.Write(p.Flags);
            w.Write(p.Points.Length);
            foreach (var v in p.Points)
            {
                w.Write(v.x);
                w.Write(v.y);
                w.Write(v.z);
            }
        }
        w.Write(pickups.Length);
        foreach (var p in pickups)
        {
            w.Write(p.Verified);
            w.Write(p.Center.x);
            w.Write(p.Center.y);
            w.Write(p.Center.z);
            w.Write(p.Radius);
        }
    }

    public static void Read(BinaryReader r, PracticeMapData d, int format = 14)
    {
        byte yes = r.ReadByte();
        if (yes > 1)
            throw new InvalidDataException("Invalid physical capture flag");
        d.PhysicalCaptured = yes != 0;
        d.PhysicalOmitted = r.ReadInt32();
        int n = r.ReadInt32();
        if (d.PhysicalOmitted < 0 || n < 0 || n > 20000)
            throw new InvalidDataException("Invalid physical capture count");
        var parts = new MapFootprint[n];
        int total = 0;
        for (int i = 0; i < n; i++)
        {
            var p = new MapFootprint { Flags = r.ReadUInt32() };
            int count = r.ReadInt32();
            total += count;
            if (count < 2 || count > 64 || total > 60000 || (p.Flags & ~(format >= 14 ? 15u : 7u)) != 0)
                throw new InvalidDataException("Invalid physical footprint");
            p.Points = new Vector3[count];
            for (int j = 0; j < count; j++)
                p.Points[j] = new Vector3(Number(r), Number(r), Number(r));
            Validate(p, format);
            parts[i] = p;
        }
        n = r.ReadInt32();
        if (n != 0 && n != d.Points.Length)
            throw new InvalidDataException("Pickup regions do not match the catalog");
        var pickups = new PickupRegion[n];
        for (int i = 0; i < n; i++)
        {
            byte verified = r.ReadByte();
            var p = new PickupRegion { Verified = verified == 1,
                                       Center = new Vector3(Number(r), Number(r), Number(r)),
                                       Radius = Number(r) };
            if (verified > 1 || p.Radius < 0 || p.Radius > 10 || p.Verified && p.Radius <= 0)
                throw new InvalidDataException("Invalid pickup region");
            pickups[i] = p;
        }
        if (!d.PhysicalCaptured && parts.Length != 0)
            throw new InvalidDataException("Unexpected physical geometry");
        d.PhysicalParts = parts;
        d.PickupRegions = pickups;
    }

    private static float Number(BinaryReader r)
    {
        float v = r.ReadSingle();
        if (float.IsNaN(v) || float.IsInfinity(v) || Math.Abs(v) > 100000)
            throw new InvalidDataException("Invalid physical coordinate");
        return v;
    }
}

// Exact 2D segment-to-rectangle distance, including rounded player clearance.
// Unlike expanding an AABB, this does not close a valid diagonal corner gap.
public static class PlayerWallLeg
{
    private static bool Clip(double a, double b, double half, ref double lo, ref double hi)
    {
        double delta = b - a;
        if (Math.Abs(delta) < 1e-12)
            return a >= -half && a <= half;
        double p = (-half - a) / delta, q = (half - a) / delta;
        lo = Math.Max(lo, Math.Min(p, q));
        hi = Math.Min(hi, Math.Max(p, q));
        return lo <= hi;
    }

    private static double PointSegment(double x, double z, double ax, double az, double bx, double bz)
    {
        double dx = bx - ax, dz = bz - az, length = dx * dx + dz * dz;
        double t = length > 0 ? Math.Max(0, Math.Min(1, ((x - ax) * dx + (z - az) * dz) / length)) : 0;
        dx = x - ax - t * dx;
        dz = z - az - t * dz;
        return dx * dx + dz * dz;
    }

    public static bool Blocks(double ax, double az, double bx, double bz, double halfX, double halfZ,
                              double radius)
    {
        double lo = 0, hi = 1;
        if (Clip(ax, bx, halfX, ref lo, ref hi) && Clip(az, bz, halfZ, ref lo, ref hi))
            return true;
        double dx = Math.Max(0, Math.Abs(ax) - halfX), dz = Math.Max(0, Math.Abs(az) - halfZ),
               best = dx * dx + dz * dz;
        dx = Math.Max(0, Math.Abs(bx) - halfX);
        dz = Math.Max(0, Math.Abs(bz) - halfZ);
        best = Math.Min(best, dx * dx + dz * dz);
        for (int x = -1; x <= 1; x += 2)
            for (int z = -1; z <= 1; z += 2)
                best = Math.Min(best, PointSegment(x * halfX, z * halfZ, ax, az, bx, bz));
        return best < radius * radius;
    }
}
