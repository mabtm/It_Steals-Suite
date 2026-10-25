using System;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Threading;
using UnityEngine;

// Optional capture presentation only; never changes the game camera or run data.
public sealed class CaptureOverlay : IDisposable
{
    private const int Capacity = 32 * 1024 * 1024 + 256;
    private readonly Texture2D[] images = new Texture2D[4];
    private readonly byte[][] pixels = new byte[4][];
    private readonly Rect[] bounds = new Rect[4];
    private MemoryMappedFile mapping;
    private MemoryMappedViewAccessor view;
    private Mutex gate;
    private uint serial;
    private int count, width, height;
    private double received;
    private CaptureGpu gpu;
    private bool gpuVisible;

    public CaptureOverlay(int pid)
    {
        string name = "Local\\ItSteals.Capture.v1." + pid;
        try
        {
            gate = new Mutex(false, name + ".Lock");
            mapping = MemoryMappedFile.CreateNew(name, Capacity, MemoryMappedFileAccess.ReadWrite);
            view = mapping.CreateViewAccessor();
            view.Write(0, 0x48505349u);
            view.Write(4, 1u);
            view.Write(8, (uint)Capacity);
            view.Write(12, pid);
            view.Write(44,
                       SystemInfo.graphicsDeviceType == UnityEngine.Rendering.GraphicsDeviceType.Direct3D11
                           ? 0u
                           : 2u);
        }
        catch
        {
            Dispose();
            throw;
        }
    }

    public void Tick(bool connected)
    {
        if (gpu != null && gpu.Failed)
        {
            CloseGpu();
            view.Write(44, 2u);
        }
        if (!connected)
        {
            count = 0;
            gpuVisible = false;
            return;
        }
        bool locked = false;
        try
        {
            try
            {
                locked = gate.WaitOne(0);
            }
            catch (AbandonedMutexException)
            {
                gate.ReleaseMutex();
                count = 0;
                return;
            }
            if (!locked)
                return;
            if (view.ReadUInt32(36) == 0)
            {
                count = 0;
                gpuVisible = false;
                return;
            }
            uint next = view.ReadUInt32(16);
            if (next == serial)
                return;
            int w = view.ReadInt32(24), h = view.ReadInt32(28), n = view.ReadInt32(32);
            if (view.ReadUInt32(40) == 1)
            {
                long handle = view.ReadInt64(48);
                if (w < 1 || h < 1 || w > 16384 || h > 16384 || handle == 0)
                    throw new InvalidOperationException("Invalid GPU capture dimensions");
                gate.ReleaseMutex();
                locked = false;
                try
                {
                    if (gpu == null || gpu.Handle != handle || gpu.Image.width != w || gpu.Image.height != h)
                    {
                        CloseGpu();
                        gpu = new CaptureGpu(handle, w, h);
                    }
                    if (gpu.Failed)
                        throw new InvalidOperationException("GPU capture copy failed");
                    view.Write(44, 1u);
                    width = w;
                    height = h;
                    count = 0;
                    serial = next;
                    received = Time.realtimeSinceStartupAsDouble;
                    gpuVisible = true;
                }
                catch
                {
                    CloseGpu();
                    view.Write(44, 2u);
                }
                return;
            }
            gpuVisible = false;
            if (w < 1 || h < 1 || w > 16384 || h > 16384 || n < 1 || n > 4)
                throw new InvalidOperationException("Invalid capture overlay dimensions");
            for (int i = 0; i < n; i++)
            {
                int at = 64 + i * 24, x = view.ReadInt32(at), y = view.ReadInt32(at + 4);
                int iw = view.ReadInt32(at + 8), ih = view.ReadInt32(at + 12);
                int offset = view.ReadInt32(at + 16), bytes = view.ReadInt32(at + 20);
                if (iw < 1 || ih < 1 || x < 0 || y < 0 || (long)x + iw > w || (long)y + ih > h ||
                    (long)iw * ih * 4 != bytes || offset < 256 || (long)offset + bytes > Capacity)
                    throw new InvalidOperationException("Invalid capture overlay region");
                if (pixels[i] == null || pixels[i].Length != bytes)
                    pixels[i] = new byte[bytes];
                // Mono's accessor ReadArray<byte> copies element by element.
                // The validated region is contiguous; copy it in one operation.
                Marshal.Copy(IntPtr.Add(view.SafeMemoryMappedViewHandle.DangerousGetHandle(),
                                        checked((int)view.PointerOffset + offset)),
                             pixels[i], 0, bytes);
                bounds[i] = new Rect(x, y, iw, ih);
            }
            // Never hold the producer's mutex during texture creation/upload.
            gate.ReleaseMutex();
            locked = false;
            for (int i = 0; i < n; i++)
            {
                int iw = (int)bounds[i].width, ih = (int)bounds[i].height;
                if (images[i] == null || images[i].width != iw || images[i].height != ih)
                {
                    if (images[i] != null)
                        UnityEngine.Object.Destroy(images[i]);
                    images[i] = new Texture2D(iw, ih, TextureFormat.BGRA32,
                                              false) { hideFlags = HideFlags.HideAndDontSave,
                                                       filterMode = FilterMode.Point,
                                                       wrapMode = TextureWrapMode.Clamp };
                }
                images[i].LoadRawTextureData(pixels[i]);
                images[i].Apply(false, false);
            }
            GC.KeepAlive(view);
            width = w;
            height = h;
            count = n;
            serial = next;
            received = Time.realtimeSinceStartupAsDouble;
        }
        catch
        {
            count = 0;
        }
        finally
        {
            if (locked)
                gate.ReleaseMutex();
        }
    }

    public void Draw()
    {
        if (Event.current.type != EventType.Repaint)
            return;
        // Also drain a hidden pending image so an Alt-Tab cannot leave the
        // producer waiting on a shared texture that nobody will consume.
        if (gpu != null)
            gpu.Schedule(serial);
        if ((!gpuVisible && count == 0) || Time.realtimeSinceStartupAsDouble - received > .35)
            return;
        var matrix = GUI.matrix;
        var color = GUI.color;
        int depth = GUI.depth;
        try
        {
            GUI.matrix =
                Matrix4x4.Scale(new Vector3((float)Screen.width / width, (float)Screen.height / height, 1));
            GUI.color = Color.white;
            GUI.depth = -10000;
            if (gpuVisible && gpu != null)
            {
                GUI.DrawTextureWithTexCoords(new Rect(0, 0, width, height), gpu.Image, new Rect(0, 1, 1, -1),
                                             true);
                if (gpu.Copied != 0)
                    view.Write(20, gpu.Copied);
            }
            else
            {
                for (int i = 0; i < count; i++)
                    // Raw DX11 rows are top-down; Unity texture UVs are bottom-up.
                    GUI.DrawTextureWithTexCoords(bounds[i], images[i], new Rect(0, 1, 1, -1), true);
                view.Write(20, serial);
            }
        }
        finally
        {
            GUI.matrix = matrix;
            GUI.color = color;
            GUI.depth = depth;
        }
    }

    public void Dispose()
    {
        CloseGpu();
        count = 0;
        for (int i = 0; i < images.Length; i++)
            if (images[i] != null)
                UnityEngine.Object.Destroy(images[i]);
        if (view != null)
            view.Dispose();
        if (mapping != null)
            mapping.Dispose();
        if (gate != null)
            gate.Dispose();
        view = null;
        mapping = null;
        gate = null;
    }

    private void CloseGpu()
    {
        gpuVisible = false;
        if (gpu == null)
            return;
        gpu.Dispose();
        UnityEngine.Object.Destroy(gpu.Image);
        gpu = null;
    }
}
