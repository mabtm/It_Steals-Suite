#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include "ExperienceModel.h"
#include "SuiteAppearance.h"
#include "TrainerHotkeys.h"
#include <cstring>
#include <string>

namespace suite_ui
{
inline ExperiencePreferences preferences;
inline MotionTable motion;
inline Lifecycle lifecycle;
inline bool menu = false, run = false;
inline ImVec4 tint(.37f, .83f, .77f, 1);
inline Tone requested = Tone::None;
inline double requestedAt = -10;
inline std::string notice;
inline bool noticeError = false;
inline double noticeAt = -10, menuAt = -10;
inline ImGuiID noticeSource = 0;
inline double lifecycleTime = -1;
inline ImTextureID introBackdrop = 0;
inline bool introFrameDrawn = false;

inline double LifecycleNow()
{
    return lifecycleTime >= 0 ? lifecycleTime : ImGui::GetTime();
}

inline int lastMenu = -2, trackedFrame = -1;
inline ImGuiContext *context = nullptr;

inline float Unit()
{
    return ImGui::GetFontSize() / 18.f;
}

inline float IntroUnit()
{
    auto size = ImGui::GetIO().DisplaySize;
    return std::min(Unit(), std::min(size.x / 700.f, size.y / 340.f));
}

inline ImVec4 Mix(ImVec4 a, ImVec4 b, float t)
{
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

inline Tone gameTone = Tone::None;
inline double gameToneAt = -100;

inline void PostGameTone(Tone t)
{
    if (preferences.sounds && preferences.volume > 0)
    {
        gameTone = t;
        gameToneAt = LifecycleNow();
    }
}

inline bool GameTonePending()
{
    return gameTone != Tone::None && LifecycleNow() - gameToneAt >= 0 && LifecycleNow() - gameToneAt < .25;
}

inline Tone ConsumeGameTone(bool allowed)
{
    if (!GameTonePending())
    {
        gameTone = Tone::None;
        return Tone::None;
    }
    if (!allowed)
        return Tone::None;
    auto t = gameTone;
    gameTone = Tone::None;
    return t;
}

inline int Priority(Tone t)
{
    return t == Tone::PersonalBest           ? 6
           : t == Tone::Complete             ? 5
           : t == Tone::Confirm              ? 4
           : t == Tone::On || t == Tone::Off ? 3
           : t == Tone::Tab                  ? 2
           : t == Tone::Click                ? 1
                                             : 0;
}

inline void Request(Tone tone)
{
    if (menu && !run && preferences.sounds && preferences.volume > 0 && Priority(tone) >= Priority(requested))
    {
        requested = tone;
        requestedAt = ImGui::GetTime();
    }
}

inline Tone ConsumeTone()
{
    auto tone = ImGui::GetTime() - requestedAt <= .18 ? requested : Tone::None;
    requested = Tone::None;
    return tone;
}

inline void BeginFrame(bool interactive, bool running)
{
    auto *c = ImGui::GetCurrentContext();
    int frame = ImGui::GetFrameCount();
    if (context != c || frame < trackedFrame)
    {
        motion.Reset();
        lastMenu = -2;
        notice.clear();
        requested = Tone::None;
        context = c;
    }
    trackedFrame = frame;
    menu = interactive;
    run = running;
    if (!interactive || running)
        requested = Tone::None;
}

inline float Animate(ImGuiID id, float target, bool appear = false, float duration = .16f)
{
    return motion.Value(id, target, ImGui::GetFrameCount(), ImGui::GetTime(), preferences.motion, appear,
                        duration);
}

inline void Notify(const char *message, bool error = false, ImGuiID source = 0, Tone tone = Tone::Confirm)
{
    notice = message ? std::string(message).substr(0, 160) : "";
    noticeError = error;
    noticeAt = ImGui::GetTime();
    noticeSource = source;
    if (!error)
        Request(tone);
}

inline bool RecentSource(ImGuiID id)
{
    return id && id == noticeSource && !noticeError && ImGui::GetTime() - noticeAt < 1.15;
}

inline bool Button(const char *label, const ImVec2 &requestedSize = ImVec2(0, 0))
{
    auto *window = ImGui::GetCurrentWindow();
    auto id = window->GetID(label);
    auto &style = ImGui::GetStyle();
    auto text = ImGui::CalcTextSize(label, nullptr, true);
    ImVec2 size = ImGui::CalcItemSize(requestedSize, text.x + style.FramePadding.x * 2,
                                      text.y + style.FramePadding.y * 2);
    auto at = ImGui::GetCursorScreenPos();
    bool over = GImGui->HoveredWindow == window && !(GImGui->CurrentItemFlags & ImGuiItemFlags_Disabled) &&
                ImGui::IsMouseHoveringRect(at, ImVec2(at.x + size.x, at.y + size.y));
    float hover = Animate(id ^ 0x1c701u, over ? 1.f : 0.f);
    float press = Animate(id ^ 0xb4511u, GImGui->ActiveId == id ? 1.f : 0.f, false, .09f);
    ImVec4 normal = style.Colors[ImGuiCol_Button], lit = style.Colors[ImGuiCol_ButtonHovered];
    auto color = Mix(Mix(normal, lit, hover), style.Colors[ImGuiCol_ButtonActive], press);
    ImGui::PushStyleColor(ImGuiCol_Button, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    bool confirmed = RecentSource(id);
    if (confirmed)
    {
        auto hidden = style.Colors[ImGuiCol_Text];
        hidden.w = 0;
        ImGui::PushStyleColor(ImGuiCol_Text, hidden);
    }
    bool clicked = ImGui::Button(label, size);
    if (confirmed)
        ImGui::PopStyleColor();
    ImGui::PopStyleColor(3);
    if (confirmed && ImGui::IsItemVisible())
    {
        auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        ImGui::RenderTextClipped(ImVec2(a.x + style.FramePadding.x, a.y + style.FramePadding.y),
                                 ImVec2(b.x - style.FramePadding.x, b.y - style.FramePadding.y), "Copied",
                                 nullptr, nullptr, style.ButtonTextAlign);
    }

    if (ImGui::IsItemVisible() && (hover > .01f || ImGui::IsItemHovered() || ImGui::IsItemFocused()))
    {
        auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        ImVec4 edge = Color(AppearancePreferences::Animation);
        edge.w = (.12f + .22f * hover) * style.Alpha;
        ImGui::GetWindowDrawList()->AddRect(a, b, ImGui::ColorConvertFloat4ToU32(edge), style.FrameRounding,
                                            0, 1);
    }
    if (clicked)
        Request(Tone::Click);
    return clicked;
}

inline bool SmallButton(const char *label)
{
    auto pad = ImGui::GetStyle().FramePadding;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(pad.x, 0));
    bool pressed = Button(label);
    ImGui::PopStyleVar();
    return pressed;
}

inline bool Checkbox(const char *label, bool *value)
{
    auto *window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;
    auto id = window->GetID(label);
    auto &style = ImGui::GetStyle();
    float h = ImGui::GetFrameHeight(), w = h * 1.55f;
    auto text = ImGui::CalcTextSize(label, nullptr, true);
    auto p = ImGui::GetCursorScreenPos();
    bool clicked =
        ImGui::InvisibleButton(label, ImVec2(w + (text.x > 0 ? style.ItemInnerSpacing.x + text.x : 0), h),
                               ImGuiButtonFlags_EnableNav);
    if (clicked)
    {
        *value = !*value;
        ImGui::MarkItemEdited(id);
        Request(*value ? Tone::On : Tone::Off);
    }
    if (ImGui::IsItemVisible())
    {
        float on = Animate(id ^ 0x710901u, *value ? 1.f : 0.f),
              hover = Animate(id ^ 0x91021u, ImGui::IsItemHovered() ? 1.f : 0.f);
        auto *draw = ImGui::GetWindowDrawList();
        float inset = h * .17f, track = h - 2 * inset;
        ImVec2 a(p.x, p.y + inset), b(p.x + w, p.y + h - inset);
        auto off = Mix(style.Colors[ImGuiCol_FrameBg], style.Colors[ImGuiCol_FrameBgHovered], hover * .5f);
        auto active = heading;
        active.w = 1;
        auto background = Mix(off, active, on * .75f);
        draw->AddRectFilled(a, b, ImGui::GetColorU32(background), track * .5f);
        auto knob = Mix(style.Colors[ImGuiCol_TextDisabled], style.Colors[ImGuiCol_Text], on);
        draw->AddCircleFilled(ImVec2(a.x + track * .5f + on * (w - track), p.y + h * .5f), track * .34f,
                              ImGui::GetColorU32(knob), 16);
        if (ImGui::IsItemFocused())
            draw->AddRect(ImVec2(p.x - 2, p.y + inset - 2), ImVec2(p.x + w + 2, p.y + h - inset + 2),
                          ImGui::GetColorU32(tint), track * .5f, 0, 1);
        if (text.x > 0)
            draw->AddText(ImVec2(p.x + w + style.ItemInnerSpacing.x, p.y + style.FramePadding.y),
                          ImGui::GetColorU32(ImGuiCol_Text), label, ImGui::FindRenderedTextEnd(label));
    }
    return clicked;
}

inline bool BeginTabItem(const char *label, bool *open = nullptr, ImGuiTabItemFlags flags = 0)
{
    bool shown = ImGui::BeginTabItem(label, open, flags);
    if (!shown)
        return false;
    auto id = GImGui->LastItemData.ID;
    float progress = Animate(id ^ 0x31a1bu, 1, true);
    auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    auto color = Color(AppearancePreferences::Animation);
    color.w = ImGui::GetStyle().Alpha;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(a.x + 5, b.y - 1),
                                        ImVec2(a.x + 5 + (b.x - a.x - 10) * progress, b.y - 1),
                                        ImGui::ColorConvertFloat4ToU32(color), 2 * Unit());
    if (ImGui::IsItemActivated())
        Request(Tone::Tab);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (.55f + .45f * progress));
    return true;
}

inline void EndTabItem()
{
    ImGui::PopStyleVar();
    ImGui::EndTabItem();
}

inline bool CollapsingHeader(const char *label, ImGuiTreeNodeFlags flags = 0)
{
    bool open = ImGui::CollapsingHeader(label, flags);
    if (ImGui::IsItemToggledOpen())
        Request(Tone::Tab);
    return open;
}

// Clip the existing content while its revealed height changes. Layout and
// interaction still use ImGui's normal items; offscreen controls stay clipped.
template <class Draw> inline void Section(const char *label, ImGuiTreeNodeFlags flags, Draw drawContent)
{
    bool open = CollapsingHeader(label, flags);
    ImGuiID id = ImGui::GetItemID();
    float reveal = Animate(id ^ 0x7f011u, open ? 1.f : 0.f);

    struct Height
    {
        ImGuiID id = 0;
        float value = 0;
    };

    static std::array<Height, 128> heights{};
    auto &saved = heights[id % heights.size()];
    if (saved.id != id)
    {
        saved.id = id;
        saved.value = 0;
    }
    if (!open && reveal <= .001f)
        return;
    auto *window = ImGui::GetCurrentWindow();
    auto start = ImGui::GetCursorPos();
    auto screen = ImGui::GetCursorScreenPos();
    auto before = window->DC.CursorMaxPos;
    bool clipped = reveal < .999f;
    if (clipped)
    {
        ImGui::PushClipRect(screen, ImVec2(window->ClipRect.Max.x, screen.y + saved.value * reveal), true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (.55f + .45f * reveal));
    }
    ImGui::BeginDisabled(!open);
    ImGui::BeginGroup();
    drawContent();
    ImGui::EndGroup();
    ImGui::EndDisabled();
    float height = std::max(0.f, ImGui::GetCursorPosY() - start.y);
    saved.value = height;
    if (clipped)
    {
        ImGui::PopStyleVar();
        ImGui::PopClipRect();
        ImGui::SetCursorPosY(start.y + height * reveal);
        window->DC.CursorMaxPos.y = std::max(before.y, screen.y + height * reveal);
    }
}

inline void CopyText(const char *text)
{
    ImGui::SetClipboardText(text);
    const char *copied = ImGui::GetClipboardText();
    bool ok = text && copied && std::strcmp(copied, text) == 0;
    Notify(ok ? "Copied to clipboard" : "Clipboard unavailable", !ok, ok ? GImGui->LastItemData.ID : 0);
}

inline void Logo(ImDrawList *draw, ImVec2 origin, float size, float progress = 1, float alpha = 1,
                 const ImVec4 *routeColor = nullptr)
{
    const std::array<ImVec2, 6> nodes = {ImVec2(.12f, .78f), ImVec2(.12f, .24f), ImVec2(.45f, .24f),
                                         ImVec2(.45f, .56f), ImVec2(.85f, .56f), ImVec2(.85f, .12f)};
    auto point = [&](ImVec2 p) { return ImVec2(origin.x + p.x * size, origin.y + p.y * size); };
    float distance = std::clamp(progress, 0.f, 1.f) * 5;
    auto route = routeColor ? *routeColor : tint;
    ImU32 color = ImGui::ColorConvertFloat4ToU32(Alpha(route, alpha));
    for (size_t i = 1; i < nodes.size(); i++)
    {
        float t = std::clamp(distance - float(i - 1), 0.f, 1.f);
        if (t > 0)
        {
            auto a = point(nodes[i - 1]), b = point(nodes[i]);
            draw->AddLine(a, ImVec2(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t), color,
                          std::max(1.f, size * .038f));
        }
    }
    auto spawn = point(nodes[0]);
    draw->AddCircle(spawn, size * .065f, color, 16, std::max(1.f, size * .025f));
    for (size_t i : {size_t(2), size_t(4), size_t(5)})
        if (distance >= float(i) - .03f)
        {
            auto p = point(nodes[i]);
            float r = size * .065f;
            ImVec2 diamond[] = {{p.x, p.y - r}, {p.x + r, p.y}, {p.x, p.y + r}, {p.x - r, p.y}};
            draw->AddConvexPolyFilled(diamond, 4, color);
        }
}

inline void BrandHeader(const char *mode, bool connected, const char *status)
{
    auto p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x, u = Unit();
    ImGui::InvisibleButton("##SuiteDrag", ImVec2(width, 38 * u));
    if (ImGui::IsItemActivated() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        ImGui::StartMouseMovingWindow(ImGui::GetCurrentWindow());
    auto *draw = ImGui::GetWindowDrawList();
    Logo(draw, ImVec2(p.x + 1 * u, p.y + 1 * u), 34 * u);
    draw->AddText(ImGui::GetFont(), 17 * u, ImVec2(p.x + 47 * u, p.y), ImGui::GetColorU32(ImGuiCol_Text),
                  "PRACTICE SUITE");
    std::string sub = std::string(mode ? mode : "") + "  /  " +
                      (connected ? "Connected"
                       : status  ? status
                                 : "Waiting for game");
    auto clip = ImVec4(p.x + 47 * u, p.y + 19 * u, p.x + width, p.y + 38 * u);
    draw->AddText(ImGui::GetFont(), 14 * u, ImVec2(p.x + 47 * u, p.y + 21 * u),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), sub.c_str(), nullptr, 0, &clip);
    auto version = ImGui::CalcTextSize(suite_brand::Credit);
    if (width > 330 * u)
        draw->AddText(ImVec2(p.x + width - version.x, p.y + 1 * u), ImGui::GetColorU32(ImGuiCol_TextDisabled),
                      suite_brand::Credit);
    else
    {
        draw->AddRectFilled({p.x + 46 * u, p.y + 19 * u}, {p.x + width, p.y + 38 * u},
                            ImGui::GetColorU32(ImGuiCol_WindowBg));
        draw->AddText(ImGui::GetFont(), 11 * u, {p.x + 47 * u, p.y + 22 * u},
                      ImGui::GetColorU32(ImGuiCol_TextDisabled), suite_brand::Credit);
    }
}

inline float MenuAlpha()
{
    int frame = ImGui::GetFrameCount();
    if (frame != lastMenu)
    {
        if (frame != lastMenu + 1)
            menuAt = ImGui::GetTime();
        lastMenu = frame;
    }
    return preferences.motion ? .65f + .35f * Ease(float((ImGui::GetTime() - menuAt) / .16)) : 1;
}

inline void RenderNotice()
{
    double age = ImGui::GetTime() - noticeAt;
    if (notice.empty() || age < 0 || age > 1.7 || !menu)
        return;
    auto *window = ImGui::GetCurrentWindow();
    float u = Unit(),
          width = std::min(window->Size.x - 24 * u, ImGui::CalcTextSize(notice.c_str()).x + 30 * u);
    float fade = preferences.motion ? Ease(float(age / .12)) * Ease(float((1.7 - age) / .18)) : 1;
    float height = 32 * u;
    ImVec2 a(window->Pos.x + window->Size.x - width - 12 * u,
             window->Pos.y + window->Size.y - height - 12 * u),
        b(a.x + width, a.y + height);
    auto *draw = window->DrawList;
    draw->PushClipRect(window->Pos, ImVec2(window->Pos.x + window->Size.x, window->Pos.y + window->Size.y),
                       true);
    draw->AddRectFilled(a, b,
                        ImGui::ColorConvertFloat4ToU32(
                            Alpha(Blend(background, Color(AppearancePreferences::Button), .5f), .98f * fade)),
                        6 * u);
    auto color = noticeError ? ImVec4(1, .69f, .38f, fade) : ImVec4(tint.x, tint.y, tint.z, fade);
    draw->AddLine(ImVec2(a.x + 1, a.y + 6 * u), ImVec2(a.x + 1, b.y - 6 * u),
                  ImGui::ColorConvertFloat4ToU32(color), 2 * u);
    draw->AddText(ImVec2(a.x + 12 * u, a.y + 8 * u), ImGui::ColorConvertFloat4ToU32(Alpha(ink, fade)),
                  notice.c_str());
    draw->PopClipRect();
}

inline void Busy(const char *text)
{
    auto p = ImGui::GetCursorScreenPos();
    float u = Unit(), r = 5 * u;
    ImGui::Dummy(ImVec2(14 * u, ImGui::GetTextLineHeight()));
    auto *draw = ImGui::GetWindowDrawList();
    float phase = preferences.motion ? float(ImGui::GetTime() * 5) : 0;
    draw->PathArcTo(ImVec2(p.x + r, p.y + 7 * u), r, phase, phase + 4.4f, 16);
    draw->PathStroke(ImGui::GetColorU32(tint), 0, 1.6f * u);
    ImGui::SameLine(0, 5 * u);
    ImGui::TextDisabled("%s", text);
}

inline void RenderLifecycle(bool visible, bool running, bool ready = true)
{
    introFrameDrawn = false;
    double now = LifecycleNow();
    lifecycle.Consider(now, visible && ready, preferences, running);
    bool closing = lifecycle.Exiting();
    if (!visible || running || (!closing && (!ready || !lifecycle.Startup(now, preferences, running))))
        return;
    double age = lifecycle.StartupAge(now);
    float t = closing ? float((now - lifecycle.exit) / Lifecycle::ExitDuration)
                      : float(age / Lifecycle::StartupDuration);
    if (t >= 1)
        return;
    float fade = closing ? 1 - Ease(t)
                 : preferences.motion
                     ? Ease(float(age / .28)) * Ease(float((Lifecycle::StartupDuration - age) / .42))
                     : 1;
    float u = IntroUnit(), w = 640 * u, h = 272 * u;
    auto display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({(display.x - w) * .5f, (display.y - h) * .5f + (1 - fade) * 8 * u},
                            ImGuiCond_Always);
    ImGui::SetNextWindowSize({w, h}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 16 * u);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##SuiteLifecycle", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    introFrameDrawn = !closing && !ImGui::GetCurrentWindow()->Hidden;
    auto p = ImGui::GetWindowPos();
    ImVec2 b(p.x + w, p.y + h);
    auto *draw = ImGui::GetWindowDrawList();
    bool blurred = !closing && appearance.introBlur && introBackdrop;
    if (blurred)
        draw->AddImageRounded(introBackdrop, p, b, {0, 0}, {1, 1},
                              ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, fade)), 16 * u);
    draw->AddRectFilled(
        p, b, ImGui::ColorConvertFloat4ToU32(Alpha(background, fade * (blurred ? .78f : .98f))), 16 * u);
    auto color = Color(AppearancePreferences::Animation);
    draw->AddRect(p, b,
                  ImGui::ColorConvertFloat4ToU32(Alpha(Color(AppearancePreferences::Outline), fade * .9f)),
                  16 * u, 0, u);
    for (int i = 1; i < 5; i++)
    {
        float x = p.x + i * 27 * u;
        draw->AddLine({x, p.y + 24 * u}, {x, p.y + h - 24 * u},
                      ImGui::ColorConvertFloat4ToU32(Alpha(color, .035f * fade)), u);
    }
    float progress = closing ? 1 - Ease(t) : preferences.motion ? Ease(std::min(1.f, t * 1.7f)) : 1;
    Logo(draw, {p.x + 25 * u, p.y + 44 * u}, 114 * u, progress, fade, &color);
    if (!closing && preferences.motion)
    {
        float pulse = .5f + .5f * std::sin(float(age) * 4.f);
        draw->AddCircle({p.x + 39 * u, p.y + 133 * u}, (10 + 4 * pulse) * u,
                        ImGui::ColorConvertFloat4ToU32(Alpha(color, .12f * fade)), 24, u);
    }
    auto text = [&](float x, float y, ImFont *font, ImVec4 c, const char *value, float wrap = 0) {
        if (!font)
            font = ImGui::GetFont();
        float factor = u / Unit();
        draw->AddText(font, font->FontSize * factor, {std::floor(p.x + x * u), std::floor(p.y + y * u)},
                      ImGui::ColorConvertFloat4ToU32(Alpha(c, fade)), value, nullptr, wrap);
    };
    text(164, 30, introBodyFont, color, "IT STEALS / PRACTICE TOOLS");
    text(162, 72, introTitleFont, ink, suite_brand::Title);
    text(164, 123, introBodyFont, Blend(ink, background, .20f),
         closing ? "Until the next attempt." : "A clear route. A better run.");
    text(164, 177, introBodyFont, heading, suite_brand::Credit);
    text(164, 216, ImGui::GetFont(), Blend(ink, background, .25f),
         closing ? "SEE YOU NEXT RUN"
                 : (suite_keys::Hint(suite_keys::Menu) + "  MENU    /    " +
                    suite_keys::Hint(suite_keys::Hud) + "  COMPACT HUD")
                       .c_str(),
         w - 188 * u);
    float bar = (w - 48 * u) * progress;
    draw->AddLine({p.x + 24 * u, p.y + h - 16 * u}, {p.x + 24 * u + bar, p.y + h - 16 * u},
                  ImGui::ColorConvertFloat4ToU32(Alpha(color, .6f * fade)), 2 * u);
    ImGui::End();
    ImGui::PopStyleVar(2);
}

inline void CommitLifecycleFrame(bool visiblePresentation, double now)
{
    if (visiblePresentation && introFrameDrawn)
        lifecycle.Presented(now);
}
} // namespace suite_ui
