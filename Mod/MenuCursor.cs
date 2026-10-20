using UnityEngine;

// The external menu owns keyboard/mouse focus. Unity remains running, but must
// release its cursor lock. Never change Windows' process-global ShowCursor count.
public sealed class MenuCursor
{
    private static MenuCursor instance;
    private bool owned, visible;
    private bool remembered;
    private CursorLockMode lockMode;
    private static float blockUntil;
    public MenuCursor() {instance=this;}
    public static bool OwnsInput {get{return instance!=null&&instance.owned;}}
    public static bool GameHotkeysAllowed { get { return !OwnsInput && Application.isFocused && Time.realtimeSinceStartup >= blockUntil; } }
    public static void SetGameVisible(bool value)
    {
        if(instance!=null) {instance.visible=value;instance.remembered=true;}
        Cursor.visible=instance!=null && instance.owned ? true : value;
    }
    public static void SetGameLock(CursorLockMode value)
    {
        if(instance!=null) {instance.lockMode=value;instance.remembered=true;}
        Cursor.lockState=instance!=null && instance.owned ? CursorLockMode.None : value;
    }
    public void ObserveFocused()
    {
        if(owned || !Application.isFocused) return;
        visible=Cursor.visible;lockMode=Cursor.lockState;remembered=true;
    }
    public void Tick(bool menuOwnsInput)
    {
        if (menuOwnsInput)
        {
            if (!owned) {if(!remembered) {visible=Cursor.visible;lockMode=Cursor.lockState;}owned=true;}
            if (Cursor.lockState != CursorLockMode.None) Cursor.lockState = CursorLockMode.None;
            if (!Cursor.visible) Cursor.visible = true;
        }
        else if (owned) {Restore();blockUntil=Time.realtimeSinceStartup+.15f;}
    }
    public void Restore()
    {
        if (!owned) return;
        owned = false; Cursor.lockState = lockMode; Cursor.visible = visible;
    }
}
