using UnityEngine;
using UnityEngine.SceneManagement;

public static class PlayerContext
{
    public static MouseLook Observed;
    public static MouseLook Find()
    {
        int scene=SceneManager.GetActiveScene().handle;
        if(Observed!=null&&Observed.controller!=null&&Observed.gameObject.activeInHierarchy&&Observed.gameObject.scene.handle==scene)return Observed;
        foreach(var player in Object.FindObjectsOfType<MouseLook>())if(player.gameObject.scene.handle==scene&&player.gameObject.activeInHierarchy&&player.controller!=null){Observed=player;return player;}
        return null;
    }
    public static Camera View(MouseLook player)
    {
        // The gameplay camera stays bound while a separate spectator renders.
        var original=FreeCam.GameplayCamera(player);if(original!=null)return original;
        if(player!=null){
            if(player.mainCamera!=null&&player.mainCamera.enabled&&player.mainCamera.gameObject.activeInHierarchy)return player.mainCamera;
            if(player.playerCamera!=null){var cam=player.playerCamera.GetComponent<Camera>();if(cam!=null&&cam.enabled)return cam;}
            if(player.CameraHolder!=null)foreach(var cam in player.CameraHolder.GetComponentsInChildren<Camera>())if(cam.enabled&&cam!=player.UICamera&&cam!=player.radarCam)return cam;
        }
        return Camera.main;
    }
    public static Transform Body(MouseLook player){return player==null?null:player.playerBody!=null?player.playerBody:player.controller!=null?player.controller.transform:player.transform;}
}
