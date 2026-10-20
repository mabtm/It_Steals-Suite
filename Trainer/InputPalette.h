#pragma once
#include "imgui.h"
#include <algorithm>
#include <cstdint>

namespace suite_input
{
struct Palette
{
    ImU32 background = IM_COL32(12, 19, 24, 230), ink = IM_COL32(232, 244, 244, 255),
          accent = IM_COL32(95, 216, 194, 255), muted = IM_COL32(140, 164, 172, 255),
          edge = IM_COL32(57, 82, 90, 255);
};

inline ImU32 Mix(ImU32 a, ImU32 b, float t)
{
    ImU32 c = 0;
    for (int i = 0; i < 3; i++)
        c |= uint32_t(((a >> (i * 8)) & 255) * (1 - t) + ((b >> (i * 8)) & 255) * t) << (i * 8);
    return c | 0xf0000000u;
}

inline ImU32 PressedInk(ImU32 c)
{
    return .2126f * (c & 255) + .7152f * ((c >> 8) & 255) + .0722f * ((c >> 16) & 255) > 135
               ? IM_COL32(8, 20, 26, 255)
               : IM_COL32(244, 250, 250, 255);
}

inline ImU32 Fade(ImU32 c, float a)
{
    return (c & 0x00ffffffu) | (uint32_t(std::clamp(a, 0.f, 1.f) * float(c >> 24)) << 24);
}

inline void Label(ImDrawList *d, ImFont *font, float size, ImVec2 p, ImU32 color, const char *text)
{
    d->AddText(font, size, p, color, text);
}
} // namespace suite_input
