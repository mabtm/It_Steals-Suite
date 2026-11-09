#pragma once

// Display names only. Stored recipe IDs and their seed calculations stay
// compatible with earlier runs, search results and replay packages.
namespace suite_seed
{
inline constexpr const char *Direct = "Run seed (direct)";
inline constexpr const char *Calculated = "Suite seed (calculated)";
inline constexpr const char *UseDirect = "Use run seed";
inline constexpr const char *Help =
    "Run seed: use a layout seed copied from Seeds & routes directly. "
    "Suite seed: the Suite calculates the layout seed from your number. "
    "The same number can give different positions with a different seed type. "
    "Share the game mode, number and type together. Both types are allowed in Set-Seed.";

inline const char *Name(int recipe)
{
    return recipe == 1 ? Direct : recipe == 2 ? Calculated : "Unknown seed type";
}
}
