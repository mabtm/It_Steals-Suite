#pragma once
#include "SuiteExperience.h"

inline void AppearanceColor(const char *label, float *rgb, const float *defaults)
{
    ImGui::PushID(label);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableNextColumn();
    ImGui::SetNextItemWidth(-1);
    ImGui::ColorEdit3("##color", rgb, ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_NoLabel);
    Help(
        "Click the swatch to pick a color, or enter a hex code. Right-click to reset this color. Changes are live and saved automatically.");
    if (ImGui::BeginPopupContextItem("Reset color"))
    {
        if (ImGui::MenuItem("Reset this color"))
            std::copy(defaults, defaults + 3, rgb);
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

inline void AppearanceColor(const char *label, AppearancePreferences::Color c)
{
    AppearancePreferences defaults;
    AppearanceColor(label, suite_ui::appearance.colors[c].data(), defaults.colors[c].data());
}

inline bool AppearanceTable(const char *id)
{
    bool open = ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp);
    if (open)
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, .43f);
        ImGui::TableSetupColumn("Color", ImGuiTableColumnFlags_WidthStretch, .57f);
    }
    return open;
}

inline void AppearanceFont(const char *label, int &selected, int slot)
{
    ImGui::SetNextItemWidth(std::min(220 * g_dpiScale, ImGui::GetContentRegionAvail().x * .55f));
    if (ImGui::BeginCombo(label, kSuiteFonts[selected]))
    {
        for (int f = 0; f < kSuiteFontCount; f++)
        {
            ImGui::PushID(f);
            ImGui::BeginDisabled(!suite_ui::fontAvailable[f]);
            if (ImGui::Selectable(kSuiteFonts[f], selected == f))
            {
                selected = f;
                suite_ui::fontsDirty = true;
            }
            ImGui::EndDisabled();
            if (!suite_ui::fontAvailable[f])
                Help("This font is not installed on this PC.");
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    std::string tip =
        "Loaded: " + suite_ui::loadedFonts[slot] +
        ". Fonts are loaded from Windows; unavailable selections fall back to Segoe UI, then the built-in font. Only selected font families are loaded at the sizes needed for sharp text.";
    Help(tip.c_str());
}

inline void AppearancePreview()
{
    float u = g_dpiScale;
    ImGui::BeginChild("Style preview", {0, 100 * u}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    ImGui::TextColored(g_headerCol, "MAKE IT YOURS");
    ImGui::TextUnformatted("A clear route. Your own style.");
    suite_ui::Button("Hover to preview");
    Help("Live button, text, outline and hover colors. This sample has no game action.");
    ImGui::EndChild();
}

inline void RenderAppearanceSettings()
{
    AppearancePreview();
    if (AppearanceTable("Interface palette"))
    {
        TrainerPreferences defaults;
        AppearanceColor("Background", &g_mainBgCol.x, defaults.background);
        AppearanceColor("Text", &g_textCol.x, defaults.text);
        AppearanceColor("Headings", &g_headerCol.x, defaults.header);
        AppearanceColor("Highlights", &g_accentCol.x, defaults.accent);
        AppearanceColor("Buttons", AppearancePreferences::Button);
        AppearanceColor("Hover / animation", AppearancePreferences::Animation);
        AppearanceColor("Outlines", AppearancePreferences::Outline);
        ImGui::EndTable();
    }
    AppearancePreferences::RGB bg{g_mainBgCol.x, g_mainBgCol.y, g_mainBgCol.z},
        fg{g_textCol.x, g_textCol.y, g_textCol.z};
    if (AppearancePreferences::Contrast(bg, fg) < 4.5f)
    {
        ImGui::TextColored({1, .69f, .38f, 1}, "Low text contrast");
        Help(
            "Your colors are kept. This shortcut chooses a light or dark text color to make the menu easier to read.");
        auto b = suite_ui::appearance.colors[AppearancePreferences::Button];
        ImGui::PushStyleColor(ImGuiCol_Text, AppearancePreferences::Luminance(b) > .18f ? ImVec4(0, 0, 0, 1)
                                                                                        : ImVec4(1, 1, 1, 1));
        if (suite_ui::SmallButton("Improve text contrast"))
            g_textCol = AppearancePreferences::Luminance(bg) > .18f ? ImVec4(0, 0, 0, 1) : ImVec4(1, 1, 1, 1);
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();
    ImGui::TextColored(g_headerCol, "Typography");
    AppearanceFont("Interface font", suite_ui::appearance.menuFont, 0);
    AppearanceFont("HUD font", suite_ui::appearance.hudFont, 1);
    AppearanceFont("Timer font", suite_ui::appearance.timerFont, 2);
    if (suite_ui::SmallButton("Restore original style"))
    {
        TrainerPreferences defaults;
        g_mainBgCol = {defaults.background[0], defaults.background[1], defaults.background[2], 1};
        g_textCol = {defaults.text[0], defaults.text[1], defaults.text[2], 1};
        g_headerCol = {defaults.header[0], defaults.header[1], defaults.header[2], 1};
        g_accentCol = {defaults.accent[0], defaults.accent[1], defaults.accent[2], 1};
        auto &a = suite_ui::appearance;
        for (auto c : {AppearancePreferences::Button, AppearancePreferences::Animation,
                       AppearancePreferences::Outline})
            a.colors[c] = defaults.appearance.colors[c];
        a.menuFont = 0;
        a.hudFont = 0;
        a.timerFont = 8;
        suite_ui::fontsDirty = true;
    }
    Help(
        "Restore the interface palette and fonts. HUD layouts, map settings, sound and custom HUD/map colors are kept.");
}

inline void RenderHudColors()
{
    suite_ui::Checkbox("Use interface colors", &suite_ui::appearance.hudFollowsTheme);
    Help(
        "Both HUDs follow the interface background, text, headings and outlines. Turn off to give the HUDs their own colors.");
    if (!suite_ui::appearance.hudFollowsTheme && AppearanceTable("HUD palette"))
    {
        AppearanceColor("Background", AppearancePreferences::HudBackground);
        AppearanceColor("Text", AppearancePreferences::HudText);
        AppearanceColor("Accent", AppearancePreferences::HudAccent);
        ImGui::EndTable();
    }
}

inline void RenderHudPreview(const OverlayLayout &layout, bool speedrun)
{
    ImGui::TextDisabled(g_pShared ? "Style preview / live values" : "Style preview / example values");
    GameData sample{};
    const GameData *data = g_pShared;
    if (!data)
    {
        sample.gameFps = 144;
        sample.speedrunModeActive = speedrun;
        sample.timerState = 1;
        sample.currentRunTimeSeconds = 84.386;
        sample.personalBestSeconds = 81.094;
        sample.totalRuns = 128;
        sample.totalFinishedRuns = 32;
        sample.totalResets = 96;
        sample.runProgress = .5f;
        sample.totalObjectives = 24;
        sample.objectives = 12;
        sample.seedKnown = sample.seedKind = 1;
        sample.runSeed = 2112625;
        data = &sample;
    }
    int level = MapLevelIndex(data->currentSceneIndex);
    RenderOverlayPanel(speedrun ? "Speedrun style preview" : "Compact style preview", *data, layout, speedrun,
                       level >= 0 ? kMapLevelNames[level] : "Classic", 0, false, true);
}

inline void RenderMinimapColors()
{
    if (AppearanceTable("Minimap palette"))
    {
        AppearanceColor("Background", AppearancePreferences::MapBackground);
        AppearanceColor("Walls", AppearancePreferences::MapWalls);
        AppearanceColor("Player", AppearancePreferences::MapPlayer);
        AppearanceColor("Optimal route", AppearancePreferences::MapRoute);
        AppearanceColor("Crystals", AppearancePreferences::MapCrystal);
        ImGui::EndTable();
    }
}
