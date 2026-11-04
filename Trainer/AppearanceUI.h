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
    auto &id = suite_ui::appearance.fontIds[slot];
    const auto *face = suite_fonts::catalog.Find(id);
    std::string preview = id.empty() ? kSuiteFonts[std::clamp(selected, 0, kSuiteFontCount - 1)]
                                    : face ? face->name : "Unavailable font (using fallback)";
    ImGui::PushID(label);
    ImGui::SetNextItemWidth(std::min(320 * g_dpiScale, ImGui::GetContentRegionAvail().x * .65f));
    if (ImGui::BeginCombo(label, preview.c_str(), ImGuiComboFlags_HeightLarge))
    {
        static std::array<std::array<char, 128>, 3> queries{};
        auto &query = queries[slot];
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##FontSearch", "Search installed fonts...", query.data(), query.size());
        ImGui::BeginChild("Font list", {0, 260 * g_dpiScale});
        for (int f = 0; f < kSuiteFontCount; ++f)
        {
            if (suite_fonts::Fold(kSuiteFonts[f]).find(suite_fonts::Fold(query.data())) == std::string::npos)
                continue;
            ImGui::PushID(f);
            ImGui::BeginDisabled(!suite_ui::fontAvailable[f]);
            if (ImGui::Selectable(kSuiteFonts[f], id.empty() && selected == f))
            {
                selected = f;
                id.clear();
                suite_ui::fontsDirty = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        for (const auto &entry : suite_fonts::catalog.faces)
        {
            if (!suite_fonts::Matches(entry, query.data()))
                continue;
            ImGui::PushID(entry.id.c_str());
            auto name = entry.name + " (" + entry.source + ")";
            if (ImGui::Selectable(name.c_str(), id == entry.id))
            {
                id = entry.id;
                suite_ui::fontsDirty = true;
                ImGui::CloseCurrentPopup();
            }
            Help(suite_fonts::Utf8(entry.path.filename()).c_str());
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::EndCombo();
    }
    std::string tip = "Loaded: " + suite_ui::loadedFonts[slot] +
        ". Windows system fonts, per-user fonts and the Fonts folder beside the trainer are available. "
        "Unsupported files are omitted. Missing selections fall back to the original font.";
    Help(tip.c_str());
    ImGui::PopID();
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
    if (suite_ui::SmallButton("Refresh font list"))
    {
        suite_fonts::RefreshInstalled();
        suite_ui::fontsDirty = true;
    }
    Help("Rescan Windows fonts and the Fonts folder beside the trainer after installing or copying a font. TrueType (.ttf), OpenType (.otf) and collection (.ttc) faces are supported.");
    ImGui::TextDisabled("%zu installed font faces", suite_fonts::catalog.faces.size());
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
        a.fontIds = {};
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
