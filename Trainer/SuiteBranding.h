#pragma once

#define SUITE_VERSION "7.21.0"
#define SUITE_VERSION_COMPONENTS 7, 21, 0, 0
#define SUITE_MOD_VERSION "7.21.0"
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
