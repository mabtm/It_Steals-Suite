using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class Native
{
    [DllImport("capture_gpu_fixture")]
    public static extern IntPtr CaptureTexture();
    [DllImport("capture_gpu_fixture")]
    public static extern void CaptureReady(int value);
    [DllImport("capture_gpu_fixture")]
    public static extern void CaptureFault(int value);
    [DllImport("capture_gpu_fixture")]
    public static extern int CaptureCopies();
    [DllImport("capture_gpu_fixture")]
    public static extern int CaptureReferences();
    [DllImport("capture_gpu_fixture")]
    public static extern void CaptureInvoke(IntPtr callback, int id);
}

namespace UnityEngine
{
public static class SystemInfo
{
    public static UnityEngine.Rendering.GraphicsDeviceType graphicsDeviceType =
        UnityEngine.Rendering.GraphicsDeviceType.Direct3D11;
}

public enum TextureFormat
{
    BGRA32
}

namespace Rendering
{
public enum GraphicsDeviceType
{
    Direct3D11,
    OpenGLCore
}
}

public enum HideFlags
{
    HideAndDontSave
}

public enum FilterMode
{
    Point
}

public enum TextureWrapMode
{
    Clamp
}

public class Object
{
    public static int Destroyed;

    public static void Destroy(Object value)
    {
        Destroyed++;
    }
}

public class Texture2D : Object
{
    public HideFlags hideFlags;
    public FilterMode filterMode;
    public TextureWrapMode wrapMode;

    public Texture2D(int w, int h, TextureFormat format, bool mipmaps) {}

    public IntPtr GetNativeTexturePtr()
    {
        return Native.CaptureTexture();
    }
}

public static class GL
{
    public static bool Fail;
    public static readonly Queue<Tuple<IntPtr, int>> Events = new Queue<Tuple<IntPtr, int>>();

    public static void IssuePluginEvent(IntPtr pointer, int id)
    {
        if (Fail)
            throw new InvalidOperationException("Renderer stopped");
        Events.Enqueue(Tuple.Create(pointer, id));
    }

    public static void Flush()
    {
        while (Events.Count != 0)
        {
            var value = Events.Dequeue();
            Native.CaptureInvoke(value.Item1, value.Item2);
        }
    }
}
}

public static class CaptureGpuTests
{
    private static void Check(bool ok, string message)
    {
        if (!ok)
            throw new Exception(message);
    }

    public static int Main()
    {
        var gpu = new CaptureGpu(123, 1920, 1080);
        Check(Native.CaptureReferences() == 10, "consumer owns five COM references");
        gpu.Schedule(1);
        gpu.Schedule(2);
        Check(UnityEngine.GL.Events.Count == 1, "only one rendering callback may be queued");
        UnityEngine.GL.Flush();
        Check(gpu.Copied == 1 && Native.CaptureCopies() == 1 && !gpu.Failed,
              "actual COM slots and native rendering callback copy the requested frame");
        gpu.Schedule(2);
        UnityEngine.GL.Flush();
        Check(gpu.Copied == 1 && Native.CaptureCopies() == 1 && !gpu.Failed,
              "WAIT_TIMEOUT is not ownership and never copies or acknowledges");
        Native.CaptureFault(-1);
        gpu.Schedule(3);
        UnityEngine.GL.Flush();
        Check(gpu.Failed && gpu.Copied == 1, "device failure retains the last genuine acknowledgement");
        gpu.Dispose();
        gpu.Dispose();
        Check(Native.CaptureReferences() == 5, "repeat disposal releases each COM reference once");
        Native.CaptureFault(0);
        gpu = new CaptureGpu(123, 1280, 720);
        gpu.Schedule(4);
        gpu.Dispose();
        Check(Native.CaptureReferences() == 10, "a queued render retains the resources it owns");
        UnityEngine.GL.Flush();
        Check(Native.CaptureReferences() == 5 && Native.CaptureCopies() == 1,
              "shutdown before rendering cancels the copy and then frees resources");
        bool rejected = false;
        try
        {
            new CaptureGpu(456, 1920, 1080);
        }
        catch (InvalidOperationException)
        {
            rejected = true;
        }
        Check(rejected && Native.CaptureReferences() == 5 && UnityEngine.Object.Destroyed == 1,
              "failed shared-resource creation restores references and destroys its Unity texture");
        gpu = new CaptureGpu(123, 1920, 1080);
        UnityEngine.GL.Fail = true;
        gpu.Schedule(5);
        gpu.Dispose();
        Check(gpu.Failed && Native.CaptureReferences() == 5,
              "event submission failure cannot leak a resource");
        UnityEngine.SystemInfo.graphicsDeviceType = UnityEngine.Rendering.GraphicsDeviceType.OpenGLCore;
        bool unsupported = false;
        try
        {
            new CaptureGpu(123, 1920, 1080);
        }
        catch (NotSupportedException)
        {
            unsupported = true;
        }
        Check(unsupported && Native.CaptureReferences() == 5,
              "non-D3D11 pointers are rejected before accessing any COM interface");
        Console.WriteLine(
            "GPU consumer: real native COM vtables, render-thread handoff, bounded queue, busy/device failure, delayed shutdown and complete/idempotent rollback: PASS");
        return 0;
    }
}
