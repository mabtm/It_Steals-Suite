void EliteDesignCases(const std::string &output)
{
    auto &io = ImGui::GetIO();
    auto savedData = data;
    auto savedAppearance = suite_ui::appearance;
    auto savedPreferences = suite_ui::preferences;
    suite_ui::appearance = {};
    suite_ui::preferences = {};
    suite_ui::preferences.motion = false;
    suite_ui::preferences.launchGraphic = false;
    suite_run::presentation.Reset();
    data = {};
    data.pid = 1234;
    data.currentSceneIndex = 3;
    data.playerReady = data.health = data.rngPatchComplete = 1;
    g_pShared = &data;
    g_allOverlayEnabled = g_overlayVisible = true;
    g_compactMode = g_speedrunMode = g_practiceMode = g_runReviewOpen = g_selectRunReview = false;
    pendingRequest = false;
    g_commands = {};
    int checked = 0;
    for (auto viewport : {ImVec2(480, 360), ImVec2(1280, 900)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = viewport;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            suite_ui::navigation = {};
            assert(suite_ui::navigation.Is(suite_ui::Page::Speedrun));
            g_resetLayout = true;
            Frame();
            g_resetLayout = false;
            for (int i = 0; i < 3; ++i)
                Frame();
            ImGuiWindow *navigation = nullptr;
            auto *root = ImGui::FindWindowByName("It Steals | Practice Suite");
            for (auto *window : GImGui->Windows)
                if (window->ParentWindow == root && std::strstr(window->Name, "/Navigation_") &&
                    window->LastFrameActive == ImGui::GetFrameCount())
                    navigation = window;
            assert(navigation && root);
            if (viewport.x == 480)
            {
                // Every destination is available above the workspace at the
                // narrow viewport, without scrolling the navigation rail.
                assert(navigation->Size.x > root->Size.x * .8f && navigation->ScrollMax.y < 1);
                assert(navigation->InnerClipRect.GetHeight() >= 40 * suite_ui::Unit());
            }
            for (auto page : {suite_ui::Page::Cheats, suite_ui::Page::Speedrun, suite_ui::Page::SeedMap,
                              suite_ui::Page::Inspector, suite_ui::Page::Replay, suite_ui::Page::Settings,
                              suite_ui::Page::Updates, suite_ui::Page::Diagnostics})
            {
                ImGui::ActivateItemByID(ImHashStr("##Navigate", 0, navigation->GetID(int(page))));
                Frame();
                Frame();
                assert(suite_ui::navigation.Is(page));
                ++checked;
            }
            for (auto page : {suite_ui::Page::Cheats, suite_ui::Page::Speedrun, suite_ui::Page::Settings})
            {
                suite_ui::navigation.Select(page, ImGui::GetTime());
                for (int i = 0; i < 3; ++i)
                    Frame();
                if (!output.empty())
                    Dump(output + "/elite-" + std::to_string(int(viewport.x)) + "-" +
                         std::to_string(int(dpi * 100)) + "-" + std::to_string(int(page)) + ".draw");
            }
            assert(g_commands.speedrunLoadLevelCmd == 0 && g_commands.levelLoadCmd == 0);
        }
    suite_ui::appearance = savedAppearance;
    suite_ui::preferences = savedPreferences;
    data = savedData;
    io.DisplaySize = {1280, 720};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    std::cout << "Elite shell: " << checked
              << " real page activations, narrow all-destination navigation, two DPIs, no implicit game load: PASS\n";
}
