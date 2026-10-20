using System;

// Reuses the stock reversed Point array. Every deletion and Range call is the
// same as the game; only scratch allocations and List bookkeeping disappear.
public sealed class SpawnSelectionGenerator
{
    private readonly int spawns, points, keep;
    private readonly int[] remaining;

    public SpawnSelectionGenerator(int spawnCount, int pointCount, int crystalCount)
    {
        if (spawnCount < 1 || spawnCount > 128 || pointCount < 1 || pointCount > 1024 || crystalCount < 1 ||
            crystalCount > 64)
            throw new ArgumentOutOfRangeException("Invalid spawn catalog");
        spawns = spawnCount;
        points = pointCount;
        keep = Math.Min(crystalCount, points);
        remaining = new int[points];
    }

    public SpawnSelection Next(int forcedSpawn, Func<int, int, int> range)
    {
        var selected = new int[keep];
        int spawn = NextInto(forcedSpawn, range, selected);
        return new SpawnSelection { Spawn = spawn, Crystals = selected };
    }

    // Search reuses one output array and writes directly to its packet buffer.
    public int NextInto(int forcedSpawn, Func<int, int, int> range, int[] selected)
    {
        if (range == null)
            throw new ArgumentNullException("range");
        if (selected == null || selected.Length != keep)
            throw new ArgumentException("Invalid selection buffer");
        range(1, 30);
        int spawn = range(0, spawns);
        if (forcedSpawn >= 0 && forcedSpawn < spawns)
            spawn = forcedSpawn;
        for (int i = 0; i < points; i++)
            remaining[i] = points - 1 - i;
        for (int count = points; count > keep; count--)
        {
            int index = range(0, count);
            if (index < 0 || index >= count)
                throw new InvalidOperationException("Invalid RNG result");
            if (index < count - 1)
                Array.Copy(remaining, index + 1, remaining, index, count - index - 1);
        }
        Array.Copy(remaining, selected, keep);
        return spawn;
    }
}
