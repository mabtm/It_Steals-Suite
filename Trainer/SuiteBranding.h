#pragma once

#define SUITE_VERSION "7.22.0"
#define SUITE_VERSION_COMPONENTS 7, 22, 0, 0
#define SUITE_MOD_VERSION "7.22.0"
#define SUITE_AUTHOR "mab"
#define SUITE_PRODUCT_NAME "Elite-Suite"
#define SUITE_PREVIEW 1
#if SUITE_PREVIEW
#define SUITE_DISPLAY_VERSION SUITE_VERSION " Preview"
#else
#define SUITE_DISPLAY_VERSION SUITE_VERSION
#endif

#ifndef RC_INVOKED
namespace suite_brand
{
inline constexpr char Version[] = SUITE_VERSION;
inline constexpr char ModVersion[] = SUITE_MOD_VERSION;
inline constexpr char Author[] = SUITE_AUTHOR;
inline constexpr bool Preview = SUITE_PREVIEW != 0;
inline constexpr char DisplayVersion[] = SUITE_DISPLAY_VERSION;
inline constexpr char Credit[] = "v" SUITE_DISPLAY_VERSION "  /  " SUITE_AUTHOR;
inline constexpr char Name[] = SUITE_PRODUCT_NAME;
inline constexpr char Title[] = "ELITE-SUITE";
} // namespace suite_brand
#endif
