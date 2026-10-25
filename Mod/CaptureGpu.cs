using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine;

// Only COM graphics calls run in the rendering callback. Unity objects and
// shared-memory headers stay on the main thread.
public sealed class CaptureGpu : IDisposable
{
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void GetDevice(IntPtr self, out IntPtr device);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int OpenShared(IntPtr self, IntPtr handle, ref Guid iid, out IntPtr resource);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int Query(IntPtr self, ref Guid iid, out IntPtr resource);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int Acquire(IntPtr self, ulong key, uint milliseconds);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate int Release(IntPtr self, ulong key);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void Copy(IntPtr self, IntPtr destination, IntPtr source);
    [UnmanagedFunctionPointer(CallingConvention.StdCall)]
    private delegate void Flush(IntPtr self);
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void Event(int id);

    private static readonly object jobsLock = new object();
    private static readonly Dictionary<int, CaptureGpu> jobs = new Dictionary<int, CaptureGpu>();
    private static readonly Event callback = Render;
    private static readonly IntPtr callbackPointer = Marshal.GetFunctionPointerForDelegate(callback);
    private static int nextId;
    private readonly int id;
    private IntPtr device, context, shared, mutex, destination;
    private Acquire acquire;
    private Release release;
    private Copy copy;
    private Flush flush;
    private int pending, disposed, failed, copied, requested;
    public readonly Texture2D Image;
    public readonly long Handle;

    public bool Failed
    {
        get {
            return Volatile.Read(ref failed) != 0;
        }
    }
    public uint Copied
    {
        get {
            return unchecked((uint)Volatile.Read(ref copied));
        }
    }

    private static T Method<T>(IntPtr instance, int slot)

        where T : class
    {
        var pointer = Marshal.ReadIntPtr(Marshal.ReadIntPtr(instance), slot * IntPtr.Size);
        return Marshal.GetDelegateForFunctionPointer(pointer, typeof(T)) as T;
    }

    public CaptureGpu(long handle, int width, int height)
    {
        Handle = handle;
        Image = new Texture2D(width, height, TextureFormat.BGRA32,
                              false) { hideFlags = HideFlags.HideAndDontSave, filterMode = FilterMode.Point,
                                       wrapMode = TextureWrapMode.Clamp };
        try
        {
            destination = Image.GetNativeTexturePtr();
            if (destination == IntPtr.Zero)
                throw new InvalidOperationException("GPU texture unavailable");
            Marshal.AddRef(destination);
            Method<GetDevice>(destination, 3)(destination, out device);
            if (device == IntPtr.Zero)
                throw new InvalidOperationException("GPU device unavailable");
            var textureId = new Guid("6f15aaf2-d208-4e89-9ab4-489535d34f9c");
            if (Method<OpenShared>(device, 28)(device, new IntPtr(handle), ref textureId, out shared) < 0)
                throw new InvalidOperationException("Shared GPU texture unavailable");
            var mutexId = new Guid("9d8e1289-d7b3-465f-8126-250e349af85d");
            if (Method<Query>(shared, 0)(shared, ref mutexId, out mutex) < 0)
                throw new InvalidOperationException("GPU synchronization unavailable");
            Method<GetDevice>(device, 40)(device, out context);
            acquire = Method<Acquire>(mutex, 8);
            release = Method<Release>(mutex, 9);
            copy = Method<Copy>(context, 47);
            flush = Method<Flush>(context, 111);
            lock (jobsLock)
            {
                id = ++nextId;
                jobs.Add(id, this);
            }
        }
        catch
        {
            ReleaseResources();
            UnityEngine.Object.Destroy(Image);
            throw;
        }
    }

    public void Schedule(uint serial)
    {
        if (Volatile.Read(ref disposed) != 0 || Interlocked.CompareExchange(ref pending, 1, 0) != 0)
            return;
        Volatile.Write(ref requested, unchecked((int)serial));
        try
        {
            GL.IssuePluginEvent(callbackPointer, id);
        }
        catch
        {
            Volatile.Write(ref pending, 0);
            Volatile.Write(ref failed, 1);
        }
    }

    private static void Render(int id)
    {
        CaptureGpu job;
        lock (jobsLock) if (!jobs.TryGetValue(id, out job)) return;
        try
        {
            if (Volatile.Read(ref job.disposed) != 0)
                return;
            // WAIT_TIMEOUT is positive too: only S_OK means ownership.
            int hr = job.acquire(job.mutex, 1, 0);
            if (hr != 0)
            {
                if (hr != 258)
                    Volatile.Write(ref job.failed, 1);
                return;
            }
            try
            {
                job.copy(job.context, job.destination, job.shared);
                job.flush(job.context);
            }
            finally
            {
                if (job.release(job.mutex, 0) != 0)
                    Volatile.Write(ref job.failed, 1);
            }
            if (!job.Failed)
                Volatile.Write(ref job.copied, Volatile.Read(ref job.requested));
        }
        catch
        {
            Volatile.Write(ref job.failed, 1);
        }
        finally
        {
            Volatile.Write(ref job.pending, 0);
            if (Volatile.Read(ref job.disposed) != 0)
                job.ReleaseResources();
        }
    }

    private void ReleaseResources()
    {
        lock (jobsLock)
        {
            jobs.Remove(id);
            foreach (var value in new[] { context, mutex, shared, device, destination })
                if (value != IntPtr.Zero)
                    Marshal.Release(value);
            context = mutex = shared = device = destination = IntPtr.Zero;
        }
    }

    public void Dispose()
    {
        if (Interlocked.Exchange(ref disposed, 1) != 0)
            return;
        // A queued rendering callback owns COM references until it returns.
        if (Volatile.Read(ref pending) == 0)
            ReleaseResources();
    }
}
