using System;
using System.Collections.Generic;
using UnityEngine;

// Original scene renderers/batch ranges stay intact. Physical wall hits select
// cached, renderer-owned alpha materials. Never request GPU mesh readback or
// replace level meshes; never infer walls from a shared material alone.
public sealed class ReplaySceneVisibility : IDisposable
{
    private sealed class Source
    {
        public int Id;
        public Transform Transform;
        public Bounds Bounds;
        public bool MainWall;
    }

    private sealed class Surface
    {
        public Renderer Renderer;
        public Bounds Bounds;
        public Material[] Normal, Owned;
        public Color[] Colors;
        public Material[][] Variants;
        public bool[] Walls;
        public bool Blocked;
        public float Alpha = 1;
        public int Variant;
    }

    private readonly List<Source> sources = new List<Source>();
    private readonly List<Surface> surfaces = new List<Surface>();
    private readonly Dictionary<int, List<Surface>> bindings = new Dictionary<int, List<Surface>>();
    private readonly RaycastHit[] hits = new RaycastHit[64];
    private readonly int wallLayer, propsMask, floorMask, ceilingMask;
    private readonly float floor, height, eye;
    private int queryMask;
    private double next, nextStatus;
    private bool disposed;
    private int wallCandidates;
    public int Queries { get; private set; }
    public int Hits { get; private set; }
    public int Matches { get; private set; }
    public int Fading { get; private set; }
    public int Revision { get; private set; }
    public string Status { get; private set; } = "fade W0 H0 M0 F0";
    public int Candidates
    {
        get {
            return wallCandidates;
        }
    }

    public static bool Included(int cameraMask, int layer)
    {
        return layer >= 0 && layer < 32 && (cameraMask & (1 << layer)) != 0;
    }

    private static bool Prop(Component source, int mask)
    {
        if ((mask & (1 << source.gameObject.layer)) != 0)
            return true;
        for (var t = source.transform; t != null; t = t.parent)
            if ((mask & (1 << t.gameObject.layer)) != 0 || t.CompareTag("Prop") ||
                t.CompareTag("PhysicsProp"))
                return true;
        return false;
    }

    public static bool IsWall(Component source, int layer)
    {
        if (layer >= 0 && source.gameObject.layer == layer)
            return true;
        for (var t = source.transform; t != null; t = t.parent)
            if (t.CompareTag("Wall") || t.CompareTag("Door"))
                return true;
        return false;
    }

    public static int NamedRole(string name, int fallback)
    {
        name = (name ?? "").ToLowerInvariant();
        if (name.Contains("floor") || name.Contains("ground"))
            return 1;
        if (name.Contains("ceiling") || name.Contains("cieling") || name.Contains("roof"))
            return 2;
        if (name.Contains("door"))
            return 4;
        if (name.Contains("wall"))
            return 0;
        return fallback;
    }

    public static bool IsWallVisual(Renderer renderer, int layer)
    {
        if (IsWall(renderer, layer) || NamedRole(renderer.name, 3) == 0)
            return true;
        foreach (var material in renderer.sharedMaterials)
            if (material != null && NamedRole(material.name, 3) == 0)
                return true;
        return false;
    }

    public static int FallbackRole(Renderer renderer, int layer, float cameraHeight)
    {
        var b = renderer.bounds;
        var s = b.size;
        if (s.y <= .35f && s.y <= Math.Max(.02f, Math.Min(s.x, s.z) * .12f))
            return b.center.y < cameraHeight ? 1 : 2;
        int named = NamedRole(renderer.name, 3);
        if (named != 3)
            return named;
        if (IsWallVisual(renderer, layer))
            return 0;
        return Math.Min(s.x, s.z) < s.y * .35f ? 0 : 3;
    }

    public int ProtectedRole(Renderer renderer)
    {
        if (Prop(renderer, propsMask))
            return 3;
        int bit = 1 << renderer.gameObject.layer;
        if ((bit & floorMask) != 0)
            return 1;
        if ((bit & ceilingMask) != 0)
            return 2;
        var b = renderer.bounds;
        var s = b.size;
        float hi = b.center.y + s.y * .5f;
        if (!float.IsNaN(floor) && hi <= floor + .12f)
            return 1;
        if (s.y <= .35f && s.y <= Math.Max(.02f, Math.Min(s.x, s.z) * .12f))
            return b.center.y < eye ? 1 : 2;
        return -1;
    }

    public ReplaySceneVisibility(Collider[] colliders, int wallsLayer = -1, int propLayers = 0,
                                 int floorLayers = 0, int ceilingLayers = 0, float floorY = float.NaN,
                                 float playerHeight = 2, float cameraHeight = 1.6f)
    {
        wallLayer = wallsLayer;
        propsMask = propLayers;
        floorMask = floorLayers;
        ceilingMask = ceilingLayers;
        floor = floorY;
        height = playerHeight;
        eye = cameraHeight;
        foreach (var c in colliders)
            if (c != null && c.enabled && !c.isTrigger && c.gameObject.activeInHierarchy &&
                !Prop(c, propsMask) && (floorMask & (1 << c.gameObject.layer)) == 0 &&
                (ceilingMask & (1 << c.gameObject.layer)) == 0)
            {
                var b = c.bounds;
                if (!float.IsNaN(floor) && (b.center.y + b.size.y * .5f <= floor + .12f ||
                                            b.center.y - b.size.y * .5f >= floor + height + .2f))
                    continue;
                sources.Add(new Source { Id = c.GetInstanceID(), Transform = c.transform, Bounds = b,
                                         MainWall = IsWall(c, wallLayer) });
                queryMask |= 1 << c.gameObject.layer;
            }
    }

    public void Add(Renderer renderer, Material[] normal, int[] indices, Material[] transparent,
                    Material hidden, bool mainWall = false, bool original = false)
    {
        int protect = ProtectedRole(renderer);
        bool wall = false, ceiling = false;
        for (int i = 0; i < indices.Length; i++)
        {
            int role = protect >= 0 ? protect : indices[i] / 4;
            wall |= role == 0 || role == 4;
            ceiling |= role == 2;
        }
        if (!wall && !ceiling)
            return;
        var e = new Surface { Renderer = renderer,
                              Normal = normal,
                              Bounds = renderer.bounds,
                              Walls = new bool[normal.Length],
                              Owned = new Material[normal.Length],
                              Colors = new Color[normal.Length],
                              Variants = new Material[4][] };
        surfaces.Add(e);
        if (wall)
            wallCandidates++;
        for (int i = 0; i < normal.Length; i++)
        {
            int role = protect >= 0 ? protect : indices[i] / 4;
            e.Walls[i] = role == 0 || role == 4;
            if (e.Walls[i] && (!original || normal[i] != null))
            {
                e.Owned[i] = original ? ReplayFadeMaterial.Native(normal[i])
                                      : ReplayFadeMaterial.Instance(transparent[indices[i]]);
                e.Colors[i] = e.Owned[i].color;
            }
        }
        for (int mode = 0; mode < 4; mode++)
        {
            var array = new Material[normal.Length];
            for (int i = 0; i < array.Length; i++)
            {
                int role = protect >= 0 ? protect : indices[i] / 4;
                array[i] = (mode & 1) != 0 && role == 2    ? hidden
                           : (mode & 2) != 0 && e.Walls[i] ? e.Owned[i]
                                                           : normal[i];
            }
            e.Variants[mode] = array;
        }
        e.Bounds.Expand(.06f);
        if (!wall)
            return;
        var transform = renderer.transform;
        mainWall |= IsWallVisual(renderer, wallLayer);
        foreach (var source in sources)
        {
            if (!source.Bounds.Intersects(e.Bounds))
                continue;
            var t = source.Transform;
            if (t != transform && !t.IsChildOf(transform) && !transform.IsChildOf(t) &&
                !(mainWall && source.MainWall))
                continue;
            List<Surface> list;
            if (!bindings.TryGetValue(source.Id, out list))
            {
                list = new List<Surface>();
                bindings.Add(source.Id, list);
            }
            list.Add(e);
        }
    }

    private void Cast(Vector3 from, Vector3 direction, float distance)
    {
        int n = Physics.RaycastNonAlloc(from, direction, hits, distance, queryMask,
                                        QueryTriggerInteraction.Ignore);
        Hits += n;
        for (int i = 0; i < n; i++)
        {
            var h = hits[i];
            var c = h.collider;
            float n2 = h.normal.sqrMagnitude;
            if (c == null || !(n2 >= .8f && n2 <= 1.2f) || !(Math.Abs(h.normal.y) <= .65f))
                continue;
            List<Surface> linked;
            if (!bindings.TryGetValue(c.GetInstanceID(), out linked))
                continue;
            foreach (var e in linked)
                if (e.Bounds.Contains(h.point))
                {
                    e.Blocked = true;
                    Matches++;
                }
        }
    }

    // Offscreen frames must not depend on export speed or advance the viewer's
    // fades. Preserve the exact visible state around a deterministic capture.
    // Borrow one reusable array set. Each scope remains a distinct object, so
    // nested saves and repeated Dispose calls cannot alias an active snapshot.
    private float[] savedAlpha;
    private bool[] savedBlocked;
    private int[] savedVariant;

    private sealed class SavedVisibility : IDisposable
    {
        private ReplaySceneVisibility owner;
        private readonly float[] alpha;
        private readonly bool[] blocked;
        private readonly int[] variant;
        private readonly double next, nextStatus;
        private readonly int queries, hits, matches, fading, revision;
        private readonly string status;

        public SavedVisibility(ReplaySceneVisibility value)
        {
            owner = value;
            int count = value.surfaces.Count;
            alpha = value.savedAlpha != null && value.savedAlpha.Length == count ? value.savedAlpha
                                                                                 : new float[count];
            blocked = value.savedBlocked != null && value.savedBlocked.Length == count ? value.savedBlocked
                                                                                       : new bool[count];
            variant = value.savedVariant != null && value.savedVariant.Length == count ? value.savedVariant
                                                                                       : new int[count];
            value.savedAlpha = null;
            value.savedBlocked = null;
            value.savedVariant = null;
            for (int i = 0; i < count; i++)
            {
                var e = value.surfaces[i];
                alpha[i] = e.Alpha;
                blocked[i] = e.Blocked;
                variant[i] = e.Variant;
            }
            next = value.next;
            nextStatus = value.nextStatus;
            queries = value.Queries;
            hits = value.Hits;
            matches = value.Matches;
            fading = value.Fading;
            revision = value.Revision;
            status = value.Status;
        }

        public void Dispose()
        {
            var value = owner;
            if (value == null)
                return;
            owner = null;
            if (value.disposed)
                return;
            for (int n = 0; n < alpha.Length; n++)
            {
                var e = value.surfaces[n];
                if (e.Alpha != alpha[n])
                    for (int i = 0; i < e.Owned.Length; i++)
                        if (e.Owned[i] != null)
                            ReplayFadeMaterial.Alpha(e.Owned[i], e.Colors[i], alpha[n]);
                if (e.Renderer != null && e.Variant != variant[n])
                    e.Renderer.sharedMaterials = e.Variants[variant[n]];
                e.Alpha = alpha[n];
                e.Blocked = blocked[n];
                e.Variant = variant[n];
            }
            value.next = next;
            value.nextStatus = nextStatus;
            value.Queries = queries;
            value.Hits = hits;
            value.Matches = matches;
            value.Fading = fading;
            value.Revision = revision;
            value.Status = status;
            if (value.savedAlpha == null || value.savedAlpha.Length < alpha.Length)
            {
                value.savedAlpha = alpha;
                value.savedBlocked = blocked;
                value.savedVariant = variant;
            }
        }
    }

    public IDisposable Preserve()
    {
        return new SavedVisibility(this);
    }

    public void Update(double now, float delta, Vector3 camera, Vector3 focus, uint options,
                       bool immediate = false)
    {
        if (disposed)
            return;
        bool cut = (options & Replay3DOptions.HideCeiling) != 0,
             fade = (options & (Replay3DOptions.Follow | Replay3DOptions.FadeWalls | Replay3DOptions.Free)) ==
                    (Replay3DOptions.Follow | Replay3DOptions.FadeWalls);
        if (!fade || immediate || now >= next)
        {
            foreach (var e in surfaces)
                e.Blocked = false;
            next = now + .05;
            Hits = Matches = 0;
            if (fade)
            {
                Queries++;
                Vector3 direction = focus - camera;
                float distance = direction.magnitude;
                if (distance > .01f)
                {
                    direction /= distance;
                    Cast(camera, direction, distance);
                    Cast(focus, -direction, distance);
                }
            }
        }
        float step = immediate ? 1 : Mathf.Clamp(delta, 0, .1f) * 6;
        Fading = 0;
        foreach (var e in surfaces)
        {
            if (e.Renderer == null)
                continue;
            float target = e.Blocked ? .18f : 1;
            float alpha =
                e.Alpha < target ? Math.Min(target, e.Alpha + step) : Math.Max(target, e.Alpha - step);
            int variant = (cut ? 1 : 0) | (alpha < 1 ? 2 : 0);
            if (alpha != e.Alpha)
                for (int i = 0; i < e.Owned.Length; i++)
                    if (e.Owned[i] != null)
                        ReplayFadeMaterial.Alpha(e.Owned[i], e.Colors[i], alpha);
            if (variant != e.Variant)
            {
                e.Renderer.sharedMaterials = e.Variants[variant];
                e.Variant = variant;
            }
            e.Alpha = alpha;
            if (alpha < 1)
                Fading++;
        }
        if (now >= nextStatus)
        {
            nextStatus = now + 1;
            Status =
                "fade: walls " + Candidates + ", hits " + Hits + ", matched " + Matches + ", faded " + Fading;
            Revision++;
        }
    }

    public void Dispose()
    {
        if (disposed)
            return;
        disposed = true;
        try
        {
            foreach (var e in surfaces)
                if (e.Renderer != null)
                    e.Renderer.sharedMaterials = e.Normal;
        }
        finally
        {
            foreach (var e in surfaces)
                foreach (var m in e.Owned)
                    if (m != null)
                        UnityEngine.Object.Destroy(m);
            surfaces.Clear();
            sources.Clear();
            bindings.Clear();
        }
    }
}
