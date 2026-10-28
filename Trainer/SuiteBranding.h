#pragma once

#define SUITE_VERSION "7.20.2"
#define SUITE_VERSION_COMPONENTS 7, 20, 2, 0
#define SUITE_MOD_VERSION "7.19.0"
#define SUITE_AUTHOR "mab"

#ifndef RC_INVOKED
namespace suite_brand
{
inline constexpr char Version[] = SUITE_VERSION;
inline constexpr char ModVersion[] = SUITE_MOD_VERSION;
inline constexpr char Author[] = SUITE_AUTHOR;
inline constexpr char Credit[] = "v" SUITE_VERSION "  /  " SUITE_AUTHOR;
inline constexpr char Title[] = "PRACTICE SUITE";
} // namespace suite_brand
#endif
