using UnityEngine;

// A camera-only spectator. Never detach or move the gameplay camera's object:
// in some scenes that object also hosts the player/pickup collision geometry.
public sealed class FreeCam : MonoBehaviour
{
    private static FreeCam owner;
    public static bool FreeCamEnabled
    {
        get {
            return owner != null && owner.active;
        }
    }
    public static Camera SpectatorCamera
    {
        get {
            return FreeCamEnabled ? owner.spectator : null;
        }
    }

    public static Camera GameplayCamera(MouseLook player)
    {
        return FreeCamEnabled && owner.controller == player ? owner.cam : null;
    }

    public static Vector3 VisibilityOrigin(Vector3 original)
    {
        var camera = SpectatorCamera;
        return camera != null ? camera.transform.position : original;
    }

    // Unity sends trigger messages to disabled behaviours too. Suppress all
    // gameplay trigger effects for this frozen actor, including crystal pickup.
    public static bool AllowPlayerTrigger(MouseLook __instance)
    {
        return !FreeCamEnabled || owner.controller != __instance;
    }

    private Camera cam, spectator;
    private MouseLook controller;
    private bool controllerWasEnabled, cameraWasEnabled, headBobWasEnabled, active;

    private void Awake()
    {
        cam = GetComponent<Camera>();
    }

    public void Bind(MouseLook player)
    {
        if (controller != player)
            Disable();
        controller = player;
    }

    private void Update()
    {
        if (Replay3D.Reserved)
        {
            Disable();
            return;
        }
        if (SpeedrunManager.Instance != null && SpeedrunManager.Instance.Restricted)
        {
            Disable();
            return;
        }
        if (controller == null || cam == null || !cam.gameObject.activeInHierarchy)
        {
            Disable();
            return;
        }
        if (!active && cam != PlayerContext.View(controller))
            return;
        if (!MenuCursor.GameHotkeysAllowed)
            return;
        if (Input.GetKeyDown(KeyCode.F1))
        {
            if (active)
                Disable();
            else
                Enable();
        }
        if (!active || spectator == null)
            return;
        var view = spectator.transform;
        float speed = 10f * Time.unscaledDeltaTime * (Input.GetKey(KeyCode.LeftShift) ? 3f : 1f);
        Vector3 direction = Vector3.zero;
        if (Input.GetKey(KeyCode.W))
            direction += view.forward;
        if (Input.GetKey(KeyCode.S))
            direction -= view.forward;
        if (Input.GetKey(KeyCode.A))
            direction -= view.right;
        if (Input.GetKey(KeyCode.D))
            direction += view.right;
        if (Input.GetKey(KeyCode.Q))
            direction -= view.up;
        if (Input.GetKey(KeyCode.E))
            direction += view.up;
        view.position += Vector3.ClampMagnitude(direction, 1f) * speed;
        if (Input.GetMouseButton(1))
        {
            view.Rotate(Vector3.up, Input.GetAxis("Mouse X") * 3f, Space.World);
            view.Rotate(Vector3.right, -Input.GetAxis("Mouse Y") * 3f, Space.Self);
        }
    }

    private void Enable()
    {
        if (cam == null || controller == null)
            return;
        DisableActive();
        // Copy only Camera settings; cloning the actor GameObject would also
        // clone colliders, scripts and pickup triggers.
        var entity = new GameObject("PracticeSuite spectator camera");
        try
        {
            spectator = entity.AddComponent<Camera>();
            spectator.CopyFrom(cam);
            entity.transform.SetPositionAndRotation(cam.transform.position, cam.transform.rotation);
            spectator.enabled = false;
            entity.AddComponent<HitboxDrawer>();
            controllerWasEnabled = controller.enabled;
            cameraWasEnabled = cam.enabled;
            headBobWasEnabled = controller.HeadBobAnimation != null && controller.HeadBobAnimation.enabled;
            active = true;
            owner = this;
            if (controller.HeadBobAnimation != null)
                controller.HeadBobAnimation.enabled = false;
            controller.enabled = false;
            cam.enabled = false;
            spectator.enabled = true;
            if (SpeedrunManager.Instance != null)
                SpeedrunManager.Instance.MarkAssisted(RunAids.FreeCamera);
        }
        catch
        {
            if (active)
                Disable();
            else
            {
                spectator = null;
                Object.Destroy(entity);
            }
            throw;
        }
    }

    public void Disable()
    {
        if (!active)
            return;
        active = false;
        if (spectator != null)
        {
            spectator.enabled = false;
            Object.Destroy(spectator.gameObject);
            spectator = null;
        }
        if (cam != null)
            cam.enabled = cameraWasEnabled;
        if (controller != null)
        {
            controller.enabled = controllerWasEnabled;
            if (controller.HeadBobAnimation != null)
                controller.HeadBobAnimation.enabled = headBobWasEnabled;
        }
        if (owner == this)
            owner = null;
    }

    public static void DisableActive()
    {
        if (owner != null)
            owner.Disable();
    }

    public static bool Focus(MouseLook player, Bounds bounds)
    {
        if (player == null || Replay3D.Reserved ||
            SpeedrunManager.Instance != null && SpeedrunManager.Instance.Restricted)
            return false;
        var view = PlayerContext.View(player);
        if (view == null)
            return false;
        var free = owner != null ? owner : view.GetComponent<FreeCam>();
        if (free == null)
            return false;
        if (!free.active)
            free.Enable();
        if (free.spectator == null)
            return false;
        var camera = free.spectator.transform;
        float distance = Mathf.Clamp(bounds.extents.magnitude * 2.3f, 2, 60);
        camera.position = bounds.center - camera.forward * distance;
        camera.LookAt(bounds.center);
        return true;
    }

    private void OnDisable()
    {
        Disable();
    }

    private void OnDestroy()
    {
        Disable();
    }
}
