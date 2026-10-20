void ReplayInputCases(const std::string &output)
{
    auto saved = data;
    auto &io = ImGui::GetIO();
    g_pShared = &data;
    data.timerState = 0;
    pendingRequest = false;
    auto base = std::filesystem::path(output).parent_path() / "managed-data";
    auto read = [](const auto &p) {
        std::ifstream in(p, std::ios::binary);
        assert(in);
        return std::vector<uint8_t>{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    };
    std::string error;
    auto motion = practice::DecodeMotion(read(base / "input-wire.isinput"), error);
    auto map = practice::DecodeMap(read(base / "input-map.isrun"), error);
    assert(motion && map && motion->Matches(*map));
    practice::ReviewPlayback playback;
    playback.Bind(motion->id);
    playback.playing = false;
    playback.seconds = .25;
    io.DisplaySize = {1280, 900};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    io.AddMousePosEvent(-1000, -1000);
    suite_ui::preferences.motion = false;
    auto controls = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        ImGui::SetNextWindowPos({30, 30}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({440, 800}, ImGuiCond_Always);
        ImGui::Begin("Replay input QA");
        RenderReplayInputs(*motion, playback);
        ImGui::End();
        ImGui::Render();
        CheckHudRegions();
    };
    for (int i = 0; i < 3; i++)
    {
        controls();
    }
    auto *window = ImGui::FindWindowByName("Replay input QA");
    assert(window);
    double initial = playback.seconds;
    ImGui::ActivateItemByID(window->GetID("Frame >"));
    controls();
    assert(playback.seconds > initial && !playback.playing);
    ImGui::ActivateItemByID(window->GetID("< Frame"));
    controls();
    assert(playback.seconds <= initial);
    ImGui::ActivateItemByID(window->GetID("Events"));
    controls();
    assert(!GImGui->OpenPopupStack.empty());
    auto *popup = GImGui->OpenPopupStack.back().Window;
    assert(popup);
    assert(popup->Size.x <= io.DisplaySize.x);
    GImGui->OpenPopupStack.clear();
    for (auto viewport : {ImVec2(480, 360), ImVec2(1920, 1080)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = viewport;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            playback.seconds = .25;
            for (int i = 0; i < 3; i++)
                controls();
        }
    data.replay3dState = 2;
    data.replay3dRunId = motion->id;
    data.replay3dSeconds = .25;
    g_commands = {};
    playback.seconds = .4;
    playback.playing = false;
    playback.speed = .5f;
    Replay3DControl(*map->review, playback, true);
    assert(g_commands.replay3dCmd == 3 && g_commands.replay3dRunId == motion->id &&
           g_commands.replay3dSeconds == .4 &&
           g_commands.replay3dOptions == (5762u | 65536u | (100u << 24)) && g_commands.replay3dSpeed == .5f);
    g_commands = {};
    data.replay3dRunId++;
    Replay3DControl(*map->review, playback, false);
    assert(g_commands.replay3dCmd == 0);
    data.replay3dRunId = motion->id;
    StopReplay3D();
    assert(g_commands.replay3dCmd == 2);
    Replay3DControl(*map->review, playback, false);
    assert(g_commands.replay3dCmd == 2);
    auto fixtureSource = base / "input-store-test" / "RunReviews";
    std::filesystem::create_directories(fixtureSource);
    for (const char *extension : {".isinput", ".isrun"})
    {
        auto bytes =
            read(base / (std::string(extension) == ".isinput" ? "input-wire.isinput" : "input-map.isrun"));
        std::ofstream out(fixtureSource / ("run-" + std::to_string(motion->id) + extension),
                          std::ios::binary);
        out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
        assert(out);
    }
    data.replay3dState = 0;
    auto root = std::filesystem::path(output) / "motion-ui-library";
    std::filesystem::remove_all(root);
    practice::g_replayArchive.SetDirectory(root);
    practice::g_replayArchive.SetSourceDirectory(base / "input-store-test" / "RunReviews");
    practice::g_replayArchive.LoadMotion(map);
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < end && !practice::g_replayArchive.Motion())
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    assert(practice::g_replayArchive.Motion() && practice::g_replayArchive.Motion()->Matches(*map));
    g_mapWorker.Submit(map->bytes, g_session);
    end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < end)
    {
        auto result = g_mapWorker.Get(MapLevelIndex(map->scene));
        if (result && result->data->review && result->data->review->id == motion->id && !result->working)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    g_runReviewOpen = true;
    g_reviewFromArchive = false;
    g_reviewTarget = motion->id;
    g_reviewLevel = MapLevelIndex(map->scene);
    g_reviewPlayback = playback;
    g_reviewPlayback.seconds = .25;
    data.reviewRunId = motion->id;
    data.reviewRetained = data.reviewReady = 1;
    data.reviewScene = map->scene;
    data.replay3dState = 0;
    g_commands = {};
    io.DisplaySize = {1920, 1080};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    auto full = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        ImGui::SetNextWindowPos({50, 40}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({1580, 970}, ImGuiCond_Always);
        ImGui::Begin("Replay / recorded inputs / synthetic test data");
        RenderRunReview();
        ImGui::End();
        ImGui::Render();
        CheckHudRegions();
    };
    for (int i = 0; i < 4; i++)
        full();
    if (!output.empty())
        Dump(output + "/replay-inputs-720.draw");
    data.replay3dState = 2;
    data.replay3dRunId = motion->id;
    data.replay3dSeconds = .25;
    g_overlayVisible = false;
    g_compactMode = false;
    for (int i = 0; i < 3; i++)
    {
        ImGui::NewFrame();
        RenderReplay3DOverlay();
        ImGui::Render();
        CheckHudRegions();
    }
    assert(ImGui::FindWindowByName("##Replay3D status"));
    if (!output.empty())
        Dump(output + "/replay-3d-status-720.draw");
    // Directly operate the production 3D dock with recorded test data.
    suite_replay::controls = true;
    suite_replay::Bind(map);
    data.replay3dSpeed = 1;
    data.replay3dSeconds = .25;
    g_commands = {};
    auto dock = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(suite_replay::controls, false);
        RenderReplay3DOverlay();
        ImGui::Render();
        CheckHudRegions();
    };
    for (int i = 0; i < 4; i++)
        dock();
    auto *dockWindow = ImGui::FindWindowByName("##Replay3D status");
    assert(dockWindow && dockWindow->Pos.x > 300 &&
           dockWindow->Pos.y + dockWindow->Size.y <= io.DisplaySize.y &&
           suite_replay::source->review->id == motion->id);
    ImGui::ActivateItemByID(dockWindow->GetID("Frame >"));
    dock();
    assert(g_commands.replay3dCmd == 3 && g_commands.replay3dSeconds > .25 &&
           !(g_commands.replay3dOptions & 1));
    g_commands = {};
    ImGui::ActivateItemByID(dockWindow->GetID("Pause"));
    dock();
    assert(g_commands.replay3dCmd == 3 && !(g_commands.replay3dOptions & 1));
    g_commands = {};
    assert(suite_replay::ControlsRequested(&data) && !suite_replay::ControlsRequested(nullptr));
    data.timerState = 1;
    assert(!suite_replay::ControlsRequested(&data));
    data.timerState = 0;
    ImGui::ActivateItemByID(dockWindow->GetID("Time (s)"));
    dock();
    io.AddInputCharactersUTF8("0.125");
    dock();
    assert(g_commands.replay3dCmd == 0);
    ImGui::ActivateItemByID(dockWindow->GetID("Go"));
    dock();
    assert(g_commands.replay3dCmd == 3 && g_commands.replay3dSeconds == .125 &&
           !(g_commands.replay3dOptions & 1) && (g_commands.replay3dOptions & 1024));
    g_commands = {};
    ImGui::ActivateItemByID(dockWindow->GetID("Time (s)"));
    dock();
    io.AddInputCharactersUTF8("0.200");
    dock();
    io.AddKeyEvent(ImGuiKey_Enter, true);
    dock();
    io.AddKeyEvent(ImGuiKey_Enter, false);
    assert(g_commands.replay3dCmd == 3 && g_commands.replay3dSeconds == .2 &&
           !(g_commands.replay3dOptions & 1));
    g_commands = {};
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!suite_replay::marks && std::chrono::steady_clock::now() < until)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        dock();
    }
    assert(suite_replay::marks);
    data.replay3dSeconds = .1;
    ImGui::ActivateItemByID(dockWindow->GetID("Set A"));
    dock();
    assert(suite_replay::marks->hasA && suite_replay::marks->a == .1);
    g_commands = {};
    data.replay3dSeconds = .4;
    ImGui::ActivateItemByID(dockWindow->GetID("Set B"));
    dock();
    assert(suite_replay::marks->LoopValid());
    g_commands = {};
    ImGui::ActivateItemByID(dockWindow->GetID("A-B loop"));
    dock();
    assert(suite_replay::marks->loop && g_commands.replay3dCmd == 3 && (g_commands.replay3dOptions & 256) &&
           g_commands.replay3dLoopA == .1 && g_commands.replay3dLoopB == .4 &&
           g_commands.replay3dSeconds == .1);
    g_commands = {};
    assert(std::abs(suite_replay::LoopTime(1.1, true) - .2) < 1e-8 &&
           suite_replay::LoopTime(1.1, false) == 1.1);
    ImGui::ActivateItemByID(dockWindow->GetID("3D options"));
    dock();
    assert(!GImGui->OpenPopupStack.empty());
    auto *viewOptions = GImGui->OpenPopupStack.back().Window;
    assert(viewOptions);
    ImGui::ActivateItemByID(viewOptions->GetID("Player collider"));
    dock();
    assert(suite_replay::player && g_commands.replay3dCmd == 3 && (g_commands.replay3dOptions & 8) &&
           !(g_commands.replay3dOptions & 1024));
    g_commands = {};
    ImGui::ActivateItemByID(viewOptions->GetID("Monster colliders"));
    dock();
    assert(suite_replay::monster && (g_commands.replay3dOptions & 16) && (g_commands.replay3dOptions & 512));
    g_commands = {};
    ImGui::ActivateItemByID(viewOptions->GetID("Crystal colliders"));
    dock();
    assert(suite_replay::crystal && (g_commands.replay3dOptions & 32) &&
           !(g_commands.replay3dOptions & 1024));
    GImGui->OpenPopupStack.clear();
    g_commands = {};
    suite_replay::camera = 1;
    Replay3DControl(*map->review, suite_replay::playback, false);
    assert((g_commands.replay3dOptions & 4) && !(g_commands.replay3dOptions & 2));
    g_commands = {};
    ImGui::ActivateItemByID(dockWindow->GetID("3D options"));
    dock();
    viewOptions = GImGui->OpenPopupStack.back().Window;
    assert(viewOptions);
    ImGui::ActivateItemByID(viewOptions->GetID("Hide ceiling"));
    dock();
    assert(suite_replay::hideCeiling && (g_commands.replay3dOptions & 2048) &&
           !(g_commands.replay3dOptions & 1024));
    g_commands = {};
    ImGui::ActivateItemByID(viewOptions->GetID("Fade blocking walls"));
    dock();
    assert(!suite_replay::fadeWalls && !(g_commands.replay3dOptions & 4096));
    g_commands = {};
    ImGui::ActivateItemByID(viewOptions->GetID("Fade blocking walls"));
    dock();
    assert(suite_replay::fadeWalls && (g_commands.replay3dOptions & 4096));
    GImGui->OpenPopupStack.clear();
    g_commands = {};
    suite_replay::requestMarker = true;
    data.replay3dSeconds = .25;
    dock();
    dock();
    io.AddInputCharactersUTF8("Check corner");
    dock();
    assert(!GImGui->OpenPopupStack.empty());
    auto *markerPopup = GImGui->OpenPopupStack.back().Window;
    assert(markerPopup);
    ImGui::ActivateItemByID(markerPopup->GetID("Save marker"));
    dock();
    assert(suite_replay::marks->markers.size() == 1 &&
           suite_replay::marks->markers[0].name == "Check corner" &&
           map->review->flashes.size() == suite_replay::source->review->flashes.size());
    g_commands = {};
    data.replay3dSeconds = .25;
    assert(Replay3DShortcut(1) && g_commands.replay3dCmd == 3 && g_commands.replay3dSeconds < .25);
    g_commands = {};
    data.timerState = 1;
    assert(!Replay3DShortcut(0) && g_commands.replay3dCmd == 0);
    data.timerState = 0;
    for (auto viewport : {ImVec2(480, 360), ImVec2(1920, 1080)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = viewport;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            for (int i = 0; i < 3; i++)
                dock();
            dockWindow = ImGui::FindWindowByName("##Replay3D status");
            assert(dockWindow->Pos.y >= 0 && dockWindow->Pos.y + dockWindow->Size.y <= viewport.y &&
                   dockWindow->Size.x <= viewport.x);
        }
    io.DisplaySize = {1920, 1080};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    for (int i = 0; i < 3; i++)
        dock();
    if (!output.empty())
        Dump(output + "/replay-3d-dock-735.draw");
    io.DisplaySize = {1120, 540};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    for (int i = 0; i < 3; i++)
    {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        RenderReplay3DOverlay();
        ImGui::GetForegroundDrawList()->AddText({16, 14}, IM_COL32(160, 175, 181, 255),
                                                "GUI preview / synthetic test data / not a game capture");
        ImGui::Render();
        CheckHudRegions();
    }
    if (!output.empty())
        Dump(output + "/replay-3d-preview-735.draw");
    io.DisplaySize = {1920, 1080};
    for (int i = 0; i < 3; i++)
        dock();
    ImGui::ActivateItemByID(dockWindow->GetID("Hide controls [Tab]"));
    dock();
    assert(!suite_replay::controls && !lastInteractiveRequest);
    for (int i = 0; i < 3; i++)
        dock();
    if (!output.empty())
        Dump(output + "/replay-3d-hud-735.draw");
    suite_replay::controls = true;
    dock();
    ImGui::ActivateItemByID(dockWindow->GetID("Exit 3D"));
    dock();
    assert(g_commands.replay3dCmd == 2 && !suite_replay::controls && g_overlayVisible &&
           lastInteractiveRequest);
    suite_replay::player = false;
    suite_replay::camera = 0;
    std::cout
        << "3D dock: production play/pause/frame controls, direct time entry with Go/Enter, input ownership guard, A-B transport and persistence, collider/follow/ceiling/wall options, named marker entry, keyboard active-run guard, centre placement, 480/1920 at 100/150 percent DPI and explicit focus return: PASS\n";
    data = saved;
    g_runReviewOpen = false;
    g_reviewFromArchive = false;
    g_commands = {};
    suite_ui::preferences.motion = true;
    g_overlayVisible = true;
    std::cout
        << "Replay input UI: actual frame-step and event controls, 480/1920 viewports at 100/150 percent DPI, 3D command identity/stop priority, synced input panel and clearly-labelled passive 3D status: PASS\n";
}
