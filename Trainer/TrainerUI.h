#pragma once
#include "SuiteExperience.h"
#include "SuiteDesign.h"
#include "SuiteUpdateUI.h"

// Application UI. Game-window handling, rendering resources and preferences
// are separate components; none of these controls accesses mapped memory.

void ApplyTheme(float scale)
{
    g_dpiScale = std::clamp(scale, .75f, 4.f);
    ImGuiStyle style;
    ImGui::GetStyle() = style;
    ImGui::StyleColorsDark();
    auto &current = ImGui::GetStyle();
    current.ItemInnerSpacing = ImVec2(8, 6);
    current.WindowBorderSize = 1;
    current.WindowPadding = ImVec2(16, 14);
    current.FramePadding = ImVec2(10, 6);
    current.ItemSpacing = ImVec2(10, 9);
    current.WindowRounding = 12;
    current.ChildRounding = 8;
    current.FrameRounding = 6;
    current.TabRounding = 5;
    current.GrabRounding = 4;
    current.PopupRounding = 8;
    current.ScrollbarRounding = 8;
    current.ScrollbarSize = 10;
    suite_ui::ApplyPalette(g_mainBgCol, g_textCol, g_headerCol);
    current.ScaleAllSizes(g_dpiScale);
    ImGuiIO &io = ImGui::GetIO();
    io.Fonts->Clear();
    io.FontDefault = nullptr;
    io.FontDefault =
        LoadUiFont(18 * g_dpiScale, suite_ui::appearance.menuFont, suite_ui::appearance.fontIds[0]);
    suite_ui::loadedFonts[0] = g_uiFontName;
    suite_ui::loadedHudScale =
        g_speedrunMode && !(g_practiceMode && g_compactMode) ? g_speedrunLayout.scale : g_hudLayout.scale;
    suite_ui::hudFont = LoadUiFont(19 * g_dpiScale * suite_ui::loadedHudScale,
                                 suite_ui::appearance.hudFont, suite_ui::appearance.fontIds[1]);
    suite_ui::loadedFonts[1] = g_uiFontName;
    g_timerFont = LoadUiFont(44 * g_dpiScale * suite_ui::loadedHudScale,
                             suite_ui::appearance.timerFont, suite_ui::appearance.fontIds[2]);
    suite_ui::loadedFonts[2] = g_uiFontName;
    suite_ui::labelFont =
        LoadUiFont(16 * g_dpiScale * suite_ui::loadedHudScale, suite_ui::appearance.hudFont,
                   suite_ui::appearance.fontIds[1]);
    suite_ui::introTitleFont = LoadUiFont(32 * g_dpiScale, suite_ui::appearance.menuFont, suite_ui::appearance.fontIds[0]);
    suite_ui::introBodyFont = LoadUiFont(20 * g_dpiScale, suite_ui::appearance.menuFont, suite_ui::appearance.fontIds[0]);
    suite_ui::resultTimeFont = LoadUiFont(56 * g_dpiScale, suite_ui::appearance.timerFont, suite_ui::appearance.fontIds[2]);
    suite_ui::resultDetailFont = LoadUiFont(16 * g_dpiScale, suite_ui::appearance.menuFont, suite_ui::appearance.fontIds[0]);
    suite_ui::fontsDirty = false;
    InvalidateFontTexture();
}

inline void Help(const char *text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort | ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(std::min(ImGui::GetFontSize() * 30, ImGui::GetIO().DisplaySize.x - 48));
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

inline void SameLineIfFits(const char *label)
{
    float right = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    float needed =
        ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x;
    if (ImGui::GetItemRectMax().x + needed < right)
        ImGui::SameLine();
}

#include "OverlayUI.h"
#include "AppearanceUI.h"
#include "InputUI.h"
#include "HudEditor.h"
#include "OverlayPolicy.h"
#include "SeedMapUI.h"
#include "RunReviewUI.h"
#include "InspectorUI.h"

inline void RenderTimerOverlay(const std::string &mode)
{
    if (suite_hud::active || suite_clip::open)
        return;
    if (g_pShared && g_pShared->replay3dState >= 1 && g_pShared->replay3dState <= 3)
    {
        RenderReplay3DOverlay();
        return;
    }
    if (SpeedrunHudAllowed(g_speedrunMode, g_practiceMode))
    {
        const GameData disconnected{};
        bool compactPractice = g_practiceMode && g_compactMode;
        RenderOverlayPanel(compactPractice ? "Compact HUD" : "Speedrun overlay",
                           g_pShared ? *g_pShared : disconnected,
                           compactPractice ? g_hudLayout : g_speedrunLayout, true, mode,
                           g_pShared ? SnapshotAge() : 1., compactPractice);
    }
}

inline void RenderDiagnosticSections(const std::string &text)
{
    std::istringstream lines(text);
    std::string line, title = "Diagnostics", body;
    auto draw = [&]() {
        if (body.empty())
            return;
        if (suite_ui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
            ImGui::TextWrapped("%s", body.c_str());
    };
    while (std::getline(lines, line))
    {
        if (line.size() > 2 && line.front() == '[' && line.back() == ']')
        {
            draw();
            title = line.substr(1, line.size() - 2);
            body.clear();
        }
        else
        {
            body += line;
            body += '\n';
        }
    }
    draw();
}

#include "DiagnosticUI.h"
#include "RunExperienceUI.h"

inline bool SetSpeedrunMode(bool enabled, bool currentLevel = false)
{
    if (!g_pShared || HasPendingRequest() || suite_hud::active || suite_clip::open)
        return false;
    StopReplay3D();
    g_speedrunMode = enabled;
    if (currentLevel)
    {
        g_practiceMode = g_seededPractice = false;
        int level = MapLevelIndex(g_pShared->currentSceneIndex);
        if (level >= 0)
            g_selectedSpeedrunLevel = level;
    }
    if (!enabled)
        g_practiceMode = g_seededPractice = false;
    if (g_runReviewOpen)
    {
        g_commands.runReviewRequest = 2;
        g_runReviewOpen = false;
    }
    if (enabled)
    {
        g_commands.speedrunLevelIndex = byte(g_selectedSpeedrunLevel);
        g_commands.speedrunLoadLevelCmd = 1;
    }
    g_commands.speedrunModeActive = enabled;
    g_commands.practiceModeActive = g_practiceMode;
    suite_run::presentation.Mode(enabled, suite_ui::LifecycleNow());
    suite_ui::PostGameTone(enabled ? suite_ui::Tone::On : suite_ui::Tone::Off);
    suite_ui::requested = suite_ui::Tone::None;
    return true;
}

inline void RenderCheatsPage(bool cheatRestricted)
{
    ImGui::BeginChild("PracticeScroll", ImVec2(0, 0));
    ImGui::BeginDisabled(!g_pShared || cheatRestricted);
    bool cards = ImGui::GetContentRegionAvail().x >= 600 * suite_ui::Unit();
    bool grid = cards && ImGui::BeginTable("Feature cards", 2, ImGuiTableFlags_SizingStretchSame);
    if (grid)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
    }
    suite_ui::FeatureCard(
        "Actual colliders", suite_ui::Icon::Collider, &g_hitboxCheck,
        g_pShared && g_pShared->hitboxEnabled ? "Applied in game" : "Exact live collider shapes",
        "Cyan: player body. Green: triggers. Yellow: crystal pickup colliders. Mesh and terrain colliders are omitted. Training aids invalidate a clean run.");
    if (grid)
        ImGui::TableNextColumn();
    suite_ui::FeatureCard(
        "Fullbright", suite_ui::Icon::Sun, &g_brightnessOn,
        g_pShared && g_pShared->brightnessEnabled ? "Applied in game" : "Bright scene lighting",
        "Use the available unlit texture shaders to brighten the scene while retaining textures and transparency. Original shaders are restored when disabled. This aid invalidates a clean run.");
    if (grid)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
    }
    suite_ui::FeatureCard(
        "Minimap", suite_ui::Icon::Map, &g_showMinimapWindow, "Live geometry and markers",
        "Show the observed map, player heading and enabled crystal or monster markers. Configure colors and compass in Settings.");
    if (grid)
        ImGui::TableNextColumn();
    bool freecam = g_pShared && g_pShared->freecamActive;
    suite_ui::FeatureCard(
        "Free camera", suite_ui::Icon::Camera, &freecam,
        freecam ? "Active / player stays in place" : "Press F1 in the game",
        "F1 in the game toggles a camera without colliders. WASD / Q / E moves; Shift is faster; right mouse looks around. The player cannot collect crystals while using Freecam.",
        true, "F1");
    if (grid)
        ImGui::EndTable();
    suite_ui::Section("Visual options", 0, [&] {
        ImGui::BeginDisabled(!g_hitboxCheck);
        suite_ui::Checkbox("Hide player hitbox", &g_hidePlayerHitbox);
        ImGui::EndDisabled();
        ImGui::BeginDisabled(!g_showMinimapWindow);
        suite_ui::Checkbox("Monster navigation path", &g_showPath);
        ImGui::EndDisabled();
        Help("Path data is requested only while the map and this option are visible.");
        if (g_pShared && g_pShared->hitboxDetails[0])
        {
            ImGui::TextDisabled("Collider details");
            Help(g_pShared->hitboxDetails);
        }
    });
    ImGui::Spacing();
    ImGui::BeginDisabled(!g_pShared || !g_pShared->freecamActive || HasPendingRequest());
    if (suite_ui::Button("Teleport player to camera"))
    {
        g_commands.teleportX = g_pShared->freecamX;
        g_commands.teleportY = g_pShared->freecamY;
        g_commands.teleportZ = g_pShared->freecamZ;
        g_commands.teleportCmd = 1;
        g_commands.teleportScene = static_cast<uint32_t>(g_pShared->currentSceneIndex + 1);
        g_commands.teleportExitFreecam = 0;
    }
    Help("Move the player to the camera location. This marks the run as assisted.");
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    suite_ui::Section("Advanced scene tools", 0, [&]() {
        ImGui::TextColored(g_headerCol, "Scene selection");
        static int scene = 3;
        const char *scenes[] = {"Intro",
                                "DeathMessage",
                                "TitleScreen",
                                "Classic",
                                "Camera",
                                "Hide & Seek",
                                "SpiderModeEnding",
                                "Timer",
                                "Ghost",
                                "GhostModeEnding",
                                "Credits",
                                "ClassicEnding",
                                "Camera Bright",
                                "Hide & Seek Outside",
                                "Classic Bonus",
                                "SoundTestScreen",
                                "FileDeletionScreen"};
        ImGui::BeginDisabled(!g_pShared || g_speedrunMode || HasPendingRequest());
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##Scene", &scene, scenes, IM_ARRAYSIZE(scenes));
        if (suite_ui::Button("Load selected scene"))
        {
            g_commands.levelLoadIndex = scene;
            g_commands.levelLoadCmd = 1;
        }
        ImGui::EndDisabled();
        if (g_speedrunMode)
            ImGui::TextDisabled("Use the Speedrun level selector while that mode is active.");
    });
    ImGui::EndChild();
}

#include "RunStatisticsUI.h"

inline void RenderSpeedrunPage()
{
    run_stats::Render();
}

inline void RenderHudSettings()
{
    ImGui::BeginDisabled(g_pShared && g_pShared->timerState == 1);
    if (suite_ui::Button("Edit HUD on screen"))
        suite_hud::Open();
    ImGui::EndDisabled();
    RenderHudColors();
    if (ImGui::BeginTabBar("OverlayProfiles"))
    {
        if (suite_ui::BeginTabItem("Compact HUD"))
        {
            RenderHudPreview(g_hudLayout, false);
            RenderOverlaySettings(g_hudLayout, false);
            suite_ui::EndTabItem();
        }
        if (suite_ui::BeginTabItem("Inputs"))
        {
            suite_input::Settings();
            suite_ui::EndTabItem();
        }
        if (suite_ui::BeginTabItem("Speedrun overlay"))
        {
            RenderHudPreview(g_speedrunLayout, true);
            RenderOverlaySettings(g_speedrunLayout, true);
            suite_ui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

inline void RenderMinimapSettings()
{
    RenderMinimapColors();
    ImGui::Separator();

    suite_ui::Checkbox("Follow player", &g_mapOptions.follow);
    Help("Follow uses a stable local range; turn it off to fit the observed map bounds.");
    suite_ui::Checkbox("Player-up orientation", &g_mapOptions.playerUp);
    Help("Off = North-up. North is the game's +Z axis. Player-up keeps the heading at the top.");
    ImGui::SliderFloat("Zoom", &g_mapOptions.zoom, .5f, 4.f, "%.2fx");
    ImGui::BeginDisabled(!g_mapOptions.follow);
    ImGui::SliderFloat("Visible radius (world units)", &g_mapOptions.range, 10, 150, "%.0f");
    ImGui::EndDisabled();
    suite_ui::Checkbox("Wall geometry", &g_mapOptions.walls);
    SameLineIfFits("Crystal markers");
    suite_ui::Checkbox("Crystal markers", &g_mapOptions.crystals);
    suite_ui::Checkbox("Monster marker", &g_mapOptions.monster);
    Help(
        "Timer enemies are always red, including stationary enemies. An amber/red ring highlights spotted/angry states. Displays actual WallAI enemies; the closest enemy supplies the navigation path.");
    suite_ui::Checkbox("Movement trail", &g_mapOptions.trail);
    Help("At most 128 samples at 10 Hz. Cleared on teleport, world changes and observation gaps.");
    const char *mapPositions[] = {"Top left", "Top right", "Bottom left", "Bottom right"};
    ImGui::Combo("Map position", &g_mapOptions.anchor, mapPositions, 4);
    suite_ui::Checkbox("Square map", &minimapLockAspect);
    if (ImGui::SliderInt("Width", &g_minimapWidth, 100, 500) && minimapLockAspect)
        g_minimapHeight = g_minimapWidth;
    ImGui::BeginDisabled(minimapLockAspect);
    ImGui::SliderInt("Height", &g_minimapHeight, 100, 500);
    ImGui::EndDisabled();
    if (minimapLockAspect)
        g_minimapHeight = g_minimapWidth;
    float percent = g_minimapOpacity * 100;
    if (ImGui::SliderFloat("Opacity", &percent, 10.f, 100.f, "%.0f%%"))
        g_minimapOpacity = percent / 100;
}

inline void RenderSystemSettings()
{
    ImGui::TextColored(g_headerCol, "Startup");
    suite_ui::Checkbox("Start It Steals with PracticeSuite", &g_autoStartGame);
    Help(
        "Start the installed game once when the trainer opens. An existing game is reused. The trainer closes when that game exits. Use --no-launch to skip automatic startup once.");
    suite_ui::Checkbox("Skip game intro", &g_skipGameIntro);
    Help(
        "Skip startup logos, the loader start screen and game intro when launching through the Suite. Restart the game to apply changes. Original startup settings are restored after loading. Sound and first-launch setup still run before the main menu.");
    ImGui::Separator();
    ImGui::TextColored(g_headerCol, "Interface");
    suite_ui::Checkbox("UI sounds", &suite_ui::preferences.sounds);
    Help(
        "Quiet original button, tab and confirmation cues. Inspector cues have a separate level under Inspector > Style and also follow this master switch. Automatically muted during active speedruns and when another app has focus.");
    ImGui::BeginDisabled(!suite_ui::preferences.sounds);
    float volume = suite_ui::preferences.volume * 100;
    if (ImGui::SliderFloat("UI volume", &volume, 0, 100, "%.0f%%"))
        suite_ui::preferences.volume = volume / 100;
    if (suite_ui::Button("Preview sound"))
        suite_ui::Request(suite_ui::Tone::Confirm);
    ImGui::EndDisabled();
    suite_ui::Checkbox("Interface animations", &suite_ui::preferences.motion);
    Help(
        "Short hover, switch, tab and appearance transitions. Turn off for immediate static controls. HUD timers and validity remain steady.");
    suite_ui::Checkbox("Start / exit graphic", &suite_ui::preferences.launchGraphic);
    Help(
        "A 2.6-second animated introduction on the first visible game overlay, plus a brief standalone exit graphic after the game closes. The introduction is skipped during active runs; neither graphic captures input.");
    suite_ui::Checkbox("Blur behind intro", &suite_ui::appearance.introBlur);
    Help(
        "A one-time, locally blurred game image behind the intro card. No continuous capture or full-screen effect. If capture is unavailable, the card uses its normal background.");
    ImGui::Separator();
    ImGui::TextColored(g_headerCol, "Display");
    suite_ui::Checkbox("Include trainer in game captures (OBS)", &g_captureOverlayEnabled);
    Help(
        "Include the complete trainer UI, menus and HUD when OBS captures It Steals. DirectX 11 GPU sharing avoids CPU image copies. Native input stays active; if sharing is unavailable, the normal trainer window remains visible and HUD capture uses its fallback.");
    const char *rates[] = {"30 FPS", "60 FPS", "120 FPS"};
    int rate = g_renderFps == 30 ? 0 : g_renderFps == 120 ? 2 : 1;
    if (ImGui::Combo("Overlay frame limit", &rate, rates, 3))
        g_renderFps = rate == 0 ? 30 : rate == 2 ? 120 : 60;
    Help("Limits overlay rendering. Rendering pauses automatically outside the game.");
    Help(
        "Limits menu rendering; the passive HUD refreshes at 30 FPS and preserves panel opacity. Both rates are independent of game FPS.");
    const char *gameRates[] = {"Use game settings", "Unlimited", "Custom limit"};
    if (ImGui::Combo("Game frame limit", &g_frameLimitMode, gameRates, 3) && g_frameLimitMode == 1)
        g_vSyncMode = 1;
    if (g_frameLimitMode == 2)
        ImGui::SliderInt("Game FPS limit", &g_gameFrameLimit, 30, 1000);
    const char *syncModes[] = {"Use game settings", "Off", "On"};
    ImGui::Combo("Game VSync", &g_vSyncMode, syncModes, 3);
    Help(
        "Temporary display overrides. Unlimited also selects VSync Off. The game preferences are not rewritten; disconnecting restores the previous settings.");
    if (g_pShared)
        ImGui::Text("Game target: %d | VSync: %d | display: %.0f Hz", g_pShared->gameTargetFps,
                    g_pShared->gameVSync, g_pShared->gameRefreshHz);
    ImGui::BeginDisabled(!g_pShared || HasPendingRequest());
    if (suite_ui::Button(suite_keys::Label("Use borderless fullscreen", suite_keys::Borderless).c_str()))
        g_commands.windowModeCmd = 1;
    Help(
        "Switch the game through Unity's window-mode setting. Useful when exclusive fullscreen hides external overlays.");
    ImGui::EndDisabled();
}

inline void RenderShortcutSettings()
{
    ImGui::TextWrapped(
        "Click a shortcut, then press the new key combination. Ctrl, Alt and Shift are supported.");
    if (ImGui::BeginTable("Trainer shortcuts", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch, .56f);
        ImGui::TableSetupColumn("Shortcut", ImGuiTableColumnFlags_WidthStretch, .44f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < suite_keys::Count; i++)
        {
            ImGui::PushID(i);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextWrapped("%s", suite_keys::Actions[i]);
            ImGui::TableNextColumn();
            if (suite_ui::Button(suite_keys::Hint(suite_keys::Action(i)).c_str(), {-1, 0}))
                suite_keys::Capture(i);
            Help(suite_keys::Hint(suite_keys::Action(i)).c_str());
            if (suite_keys::unavailable & (1u << i))
            {
                ImGui::TextDisabled("Focused input fallback");
                Help(
                    "Windows could not reserve this combination. It is checked while the game has focus; choose another combination if another application also uses it.");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (suite_ui::Button("Restore default shortcuts"))
        suite_keys::preferences = {};
    ImGui::TextWrapped(
        "Shortcuts apply while the game or trainer has focus and pause during text entry. Quick restart defaults to T and can be reassigned above. Custom trainer shortcuts take priority over replay keys. F1 (Freecam) and replay playback controls remain game controls.");
    ImGui::TextDisabled("The tray menu can always reopen the trainer.");
    if (suite_keys::openRequested)
    {
        ImGui::OpenPopup("Assign trainer shortcut");
        suite_keys::openRequested = false;
    }
    ImGui::SetNextWindowSize({std::min(460.f * g_dpiScale, ImGui::GetIO().DisplaySize.x - 24), 0},
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Assign trainer shortcut", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (suite_keys::recording < 0)
            ImGui::CloseCurrentPopup();
        else
        {
            ImGui::TextWrapped("%s", suite_keys::Actions[suite_keys::recording]);
            ImGui::TextWrapped("Press shortcut now.\nEsc: cancel / Backspace: clear");
            ImGui::BeginChild("Shortcut feedback", {0, 2.5f * ImGui::GetTextLineHeight()});
            if (!suite_keys::error.empty())
                ImGui::TextWrapped("%s", suite_keys::error.c_str());
            ImGui::EndChild();
            if (suite_ui::Button("Unassign"))
                suite_keys::Offer(8, 0);
            SameLineIfFits("Cancel");
            if (suite_ui::Button("Cancel"))
                suite_keys::recording = -1;
        }
        ImGui::EndPopup();
    }
}

inline void RenderSettingsPage()
{
    ImGui::BeginChild("SettingsScroll", ImVec2(0, 0));
    if (ImGui::BeginTabBar("SettingsPages", ImGuiTabBarFlags_FittingPolicyScroll))
    {
        if (suite_ui::BeginTabItem("Appearance"))
        {
            RenderAppearanceSettings();
            suite_ui::EndTabItem();
        }
        if (suite_ui::BeginTabItem("HUD"))
        {
            RenderHudSettings();
            suite_ui::EndTabItem();
        }
        if (suite_ui::BeginTabItem("Minimap"))
        {
            RenderMinimapSettings();
            suite_ui::EndTabItem();
        }
        if (suite_ui::BeginTabItem("System"))
        {
            RenderSystemSettings();
            suite_ui::EndTabItem();
        }
        if (suite_ui::BeginTabItem("Shortcuts"))
        {
            RenderShortcutSettings();
            suite_ui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Spacing();
    if (suite_ui::Button("Reset layout"))
        g_resetLayout = true;
    SameLineIfFits("Restore display defaults");
    if (suite_ui::Button("Restore display defaults"))
    {
        TrainerPreferences defaults;
        defaults.level = g_selectedSpeedrunLevel;
        defaults.seed = g_practiceSeed;
        defaults.reports = g_reportsEnabled;
        defaults.autoStartGame = g_autoStartGame;
        defaults.skipGameIntro = g_skipGameIntro;
        defaults.hotkeys = suite_keys::preferences;
        ApplyPreferences(defaults);
    }
    if (suite_ui::Button("Save settings now"))
        SavePreferencesNow(true);
    ImGui::TextDisabled("Settings saved automatically");
    Help(
        "Display settings, selected level and seed are saved automatically. Training aids and run modes start disabled.");
    if (!g_settings.error.empty())
        ImGui::TextColored(g_accentCol, "%s", g_settings.error.c_str());
    if (!g_settings.loadWarning.empty())
        ImGui::TextWrapped("%s", g_settings.loadWarning.c_str());
    ImGui::EndChild();
}

inline void RenderDiagnosticsPage()
{
    ImGui::BeginChild("DiagnosticsScroll", ImVec2(0, 0));
    ImGui::BeginDisabled(!inspector_ui::Allowed());
    if (suite_ui::Button("Live Inspector"))
        inspector_ui::Open();
    ImGui::EndDisabled();
    Help("Read-only scene inspection, geometry decisions, observed property changes and local snapshots.");
    RenderDiagnosticScan();
    ImGui::Separator();
    ImGui::TextDisabled("Overlay visibility (%s)", suite_keys::Hint(suite_keys::Borderless).c_str());
    Help(
        "Windowed and borderless use the game client area. Use the borderless shortcut to request borderless fullscreen if exclusive fullscreen hides the overlay.");
    ImGui::TextDisabled("%s: toggle overlay. %s: open/close menu.",
                        suite_keys::Hint(suite_keys::Panels).c_str(),
                        suite_keys::Hint(suite_keys::Menu).c_str());
    if (suite_ui::Button("Close trainer"))
        g_quit = true;
    ImGui::EndChild();
}

void RenderMainWindow(const std::string &mode, int seed)
{
    if (suite_hud::active || suite_clip::open)
        return;
    if (inspector::picking || inspector::dockInteractive)
        return;
    if (suite_run::presentation.WantsInput(g_pShared, g_runReviewOpen) || !g_allOverlayEnabled ||
        !g_overlayVisible)
        return;
    if (g_compactMode && g_pShared && g_pShared->replay3dState >= 1 && g_pShared->replay3dState <= 3)
        return;
    static bool reviewExpanded = false, reviewResizeQueued = false;
    static ImVec2 beforeReviewSize, beforeReviewPos, reviewSize, reviewPos;
    if (g_selectRunReview)
    {
        suite_ui::navigation.Select(suite_ui::Page::Replay, ImGui::GetTime());
        g_selectRunReview = false;
    }
    bool replayHeader = suite_ui::navigation.Is(suite_ui::Page::Replay) && g_runReviewOpen;
    g_reviewTabActive = false;
    bool cheatRestricted = g_speedrunMode && !g_practiceMode;
    if (g_compactMode)
    {
        if (!g_speedrunMode && g_pShared)
            RenderOverlayPanel("Compact HUD", *g_pShared, g_hudLayout, false, mode, SnapshotAge());
        return;
    }
    ImVec2 display = ImGui::GetIO().DisplaySize;
    ImVec2 maximum(std::max(180.f, display.x - 24), std::max(160.f, display.y - 24));
    ImGui::SetNextWindowSize(
        ImVec2(std::min(1000.f * g_dpiScale, maximum.x), std::min(840.f * g_dpiScale, maximum.y)),
        g_resetLayout ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(20, std::min(72.f, std::max(12.f, display.y - 680.f * g_dpiScale))),
                            g_resetLayout ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(
        ImVec2(std::min(400.f * g_dpiScale, maximum.x), std::min(280.f * g_dpiScale, maximum.y)), maximum);
    if (reviewResizeQueued)
    {
        ImVec2 target(std::min(reviewSize.x, maximum.x), std::min(reviewSize.y, maximum.y));
        ImGui::SetNextWindowSize(target, ImGuiCond_Always);
        ImGui::SetNextWindowPos(ImVec2(std::clamp(reviewPos.x, 0.f, std::max(0.f, display.x - target.x)),
                                       std::clamp(reviewPos.y, 0.f, std::max(0.f, display.y - target.y))),
                                ImGuiCond_Always);
        reviewResizeQueued = false;
    }
    ImGui::PushStyleColor(ImGuiCol_WindowBg, g_mainBgCol);
    ImGui::PushStyleColor(ImGuiCol_Text, g_textCol);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * suite_ui::MenuAlpha());
    ImGui::Begin("It Steals | Practice Suite", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
    suite_ui::menu = true;
    suite_ui::tint = g_headerCol;
    ImVec2 position = ImGui::GetWindowPos(), size = ImGui::GetWindowSize();
    ImGui::SetWindowPos(ImVec2(std::clamp(position.x, 0.f, std::max(0.f, display.x - size.x)),
                               std::clamp(position.y, 0.f, std::max(0.f, display.y - size.y))));
    suite_ui::BrandHeader(replayHeader ? "Replay" : mode.c_str(), g_pShared != nullptr,
                          g_connectionStatus.c_str());
    if (!replayHeader && g_pShared && g_pShared->seedActive)
        ImGui::TextColored(g_accentCol, "SET SEED | Practice seed: %d", seed);
    bool narrow = ImGui::GetContentRegionAvail().x < 650 * suite_ui::Unit();
    if (suite_ui::Button(suite_keys::Label(narrow ? "Back" : "Return to game", suite_keys::Menu).c_str()))
    {
        g_overlayVisible = false;
        SetOverlayInteractive(false);
    }
    Help("Close the menu and pass mouse/keyboard input back to the game. The map and timer remain visible.");
    auto hudLabel = suite_keys::Label(narrow ? "HUD" : "Compact HUD", suite_keys::Hud);
    SameLineIfFits(hudLabel.c_str());
    ImGui::BeginDisabled(!CompactHudAllowed(g_speedrunMode, g_practiceMode));
    if (suite_ui::Button(hudLabel.c_str()))
    {
        g_compactMode = true;
        SetOverlayInteractive(false);
    }
    ImGui::EndDisabled();
    Help(
        "Compact HUD is available in Normal and Practice mode. Clean Speedrun uses only the Speedrun overlay.");
    if (cheatRestricted)
    {
        SameLineIfFits("Clean speedrun");
        ImGui::TextColored(g_headerCol, "Clean speedrun");
        Help("Training aids are locked while a clean speedrun is selected.");
    }
    ImGui::Separator();
    {
        float u = suite_ui::Unit(), rail = ImGui::GetContentRegionAvail().x >= 730 * u ? 184 * u : 64 * u;
        float bodyHeight = std::max(4.f, ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("Navigation", ImVec2(rail, bodyHeight));
        suite_ui::Sidebar(bodyHeight);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("Workspace", ImVec2(0, bodyHeight), ImGuiChildFlags_None,
                          suite_ui::navigation.Is(suite_ui::Page::Replay)
                              ? ImGuiWindowFlags_None
                              : ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        float reveal = suite_ui::Animate(
            ImGui::GetID(suite_ui::PageNames[int(suite_ui::navigation.page)]) ^ 0x78100u, 1, true, .17f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (.72f + .28f * reveal));
        if (!suite_ui::navigation.Is(suite_ui::Page::Replay) || !g_runReviewOpen)
            suite_ui::PageHeading(suite_ui::navigation.page);
        if (suite_ui::navigation.Is(suite_ui::Page::Updates))
            suite_update_ui::Page(*suite_update::service.View(), (g_pShared && g_pShared->timerState == 1) ||
                                                                     suite_clip::open ||
                                                                     suite_update::service.installBlocked);
        if (suite_ui::navigation.Is(suite_ui::Page::Inspector))
        {
            ImGui::BeginChild("Inspector workspace", {0, 0});
            inspector_ui::RenderTab();
            ImGui::EndChild();
        }
        if (suite_ui::navigation.Is(suite_ui::Page::Cheats))
            RenderCheatsPage(cheatRestricted);
        if (suite_ui::navigation.Is(suite_ui::Page::SeedMap))
        {
            ImGui::BeginChild("SeedMapScroll", ImVec2(0, 0));
            RenderSeedMapTab(cheatRestricted);
            ImGui::EndChild();
        }
        if (suite_ui::navigation.Is(suite_ui::Page::Speedrun))
            RenderSpeedrunPage();
        if (suite_ui::navigation.Is(suite_ui::Page::Settings))
            RenderSettingsPage();
        if (suite_ui::navigation.Is(suite_ui::Page::Replay))
            RenderRunReview();
        if (suite_ui::navigation.Is(suite_ui::Page::Diagnostics))
            RenderDiagnosticsPage();
        ImGui::PopStyleVar();
        ImGui::EndChild();
        bool expand = g_reviewTabActive && g_reviewWide;
        if (expand != reviewExpanded)
        {
            if (expand)
            {
                beforeReviewSize = ImGui::GetWindowSize();
                beforeReviewPos = ImGui::GetWindowPos();
                reviewSize = ImVec2(std::min(1440.f * g_dpiScale, maximum.x), maximum.y);
                reviewPos = ImVec2((display.x - reviewSize.x) * .5f, 12);
            }
            else
            {
                reviewSize = beforeReviewSize;
                reviewPos = beforeReviewPos;
            }
            // Apply on the next Begin: moving/resizing a window halfway
            // through rendering leaves stale size/clip rectangles this frame.
            reviewResizeQueued = true;
            reviewExpanded = expand;
        }
    }
    suite_ui::RenderNotice();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}
