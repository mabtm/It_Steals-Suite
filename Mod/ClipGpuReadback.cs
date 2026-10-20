using System;
using System.Diagnostics;
using UnityEngine;
using UnityEngine.Rendering;

// Small one-time orientation/format calibration, not an assumption about D3D
// vs GL row order. An unsupported or failing device uses ReadPixels safely.
public static class ClipReadbackCalibration
{
    private static bool started, finished, enabled;
    private static RenderTexture target;
    private static Texture2D image, pattern;
    private static AsyncGPUReadbackRequest request;
    private static byte[] reference;
    public static bool BottomUp { get; private set; }
    public static bool Bgra { get; private set; }
    public static bool Ready
    {
        get {
            return finished;
        }
    }
    public static bool Enabled
    {
        get {
            return finished && enabled;
        }
    }
    public static string Failure { get; private set; }
    public static TextureFormat Format
    {
        get {
            return Bgra ? TextureFormat.BGRA32 : TextureFormat.RGBA32;
        }
    }

    public static void Disable(string why)
    {
        enabled = false;
        Failure = why;
    }

    public static void Tick(bool begin)
    {
        if (!started)
        {
            if (!begin)
                return;
            started = true;
            Bgra = SystemInfo.SupportsTextureFormat(TextureFormat.BGRA32);
            if (!SystemInfo.supportsAsyncGPUReadback)
            {
                finished = true;
                Failure = "Device does not support async readback";
                return;
            }
            var previous = RenderTexture.active;
            try
            {
                target = new RenderTexture(
                    2, 2, 0, RenderTextureFormat.ARGB32) { hideFlags = HideFlags.HideAndDontSave };
                target.Create();
                pattern = new Texture2D(2, 2, TextureFormat.RGBA32,
                                        false) { hideFlags = HideFlags.HideAndDontSave };
                pattern.SetPixels32(new[] { new Color32(255, 0, 0, 255), new Color32(0, 255, 0, 255),
                                            new Color32(0, 0, 255, 255), new Color32(255, 255, 255, 255) });
                pattern.Apply(false, false);
                Graphics.Blit(pattern, target);
                image = new Texture2D(2, 2, Format, false) { hideFlags = HideFlags.HideAndDontSave };
                RenderTexture.active = target;
                image.ReadPixels(new Rect(0, 0, 2, 2), 0, 0, false);
                reference = new byte[16];
                image.GetRawTextureData<byte>().CopyTo(reference);
                request = AsyncGPUReadback.Request(target, 0, Format);
            }
            catch (Exception e)
            {
                finished = true;
                Disable("GPU calibration: " + e.Message);
                Release();
            }
            finally
            {
                RenderTexture.active = previous;
            }
        }
        if (finished || !request.done)
            return;
        try
        {
            if (request.hasError)
                throw new InvalidOperationException("Format readback is unavailable");
            var bytes = request.GetData<byte>();
            if (bytes.Length != 16)
                throw new InvalidOperationException("Unexpected calibration byte count");
            var result = new byte[16];
            bytes.CopyTo(result);
            bool bottom;
            if (!ClipPixelLayout.InferBottomUp(result, reference, out bottom))
                throw new InvalidOperationException("GPU row order could not be verified");
            BottomUp = bottom;
            enabled = true;
        }
        catch (Exception e)
        {
            Disable("GPU calibration: " + e.Message);
        }
        finally
        {
            finished = true;
            Release();
        }
    }

    private static void Release()
    {
        if (target != null)
        {
            target.Release();
            UnityEngine.Object.Destroy(target);
        }
        if (image != null)
            UnityEngine.Object.Destroy(image);
        if (pattern != null)
            UnityEngine.Object.Destroy(pattern);
        target = null;
        image = pattern = null;
        reference = null;
    }
}

// One slot owns its render target until its asynchronous request has finished.
// GetData is copied immediately on completion, before Unity retires its buffer.
public sealed class ClipGpuReadback : IDisposable
{
    public readonly RenderTexture Target;
    private Texture2D image;
    private readonly byte[] raw;
    public readonly byte[] Pixels;
    private AsyncGPUReadbackRequest request;
    private bool pending, ready, disposed;
    private long started;
    public int Mode { get; private set; }
    public float Milliseconds { get; private set; }
    public bool Busy
    {
        get {
            return pending;
        }
    }

    public ClipGpuReadback(int width, int height)
    {
        raw = new byte[checked(width * height * 4)];
        Pixels = new byte[raw.Length];
        Target = new RenderTexture(width, height, 24,
                                   RenderTextureFormat.ARGB32) { hideFlags = HideFlags.HideAndDontSave };
        try
        {
            if (!Target.Create())
                throw new InvalidOperationException("Clip render target could not be created");
        }
        catch
        {
            Target.Release();
            UnityEngine.Object.Destroy(Target);
            throw;
        }
    }

    public void Begin()
    {
        if (disposed || pending)
            throw new InvalidOperationException("GPU slot is still in use");
        ready = false;
        started = Stopwatch.GetTimestamp();
        Mode = ClipReadbackCalibration.Enabled ? 1 : 0;
        if (Mode == 1)
            try
            {
                request = AsyncGPUReadback.Request(Target, 0, ClipReadbackCalibration.Format);
                pending = true;
                return;
            }
            catch (Exception e)
            {
                ClipReadbackCalibration.Disable(e.Message);
                Mode = 0;
            }
        Synchronous();
    }

    private void Synchronous()
    {
        var previous = RenderTexture.active;
        try
        {
            if (image == null)
                image = new Texture2D(Target.width, Target.height, ClipReadbackCalibration.Format,
                                      false) { hideFlags = HideFlags.HideAndDontSave };
            RenderTexture.active = Target;
            image.ReadPixels(new Rect(0, 0, Target.width, Target.height), 0, 0, false);
            image.GetRawTextureData<byte>().CopyTo(raw);
            Convert(true);
        }
        finally
        {
            RenderTexture.active = previous;
        }
    }

    private void Convert(bool bottomUp)
    {
        ClipPixelLayout.TopDownBgra(raw, Pixels, Target.width, Target.height, bottomUp,
                                    ClipReadbackCalibration.Bgra);
        ready = true;
        Milliseconds = (float)((Stopwatch.GetTimestamp() - started) * 1000.0 / Stopwatch.Frequency);
    }

    public bool Poll()
    {
        if (disposed)
            return false;
        if (ready)
            return true;
        if (!pending || !request.done)
            return false;
        pending = false;
        try
        {
            if (request.hasError)
                throw new InvalidOperationException("Async GPU readback failed");
            var data = request.GetData<byte>();
            if (data.Length != raw.Length)
                throw new InvalidOperationException("Unexpected GPU byte count");
            data.CopyTo(raw);
            Convert(ClipReadbackCalibration.BottomUp);
        }
        catch (Exception e)
        {
            ClipReadbackCalibration.Disable(e.Message);
            Mode = 0;
            Synchronous();
        }
        return ready;
    }

    public bool CanDispose
    {
        get {
            return !pending || request.done;
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        if (!CanDispose)
            throw new InvalidOperationException("GPU readback is still pending");
        disposed = true;
        Target.Release();
        UnityEngine.Object.Destroy(Target);
        if (image != null)
            UnityEngine.Object.Destroy(image);
    }
}
