// Actual Suite widgets with synthetic release metadata; no network calls.
void UpdateUICases(const std::string &output)
{
    auto saved = data;
    auto &io = ImGui::GetIO();
    suite_update::service.bundled = suite_update::Changelog(
        "### v7.12.1 Changelog\n- Improved updates by implementing verified GitHub downloads, transactional installation and automatic restart.\n- Improved the interface by implementing a complete offline version history.\n\n### v7.12.0 Changelog\n- Improved installation by implementing a guided installer.\n");
    auto snapshot = std::make_shared<suite_update::Snapshot>();
    snapshot->phase = suite_update::Phase::Available;
    snapshot->message = "A new PracticeSuite update is available.";
    snapshot->latest.version = "7.12.2";
    snapshot->latest.asset = 1234;
    snapshot->latest.size = 20 * 1024 * 1024;
    snapshot->latest.sha256 = std::string(64, 'a');
    snapshot->latest.notes =
        "### v7.12.2 Changelog\n\n- Improved replay tools with a verified release update.\n- Fixed a bug caused by a disconnected session.\n\nAuthor: mab\n";
    snapshot->releases.push_back(snapshot->latest);
    suite_ui::preferences = {};
    g_pShared = &data;
    data.timerState = 0;
    suite_run::presentation.Reset();
    for (auto size : {ImVec2(480, 640), ImVec2(1280, 900), ImVec2(1920, 1080)})
        for (float scale : {1.f, 1.5f})
        {
            io.DisplaySize = size;
            ApplyTheme(scale);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            suite_update::service.presentation = {};
            suite_update_ui::historySource.reset();
            suite_update_ui::historyOpenedAt = -1;
            suite_update_ui::showAccess = true;
            for (auto phase : {suite_update::Phase::Available, suite_update::Phase::Downloading,
                               suite_update::Phase::Error})
            {
                snapshot->phase = phase;
                snapshot->message =
                    phase == suite_update::Phase::Error
                        ? "Private builds require repository access. Connect a token with Contents: read access and try again."
                        : "A new PracticeSuite update is available.";
                for (int f = 0; f < 3; f++)
                {
                    ImGui::NewFrame();
                    suite_ui::BeginFrame(true, false);
                    ImGui::SetNextWindowPos({12, 12}, ImGuiCond_Always);
                    ImGui::SetNextWindowSize({size.x - 24, size.y - 24}, ImGuiCond_Always);
                    ImGui::Begin("Updates / UI verification");
                    suite_update_ui::Page(*snapshot, false);
                    ImGui::End();
                    ImGui::Render();
                    CheckHudRegions();
                }
                if (!output.empty() && size.x == 1920 && scale == 1 &&
                    phase == suite_update::Phase::Available)
                    Dump(output + "/updates-workspace.draw");
            }
            snapshot->phase = suite_update::Phase::Available;
            snapshot->message = "A new PracticeSuite update is available.";
            suite_update::service.presentation.changelog = true;
            suite_update::service.presentation.appeared = 10;
            suite_ui::lifecycleTime = 11;
            for (int f = 0; f < 3; f++)
            {
                ImGui::NewFrame();
                suite_ui::BeginFrame(true, false);
                suite_update_ui::Overlays(snapshot, false);
                ImGui::Render();
                CheckHudRegions();
            }
            assert(suite_update_ui::history.size() == 3);
            assert(suite_update_ui::history[suite_update_ui::selected].version == suite_brand::Version);
            if (!output.empty() && size.x == 1920 && scale == 1)
                Dump(output + "/update-changelog.draw");
            suite_update::service.presentation.changelog = false;
            suite_update::service.presentation.prompt = true;
            for (int f = 0; f < 3; f++)
            {
                ImGui::NewFrame();
                suite_ui::BeginFrame(true, false);
                suite_update_ui::Overlays(snapshot, false);
                ImGui::Render();
                CheckHudRegions();
            }
            if (!output.empty() && size.x == 1920 && scale == 1)
                Dump(output + "/update-notification.draw");
            // Active runs suspend all update popups without consuming them.
            ImGui::NewFrame();
            suite_ui::BeginFrame(false, true);
            suite_update_ui::Overlays(snapshot, true);
            ImGui::Render();
            for (auto *window : GImGui->Windows)
                if (std::strstr(window->Name, "##SuiteUpdateAvailable"))
                    assert(window->LastFrameActive != ImGui::GetFrameCount());
        }
    suite_update::service.presentation = {};
    suite_update_ui::showAccess = false;
    suite_ui::lifecycleTime = -1;
    io.DisplaySize = {1280, 900};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    data = saved;
    std::cout
        << "Actual update workspace, complete changelog/history, in-game notification, 480/1280/1920 and 100/150 DPI: PASS\n";
}
