using UnityEngine;

// Shared actual-shape wire tessellation; no approximate bounds fallback.
public sealed class ColliderWireGeometry
{
    private readonly System.Action<Vector3, Vector3> sink;

    public ColliderWireGeometry(System.Action<Vector3, Vector3> output = null)
    {
        sink = output;
    }

    private static readonly int[] edges = { 0, 1, 1, 2, 2, 3, 3, 0, 4, 5, 5, 6,
                                            6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7 };
    private readonly Vector3[] points = new Vector3[8];
    private static readonly Vector2[] circle = Samples(24, Mathf.PI * 2), cap = Samples(6, Mathf.PI * .5f);

    private static Vector2[] Samples(int segments, float arc)
    {
        var values = new Vector2[segments + 1];
        for (int i = 0; i <= segments; i++)
        {
            float a = i * arc / segments;
            values[i] = new Vector2(Mathf.Cos(a), Mathf.Sin(a));
        }
        return values;
    }

    public void Draw(int kind, ReplayColliderShape shape)
    {
        if (kind == 1)
        {
            DrawBox(shape.Matrix, shape.Center, shape.Size);
            return;
        }
        if (kind == 2)
        {
            DrawSphere(shape.Center, shape.Radius);
            return;
        }
        if (kind == 3 || kind == 4)
        {
            var axis = shape.Direction == 0   ? Vector3.right
                       : shape.Direction == 2 ? Vector3.forward
                                              : Vector3.up;
            DrawCapsule(shape.Center, shape.Matrix.MultiplyVector(axis).normalized, shape.Height,
                        shape.Radius);
        }
    }

    private void Line(Vector3 a, Vector3 b)
    {
        if (sink != null)
            sink(a, b);
        else
        {
            GL.Vertex(a);
            GL.Vertex(b);
        }
    }

    private void DrawBox(Matrix4x4 matrix, Vector3 center, Vector3 size)
    {
        FillCorners(center, size);
        for (int i = 0; i < points.Length; i++)
            points[i] = matrix.MultiplyPoint3x4(points[i]);
        DrawEdges();
    }

    private void FillCorners(Vector3 center, Vector3 size)
    {
        Vector3 half = size * 0.5f;
        points[0] = center + new Vector3(-half.x, -half.y, -half.z);
        points[1] = center + new Vector3(half.x, -half.y, -half.z);
        points[2] = center + new Vector3(half.x, -half.y, half.z);
        points[3] = center + new Vector3(-half.x, -half.y, half.z);
        points[4] = center + new Vector3(-half.x, half.y, -half.z);
        points[5] = center + new Vector3(half.x, half.y, -half.z);
        points[6] = center + new Vector3(half.x, half.y, half.z);
        points[7] = center + new Vector3(-half.x, half.y, half.z);
    }

    private void DrawEdges()
    {
        for (int i = 0; i < edges.Length; i += 2)
            Line(points[edges[i]], points[edges[i + 1]]);
    }

    private void Ring(Vector3 center, Vector3 u, Vector3 v, float radius)
    {
        for (int i = 0; i < 24; i++)
        {
            Line(center + (u * circle[i].x + v * circle[i].y) * radius,
                 center + (u * circle[i + 1].x + v * circle[i + 1].y) * radius);
        }
    }

    private void DrawSphere(Vector3 center, float radius)
    {
        Ring(center, Vector3.right, Vector3.up, radius);
        Ring(center, Vector3.right, Vector3.forward, radius);
        Ring(center, Vector3.up, Vector3.forward, radius);
    }

    private void DrawCapsule(Vector3 center, Vector3 axis, float height, float radius)
    {
        Vector3 u =
            Vector3.Cross(axis, Mathf.Abs(Vector3.Dot(axis, Vector3.up)) < 0.9f ? Vector3.up : Vector3.right)
                .normalized;
        Vector3 v = Vector3.Cross(axis, u).normalized;
        float halfSegment = Mathf.Max(0f, height * 0.5f - radius);
        Vector3 top = center + axis * halfSegment, bottom = center - axis * halfSegment;
        Ring(top, u, v, radius);
        Ring(bottom, u, v, radius);
        for (int side = 0; side < 4; side++)
        {
            Vector3 radial = side == 0 ? u : side == 1 ? -u : side == 2 ? v : -v;
            Line(bottom + radial * radius, top + radial * radius);
            for (int i = 0; i < 6; i++)
            {
                Line(top + (radial * cap[i].x + axis * cap[i].y) * radius,
                     top + (radial * cap[i + 1].x + axis * cap[i + 1].y) * radius);
                Line(bottom + (radial * cap[i].x - axis * cap[i].y) * radius,
                     bottom + (radial * cap[i + 1].x - axis * cap[i + 1].y) * radius);
            }
        }
    }
}
