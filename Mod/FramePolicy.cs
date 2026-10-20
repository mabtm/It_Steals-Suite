using UnityEngine;

// Optional display-only overrides; never rewrite the game's saved preferences.
public sealed class FramePolicy
{
    private bool owned;private int frames,sync;
    public void Tick(TrainerCommands command,bool connected)
    {
        bool requested=connected&&(command.FrameLimitMode!=0||command.VSyncMode!=0);
        if(!requested){Restore();return;}
        if(!owned){frames=Application.targetFrameRate;sync=QualitySettings.vSyncCount;owned=true;}
        int target=command.FrameLimitMode==1?-1:command.FrameLimitMode==2?Mathf.Clamp(command.GameFrameLimit,30,1000):frames;
        int vs=command.VSyncMode==1?0:command.VSyncMode==2?1:sync;
        if(Application.targetFrameRate!=target)Application.targetFrameRate=target;
        if(QualitySettings.vSyncCount!=vs)QualitySettings.vSyncCount=vs;
    }
    public void SceneLoaded(){Restore();}
    public void Restore(){if(!owned)return;owned=false;Application.targetFrameRate=frames;QualitySettings.vSyncCount=sync;}
}
