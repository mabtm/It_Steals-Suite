void RunStatisticsCases(const std::string &output)
{
    auto savedData = data;
    auto savedCommands = g_commands;
    auto savedNavigation = suite_ui::navigation;
    auto savedDashboard = run_stats::dashboard;
    auto savedObserved = run_stats::observedSelection;
    auto savedPlannerOptions = g_plannerOptions;
    bool savedSetupReady = run_stats::setupReady, savedSetupPractice = run_stats::setupPractice;
    bool savedSetupFixed = run_stats::setupFixed, savedSetupSpawn = run_stats::setupSpawn;
    int savedSetupSeed = run_stats::setupSeed;
    auto *savedShared = g_pShared;
    int savedLevel = g_selectedSpeedrunLevel;
    bool savedSpeedrun = g_speedrunMode, savedPractice = g_practiceMode;
    bool savedFixed = g_seededPractice, savedSpawn = g_rawPracticeSpawn;
    int savedSeed = g_practiceSeed;
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
    // Draft PB selection is independent of the applied/active category.
    for (int mode = 0; mode < 8; mode++)
        for (int category = 0; category < 3; category++)
        {
            data.allModePersonalBest[mode * 3 + category] = 30 + mode + category * 10;
            data.allModePbUtc[mode * 3 + category] = data.modeStatsSinceUtc;
        }
    run_stats::dashboard.snapshot = data;
    for (int mode = 0; mode < 8; mode++)
        for (int category = 0; category < 3; category++)
        {
            run_stats::setupPractice = category == 2; run_stats::setupFixed = category == 1;
            assert(run_stats::SetupBest(mode) == data.allModePersonalBest[mode * 3 + category]);
            assert(g_commands.speedrunLoadLevelCmd == savedCommands.speedrunLoadLevelCmd);
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
            g_seededPractice = g_rawPracticeSpawn = false;
            g_practiceSeed = 123;
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
            ImRect card, details, start;
            auto controls = [&] {
                ImGui::NewFrame();
                suite_ui::BeginFrame(true, false);
                ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always);
                ImGui::SetNextWindowSize({viewport.x - 40, viewport.y - 40}, ImGuiCond_Always);
                ImGui::Begin("Mode card interaction", nullptr, ImGuiWindowFlags_NoSavedSettings);
                run_stats::Card(7, ImGui::GetContentRegionAvail().x);
                card = GImGui->LastItemData.Rect;
                if (suite_ui::Button("Details"))
                    ImGui::OpenPopup("Mode statistics");
                details = GImGui->LastItemData.Rect;
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
            assert((!modal || !modal->Active) && !ImGui::IsPopupOpen("Mode statistics", ImGuiPopupFlags_AnyPopupId));
            io.AddMousePosEvent(details.GetCenter().x, details.GetCenter().y);
            controls();
            io.AddMouseButtonEvent(0, true);
            controls();
            io.AddMouseButtonEvent(0, false);
            controls();
            controls();
            modal = ImGui::FindWindowByName("Mode statistics");
            assert(modal && modal->Active && modal->Pos.x >= 0 && modal->Pos.y >= 0 &&
                   modal->Pos.x + modal->Size.x <= viewport.x + 1 &&
                   modal->Pos.y + modal->Size.y <= viewport.y + 1);
            assert(start.Min.y >= modal->Pos.y && start.Max.y <= modal->Pos.y + modal->Size.y + 1);
            if (!output.empty())
                Dump(output + "/mode-details-" + std::to_string(int(viewport.x)) + "-" +
                     std::to_string(int(dpi * 100)) + ".draw");
            if (run_stats::setupFixed)
            {
                data.rngPatchComplete = 0;
                assert(!run_stats::StartSelected() && !g_commands.speedrunLoadLevelCmd && !g_speedrunMode);
                data.rngPatchComplete = 1;
            }
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

    // Ordinary repainting preserves an uncommitted draft. Adopting a seed on
    // another page must replace that draft before the next explicit Start.
    io.DisplaySize = {1280, 900};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    g_commands = {};
    g_speedrunMode = g_practiceMode = g_seededPractice = g_rawPracticeSpawn = false;
    g_selectedSpeedrunLevel = 0;
    g_practiceSeed = 123;
    run_stats::setupReady = false;
    suite_run::presentation.Reset();
    suite_ui::navigation.Select(suite_ui::Page::Speedrun, ImGui::GetTime());
    Frame();
    run_stats::setupPractice = run_stats::setupFixed = true;
    run_stats::setupSeed = 54321;
    run_stats::dashboard.selected = 2;
    Frame();
    Frame();
    assert(run_stats::setupPractice && run_stats::setupFixed && run_stats::setupSeed == 54321 &&
           run_stats::dashboard.selected == 2 && !g_commands.speedrunLoadLevelCmd);
    SetPracticeSelection(73621, 1, 6);
    Frame();
    assert(run_stats::setupPractice && run_stats::setupFixed && run_stats::setupSpawn &&
           run_stats::setupSeed == 73621 && run_stats::dashboard.selected == 6 &&
           !g_commands.speedrunLoadLevelCmd);
    assert(run_stats::StartSelected());
    assert(g_commands.speedrunLoadLevelCmd == 1 && g_commands.speedrunLevelIndex == 6 &&
           g_commands.setSeedCmd && g_commands.practiceSeed == 73621 && g_practiceSeed == 73621 &&
           g_rawPracticeSpawn);

    // A blocked start is side-effect free, including the local seed selection.
    g_commands = {};
    auto blockedCommands = g_commands;
    const auto blockedSelection = run_stats::CurrentSelection();
    suite_clip::open = true;
    assert(!run_stats::StartSelected());
    suite_clip::open = false;
    assert(std::memcmp(&g_commands, &blockedCommands, sizeof(g_commands)) == 0 &&
           run_stats::CurrentSelection() == blockedSelection);

    // Use the actual category combo, then check the complete start command.
    // Set-Seed must stay separate from Practice and must not silently run as
    // Random Seed with an older mod or unavailable RNG hooks.
    io.DisplaySize = {1000, 700};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    run_stats::setupPractice = run_stats::setupFixed = run_stats::setupSpawn = false;
    ImVec2 categoryCombo;
    auto categoryFrame = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        ImGui::SetNextWindowPos({30, 30}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({700, 500}, ImGuiCond_Always);
        ImGui::Begin("Run category interaction", nullptr, ImGuiWindowFlags_NoSavedSettings);
        auto at = ImGui::GetCursorScreenPos();
        categoryCombo = {at.x + 40, at.y + ImGui::GetFrameHeight() * .5f};
        run_stats::CategorySetup();
        ImGui::End();
        ImGui::Render();
    };
    categoryFrame();
    categoryFrame();
    auto selectCategory = [&](int row) {
        io.AddMousePosEvent(categoryCombo.x, categoryCombo.y);
        categoryFrame();
        io.AddMouseButtonEvent(0, true);
        categoryFrame();
        io.AddMouseButtonEvent(0, false);
        categoryFrame();
        assert(!GImGui->OpenPopupStack.empty());
        auto *popup = GImGui->OpenPopupStack.back().Window;
        assert(popup && popup->Active);
        auto at = popup->DC.CursorStartPos;
        io.AddMousePosEvent(at.x + 30,
                           at.y + (row + .5f) * ImGui::GetTextLineHeightWithSpacing());
        categoryFrame();
        io.AddMouseButtonEvent(0, true);
        categoryFrame();
        io.AddMouseButtonEvent(0, false);
        categoryFrame();
    };
    selectCategory(1);
    assert(!run_stats::setupPractice && run_stats::setupFixed);
    selectCategory(2);
    assert(run_stats::setupPractice && !run_stats::setupFixed);
    selectCategory(0);
    assert(!run_stats::setupPractice && !run_stats::setupFixed);

    g_commands = {};
    g_speedrunMode = g_practiceMode = g_seededPractice = false;
    g_rawPracticeSpawn = true;
    run_stats::setupReady = false;
    run_stats::SyncSetupSelection();
    run_stats::setupPractice = false;
    run_stats::setupFixed = run_stats::setupSpawn = true;
    run_stats::setupSeed = 0;
    std::strcpy(data.version, "v7.21.0");
    assert(!run_stats::StartSelected() && !g_commands.speedrunLoadLevelCmd);
    std::strcpy(data.version, "v7.22.0");
    data.rngPatchComplete = 0;
    assert(!run_stats::StartSelected() && !g_commands.speedrunLoadLevelCmd);
    data.rngPatchComplete = 1;
    assert(run_stats::StartSelected());
    assert(g_speedrunMode && !g_practiceMode && g_seededPractice && g_rawPracticeSpawn &&
           g_commands.speedrunModeActive && !g_commands.practiceModeActive &&
           g_commands.seededPractice && g_commands.setSeedCmd && g_commands.practiceSeed == 0);
    data = savedData;
    g_commands = savedCommands;
    g_pShared = savedShared;
    g_selectedSpeedrunLevel = savedLevel;
    g_speedrunMode = savedSpeedrun;
    g_practiceMode = savedPractice;
    g_seededPractice = savedFixed;
    g_rawPracticeSpawn = savedSpawn;
    g_practiceSeed = savedSeed;
    g_plannerOptions = savedPlannerOptions;
    suite_ui::preferences.motion = savedMotion;
    suite_ui::navigation = savedNavigation;
    run_stats::dashboard = savedDashboard;
    run_stats::observedSelection = savedObserved;
    run_stats::setupReady = savedSetupReady;
    run_stats::setupPractice = savedSetupPractice;
    run_stats::setupFixed = savedSetupFixed;
    run_stats::setupSpawn = savedSetupSpawn;
    run_stats::setupSeed = savedSetupSeed;
    std::cout
        << "Mode dashboard: eight-mode data, select without modal/reload, explicit details/start mouse controls, bounded modal/footer, preserved drafts and adopted seed setup, blocked start, 480/800/1280 at 100/150 DPI: PASS\n";
}
