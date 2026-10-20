using System;
using System.Collections.Generic;

// This is the selection part of the supplied MouseLook.Start, not a replacement
// RNG. Production passes UnityEngine.Random.Range with state saved/restored.
public sealed class SpawnSelection
{
    public int Spawn;
    public int[] Crystals;

    public static SpawnSelection Select(int spawns, int points, int keep, int forcedSpawn,
                                        Func<int, int, int> range)
    {
        if (spawns < 1 || spawns > 128 || points < 1 || points > 1024 || keep < 1 || range == null)
            throw new ArgumentOutOfRangeException("Invalid spawn catalog");
        keep = Math.Min(keep, points);
        range(1, 30); // Stock fake-monster draw before choosing the player spawn.
        int spawn = range(0, spawns);
        if (forcedSpawn >= 0 && forcedSpawn < spawns)
            spawn = forcedSpawn;
        var remaining = new List<int>(points);
        // The game builds gemsList by walking the tagged Point array backwards.
        for (int i = points - 1; i >= 0; i--)
            remaining.Add(i);
        for (int i = 0; i < points - keep; i++)
            remaining.RemoveAt(range(0, remaining.Count));
        return new SpawnSelection { Spawn = spawn, Crystals = remaining.ToArray() };
    }

    public static bool Matches(SpawnSelection expected, int spawn, int[] selected)
    {
        if (expected == null || expected.Spawn != spawn || selected == null ||
            expected.Crystals.Length != selected.Length)
            return false;
        var wanted = new HashSet<int>(expected.Crystals);
        foreach (int i in selected)
            if (!wanted.Remove(i))
                return false;
        return wanted.Count == 0;
    }
}
