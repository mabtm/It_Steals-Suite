#pragma once
#include <array>
#include <algorithm>
inline constexpr std::array<int, 8> kMapScenes{3, 4, 5, 7, 8, 14, 12, 13};
inline constexpr const char *kMapLevelNames[] = {
    "Classic", "Camera",        "Hide & Seek",   "Timer",
    "Ghost",   "Classic Bonus", "Camera Bright", "Hide & Seek Outside"};
inline constexpr int kMapLevelCount = static_cast<int>(kMapScenes.size());

inline int MapLevelIndex(int scene)
{
    auto i = std::find(kMapScenes.begin(), kMapScenes.end(), scene);
    return i == kMapScenes.end() ? -1 : static_cast<int>(i - kMapScenes.begin());
}

inline bool IsHideSeekScene(int scene)
{
    return scene == 5 || scene == 13;
}

inline constexpr const char *kHideSeekWipHelp =
    "Hide & Seek and Hide & Seek Outside: map geometry and seed search are work in progress. Some passages or wall outlines can still be inaccurate; verify routes in game.";
