#pragma once
#include "SuiteExperience.h"
#include "DesignModel.h"
#include <cstdio>

namespace suite_ui
{
inline Navigation navigation;
enum class Icon
{
    Collider,
    Map,
    Timer,
    Settings,
    Replay,
    Scan,
    Sun,
    Camera,
    Path,
    Crystal,
    Trophy,
    Lock,
    Update
};
inline constexpr std::array<const char *, PageCount> PageNames = {
    "Cheats", "Seed Map", "Speedrun", "Settings", "Replay", "Diagnostics", "Inspector", "Updates"};
inline constexpr std::array<const char *, PageCount> PageDetails = {
    "Live training tools",    "Explore layouts and routes",  "Clean runs and practice",
    "Make the suite yours",   "Review every turn",           "Connection and system health",
    "Inspect the live scene", "Releases and version history"};

inline void DrawIcon(ImDrawList *d, Icon icon, ImVec2 p, float size, ImU32 color)
{
    auto q = [&](float x, float y) { return ImVec2(p.x + x * size, p.y + y * size); };
    float line = std::max(1.f, size * .065f);
    auto leg = [&](float x, float y, float a, float b) { d->AddLine(q(x, y), q(a, b), color, line); };
    auto circle = [&](float x, float y, float r) { d->AddCircle(q(x, y), r * size, color, 20, line); };
    switch (icon)
    {
    case Icon::Update:
        circle(.5f, .5f, .37f);
        leg(.5f, .2f, .5f, .62f);
        leg(.32f, .46f, .5f, .64f);
        leg(.5f, .64f, .68f, .46f);
        leg(.31f, .78f, .69f, .78f);
        break;
    case Icon::Collider:
        leg(.2f, .32f, .5f, .15f);
        leg(.5f, .15f, .8f, .32f);
        leg(.8f, .32f, .8f, .72f);
        leg(.8f, .72f, .5f, .89f);
        leg(.5f, .89f, .2f, .72f);
        leg(.2f, .72f, .2f, .32f);
        leg(.2f, .32f, .5f, .5f);
        leg(.8f, .32f, .5f, .5f);
        leg(.5f, .5f, .5f, .89f);
        break;
    case Icon::Map:
        leg(.14f, .2f, .36f, .12f);
        leg(.36f, .12f, .65f, .24f);
        leg(.65f, .24f, .88f, .16f);
        leg(.88f, .16f, .88f, .8f);
        leg(.88f, .8f, .65f, .88f);
        leg(.65f, .88f, .36f, .76f);
        leg(.36f, .76f, .14f, .84f);
        leg(.14f, .84f, .14f, .2f);
        leg(.36f, .12f, .36f, .76f);
        leg(.65f, .24f, .65f, .88f);
        break;
    case Icon::Timer:
        circle(.5f, .55f, .32f);
        leg(.5f, .55f, .5f, .34f);
        leg(.5f, .55f, .66f, .55f);
        leg(.4f, .08f, .6f, .08f);
        leg(.5f, .08f, .5f, .21f);
        leg(.76f, .23f, .84f, .16f);
        break;
    case Icon::Settings:
        circle(.5f, .5f, .25f);
        circle(.5f, .5f, .09f);
        for (int i = 0; i < 8; i++)
        {
            float a = i * .78539816f;
            leg(.5f + .28f * std::sin(a), .5f + .28f * std::cos(a), .5f + .4f * std::sin(a),
                .5f + .4f * std::cos(a));
        }
        break;
    case Icon::Replay:
        circle(.5f, .5f, .37f);
        d->AddTriangleFilled(q(.42f, .31f), q(.42f, .69f), q(.69f, .5f), color);
        break;
    case Icon::Scan:
        leg(.1f, .33f, .1f, .12f);
        leg(.1f, .12f, .32f, .12f);
        leg(.68f, .12f, .9f, .12f);
        leg(.9f, .12f, .9f, .33f);
        leg(.1f, .67f, .1f, .88f);
        leg(.1f, .88f, .32f, .88f);
        leg(.68f, .88f, .9f, .88f);
        leg(.9f, .88f, .9f, .67f);
        leg(.12f, .57f, .3f, .57f);
        leg(.3f, .57f, .43f, .35f);
        leg(.43f, .35f, .56f, .7f);
        leg(.56f, .7f, .69f, .48f);
        leg(.69f, .48f, .88f, .48f);
        break;
    case Icon::Sun:
        circle(.5f, .5f, .19f);
        for (int i = 0; i < 8; i++)
        {
            float a = i * .78539816f;
            leg(.5f + .3f * std::sin(a), .5f + .3f * std::cos(a), .5f + .42f * std::sin(a),
                .5f + .42f * std::cos(a));
        }
        break;
    case Icon::Camera:
        d->AddRect(q(.12f, .3f), q(.73f, .75f), color, size * .08f, 0, line);
        leg(.73f, .43f, .91f, .32f);
        leg(.91f, .32f, .91f, .73f);
        leg(.91f, .73f, .73f, .62f);
        circle(.32f, .18f, .09f);
        circle(.56f, .18f, .09f);
        break;
    case Icon::Path:
        leg(.2f, .78f, .2f, .28f);
        leg(.2f, .28f, .55f, .28f);
        leg(.55f, .28f, .55f, .62f);
        leg(.55f, .62f, .84f, .62f);
        circle(.2f, .78f, .07f);
        d->AddTriangleFilled(q(.73f, .51f), q(.73f, .73f), q(.9f, .62f), color);
        break;
    case Icon::Crystal: {
        ImVec2 diamond[] = {q(.5f, .1f), q(.82f, .44f), q(.5f, .9f), q(.18f, .44f)};
        d->AddPolyline(diamond, 4, color, ImDrawFlags_Closed, line);
        leg(.18f, .44f, .82f, .44f);
        leg(.5f, .1f, .38f, .44f);
        leg(.38f, .44f, .5f, .9f);
        break;
    }
    case Icon::Trophy:
        d->AddRect(q(.3f, .15f), q(.7f, .61f), color, size * .12f, 0, line);
        leg(.5f, .62f, .5f, .84f);
        leg(.34f, .87f, .66f, .87f);
        leg(.28f, .23f, .12f, .23f);
        leg(.12f, .23f, .16f, .49f);
        leg(.16f, .49f, .31f, .55f);
        leg(.72f, .23f, .88f, .23f);
        leg(.88f, .23f, .84f, .49f);
        leg(.84f, .49f, .69f, .55f);
        break;
    case Icon::Lock:
        d->AddRect(q(.2f, .43f), q(.8f, .86f), color, size * .07f, 0, line);
        d->PathArcTo(q(.5f, .42f), size * .22f, 3.14159265f, 6.2831853f, 16);
        d->PathStroke(color, 0, line);
        leg(.5f, .6f, .5f, .72f);
        break;
    }
}

inline void Motif(ImDrawList *d, ImVec2 a, ImVec2 b, float opacity = .07f)
{
    if (b.x - a.x < 60 * Unit() || b.y - a.y < 35 * Unit())
        return;
    float u = Unit(), s = std::min(130 * u, (b.y - a.y) * .9f);
    ImVec2 p(b.x - s - 8 * u, a.y + 5 * u);
    auto c = Color(AppearancePreferences::Animation);
    Logo(d, p, s, 1, opacity * ImGui::GetStyle().Alpha, &c);
    auto line = ImGui::GetColorU32(Alpha(c, opacity * .6f));
    d->AddRect({p.x + s * .02f, p.y + s * .04f}, {p.x + s * .7f, p.y + s * .85f}, line, 0, 0, u);
    d->AddLine({p.x + s * .7f, p.y + s * .04f}, {p.x + s * .7f, p.y + s * .4f}, line, u);
}

inline void Surface(ImDrawList *d, ImVec2 a, ImVec2 b, bool accent = false)
{
    float u = Unit();
    auto button = Color(AppearancePreferences::Button), edge = Color(AppearancePreferences::Outline);
    d->AddRectFilled({a.x + 2 * u, a.y + 3 * u}, b, ImGui::GetColorU32(Alpha(background, .7f)), 9 * u);
    ImVec2 end(b.x - 2 * u, b.y - 3 * u);
    auto fill = Blend(background, button, .38f);
    d->AddRectFilled(a, end, ImGui::GetColorU32(fill), 9 * u);
    if (end.x - a.x > 16 * u && end.y - a.y > 12 * u)
    {
        auto top = ImGui::GetColorU32(Blend(fill, ink, .018f)), bottom = ImGui::GetColorU32(fill);
        d->AddRectFilledMultiColor({a.x + 7 * u, a.y + 2 * u}, {end.x - 7 * u, end.y - 2 * u}, top, top,
                                   bottom, bottom);
    }
    d->AddRect(a, end, ImGui::GetColorU32(Alpha(edge, .58f)), 9 * u, 0, u);
    d->AddLine({a.x + 10 * u, a.y + u}, {end.x - 10 * u, a.y + u},
               ImGui::GetColorU32(Alpha(accent ? heading : ink, accent ? .5f : .045f)), u);
}

inline void TextAt(ImDrawList *d, ImVec2 p, const char *text, ImVec4 color, float size, ImVec4 clip)
{
    d->AddText(ImGui::GetFont(), size, p, ImGui::GetColorU32(color), text, nullptr, 0, &clip);
}

inline void PageHeading(Page page)
{
    float u = Unit(), w = ImGui::GetContentRegionAvail().x;
    bool compact = ImGui::GetContentRegionAvail().y < 190 * u;
    float h = (compact ? 34 : 58) * u;
    auto a = ImGui::GetCursorScreenPos();
    auto *d = ImGui::GetWindowDrawList();
    Motif(d, a, {a.x + w, a.y + h});
    DrawIcon(d,
             page == Page::Updates     ? Icon::Update
             : page == Page::Inspector ? Icon::Scan
                                       : Icon(int(page)),
             {a.x, a.y + 3 * u}, 25 * u, ImGui::GetColorU32(heading));
    ImVec4 clip(a.x + 37 * u, a.y, a.x + w, a.y + h);
    TextAt(d, {a.x + 37 * u, a.y}, PageNames[int(page)], ink, 23 * u, clip);
    if (!compact)
        TextAt(d, {a.x + 37 * u, a.y + 29 * u}, PageDetails[int(page)], Blend(ink, background, .36f), 15 * u,
               clip);
    ImGui::Dummy({w, h});
}

inline void Sidebar(float height)
{
    float u = Unit();
    bool wide = ImGui::GetContentRegionAvail().x >= 128 * u;
    float width = ImGui::GetContentRegionAvail().x;
    auto origin = ImGui::GetCursorScreenPos();
    const char *hints[] = {"Training aids; unavailable during clean speedruns.",
                           "Preview layouts, choose a spawn and search seeds.",
                           "Level, practice seed and local run verification.",
                           "Appearance, HUD, minimap and system preferences.",
                           "Review recordings with events and route playback.",
                           "Scan the connection and inspect technical details.",
                           "Picker shortcut, compact object card and engine details on request.",
                           "Stable releases, automatic installation and the full version history."};
    for (int i = 0; i < PageCount; i++)
    {
        ImGui::PushID(i);
        auto a = ImGui::GetCursorScreenPos();
        float h = 50 * u;
        bool selected = navigation.Is(Page(i));
        bool click = ImGui::InvisibleButton("##Navigate", {width, h}, ImGuiButtonFlags_EnableNav);
        if (click && navigation.Select(Page(i), ImGui::GetTime()))
            Request(Tone::Tab);
        float active = Animate(ImGui::GetItemID() ^ 0x947bfu, navigation.Is(Page(i)) ? 1.f : 0.f),
              hover = Animate(ImGui::GetItemID() ^ 0x84ab1u, ImGui::IsItemHovered() ? 1.f : 0.f);
        auto *d = ImGui::GetWindowDrawList();
        if (active > 0 || hover > 0)
            d->AddRectFilled(
                a, {a.x + width, a.y + h},
                ImGui::GetColorU32(Alpha(Blend(background, Color(AppearancePreferences::Button), .8f),
                                         std::max(active, hover * .55f))),
                7 * u);
        if (active > 0)
            d->AddLine({a.x + u, a.y + 10 * u}, {a.x + u, a.y + h - 10 * u},
                       ImGui::GetColorU32(Alpha(Color(AppearancePreferences::Animation), active)), 3 * u);
        DrawIcon(d,
                 i == int(Page::Updates)     ? Icon::Update
                 : i == int(Page::Inspector) ? Icon::Scan
                                             : Icon(i),
                 {a.x + 13 * u, a.y + 13 * u}, 24 * u,
                 ImGui::GetColorU32(Blend(Blend(ink, background, .36f), heading, active)));
        if (wide)
            TextAt(d, {a.x + 48 * u, a.y + 16 * u}, PageNames[i],
                   selected ? ink : Blend(ink, background, .25f), 17 * u,
                   {a.x + 48 * u, a.y, a.x + width, a.y + h});
        if (ImGui::IsItemFocused())
            d->AddRect(a, {a.x + width, a.y + h}, ImGui::GetColorU32(heading), 7 * u, 0, u);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(PageNames[i]);
            ImGui::PushTextWrapPos(260 * u);
            ImGui::TextUnformatted(hints[i]);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    if (wide && height > 490 * u)
    {
        float y = std::max(ImGui::GetCursorScreenPos().y + 20 * u, origin.y + height - 49 * u);
        ImGui::SetCursorScreenPos({origin.x, y});
        auto *d = ImGui::GetWindowDrawList();
        d->AddLine({origin.x, y}, {origin.x + width, y},
                   ImGui::GetColorU32(Alpha(Color(AppearancePreferences::Outline), .5f)), u);
        TextAt(d, {origin.x + 7 * u, y + 12 * u}, suite_brand::Credit, Blend(ink, background, .42f), 14 * u,
               {origin.x, y, origin.x + width, y + 43 * u});
        ImGui::Dummy({width, 44 * u});
    }
}

inline bool FeatureCard(const char *label, Icon icon, bool *value, const char *detail, const char *help,
                        bool readOnly = false, const char *shortcut = nullptr)
{
    float u = Unit(), width = ImGui::GetContentRegionAvail().x;
    bool compact = width < 280 * u;
    float h = (compact ? 91 : 76) * u;
    auto a = ImGui::GetCursorScreenPos();
    auto *d = ImGui::GetWindowDrawList();
    ImGui::PushID(label);
    auto id = ImGui::GetID("##Feature");
    bool clicked = ImGui::InvisibleButton("##Feature", {width, h}, ImGuiButtonFlags_EnableNav);
    if (clicked && !readOnly)
    {
        *value = !*value;
        ImGui::MarkItemEdited(id);
        Request(*value ? Tone::On : Tone::Off);
    }
    bool disabled = (GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) != 0;
    float on = Animate(id ^ 0x23071u, *value ? 1.f : 0.f);
    Surface(d, a, {a.x + width, a.y + h}, *value);
    DrawIcon(d, disabled ? Icon::Lock : icon, {a.x + 14 * u, a.y + (compact ? 14 : 21) * u}, 29 * u,
             ImGui::GetColorU32(disabled ? Blend(ink, background, .4f) : heading));
    float reserve = readOnly ? 65 * u : 56 * u;
    ImVec4 clip(a.x + (compact ? 14 : 57) * u, a.y, a.x + width - (compact ? 12 * u : reserve), a.y + h);
    const char *name = compact && std::strcmp(label, "Actual colliders") == 0 ? "Colliders" : label;
    TextAt(d, {a.x + (compact ? 14 : 57) * u, a.y + (compact ? 57 : 14) * u}, name, ink, 18 * u, clip);
    if (!compact)
        TextAt(d, {a.x + 57 * u, a.y + 42 * u}, disabled ? "Locked for this session" : detail,
               Blend(ink, background, .35f), 14 * u, clip);
    if (readOnly)
    {
        const char *tag = shortcut ? shortcut : (*value ? "ON" : "OFF");
        auto tw = ImGui::CalcTextSize(tag).x;
        d->AddText({a.x + width - tw - 16 * u, a.y + (compact ? 18 : 27) * u}, ImGui::GetColorU32(heading),
                   tag);
    }
    else
    {
        float x = a.x + width - 48 * u, y = a.y + (compact ? 18 : 27) * u;
        d->AddRectFilled({x, y}, {x + 32 * u, y + 19 * u},
                         ImGui::GetColorU32(Blend(Color(AppearancePreferences::Button), heading, on * .7f)),
                         10 * u);
        d->AddCircleFilled({x + (9.5f + 13.f * on) * u, y + 9.5f * u}, 6 * u, ImGui::GetColorU32(ink), 16);
    }
    if (ImGui::IsItemHovered() || ImGui::IsItemFocused())
        d->AddRect(a, {a.x + width - 2 * u, a.y + h - 3 * u},
                   ImGui::GetColorU32(Alpha(Color(AppearancePreferences::Animation), .6f)), 9 * u, 0, u);
    if (help && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(340 * u);
        ImGui::TextUnformatted(help);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    ImGui::PopID();
    return clicked && !readOnly;
}

inline void EmptyState(const char *id, Icon icon, const char *title, const char *detail,
                       float requested = 176)
{
    ImGui::PushID(id);
    float u = Unit(), w = ImGui::GetContentRegionAvail().x, h = requested * u;
    auto a = ImGui::GetCursorScreenPos();
    ImGui::Dummy({w, h});
    auto *d = ImGui::GetWindowDrawList();
    Surface(d, a, {a.x + w, a.y + h});
    Motif(d, a, {a.x + w, a.y + h}, .045f);
    DrawIcon(d, icon, {a.x + 21 * u, a.y + 21 * u}, 31 * u, ImGui::GetColorU32(heading));
    ImVec4 clip(a.x + 20 * u, a.y, a.x + w - 17 * u, a.y + h - 10 * u);
    TextAt(d, {a.x + 20 * u, a.y + 70 * u}, title, ink, 20 * u, clip);
    d->AddText(ImGui::GetFont(), 15 * u, {a.x + 20 * u, a.y + 105 * u},
               ImGui::GetColorU32(Blend(ink, background, .32f)), detail, nullptr, std::max(25.f, w - 42 * u),
               &clip);
    ImGui::PopID();
}

inline void LoadingState(const char *id, const char *title, float requested = 164)
{
    ImGui::PushID(id);
    float u = Unit(), w = ImGui::GetContentRegionAvail().x, h = requested * u;
    auto a = ImGui::GetCursorScreenPos();
    ImGui::Dummy({w, h});
    auto *d = ImGui::GetWindowDrawList();
    Surface(d, a, {a.x + w, a.y + h});
    float phase = preferences.motion ? float(std::fmod(ImGui::GetTime() * .6, 1.)) : 0;
    DrawIcon(d, Icon::Scan, {a.x + 20 * u, a.y + 16 * u}, 25 * u, ImGui::GetColorU32(heading));
    TextAt(d, {a.x + 58 * u, a.y + 20 * u}, title, ink, 18 * u, {a.x, a.y, a.x + w - 12 * u, a.y + h});
    for (int i = 0; i < 3; i++)
    {
        float width = (w - 42 * u) * (i == 1 ? .64f : i == 2 ? .82f : .96f), y = a.y + (65 + 25 * i) * u;
        d->AddRectFilled({a.x + 20 * u, y}, {a.x + 20 * u + width, y + 10 * u},
                         ImGui::GetColorU32(Alpha(Color(AppearancePreferences::Outline), .22f)), 4 * u);
        if (preferences.motion)
        {
            float x = a.x + 20 * u + width * phase;
            d->AddLine({x, y + 5 * u}, {std::min(a.x + 20 * u + width, x + 30 * u), y + 5 * u},
                       ImGui::GetColorU32(Alpha(heading, .18f)), 3 * u);
        }
    }
    ImGui::PopID();
}

inline void Metric(const char *id, const char *label, const char *value, const char *foot, float width)
{
    ImGui::PushID(id);
    float u = Unit(), h = 93 * u;
    auto a = ImGui::GetCursorScreenPos();
    ImGui::Dummy({width, h});
    auto *d = ImGui::GetWindowDrawList();
    Surface(d, a, {a.x + width, a.y + h});
    ImVec4 clip(a.x + 13 * u, a.y, a.x + width - 12 * u, a.y + h);
    TextAt(d, {a.x + 13 * u, a.y + 10 * u}, label, Blend(ink, background, .35f), 14 * u, clip);
    TextAt(d, {a.x + 13 * u, a.y + 32 * u}, value, ink, 24 * u, clip);
    TextAt(d, {a.x + 13 * u, a.y + 65 * u}, foot, heading, 13 * u, clip);
    ImGui::PopID();
}
} // namespace suite_ui
