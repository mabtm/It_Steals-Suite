using UnityEngine;

public sealed class HitboxDrawer : MonoBehaviour
{
    private static bool captureEnabled;
    public static bool Enabled {get{return captureEnabled;}set{if(value&&!captureEnabled&&SpeedrunManager.Instance!=null)SpeedrunManager.Instance.MarkAssisted(RunAids.Hitboxes);captureEnabled=value;}}
    public static bool HidePlayerHitbox;
    public static readonly HitboxInfo[] Hitboxes = new HitboxInfo[300];
    public static int HitboxCount;
    public enum HitboxType { Player, Crystal, CrystalPickup, Monster, OutOfBounds, Pickup, KillTrigger }
    public struct HitboxInfo { public HitboxType type; public Collider collider; }
    private Material lines;
    private Camera camera;
    private readonly ColliderWireGeometry geometry=new ColliderWireGeometry();
    public static void Clear() {System.Array.Clear(Hitboxes,0,Hitboxes.Length);HitboxCount=0;}

    private void Awake()
    {
        camera = GetComponent<Camera>();
        Shader shader = Shader.Find("Hidden/Internal-Colored");
        if (shader == null) return;
        lines = new Material(shader) { hideFlags = HideFlags.HideAndDontSave };
        lines.SetInt("_SrcBlend", 5); lines.SetInt("_DstBlend", 10);
        lines.SetInt("_Cull", 0); lines.SetInt("_ZWrite", 0); lines.SetInt("_ZTest", 8);
    }
    private void OnPostRender()
    {
        if (!isActiveAndEnabled || (!Enabled&&!LiveInspector.Drawing) || camera == null || lines == null || !lines.SetPass(0)) return;
        GL.PushMatrix();
        try
        {
            GL.LoadProjectionMatrix(camera.projectionMatrix);
            GL.modelview = camera.worldToCameraMatrix;
            GL.Begin(GL.LINES);
            try
            {
                int count = Enabled?Mathf.Clamp(HitboxCount, 0, Hitboxes.Length):0;
                for (int i = 0; i < count; i++)
                {
                    HitboxInfo info = Hitboxes[i];
                    if ((info.type == HitboxType.Player || info.type == HitboxType.Pickup) && HidePlayerHitbox) continue;
                    if (info.collider == null || !info.collider.enabled || !info.collider.gameObject.activeInHierarchy) continue;
                    GL.Color(Colour(info.type));
                    DrawCollider(info.collider);
                }
                LiveInspector.Draw(geometry);
            }
            finally { GL.End(); }
        }
        finally { GL.PopMatrix(); }
    }
    private static Color Colour(HitboxType type)
    {
        switch (type)
        {
            case HitboxType.Player: return Color.cyan;
            case HitboxType.Crystal: return new Color(1f,.65f,.25f);
            case HitboxType.CrystalPickup: return Color.yellow;
            case HitboxType.Monster: return Color.red;
            case HitboxType.Pickup: return Color.green;
            case HitboxType.KillTrigger: return new Color(1f, 0.5f, 0f);
            default: return new Color(0.3f, 0.3f, 0.3f);
        }
    }
    private void DrawCollider(Collider collider)
    {
        ReplayColliderShape shape;if(ReplayColliderShape.Capture(collider,out shape))geometry.Draw(ReplayColliderShape.Kind(collider),shape);
    }
    private void OnDestroy() { if (lines != null) Destroy(lines); }
}
