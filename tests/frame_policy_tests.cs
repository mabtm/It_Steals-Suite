using System;
using UnityEngine;
using UnityEngine.SceneManagement;

public static class FramePolicyTests
{
    private static void Check(bool value, string message)
    {
        if (!value)
            throw new Exception(message);
    }

    public static void Main()
    {
        var command = new TrainerCommands { TelemetryOptions = 4096 };
        var policy = new FramePolicy();
        policy.Initialize(new[] { "game.exe", "--practice-suite-skip-intro=0" });
        SceneManager.Active = new Scene { buildIndex = 0, isLoaded = true };
        Check(!policy.Tick(command, false), "ordinary disconnected startup is unchanged");
        SceneManager.Active = new Scene();
        Check(!policy.Tick(command, true), "unloaded startup is not entered");
        policy.Initialize(new[] { "game.exe", "--practice-suite-skip-intro" });
        policy.SceneLoaded();
        Check(!policy.Tick(command, false), "startup launch preference waits for a loaded scene");
        // Unity calls every Start before the first Update. No initialized-scene
        // notification or trainer heartbeat is required to finish startup.
        Application.targetFrameRate = 72;
        QualitySettings.vSyncCount = 2;
        SceneManager.Active = new Scene { buildIndex = 0, isLoaded = true };
        SceneManager.DeferLoad = true;
        Check(policy.Tick(command, false) && SceneManager.Loads == 1 && SceneManager.Last == "TitleScreen" &&
                  Cursor.visible && Cursor.lockState == CursorLockMode.None,
              "launcher enters the main menu without waiting for the trainer connection");
        Check(Application.targetFrameRate == 72 && QualitySettings.vSyncCount == 2,
              "game startup display settings are preserved");
        Check(!policy.Tick(command, true) && SceneManager.Loads == 1,
              "a deferred scene switch is requested only once");
        SceneManager.DeferLoad = false;
        foreach (int scene in new[] { -1, 1, 2, 3, 7, 13 })
        {
            SceneManager.Active = new Scene { buildIndex = scene, isLoaded = true };
            policy.SceneLoaded();
            Check(!policy.Tick(command, true) && SceneManager.Loads == 1,
                  "menu, death and run scenes cannot be skipped");
            int reads = SceneManager.Reads;
            Check(!policy.Tick(command, true) && SceneManager.Reads == reads,
                  "non-intro scenes are checked once, not on every frame");
        }
        SceneManager.Active = new Scene { buildIndex = 0, isLoaded = true };
        policy.SceneLoaded();
        Check(!policy.Tick(command, false), "consumed launch flag does not survive later scene changes");
        Check(!policy.Tick(new TrainerCommands(), true), "connected preference can be disabled");
        Check(policy.Tick(command, true) && SceneManager.Loads == 2,
              "connected intro skip works even without the initial scene callback");

        var disabled = new FramePolicy();
        disabled.Initialize(new[] { "--practice-suite-skip-intro" });
        SceneManager.Active = new Scene { buildIndex = 0, isLoaded = true };
        Check(!disabled.Tick(new TrainerCommands(), true) && !disabled.Tick(command, false),
              "a live opt-out overrides the launch preference and survives disconnect");
        var late = new FramePolicy();
        late.Initialize(new[] { "--practice-suite-skip-intro" });
        SceneManager.Active = new Scene { buildIndex = 2, isLoaded = true };
        Check(!late.Tick(command, false), "a completed menu startup consumes the launch preference");
        SceneManager.Active = new Scene { buildIndex = 0, isLoaded = true };
        late.SceneLoaded();
        Check(!late.Tick(command, false), "a launch flag cannot skip a later intro visit");
        policy.SceneLoaded();
        Check(policy.Tick(command, true) && SceneManager.Loads == 3,
              "a later startup honors the current connected preference");
        Application.targetFrameRate = 72;
        QualitySettings.vSyncCount = 2;
        command.FrameLimitMode = 2;
        command.GameFrameLimit = 2000;
        command.VSyncMode = 1;
        Check(!policy.Tick(command, true) && Application.targetFrameRate == 1000 &&
                  QualitySettings.vSyncCount == 0,
              "display overrides remain clamped and independent");
        command.FrameLimitMode = 1;
        policy.Tick(command, true);
        Check(Application.targetFrameRate == -1, "unlimited mode");
        policy.Tick(command, false);
        Check(Application.targetFrameRate == 72 && QualitySettings.vSyncCount == 2,
              "disconnect restores original display settings");
        policy.Tick(command, true);
        policy.SceneLoaded();
        Check(Application.targetFrameRate == 72 && QualitySettings.vSyncCount == 2,
              "scene change restores original display settings");
        Console.WriteLine(
            "Pre-connection intro, missing callbacks, opt-out, scene isolation and display restoration: PASS");
    }
}

public struct TrainerCommands
{
    public uint TelemetryOptions, FrameLimitMode, VSyncMode;
    public int GameFrameLimit;
}

namespace UnityEngine
{
public static class Application
{
    public static int targetFrameRate;
}

public static class QualitySettings
{
    public static int vSyncCount;
}

public static class Mathf
{
    public static int Clamp(int value, int min, int max)
    {
        return Math.Max(min, Math.Min(max, value));
    }
}

public enum CursorLockMode
{
    None,
    Locked
}

public static class Cursor
{
    public static bool visible;
    public static CursorLockMode lockState = CursorLockMode.Locked;
}
}

namespace UnityEngine.SceneManagement
{
public struct Scene
{
    public int buildIndex;
    public bool isLoaded;
}

public static class SceneManager
{
    public static int Loads;
    public static int Reads;
    public static string Last;
    public static Scene Active;
    public static bool DeferLoad;

    public static Scene GetActiveScene()
    {
        Reads++;
        return Active;
    }

    public static void LoadScene(string name)
    {
        Last = name;
        if (!DeferLoad)
            Active = new Scene { buildIndex = 2, isLoaded = true };
        Loads++;
    }
}
}
