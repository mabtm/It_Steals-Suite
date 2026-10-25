using System;
using UnityEngine;
using UnityEngine.Rendering;
using UnityEngine.SceneManagement;

// Startup convenience and reversible display overrides; keep saved game preferences intact.
public sealed class FramePolicy
{
    private bool startupIntro, introChecked, owned;
    private int frames, sync;

    // The splash runs before the first Update/IPC heartbeat. The launcher passes
    // the saved preference for this process only; no game preferences are edited.
    public void Initialize(string[] arguments)
    {
        startupIntro = Array.IndexOf(arguments, "--practice-suite-skip-intro") >= 0;
        if (startupIntro && !SplashScreen.isFinished)
            SplashScreen.Stop(SplashScreen.StopBehavior.StopImmediate);
    }

    public bool Tick(TrainerCommands command, bool connected)
    {
        bool skipIntro = connected ? (command.TelemetryOptions & 4096u) != 0 : startupIntro;
        if (connected)
            startupIntro = false;
        if (skipIntro && !introChecked)
        {
            // Called from Update, after the game's Start methods. Inspect the
            // active scene so a missed initial scene callback cannot block this.
            Scene scene = SceneManager.GetActiveScene();
            if (scene.isLoaded)
            {
                introChecked = true;
                startupIntro = false;
                if (scene.buildIndex == 0)
                {
                    Cursor.lockState = CursorLockMode.None;
                    Cursor.visible = true;
                    SceneManager.LoadScene("TitleScreen");
                    return true;
                }
            }
        }
        bool requested = connected && (command.FrameLimitMode != 0 || command.VSyncMode != 0);
        if (!requested)
        {
            Restore();
            return false;
        }
        if (!owned)
        {
            frames = Application.targetFrameRate;
            sync = QualitySettings.vSyncCount;
            owned = true;
        }
        int target = command.FrameLimitMode == 1   ? -1
                     : command.FrameLimitMode == 2 ? Mathf.Clamp(command.GameFrameLimit, 30, 1000)
                                                   : frames;
        int vs = command.VSyncMode == 1 ? 0 : command.VSyncMode == 2 ? 1 : sync;
        if (Application.targetFrameRate != target)
            Application.targetFrameRate = target;
        if (QualitySettings.vSyncCount != vs)
            QualitySettings.vSyncCount = vs;
        return false;
    }

    public void SceneLoaded()
    {
        introChecked = false;
        Restore();
    }

    public void Restore()
    {
        if (!owned)
            return;
        owned = false;
        Application.targetFrameRate = frames;
        QualitySettings.vSyncCount = sync;
    }
}
