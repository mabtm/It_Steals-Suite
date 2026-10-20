#pragma once
#include "HudEditorModel.h"
#include "InputUI.h"
#include <optional>

namespace suite_hud
{
struct Snapshot
{
    OverlayLayout compact{false}, speedrun{true};
    suite_input::Preferences input;
    AppearancePreferences appearance;

    std::string Key() const
    {
        std::ostringstream s;
        compact.Encode(s, "c.");
        speedrun.Encode(s, "s.");
        input.Encode(s);
        appearance.Encode(s);
        return s.str();
    }
};

inline bool light = false, snapping = true;
inline int selected = 0, drag = -1;
inline bool resizing = false;
inline Snapshot draft, stable;
inline std::optional<Snapshot> pending;
inline std::vector<Snapshot> undo;
inline ImVec2 dragStart;
inline Rect original;
inline float originalScale = 1;

inline void Open()
{
    if (g_pShared && g_pShared->timerState == 1)
        return;
    draft.compact = g_hudLayout;
    draft.speedrun = g_speedrunLayout;
    draft.input = suite_input::preferences;
    draft.appearance = suite_ui::appearance;
    stable = draft;
    undo.clear();
    pending.reset();
    selected = 0;
    drag = -1;
    active = true;
    g_overlayVisible = true;
    g_compactMode = false;
    SetOverlayInteractive(true);
}

inline void Close(bool save)
{
    if (save)
    {
        g_hudLayout = draft.compact;
        g_speedrunLayout = draft.speedrun;
        suite_input::preferences = draft.input;
        suite_ui::appearance = draft.appearance;
        suite_ui::fontsDirty = true;
        SavePreferencesNow();
    }
    active = false;
    drag = -1;
    canvas.active = false;
    pending.reset();
    SetOverlayInteractive(g_overlayVisible && !g_compactMode);
}

inline void Render()
{
    if (!active)
        return;
    if (g_pShared && g_pShared->timerState == 1)
    {
        Close(false);
        return;
    }
    auto display = ImGui::GetIO().DisplaySize;
    float unit = std::clamp(g_dpiScale, .75f, 2.f), sidebar = std::min(350 * unit, display.x * .38f);
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(display, ImGuiCond_Always);
    ImGui::Begin("HUD workspace", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::TextColored(g_headerCol, "HUD EDITOR");
    SameLineIfFits(suite_brand::Credit);
    ImGui::TextDisabled("%s", suite_brand::Credit);
    SameLineIfFits("Apply layout");
    if (suite_ui::Button("Apply layout"))
    {
        ImGui::End();
        Close(true);
        return;
    }
    SameLineIfFits("Cancel");
    if (suite_ui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        ImGui::End();
        Close(false);
        return;
    }
    SameLineIfFits("Undo");
    ImGui::BeginDisabled(undo.empty() && !pending);
    if (suite_ui::Button("Undo"))
    {
        if (pending)
        {
            draft = *pending;
            pending.reset();
        }
        else if (!undo.empty())
        {
            draft = undo.back();
            undo.pop_back();
        }
        stable = draft;
    }
    ImGui::EndDisabled();
    SameLineIfFits("Snap");
    suite_ui::Checkbox("Snap", &snapping);
    SameLineIfFits("Light preview");
    suite_ui::Checkbox("Light preview", &light);
    float top = ImGui::GetCursorPosY() + 12 * unit;
    float availableW = std::max(80.f, display.x - sidebar - 36 * unit),
          availableH = std::max(80.f, display.y - top - 24 * unit),
          fit = std::min(availableW / display.x, availableH / display.y);
    canvas = {true, 12 * unit, top, display.x * fit, display.y * fit, fit};
    auto lo = ImVec2(canvas.x, canvas.y), hi = ImVec2(canvas.x + canvas.w, canvas.y + canvas.h);
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(lo, hi, light ? IM_COL32(205, 215, 211, 255) : IM_COL32(10, 16, 22, 255), 5 * unit);
    for (float x = lo.x; x < hi.x; x += 48 * fit)
        draw->AddLine({x, lo.y}, {x, hi.y}, light ? IM_COL32(150, 167, 165, 70) : IM_COL32(66, 89, 99, 50));
    for (float y = lo.y; y < hi.y; y += 48 * fit)
        draw->AddLine({lo.x, y}, {hi.x, y}, light ? IM_COL32(150, 167, 165, 70) : IM_COL32(66, 89, 99, 50));
    ImGui::End();
    auto appearance = suite_ui::appearance;
    suite_ui::appearance = draft.appearance;
    GameData none{};
    const auto &data = g_pShared ? *g_pShared : none;
    RenderOverlayPanel("Editor / Compact", data, draft.compact, false, "Compact HUD",
                       g_pShared ? SnapshotAge() : 1);
    RenderOverlayPanel("Editor / Speedrun", data, draft.speedrun, true, "Speedrun HUD",
                       g_pShared ? SnapshotAge() : 1);
    auto base = suite_input::Size(draft.input);
    float scale = std::min(draft.input.scale * g_dpiScale * fit,
                           std::min((canvas.w - 24 * fit) / base.x, (canvas.h - 24 * fit) / base.y));
    ImVec2 size(base.x * scale, base.y * scale);
    auto &r = bounds[2];
    r = {canvas.x + 12 * fit + (canvas.w - 24 * fit - size.x) * draft.input.x,
         canvas.y + 12 * fit + (canvas.h - 24 * fit - size.y) * draft.input.y, size.x, size.y};
    auto *foreground = ImGui::GetForegroundDrawList();
    auto f = suite_input::Demo();
    suite_input::Draw(foreground, {r.x, r.y}, {r.x + r.w, r.y + r.h}, &f, draft.input, {}, 0,
                      suite_ui::hudFont ? suite_ui::hudFont : ImGui::GetFont(),
                      suite_input::Colors(draft.input.opacity));
    auto mouse = ImGui::GetIO().MousePos;
    bool inside = mouse.x >= lo.x && mouse.y >= lo.y && mouse.x <= hi.x && mouse.y <= hi.y;
    if (ImGui::IsMouseClicked(0) && inside)
    {
        for (int i = 2; i >= 0; i--)
            if (bounds[i].Contains(mouse.x, mouse.y))
            {
                selected = drag = i;
                original = bounds[i];
                dragStart = mouse;
                resizing = mouse.x > original.x + original.w - 18 * unit &&
                           mouse.y > original.y + original.h - 18 * unit;
                originalScale = i == 2   ? draft.input.scale
                                : i == 0 ? draft.compact.scale
                                         : draft.speedrun.scale;
                break;
            }
    }
    if (drag >= 0 && ImGui::IsMouseDown(0))
    {
        float dx = mouse.x - dragStart.x, dy = mouse.y - dragStart.y;
        if (resizing)
        {
            float factor = std::max((original.w + dx) / std::max(1.f, original.w),
                                    (original.h + dy) / std::max(1.f, original.h));
            if (drag == 2)
                draft.input.scale = std::clamp(originalScale * factor, .5f, 2.5f);
            else
                (drag == 0 ? draft.compact : draft.speedrun).scale =
                    std::clamp(originalScale * factor, .85f, 1.75f);
        }
        else
        {
            Rect local = original;
            local.x -= canvas.x;
            local.y -= canvas.y;
            auto moved = Move(local, dx, dy, canvas.w, canvas.h, 12 * fit, snapping && !ImGui::GetIO().KeyAlt,
                              9 * unit);
            float x = Fraction(moved.x, canvas.w, moved.w, 12 * fit),
                  y = Fraction(moved.y, canvas.h, moved.h, 12 * fit);
            if (drag == 2)
            {
                draft.input.x = x;
                draft.input.y = y;
            }
            else
            {
                auto &layout = drag == 0 ? draft.compact : draft.speedrun;
                layout.anchor = 4;
                layout.x = x;
                layout.y = y;
            }
        }
    }
    if (ImGui::IsMouseReleased(0))
        drag = -1;
    for (int i = 0; i < 3; i++)
    {
        const auto &b = bounds[i];
        auto color = i == selected ? IM_COL32(98, 232, 205, 255) : IM_COL32(120, 150, 160, 150);
        foreground->AddRect({b.x - 3, b.y - 3}, {b.x + b.w + 3, b.y + b.h + 3}, color, 7, 0,
                            i == selected ? 2.f : 1.f);
        if (i == selected)
        {
            foreground->AddRectFilled({b.x + b.w - 12, b.y + b.h - 12}, {b.x + b.w, b.y + b.h}, color, 2);
            foreground->AddLine({b.x + b.w * .5f, lo.y}, {b.x + b.w * .5f, hi.y}, IM_COL32(98, 232, 205, 45));
            foreground->AddLine({lo.x, b.y + b.h * .5f}, {hi.x, b.y + b.h * .5f}, IM_COL32(98, 232, 205, 45));
        }
    }
    canvas.active = false;
    ImGui::SetNextWindowPos({display.x - sidebar - 8 * unit, top}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({sidebar, display.y - top - 12 * unit}, ImGuiCond_Always);
    ImGui::Begin("HUD properties", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
    const char *names[] = {"Compact HUD", "Speedrun HUD", "Input HUD"};
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##Selected HUD", &selected, names, 3);
    ImGui::TextWrapped("Drag a panel. Drag its corner to resize. Alt temporarily disables snapping.");
    if (selected == 2)
        suite_input::Settings(draft.input);
    else
        RenderOverlaySettings(selected == 0 ? draft.compact : draft.speedrun, selected == 1, true);
    if (suite_ui::CollapsingHeader("Colors / typeface"))
    {
        RenderHudColors();
        int before = suite_ui::appearance.hudFont;
        ImGui::TextUnformatted("HUD font");
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##HUD font", &suite_ui::appearance.hudFont, kSuiteFonts, kSuiteFontCount);
        if (before != suite_ui::appearance.hudFont)
            ImGui::TextDisabled("Typeface applies when the layout is saved.");
    }
    if (suite_ui::Button("Reset selected"))
    {
        if (selected == 0)
            draft.compact = OverlayLayout(false);
        else if (selected == 1)
            draft.speedrun = OverlayLayout(true);
        else
            draft.input = suite_input::Preferences();
    }
    ImGui::TextWrapped("Preview uses current values. Input keys are demo data.");
    ImGui::End();
    draft.appearance = suite_ui::appearance;
    suite_ui::appearance = appearance;
    draft.compact.Validate();
    draft.speedrun.Validate();
    draft.input.Validate();
    draft.appearance.Validate();
    if (draft.Key() != stable.Key() && !pending)
        pending = stable;
    if (pending && drag < 0 && !ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(0))
    {
        if (undo.size() >= 32)
            undo.erase(undo.begin());
        undo.push_back(*pending);
        pending.reset();
        stable = draft;
    }
}
} // namespace suite_hud
