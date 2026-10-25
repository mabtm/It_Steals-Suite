using UnityEngine;
using UnityEngine.SceneManagement;

// Startup convenience and reversible display overrides; keep saved game preferences intact.
public sealed class FramePolicy
{
    private bool introReady, owned;
    private int frames, sync;

    // Use the existing scene lifecycle instead of depending on a Start detour.
    // Melon initializes the scene after the game's volume/quality setup.
    public void SceneInitialized(int buildIndex)
    {
        introReady = buildIndex == 0;
    }

    public bool Tick(TrainerCommands command, bool connected)
    {
        if (connected && (command.TelemetryOptions & 4096u) != 0 && introReady &&
            SceneManager.GetActiveScene().buildIndex == 0)
        {
            introReady = false;
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
        introReady = false;
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
