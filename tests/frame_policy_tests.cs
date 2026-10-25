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
        var policy = new FramePolicy();
        var command = new TrainerCommands { TelemetryOptions = 4096 };
        Check(!policy.Tick(command, true) && SceneManager.Loads == 0,
              "startup must finish its original initialization first");
        policy.SceneLoaded();
        Check(!policy.Tick(command, true), "scene-loaded alone does not bypass original Start");
        policy.SceneInitialized(0);
        Check(!policy.Tick(command, false) && SceneManager.Loads == 0,
              "disconnected trainer does not skip the game intro");
        Check(!policy.Tick(new TrainerCommands(), true) && SceneManager.Loads == 0,
              "intro preference can be disabled");
        Check(policy.Tick(command, true) && SceneManager.Loads == 1 && SceneManager.Last == "TitleScreen" &&
                  Cursor.visible && Cursor.lockState == CursorLockMode.None,
              "ready startup enters the main menu with its normal cursor");
        Check(!policy.Tick(command, true) && SceneManager.Loads == 1, "startup skip occurs once");
        foreach (int scene in new[] { 1, 2, 3, 7, 13 })
        {
            policy.SceneLoaded();
            policy.SceneInitialized(scene);
            Check(!policy.Tick(command, true) && SceneManager.Loads == 1,
                  "menu, death and run scenes cannot be skipped");
        }
        policy.SceneInitialized(0);
        Check(!policy.Tick(command, true), "stale startup readiness cannot skip another active scene");
        policy.SceneLoaded();
        Check(!policy.Tick(command, true) && SceneManager.Loads == 1, "scene change clears a pending intro");
        SceneManager.Active = new Scene { buildIndex = 0 };
        policy.SceneLoaded();
        Check(!policy.Tick(command, true), "returning to startup still waits for initialization");
        policy.SceneInitialized(0);
        Check(policy.Tick(command, true) && SceneManager.Loads == 2,
              "a later startup initializes a fresh one-shot skip");
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
            "Startup initialization gate, opt-out, connection, scene isolation and display restoration: PASS");
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
}

public static class SceneManager
{
    public static int Loads;
    public static string Last;
    public static Scene Active;

    public static Scene GetActiveScene()
    {
        return Active;
    }

    public static void LoadScene(string name)
    {
        Last = name;
        Active = new Scene { buildIndex = 2 };
        Loads++;
    }
}
}
