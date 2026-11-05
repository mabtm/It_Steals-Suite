#pragma once

// Native training controls share the application's input, aid restrictions and
// command path. This page never changes the selected run category implicitly.
inline void RenderCheatsPage(bool cheatRestricted)
{
    ImGui::BeginChild("PracticeScroll", ImVec2(0, 0));
    const float u = suite_ui::Unit();
    const bool split = ImGui::GetContentRegionAvail().x >= 760 * u;
    const bool columns = split && ImGui::BeginTable("Training groups", 2,
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV);
    if (columns)
    {
        ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch, 1);
        ImGui::TableSetupColumn("Session", ImGuiTableColumnFlags_WidthFixed, 235 * u);
        ImGui::TableNextColumn();
    }
    auto heading = [&](const char *text) {
        ImGui::PushStyleColor(ImGuiCol_Text, g_headerCol);
        ImGui::SeparatorText(text);
        ImGui::PopStyleColor();
    };
    auto detail = [&](const char *text) {
        ImGui::Indent(27 * u);
        ImGui::PushStyleColor(ImGuiCol_Text, suite_ui::Muted());
        ImGui::TextWrapped("%s", text);
        ImGui::PopStyleColor();
        ImGui::Unindent(27 * u);
    };
    ImGui::BeginDisabled(!g_pShared || cheatRestricted);
    heading("Visual aids");
    suite_ui::Checkbox("Fullbright", &g_brightnessOn);
    Help("Use the game's unlit texture shaders while retaining textures and transparency. This aid invalidates a clean run.");
    detail("Improve visibility while learning a level.");
    suite_ui::Checkbox("Actual colliders", &g_hitboxCheck);
    Help("Cyan: player body. Green: triggers. Yellow: crystal pickup colliders. Mesh and terrain colliders are omitted. This aid invalidates a clean run.");
    detail("Show the game's exact collision boundaries.");
    ImGui::Indent(27 * u);
    ImGui::BeginDisabled(!g_hitboxCheck);
    suite_ui::Checkbox("Hide player hitbox", &g_hidePlayerHitbox);
    ImGui::EndDisabled();
    if (g_pShared && g_pShared->hitboxDetails[0])
    {
        ImGui::TextDisabled("Collider details");
        Help(g_pShared->hitboxDetails);
    }
    ImGui::Unindent(27 * u);
    heading("Map & navigation");
    suite_ui::Checkbox("Minimap", &g_showMinimapWindow);
    Help("Show the observed map, player heading and enabled markers. Configure colors and compass in Settings.");
    detail("Keep your position and pickups visible.");
    ImGui::Indent(27 * u);
    ImGui::BeginDisabled(!g_showMinimapWindow);
    suite_ui::Checkbox("Monster navigation path", &g_showPath);
    ImGui::EndDisabled();
    Help("Path data is requested only while the map and this option are visible.");
    ImGui::Unindent(27 * u);
    heading("Scene tools");
    ImGui::TextColored(g_headerCol, "Free camera [F1]");
    Help("F1 in the game toggles Freecam. WASD / Q / E moves; Shift is faster; right mouse looks around. The player cannot collect crystals while using Freecam.");
    ImGui::SameLine();
    ImGui::TextDisabled("%s", g_pShared && g_pShared->freecamActive ? "Active" : "Inactive");
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
    if (columns)
        ImGui::TableNextColumn();
    heading("Training session");
    ImGui::TextWrapped("%s", !g_pShared ? "Waiting for game" : cheatRestricted ? "Clean category / aids locked" : g_practiceMode ? "Practice / assisted" : "Normal play / training tools");
    ImGui::PushStyleColor(ImGuiCol_Text, suite_ui::Muted());
    ImGui::TextWrapped("Training aids stay locked in Random Seed and Set-Seed. Select Practice on the Run page to use them.");
    ImGui::PopStyleColor();
    heading("Quick controls");
    ImGui::BeginDisabled(!g_pShared || !CompactHudAllowed(g_speedrunMode, g_practiceMode));
    auto hud = suite_keys::Label("Compact HUD", suite_keys::Hud);
    if (suite_ui::Button(hud.c_str(), {-1, 0}))
    {
        g_compactMode = true;
        SetOverlayInteractive(false);
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!g_pShared || g_pShared->timerState == 1);
    if (suite_ui::Button("Edit HUD layout", {-1, 0}))
        suite_hud::Open();
    ImGui::EndDisabled();
    if (suite_ui::Button("Inspector", {-1, 0}))
        suite_ui::navigation.Select(suite_ui::Page::Inspector, ImGui::GetTime());
    if (columns)
        ImGui::EndTable();
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

