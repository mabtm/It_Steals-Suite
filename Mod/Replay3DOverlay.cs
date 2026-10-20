using System;
using System.Collections.Generic;
using UnityEngine;

public static class Replay3DOptions
{
    public const uint Playing = 1, Free = 2, Follow = 4, Player = 8, Monster = 16, Crystal = 32, Level = 64,
                      Trail = 128, Loop = 256, Xray = 512;
    public const uint HideCeiling = 2048, FadeWalls = 4096, Simplified = 16384, Fullbright = 32768;
    public const uint Audio = 65536, Perception = 131072;
    public const uint Mask = 0x7F03DBFF, Seek = 1024; // Seek is a command pulse, never a display option.
}

public static class ReplayMotionMetrics
{
    public static float Speed(List<ReplayViewFrame> frames, int index)
    {
        if (index < 1 || index >= frames.Count)
            return -1;
        double elapsed = 0, distance = 0;
        int count = 0;
        for (int k = index; k > 0 && count++ < 64 && elapsed < .12; k--)
        {
            var a = frames[k - 1];
            var b = frames[k];
            double dt = b.Seconds - a.Seconds;
            float d = (b.Body - a.Body).magnitude;
            if (dt <= 0)
                continue;
            if (dt > .25 || d > Math.Max(4, dt * 40))
                return -1;
            elapsed += dt;
            distance += d;
        }
        return elapsed >= .025 ? (float)(distance / elapsed) : -1;
    }
}

// Only the replay camera draws these lines. Independent of cheat hitboxes,
// with one material and bounded line counts. Sources are cached at stage load.
public sealed class Replay3DOverlay : MonoBehaviour
{
    public Action BeforeRender;

    private void OnPreCull()
    {
        if (BeforeRender != null)
            BeforeRender();
    }

    public ReplayMotionData Motion;
    public uint Options;
    public double Seconds;
    public float TrailSeconds = 3;
    public int VisibleColliders;
    private Camera view;
    private Material lines;
    private bool vertexColors;
    private readonly ColliderWireGeometry wire = new ColliderWireGeometry();
    public bool Ready
    {
        get {
            return lines != null;
        }
    }
    public string ShaderName
    {
        get {
            return lines == null ? "unavailable" : lines.shader.name;
        }
    }

    private struct TrailSegment
    {
        public Vector3 A, B, SideA, SideB;
        public Color First, Last;
        public int Band;
    }

    private readonly TrailSegment[] trail = new TrailSegment[360];
    private int trailCount;
    private ColliderWireGeometry playerWire;
    private float playerPixels;
    private int playerSegments;
    private Vector3 wireCamera, wireForward, wireUp;
    private float wireNear, wireFov;
    private int wireHeight;

    private struct StaticShape
    {
        public int Kind;
        public ReplayColliderShape Shape;
        public Vector3 Position;
    }

    private readonly List<StaticShape> level = new List<StaticShape>();

    public void AddStatic(Collider source)
    {
        ReplayColliderShape s;
        int kind = ReplayColliderShape.Kind(source);
        if (kind != 0 && !source.isTrigger && level.Count < 2048 && source.enabled &&
            source.gameObject.activeInHierarchy && ReplayColliderShape.Capture(source, out s))
            level.Add(new StaticShape { Kind = kind, Shape = s, Position = source.bounds.center });
    }

    private void Awake()
    {
        playerWire = new ColliderWireGeometry(PlayerLine);
        view = GetComponent<Camera>();
        // The shipped game guarantees Unlit/Color, not the editor's internal
        // colored shader. A missing shader must never silently remove all
        // collider and trail geometry.
        foreach (string name in new[] { "Hidden/Internal-Colored", "Sprites/Default", "UI/Default",
                                        "Unlit/Color" })
        {
            var shader = Shader.Find(name);
            if (shader == null || !shader.isSupported)
                continue;
            lines = new Material(shader) { hideFlags = HideFlags.HideAndDontSave };
            vertexColors = name != "Unlit/Color";
            break;
        }
        if (lines == null)
            return;
        Set("_SrcBlend", 5);
        Set("_DstBlend", 10);
        Set("_Cull", 0);
        Set("_ZWrite", 0);
        Set("_ZTest", 4);
    }

    private void Set(string property, int value)
    {
        if (lines.HasProperty(property))
            lines.SetInt(property, value);
    }

    private bool Pass(Color color)
    {
        if (lines.HasProperty("_Color"))
            lines.SetColor("_Color", vertexColors ? Color.white : color);
        return lines.SetPass(0);
    }

    private void OnPostRender()
    {
        VisibleColliders = 0;
        if (view == null || lines == null || Motion == null ||
            (Options & (248 | Replay3DOptions.Perception)) == 0)
            return;
        var transform = view.transform;
        wireCamera = transform.position;
        wireForward = transform.forward;
        wireUp = transform.up;
        wireNear = view.nearClipPlane;
        wireFov = view.fieldOfView;
        wireHeight = view.pixelHeight;
        // At this point this dedicated camera has finished the scene. Clear
        // only its depth for x-ray: this also works with fallback shaders that
        // have a fixed ZTest. The next camera frame clears normally.
        GL.PushMatrix();
        try
        {
            if ((Options & Replay3DOptions.Xray) != 0)
                GL.Clear(true, false, Color.clear);
            GL.LoadProjectionMatrix(view.projectionMatrix);
            GL.modelview = view.worldToCameraMatrix;
            for (int group = 0; group <= 3; group++)
                DrawColliders(group);
            if ((Options & Replay3DOptions.Perception) != 0)
                DrawProbes();
            if ((Options & Replay3DOptions.Trail) != 0)
            {
                BuildTrail();
                DrawTrail();
            }
        }
        finally
        {
            GL.PopMatrix();
        }
    }

    private void DrawProbes()
    {
        var probes = Motion.Experience.Probes;
        int lo = 0, hi = probes.Count;
        while (lo < hi)
        {
            int mid = lo + (hi - lo) / 2;
            if (probes[mid].Seconds < Seconds - .12)
                lo = mid + 1;
            else
                hi = mid;
        }
        // These are the game's recorded physics queries, never a new AI test.
        for (int band = 0; band < 3; band++)
        {
            Color color = band == 2   ? new Color(1, .3f, .2f, .95f)
                          : band == 1 ? new Color(.25f, 1, .68f, .85f)
                                      : new Color(.5f, .65f, 1, .45f);
            if (!Pass(color))
                continue;
            GL.Begin(GL.LINES);
            try
            {
                GL.Color(color);
                for (int i = lo, n = 0; i < probes.Count && n < 128 && probes[i].Seconds <= Seconds; i++, n++)
                {
                    var probe = probes[i];
                    int kind = (probe.Flags & 8) != 0 ? 2 : (probe.Flags & 1) != 0 ? 0 : 1;
                    if (kind != band || (probe.Origin - view.transform.position).sqrMagnitude > 10000)
                        continue;
                    GL.Vertex(probe.Origin);
                    GL.Vertex(probe.End);
                }
            }
            finally
            {
                GL.End();
            }
        }
    }

    private void DrawColliders(int group)
    {
        uint flag = group == 0   ? Replay3DOptions.Level
                    : group == 1 ? Replay3DOptions.Player
                    : group == 2 ? Replay3DOptions.Monster
                                 : Replay3DOptions.Crystal;
        if ((Options & flag) == 0)
            return;
        Color color = group == 0   ? new Color(.64f, .76f, 1, .65f)
                      : group == 1 ? new Color(.9f, .4f, 1, 1)
                      : group == 2 ? new Color(1, .35f, .22f, 1)
                                   : new Color(.25f, 1, .72f, 1);
        if (group == 1)
        {
            DrawPlayer(new Color(.015f, .02f, .035f, 1), 4.5f);
            DrawPlayer(color, 2.5f);
            return;
        }
        if (!Pass(color))
            return;
        GL.Begin(GL.LINES);
        try
        {
            GL.Color(color);
            if (group == 0)
            {
                int drawn = 0;
                for (int i = 0; i < level.Count && drawn < 256; i++)
                {
                    var s = level[i];
                    if ((s.Position - view.transform.position).sqrMagnitude > 3600)
                        continue;
                    wire.Draw(s.Kind, s.Shape);
                    drawn++;
                    VisibleColliders++;
                }
                return;
            }
            foreach (var track in Motion.Colliders.Tracks)
            {
                if (track.Group != group)
                    continue;
                ReplayColliderFrame frame;
                if (!track.At(Seconds, out frame))
                    continue;
                var center = track.Kind == 1 ? frame.Shape.Matrix.MultiplyPoint3x4(frame.Shape.Center)
                                             : frame.Shape.Center;
                if ((center - view.transform.position).sqrMagnitude > 3600)
                    continue;
                wire.Draw(track.Kind, frame.Shape);
                VisibleColliders++;
            }
        }
        finally
        {
            GL.End();
        }
    }

    private void PlayerLine(Vector3 a, Vector3 b)
    {
        if (playerSegments++ >= 4096)
            return;
        Vector3 sideA, sideB;
        if (!ReplayWireVisual.Ribbon(ref a, ref b, wireCamera, wireForward, wireUp, wireNear, wireFov,
                                     wireHeight, playerPixels, out sideA, out sideB))
            return;
        GL.Vertex(a - sideA);
        GL.Vertex(a + sideA);
        GL.Vertex(b + sideB);
        GL.Vertex(a - sideA);
        GL.Vertex(b + sideB);
        GL.Vertex(b - sideB);
        if (!vertexColors)
        {
            GL.Vertex(b + sideB);
            GL.Vertex(a + sideA);
            GL.Vertex(a - sideA);
            GL.Vertex(b - sideB);
            GL.Vertex(b + sideB);
            GL.Vertex(a - sideA);
        }
    }

    private void DrawPlayer(Color color, float pixels)
    {
        if (!Pass(color))
            return;
        playerPixels = pixels;
        playerSegments = 0;
        GL.Begin(GL.TRIANGLES);
        try
        {
            GL.Color(color);
            foreach (var track in Motion.Colliders.Tracks)
            {
                if (track.Group != 1)
                    continue;
                ReplayColliderFrame frame;
                if (!track.At(Seconds, out frame))
                    continue;
                var center = track.Kind == 1 ? frame.Shape.Matrix.MultiplyPoint3x4(frame.Shape.Center)
                                             : frame.Shape.Center;
                if ((center - view.transform.position).sqrMagnitude > 3600)
                    continue;
                playerWire.Draw(track.Kind, frame.Shape);
                if (pixels == 2.5f)
                    VisibleColliders++;
            }
        }
        finally
        {
            GL.End();
        }
    }

    private void BuildTrail()
    {
        trailCount = 0;
        var f = Motion.Frames;
        int lo = 0, hi = f.Count;
        double start = Math.Max(0, Seconds - Mathf.Clamp(TrailSeconds, .25f, 10));
        while (lo < hi)
        {
            int mid = lo + (hi - lo) / 2;
            if (f[mid].Seconds < start)
                lo = mid + 1;
            else
                hi = mid;
        }
        int end = lo, upper = f.Count;
        while (end < upper)
        {
            int mid = end + (upper - end) / 2;
            if (f[mid].Seconds <= Seconds)
                end = mid + 1;
            else
                upper = mid;
        }
        int step = Math.Max(1, (end - lo + 359) / 360);
        Vector3 camera = view.transform.position, forward = view.transform.forward, up = view.transform.up;
        for (int previous = lo; previous < end - 1;)
        {
            int i = Math.Min(end - 1, previous + step), first = previous;
            previous = i;
            bool gap = false;
            for (int k = first + 1; k <= i; k++)
                if (f[k].Seconds - f[k - 1].Seconds > .25 ||
                    (f[k].Body - f[k - 1].Body).magnitude >
                        Math.Max(4, (f[k].Seconds - f[k - 1].Seconds) * 40))
                {
                    gap = true;
                    break;
                }
            if (gap)
                continue;
            float speed = ReplayMotionMetrics.Speed(f, i), before = ReplayMotionMetrics.Speed(f, first);
            if (speed < 0)
                continue;
            if (before < 0)
                before = speed;
            Vector3 a = f[first].Body + Vector3.up * .08f, b = f[i].Body + Vector3.up * .08f, side;
            if (!ReplayTrailVisual.Side(a, b, camera, up, out side))
                continue;
            float wa = ReplayTrailVisual.HalfWidth(Vector3.Dot(a - camera, forward), view.fieldOfView,
                                                   view.pixelHeight),
                  wb = ReplayTrailVisual.HalfWidth(Vector3.Dot(b - camera, forward), view.fieldOfView,
                                                   view.pixelHeight);
            Color ca = ReplayTrailVisual.SpeedColor(before), cb = ReplayTrailVisual.SpeedColor(speed);
            ca.a = (float)Math.Max(.35, (f[first].Seconds - start) / Math.Max(.25, Seconds - start));
            cb.a = (float)Math.Max(.35, (f[i].Seconds - start) / Math.Max(.25, Seconds - start));
            trail[trailCount++] =
                new TrailSegment { A = a,
                                   B = b,
                                   SideA = side * wa,
                                   SideB = side * wb,
                                   First = ca,
                                   Last = cb,
                                   Band = Mathf.Clamp((int)((before + speed) * 1.55f + .5f), 0, 31) };
        }
    }

    private void Ribbon(TrailSegment s, float scale, Color first, Color last)
    {
        var a = s.SideA * scale;
        var b = s.SideB * scale;
        GL.Color(first);
        GL.Vertex(s.A - a);
        GL.Vertex(s.A + a);
        GL.Color(last);
        GL.Vertex(s.B + b);
        GL.Color(first);
        GL.Vertex(s.A - a);
        GL.Color(last);
        GL.Vertex(s.B + b);
        GL.Vertex(s.B - b);
        // Unlit/Color has fixed back-face culling. Submit reverse winding too;
        // it cannot honor the internal shader's configurable _Cull property.
        if (!vertexColors)
        {
            GL.Color(last);
            GL.Vertex(s.B + b);
            GL.Color(first);
            GL.Vertex(s.A + a);
            GL.Vertex(s.A - a);
            GL.Color(last);
            GL.Vertex(s.B - b);
            GL.Vertex(s.B + b);
            GL.Color(first);
            GL.Vertex(s.A - a);
        }
    }

    private void DrawTrail()
    {
        if (trailCount == 0)
            return;
        var outline = new Color(.015f, .025f, .045f, .8f);
        if (Pass(outline))
        {
            GL.Begin(GL.TRIANGLES);
            try
            {
                for (int i = 0; i < trailCount; i++)
                    Ribbon(trail[i], 1.65f, outline, outline);
            }
            finally
            {
                GL.End();
            }
        }
        if (vertexColors)
        {
            if (!Pass(Color.white))
                return;
            GL.Begin(GL.TRIANGLES);
            try
            {
                for (int i = 0; i < trailCount; i++)
                    Ribbon(trail[i], 1, trail[i].First, trail[i].Last);
            }
            finally
            {
                GL.End();
            }
        }
        else
        {
            // The shipped solid-color fallback does not read vertex colors.
            // Batch into 32 gradient bands instead of making a draw call for
            // every segment. The normal path interpolates per vertex.
            for (int band = 0; band < 32; band++)
            {
                bool used = false;
                for (int i = 0; i < trailCount; i++)
                    if (trail[i].Band == band)
                    {
                        used = true;
                        break;
                    }
                if (!used)
                    continue;
                var color = ReplayTrailVisual.SpeedColor(band * 10f / 31);
                if (!Pass(color))
                    continue;
                GL.Begin(GL.TRIANGLES);
                try
                {
                    for (int i = 0; i < trailCount; i++)
                        if (trail[i].Band == band)
                            Ribbon(trail[i], 1, color, color);
                }
                finally
                {
                    GL.End();
                }
            }
        }
    }

    private void OnDestroy()
    {
        if (lines != null)
            Destroy(lines);
    }
}
