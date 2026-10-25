using UnityEngine;
using UnityEngine.SceneManagement;

// Startup convenience and reversible display overrides; keep saved game preferences intact.
public sealed class FramePolicy
{
    private static IntroScroll intro;
    private bool owned;
    private int frames, sync;

    // Postfix: the game's volume and first-launch quality initialization must
    // complete before skipping its startup scene. No per-frame scene searches.
    public static void IntroStarted(IntroScroll __instance)
    {
        if (__instance != null && __instance.gameObject.scene.buildIndex == 0)
            intro = __instance;
    }

    public bool Tick(TrainerCommands command, bool connected)
    {
        if (connected && (command.TelemetryOptions & 4096u) != 0 && intro != null)
        {
            intro = null;
            Cursor.lockState = CursorLockMode.None;
            Cursor.visible = true;
            SceneManager.LoadScene("TitleScreen");
            return true;
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
        intro = null;
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
