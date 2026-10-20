using System;
using System.Collections.Generic;
using UnityEngine;

// Sources are discovered by the existing player/crystal/monster catalogs.
// No scene search, per-frame allocation, file IO or additional game input read.
public static class ReplayColliderCapture
{
    private sealed class Entry
    {
        public Collider Source;
        public ReplayColliderTrack Track;
        public bool Player;
        public double Last;
    }

    private static ReplayCoverage coverage;
    private static ReplayColliderData data;
    private static readonly List<Entry> entries = new List<Entry>();
    private static readonly HashSet<int> known = new HashSet<int>();
    private static int cursor;
    private static double next;

    public static void Begin(ReplayMotionData motion, MouseLook player, PracticeMapData map)
    {
        Stop();
        data = motion.Colliders;
        data.Recorded = true;
        coverage = motion.Coverage;
        coverage.Start(ReplayCoverage.Colliders);
        try
        {
            if (player.controller != null)
                Bind(player.controller, 1, 0, true);
            foreach (var c in player.GetComponentsInChildren<Collider>(true))
                Bind(c, 1, 0, true);
            var body = PlayerContext.Body(player);
            if (body != null)
                foreach (var c in body.GetComponentsInChildren<Collider>(true))
                    Bind(c, 1, 0, true);
            if (player.gemsList != null)
                foreach (var gem in player.gemsList)
                {
                    if (gem == null)
                        continue;
                    int owner = CrystalOwner(map, gem.transform.position);
                    if (owner >= 0)
                        foreach (var c in gem.GetComponentsInChildren<Collider>(true))
                            Bind(c, 3, owner, false);
                }
            SampleAll(0);
            coverage.Observe(ReplayCoverage.Colliders, 0);
        }
        catch (Exception)
        {
            data.Drop(2);
            coverage.Fail(ReplayCoverage.Colliders);
        }
    }

    // Same selected-point / two-unit matching rule as recorded pickups.
    // A collider/prefab pivot can differ slightly from the catalog point.
    public static int CrystalOwner(PracticeMapData map, Vector3 at)
    {
        if (map == null)
            return -1;
        int owner = -1;
        float best = 4;
        foreach (int id in map.Selected)
        {
            if (id < 0 || id >= map.Points.Length)
                continue;
            float distance = (map.Points[id] - at).sqrMagnitude;
            if (distance < best)
            {
                best = distance;
                owner = id;
            }
        }
        return owner;
    }

    public static void BindActor(Component source, Transform body, int id)
    {
        if (data == null || source == null)
            return;
        try
        {
            foreach (var c in source.GetComponentsInChildren<Collider>(true))
                Bind(c, 2, id, false);
            if (body != null && body != source.transform)
                foreach (var c in body.GetComponentsInChildren<Collider>(true))
                    Bind(c, 2, id, false);
            var ghost = source as GhostScript;
            if (ghost != null && ghost.monsterKillTrigger != null)
                foreach (var c in ghost.monsterKillTrigger.GetComponentsInChildren<Collider>(true))
                    Bind(c, 2, id, false);
        }
        catch (Exception)
        {
            data.Drop(2);
            coverage.Fail(ReplayCoverage.Colliders);
        }
    }

    private static void Bind(Collider source, int group, int owner, bool player)
    {
        if (source == null || !known.Add(source.GetInstanceID()))
            return;
        int kind = ReplayColliderShape.Kind(source);
        if (kind == 0)
        {
            data.Drop(4);
            return;
        }
        if (entries.Count >= ReplayColliderData.MaxTracks)
        {
            data.Drop(1);
            return;
        }
        string name = source.name ?? "";
        while (System.Text.Encoding.UTF8.GetByteCount(name) > 128)
            name = name.Substring(0, name.Length - 1);
        var track = new ReplayColliderTrack { Group = group, Owner = owner, Kind = kind, Name = name };
        data.Tracks.Add(track);
        entries.Add(new Entry { Source = source, Track = track, Player = player, Last = -1 });
        coverage.Source(ReplayCoverage.Colliders);
    }

    private static void Sample(Entry entry, double at, bool force)
    {
        entry.Last = at;
        try
        {
            var t = entry.Track;
            ReplayColliderShape shape;
            uint state = 0;
            if (entry.Source != null)
            {
                if (!ReplayColliderShape.Capture(entry.Source, out shape))
                {
                    coverage.Fail(ReplayCoverage.Colliders);
                    return;
                }
                state = (entry.Source.enabled && entry.Source.gameObject.activeInHierarchy ? 1u : 0) |
                        (entry.Source.isTrigger ? 2u : 0);
            }
            else if (t.Samples.Count > 0)
                shape = t.Samples[t.Samples.Count - 1].Shape;
            else
            {
                coverage.Fail(ReplayCoverage.Colliders);
                return;
            }
            if (t.Samples.Count > 0)
            {
                var previous = t.Samples[t.Samples.Count - 1];
                if (at < previous.Seconds)
                    return;
                if (!force && at - previous.Seconds < .5 && previous.State == state &&
                    previous.Shape.Same(shape))
                    return;
            }
            data.Add(t, new ReplayColliderFrame { Seconds = at, State = state, Shape = shape });
            entry.Last = at;
        }
        catch (Exception)
        {
            data.Drop(2);
            coverage.Fail(ReplayCoverage.Colliders);
        }
    }

    public static void Physics(double at)
    {
        if (data == null)
            return;
        coverage.Observe(ReplayCoverage.Colliders, at);
        for (int i = 0; i < entries.Count; i++)
            if (entries[i].Player)
                Sample(entries[i], at, false);
    }

    public static void Camera(double at)
    {
        if (data == null || entries.Count == 0 || at < next)
            return;
        coverage.Observe(ReplayCoverage.Colliders, at);
        next = at + .01;
        int checkedCount = 0, sampled = 0;
        while (checkedCount++ < entries.Count && sampled < 16)
        {
            if (cursor >= entries.Count)
                cursor = 0;
            var e = entries[cursor++];
            if (e.Player || at - e.Last < .1)
                continue;
            Sample(e, at, false);
            sampled++;
        }
    }

    private static void SampleAll(double at)
    {
        for (int i = 0; i < entries.Count; i++)
            Sample(entries[i], at, true);
    }

    public static void Finish(double at)
    {
        if (data != null)
        {
            SampleAll(at);
            coverage.Observe(ReplayCoverage.Colliders, at);
            for (int i = data.Tracks.Count - 1; i >= 0; i--)
                if (data.Tracks[i].Samples.Count == 0)
                    data.Tracks.RemoveAt(i);
        }
        Stop();
    }

    public static void Stop()
    {
        entries.Clear();
        known.Clear();
        data = null;
        coverage = null;
        cursor = 0;
        next = 0;
    }
}
