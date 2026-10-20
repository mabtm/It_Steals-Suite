#pragma once
#include "AppearancePreferences.h"
#include "ReplayPreferences.h"
#include "SuiteBranding.h"
#include "imgui.h"
#include <string>

namespace suite_ui
{
inline AppearancePreferences appearance;
inline bool fontsDirty = false;
inline ImFont *hudFont = nullptr;
inline ImFont *labelFont = nullptr, *introTitleFont = nullptr, *introBodyFont = nullptr,
              *resultTimeFont = nullptr, *resultDetailFont = nullptr;
inline float loadedHudScale = 1;
inline ReplayPreferences replayPreferences;
inline std::array<bool, kSuiteFontCount> fontAvailable = {true, true, true, true, true,
                                                          true, true, true, true, true};
inline std::array<std::string, 3> loadedFonts = {"Segoe UI", "Segoe UI", "Consolas"};

inline ImVec4 Color(AppearancePreferences::Color c, float alpha = 1)
{
    const auto &a = appearance.colors[c];
    return {a[0], a[1], a[2], alpha};
}

inline ImVec4 Blend(ImVec4 a, ImVec4 b, float t)
{
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

inline ImVec4 Alpha(ImVec4 c, float a)
{
    c.w = a;
    return c;
}

inline ImVec4 background(.048f, .063f, .074f, 1), ink(.91f, .95f, .95f, 1), heading(.37f, .83f, .77f, 1);

inline ImVec4 HudBackground(float a = 1)
{
    return Alpha(appearance.hudFollowsTheme ? background : Color(AppearancePreferences::HudBackground), a);
}

inline ImVec4 HudInk()
{
    return appearance.hudFollowsTheme ? ink : Color(AppearancePreferences::HudText);
}

inline ImVec4 HudAccent()
{
    return appearance.hudFollowsTheme ? heading : Color(AppearancePreferences::HudAccent);
}

inline ImVec4 HudMuted()
{
    return Blend(HudInk(), HudBackground(), .32f);
}

// Constant-sized palette work. Changing a color never rebuilds a font atlas.
inline void ApplyPalette(ImVec4 bg, ImVec4 text, ImVec4 header)
{
    background = bg;
    ink = text;
    heading = header;
    auto &c = ImGui::GetStyle().Colors;
    auto button = Color(AppearancePreferences::Button), fade = Color(AppearancePreferences::Animation),
         outline = Color(AppearancePreferences::Outline);
    c[ImGuiCol_WindowBg] = bg;
    c[ImGuiCol_Text] = text;
    c[ImGuiCol_TextDisabled] = Blend(text, bg, .42f);
    c[ImGuiCol_ChildBg] = {0, 0, 0, 0};
    c[ImGuiCol_PopupBg] = Alpha(Blend(bg, button, .28f), .99f);
    c[ImGuiCol_Border] = Alpha(outline, .8f);
    c[ImGuiCol_BorderShadow] = {0, 0, 0, 0};
    c[ImGuiCol_FrameBg] = Blend(bg, button, .65f);
    c[ImGuiCol_FrameBgHovered] = Blend(c[ImGuiCol_FrameBg], fade, .20f);
    c[ImGuiCol_FrameBgActive] = Blend(c[ImGuiCol_FrameBg], fade, .30f);
    c[ImGuiCol_Button] = button;
    c[ImGuiCol_ButtonHovered] = Blend(button, fade, .28f);
    c[ImGuiCol_ButtonActive] = Blend(button, fade, .42f);
    c[ImGuiCol_Header] = Blend(bg, button, .8f);
    c[ImGuiCol_HeaderHovered] = Blend(c[ImGuiCol_Header], fade, .20f);
    c[ImGuiCol_HeaderActive] = Blend(c[ImGuiCol_Header], fade, .30f);
    c[ImGuiCol_Tab] = Blend(bg, button, .35f);
    c[ImGuiCol_TabHovered] = Blend(button, fade, .24f);
    c[ImGuiCol_TabSelected] = Blend(button, fade, .13f);
    c[ImGuiCol_TabDimmed] = c[ImGuiCol_Tab];
    c[ImGuiCol_TabDimmedSelected] = c[ImGuiCol_TabSelected];
    c[ImGuiCol_TabSelectedOverline] = c[ImGuiCol_TabDimmedSelectedOverline] = fade;
    c[ImGuiCol_CheckMark] = c[ImGuiCol_SliderGrab] = c[ImGuiCol_PlotHistogram] = header;
    c[ImGuiCol_SliderGrabActive] = c[ImGuiCol_PlotHistogramHovered] = fade;
    c[ImGuiCol_Separator] = Alpha(outline, .65f);
    c[ImGuiCol_SeparatorHovered] = c[ImGuiCol_SeparatorActive] = fade;
    c[ImGuiCol_ResizeGrip] = Alpha(fade, .20f);
    c[ImGuiCol_ResizeGripHovered] = Alpha(fade, .5f);
    c[ImGuiCol_ResizeGripActive] = Alpha(fade, .75f);
    c[ImGuiCol_TableHeaderBg] = Blend(bg, button, .7f);
    c[ImGuiCol_TableBorderStrong] = outline;
    c[ImGuiCol_TableBorderLight] = Alpha(outline, .35f);
    c[ImGuiCol_TableRowBg] = {0, 0, 0, 0};
    c[ImGuiCol_TableRowBgAlt] = Alpha(fade, .035f);
    c[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
    c[ImGuiCol_ScrollbarGrab] = Alpha(outline, .7f);
    c[ImGuiCol_ScrollbarGrabHovered] = Blend(outline, fade, .25f);
    c[ImGuiCol_ScrollbarGrabActive] = fade;
    c[ImGuiCol_TextSelectedBg] = Alpha(fade, .3f);
    c[ImGuiCol_NavCursor] = fade;
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = bg;
}

inline void DrawCardEdge(ImDrawList *d, ImVec2 a, ImVec2 b, float unit)
{
    auto edge = Color(AppearancePreferences::Outline, .65f), accent = HudAccent();
    d->AddRect(a, b, ImGui::ColorConvertFloat4ToU32(edge), 10 * unit, 0, unit);
    d->AddLine({a.x + 14 * unit, a.y + unit}, {std::min(b.x - 14 * unit, a.x + 70 * unit), a.y + unit},
               ImGui::ColorConvertFloat4ToU32(Alpha(accent, .85f)), 2 * unit);
}
} // namespace suite_ui
