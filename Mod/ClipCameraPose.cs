using System;
using System.IO.MemoryMappedFiles;
using UnityEngine;

public struct ClipCameraPose
{
    public Vector3 Position;
    public Quaternion Rotation;
    public float Fov;

    private static bool BadPosition(float v)
    {
        return float.IsNaN(v) || float.IsInfinity(v) || Math.Abs(v) > 1000000;
    }

    public void Validate()
    {
        if (BadPosition(Position.x) || BadPosition(Position.y) || BadPosition(Position.z))
            throw new InvalidOperationException("Invalid clip camera position");
        float norm = Rotation.x * Rotation.x + Rotation.y * Rotation.y + Rotation.z * Rotation.z +
                     Rotation.w * Rotation.w;
        if (float.IsNaN(norm) || float.IsInfinity(norm) || norm < .98f || norm > 1.02f || float.IsNaN(Fov) ||
            float.IsInfinity(Fov) || Fov < 10 || Fov > 150)
            throw new InvalidOperationException("Invalid clip camera rotation / field of view");
    }

    public static ClipCameraPose Read(MemoryMappedViewAccessor v, int offset)
    {
        return new ClipCameraPose {
            Position = new Vector3(v.ReadSingle(offset), v.ReadSingle(offset + 4), v.ReadSingle(offset + 8)),
            Rotation = new Quaternion(v.ReadSingle(offset + 12), v.ReadSingle(offset + 16),
                                      v.ReadSingle(offset + 20), v.ReadSingle(offset + 24)),
            Fov = v.ReadSingle(offset + 28)
        };
    }

    public void Write(MemoryMappedViewAccessor v, int offset)
    {
        v.Write(offset, Position.x);
        v.Write(offset + 4, Position.y);
        v.Write(offset + 8, Position.z);
        v.Write(offset + 12, Rotation.x);
        v.Write(offset + 16, Rotation.y);
        v.Write(offset + 20, Rotation.z);
        v.Write(offset + 24, Rotation.w);
        v.Write(offset + 28, Fov);
    }
}
