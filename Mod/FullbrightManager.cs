using System;
using UnityEngine;
using MelonLoader;
using UnityEngine.SceneManagement;

public sealed class FullbrightManager : MonoBehaviour
{
    public static bool AnyActive { get; private set; }
    private SceneBrightness brightness;
    private bool active;
    private int scene;
    private float nextScan, nextError;

    public void SetFullbright(bool enabled)
    {
        if (enabled == active)
            return;
        if (!enabled)
        {
            Restore();
            return;
        }
        try
        {
            brightness = new SceneBrightness();
            Scan();
            brightness.SetActive(true);
            active = AnyActive = true;
            if (SpeedrunManager.Instance != null)
                SpeedrunManager.Instance.MarkAssisted(RunAids.Fullbright);
        }
        catch (Exception error)
        {
            Restore();
            Warn(error);
        }
    }

    private void Scan()
    {
        nextScan = Time.realtimeSinceStartup + 1;
        int current = SceneManager.GetActiveScene().handle;
        if (active && current != scene)
        {
            var old = brightness;
            brightness = new SceneBrightness();
            try
            {
                old.Dispose();
            }
            catch (Exception error)
            {
                Warn(error);
            }
        }
        scene = current;
        brightness.Prune();
        foreach (var r in UnityEngine.Object.FindObjectsOfType<Renderer>(true))
            if (r.gameObject.scene.IsValid())
                brightness.Add(r);
        if (active)
            brightness.SetActive(true);
    }

    private void Update()
    {
        if (active &&
            (Time.realtimeSinceStartup >= nextScan || SceneManager.GetActiveScene().handle != scene))
            try
            {
                Scan();
            }
            catch (Exception error)
            {
                Warn(error);
            }
    }

    private void Warn(Exception error)
    {
        if (Time.realtimeSinceStartup < nextError)
            return;
        nextError = Time.realtimeSinceStartup + 5;
        MelonLogger.Warning("Fullbright texture view: " + error.Message);
    }

    private void Restore()
    {
        active = AnyActive = false;
        var old = brightness;
        brightness = null;
        if (old != null)
            try
            {
                old.Dispose();
            }
            catch (Exception error)
            {
                Warn(error);
            }
    }

    private void OnDisable()
    {
        Restore();
    }

    private void OnDestroy()
    {
        Restore();
    }
}
