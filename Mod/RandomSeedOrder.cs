// Identical sampling permutation to the native trainer. The key comes from
// independent OS randomness; Unity still creates the actual game layouts.
public sealed class RandomSeedOrder
{
    private readonly uint[] keys = new uint[8];

    public RandomSeedOrder(ulong key)
    {
        unchecked
        {
            uint low = (uint)key, high = (uint)(key >> 32);
            for (int i = 0; i < keys.Length; i++)
            {
                int shift = 7 + i;
                keys[i] =
                    Mix(low ^ ((high << shift) | (high >> (32 - shift))) ^ (0x9e3779b9u * (uint)(i + 1)));
            }
        }
    }

    private static uint Mix(uint value)
    {
        unchecked
        {
            value = (value ^ (value >> 16)) * 0x7feb352du;
            value = (value ^ (value >> 15)) * 0x846ca68bu;
            return value ^ (value >> 16);
        }
    }

    public int SeedAt(uint index)
    {
        uint left = index >> 16, right = index & 0xffffu;
        foreach (uint key in keys)
        {
            uint next = left ^ (Mix(right ^ key) & 0xffffu);
            left = right;
            right = next;
        }
        return unchecked((int)((left << 16) | right));
    }
}
