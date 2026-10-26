void RunStatisticsCases(const std::string &output)
{
    auto savedData = data;
    auto savedCommands = g_commands;
    auto savedNavigation = suite_ui::navigation;
    auto savedDashboard = run_stats::dashboard;
    auto *savedShared = g_pShared;
    int savedLevel = g_selectedSpeedrunLevel;
    bool savedSpeedrun = g_speedrunMode, savedPractice = g_practiceMode;
    bool savedMotion = suite_ui::preferences.motion;
    auto &io = ImGui::GetIO();
    data = {};
    data.pid = 1234;
    data.health = 2;
    data.playerReady = data.rngPatchComplete = 1;
    data.currentSceneIndex = 3;
    data.modeStatsSinceUtc = 639285120000000000LL;
    data.modeStatsRevision = 10;
    for (int mode = 0; mode < 8; mode++)
    {
        data.modePersonalBest[mode] = 38.125 + mode * 13.375;
        data.modePbUtc[mode] = data.modeStatsSinceUtc;
        for (int kind : {1, 2})
        {
            int i = mode * 3 + kind;
            data.modeAttempts[i] = 75 + mode * 8 + kind * 13;
            data.modeFinished[i] = 12 + mode + kind;
            data.modeResets[i] = 33 + mode;
            data.modeDeaths[i] = mode + 3;
            data.modeFinishSeconds[i] = (45 + mode * 10) * data.modeFinished[i];
            data.modePlaySeconds[i] = 2230 + mode * 543 + kind * 110;
        }
    }
    g_pShared = &data;
    g_compactMode = g_runReviewOpen = g_selectRunReview = false;
    g_allOverlayEnabled = g_overlayVisible = true;
    suite_hud::active = suite_clip::open = false;
    inspector::picking = inspector::dockInteractive = false;
    suite_run::presentation.Reset();
    for (ImVec2 viewport : {ImVec2(1280, 900), ImVec2(800, 600), ImVec2(480, 360)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = viewport;
            suite_ui::preferences.motion = dpi == 1.f;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            g_commands = {};
            g_speedrunMode = g_practiceMode = false;
            g_selectedSpeedrunLevel = 0;
            run_stats::setupReady = false;
            run_stats::viewFilter = 0;
            run_stats::dashboard = {};
            suite_ui::navigation.Select(suite_ui::Page::Speedrun, ImGui::GetTime());
            g_resetLayout = true;
            Frame();
            g_resetLayout = false;
            Frame();
            Frame();
            for (int settled = 0; settled < 16; settled++)
                Frame();
            if (!output.empty())
                Dump(output + "/mode-dashboard-" + std::to_string(int(viewport.x)) + "-" +
                     std::to_string(int(dpi * 100)) + ".draw");
            assert(run_stats::dashboard.available &&
                   run_stats::dashboard.modes[7].pb == data.modePersonalBest[7]);
            // Draft setup changes must not restart the active game.
            run_stats::setupPractice = dpi > 1.f;
            run_stats::setupFixed = dpi > 1.f;
            run_stats::setupSeed = 12345;
            ImRect card, start;
            auto controls = [&] {
                ImGui::NewFrame();
                suite_ui::BeginFrame(true, false);
                ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always);
                ImGui::SetNextWindowSize({viewport.x - 40, viewport.y - 40}, ImGuiCond_Always);
                ImGui::Begin("Mode card interaction", nullptr, ImGuiWindowFlags_NoSavedSettings);
                run_stats::Card(7, ImGui::GetContentRegionAvail().x);
                card = GImGui->LastItemData.Rect;
                run_stats::Details();
                auto *popup = ImGui::FindWindowByName("Mode statistics");
                if (popup && popup->Active)
                {
                    float width =
                        ImGui::CalcTextSize("Start this mode").x + ImGui::GetStyle().FramePadding.x * 2;
                    start = ImRect({popup->DC.CursorPosPrevLine.x - width, popup->DC.CursorPosPrevLine.y},
                                   {popup->DC.CursorPosPrevLine.x,
                                    popup->DC.CursorPosPrevLine.y + popup->DC.PrevLineSize.y});
                }
                ImGui::End();
                ImGui::Render();
            };
            controls();
            controls();
            io.AddMousePosEvent(card.GetCenter().x, card.GetCenter().y);
            io.AddMouseButtonEvent(0, true);
            controls();
            io.AddMouseButtonEvent(0, false);
            controls();
            controls();
            assert(run_stats::dashboard.selected == 7 && g_selectedSpeedrunLevel == 0 &&
                   !g_commands.speedrunLoadLevelCmd);
            auto *modal = ImGui::FindWindowByName("Mode statistics");
            assert(modal && modal->Active && modal->Pos.x >= 0 && modal->Pos.y >= 0 &&
                   modal->Pos.x + modal->Size.x <= viewport.x + 1 &&
                   modal->Pos.y + modal->Size.y <= viewport.y + 1);
            assert(start.Min.y >= modal->Pos.y && start.Max.y <= modal->Pos.y + modal->Size.y + 1);
            if (!output.empty())
                Dump(output + "/mode-details-" + std::to_string(int(viewport.x)) + "-" +
                     std::to_string(int(dpi * 100)) + ".draw");
            io.AddMousePosEvent(start.GetCenter().x, start.GetCenter().y);
            io.AddMouseButtonEvent(0, true);
            controls();
            io.AddMouseButtonEvent(0, false);
            controls();
            controls();
            assert(g_commands.speedrunLoadLevelCmd == 1 && g_commands.speedrunLevelIndex == 7 &&
                   g_selectedSpeedrunLevel == 7 && g_speedrunMode);
            assert(g_practiceMode == (dpi > 1.f) && g_seededPractice == (dpi > 1.f));
            assert(!g_seededPractice || (g_commands.setSeedCmd && g_commands.practiceSeed == 12345));
        }
    data = savedData;
    g_commands = savedCommands;
    g_pShared = savedShared;
    g_selectedSpeedrunLevel = savedLevel;
    g_speedrunMode = savedSpeedrun;
    g_practiceMode = savedPractice;
    suite_ui::preferences.motion = savedMotion;
    suite_ui::navigation = savedNavigation;
    run_stats::dashboard = savedDashboard;
    std::cout
        << "Mode dashboard: eight-mode data, real card/detail/start mouse controls, browse without reload, bounded modal/footer, 480/800/1280 at 100/150 DPI: PASS\n";
}
