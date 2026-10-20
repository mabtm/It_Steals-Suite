// Execute production widgets. The gallery uses synthetic observations and
// the same map worker, search engine and renderer as the application.
void DesignInteractionCases()
{
    auto saved = data;
    auto &io = ImGui::GetIO();
    auto config = io.ConfigFlags;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = {1280, 900};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    suite_run::presentation.Reset();
    suite_ui::preferences = {};
    suite_ui::notice.clear();
    suite_ui::run = false;
    g_pShared = &data;
    data.timerState = 0;
    g_allOverlayEnabled = g_overlayVisible = true;
    g_compactMode = g_speedrunMode = g_practiceMode = g_runReviewOpen = false;
    g_selectRunReview = false;
    suite_ui::navigation = {};
    g_resetLayout = true;
    Frame();
    g_resetLayout = false;
    Frame();
    Frame();
    ImGuiWindow *rail = nullptr;
    for (auto *w : GImGui->Windows)
        if (std::strstr(w->Name, "/Navigation_") && w->LastFrameActive == ImGui::GetFrameCount())
            rail = w;
    assert(rail);
    auto click = rail->DC.CursorStartPos;
    click.x += rail->Size.x * .5f;
    click.y += 2 * (50 + ImGui::GetStyle().ItemSpacing.y) + 25;
    io.AddMousePosEvent(click.x, click.y);
    Frame();
    assert(suite_ui::navigation.Is(suite_ui::Page::Cheats));
    io.AddMouseButtonEvent(0, true);
    Frame();
    io.AddMouseButtonEvent(0, false);
    Frame();
    assert(suite_ui::navigation.Is(suite_ui::Page::Speedrun));
    ImGui::ActivateItemByID(ImHashStr("##Navigate", 0, rail->GetID(3)));
    Frame();
    assert(suite_ui::navigation.Is(suite_ui::Page::Settings));
    suite_ui::navigation.Select(suite_ui::Page::Cheats, ImGui::GetTime());
    g_speedrunMode = true;
    g_practiceMode = false;
    g_hitboxCheck = false;
    Frame();
    Frame();
    ImGuiWindow *tools = nullptr;
    for (auto *w : GImGui->Windows)
        if (std::strstr(w->Name, "/PracticeScroll_") && w->LastFrameActive == ImGui::GetFrameCount())
            tools = w;
    assert(tools);
    auto collider = ImHashStr("##Feature", 0, tools->GetID("Actual colliders"));
    ImGui::ActivateItemByID(collider);
    Frame();
    assert(!g_hitboxCheck);
    g_speedrunMode = false;
    g_pShared = nullptr;
    Frame();
    ImGui::ActivateItemByID(collider);
    Frame();
    assert(!g_hitboxCheck);
    g_pShared = &data;
    bool value = false, locked = false;
    int changes = 0;
    ImVec2 at;
    ImGuiID feature = 0;
    auto card = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        ImGui::SetNextWindowPos({40, 40}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({720, 260}, ImGuiCond_Always);
        ImGui::Begin("Design cards QA");
        ImGui::BeginDisabled(locked);
        if (suite_ui::FeatureCard("Actual colliders", suite_ui::Icon::Collider, &value, "Exact live shapes",
                                  "Actual collider geometry"))
            changes++;
        feature = ImGui::GetItemID();
        at = ImGui::GetItemRectMin();
        at.x += 90;
        at.y += 35;
        ImGui::EndDisabled();
        ImGui::End();
        ImGui::Render();
        CheckHudRegions();
    };
    io.AddMousePosEvent(-1000, -1000);
    for (int f = 0; f < 3; f++)
        card();
    suite_ui::ConsumeTone();
    io.AddMousePosEvent(at.x, at.y);
    card();
    io.AddMouseButtonEvent(0, true);
    card();
    io.AddMouseButtonEvent(0, false);
    card();
    assert(value && changes == 1 && suite_ui::ConsumeTone() == suite_ui::Tone::On);
    locked = true;
    io.AddMouseButtonEvent(0, true);
    card();
    io.AddMouseButtonEvent(0, false);
    card();
    assert(value && changes == 1 && suite_ui::ConsumeTone() == suite_ui::Tone::None);
    locked = false;
    ImGui::ActivateItemByID(feature);
    card();
    assert(!value && changes == 2);
    practice::RunReview review;
    review.id = 710;
    review.seconds = 10;
    review.pickupSeconds = {2};
    review.flashes = {{6.25, {}, 1, 0}};
    practice::ReviewPlayback playback;
    playback.seconds = 0;
    playback.playing = true;
    ImVec2 track;
    float trackWidth = 0;
    ImGuiID timeline = 0;
    ImGuiWindow *player = nullptr;
    auto timelineFrame = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        ImGui::SetNextWindowPos({40, 40}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({800, 240}, ImGuiCond_Always);
        ImGui::Begin("Timeline events QA");
        player = ImGui::GetCurrentWindow();
        auto bounds = RenderReplayTimeline(review, playback, 10);
        track = bounds.Min;
        trackWidth = bounds.GetWidth();
        timeline = player->GetID("##Review timeline");
        ImGui::End();
        ImGui::Render();
        CheckHudRegions();
    };
    io.AddMousePosEvent(-1000, -1000);
    for (int f = 0; f < 3; f++)
        timelineFrame();
    auto seek = [&](double seconds, bool flash) {
        float x = track.x + 8 + (trackWidth - 16) * float(seconds / 10);
        io.AddMousePosEvent(x, track.y + (flash ? 14 : 38));
        timelineFrame();
        io.AddMouseButtonEvent(0, true);
        timelineFrame();
        io.AddMouseButtonEvent(0, false);
        timelineFrame();
        assert(std::abs(playback.seconds - seconds) < 1e-8 && !playback.playing);
    };
    seek(2, false);
    seek(6.25, true);
    float x = track.x + 8 + (trackWidth - 16) * .4f;
    io.AddMousePosEvent(x, track.y + 27);
    timelineFrame();
    io.AddMouseButtonEvent(0, true);
    timelineFrame();
    io.AddMousePosEvent(track.x + 8 + (trackWidth - 16) * .9f, track.y + 27);
    timelineFrame();
    io.AddMouseButtonEvent(0, false);
    timelineFrame();
    assert(std::abs(playback.seconds - 9) <= 10 / (trackWidth - 16) + 1e-6 && !playback.playing);
    ImGui::FocusWindow(player);
    ImGui::SetFocusID(timeline, player);
    timelineFrame();
    io.AddKeyEvent(ImGuiKey_Home, true);
    timelineFrame();
    io.AddKeyEvent(ImGuiKey_Home, false);
    timelineFrame();
    assert(playback.seconds == 0);
    io.AddKeyEvent(ImGuiKey_RightArrow, true);
    timelineFrame();
    io.AddKeyEvent(ImGuiKey_RightArrow, false);
    timelineFrame();
    assert(std::abs(playback.seconds - .1) < 1e-8 && GImGui->NavId == timeline);
    io.AddKeyEvent(ImGuiMod_Shift, true);
    io.AddKeyEvent(ImGuiKey_RightArrow, true);
    timelineFrame();
    io.AddKeyEvent(ImGuiKey_RightArrow, false);
    io.AddKeyEvent(ImGuiMod_Shift, false);
    timelineFrame();
    assert(std::abs(playback.seconds - 1.1) < 1e-8 && GImGui->NavId == timeline);
    ImGui::FocusWindow(player);
    ImGui::SetFocusID(timeline, player);
    io.AddKeyEvent(ImGuiKey_End, true);
    timelineFrame();
    io.AddKeyEvent(ImGuiKey_End, false);
    timelineFrame();
    assert(playback.seconds == 10);
    auto retained = playback.seconds;
    review.flashes.clear();
    review.pickupSeconds.clear();
    review.id++;
    for (int f = 0; f < 3; f++)
        timelineFrame();
    assert(playback.seconds == retained);
    suite_ui::navigation = {};
    suite_ui::preferences = {};
    suite_ui::ConsumeTone();
    io.AddMousePosEvent(-1000, -1000);
    data = saved;
    io.ConfigFlags = config;
    std::cout
        << "Design interaction: actual mouse and keyboard sidebar; feature card toggle once / disabled lock; pickup and flash click seek exact timestamps; timeline drag, Home/End and cache rebinding; bounded clipped drawing: PASS\n";
}

void DesignGallery(const std::string &output)
{
    if (output.empty())
        return;
    auto &io = ImGui::GetIO();
    io.DisplaySize = {1920, 1080};
    io.DeltaTime = .1f;
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    suite_ui::preferences = {};
    suite_ui::notice.clear();
    suite_ui::lifecycleTime = -1;
    suite_run::presentation.Reset();
    data = {};
    data.pid = 1234;
    data.currentSceneIndex = 3;
    data.sceneEpoch = 710;
    data.playerReady = data.health = 1;
    data.rngPatchComplete = 1;
    std::strcpy(data.version, "v7.1.0");
    g_session = 710;
    g_pShared = &data;
    g_allOverlayEnabled = g_overlayVisible = true;
    g_speedrunMode = g_practiceMode = g_compactMode = g_runReviewOpen = g_reviewWide = g_selectRunReview =
        false;
    pendingRequest = false;
    g_commands = {};
    g_hitboxCheck = true;
    g_brightnessOn = false;
    g_showMinimapWindow = true;
    g_minimapWidth = g_minimapHeight = 330;
    auto menuFrame = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        RenderMainWindow("Classic", data.runSeed);
        ImGui::Render();
        CheckHudRegions();
    };
    suite_ui::navigation = {};
    g_resetLayout = true;
    menuFrame();
    g_resetLayout = false;
    auto *root = ImGui::FindWindowByName("It Steals | Practice Suite");
    ImGui::SetWindowPos(root, {100, 80});
    ImGui::SetWindowSize(root, {1140, 920});
    for (int i = 0; i < 5; i++)
        menuFrame();
    Dump(output + "/tour-01-tools.draw");
    auto fixture = SeedFixture();
    fixture.epoch = 710;
    fixture.agentRadius = .35f;
    fixture.agentHeight = 1.8f;
    fixture.agentSlope = 45;
    fixture.agentStep = .3f;
    fixture.movementTick = .02f;
    fixture.movementSprintStep = .14f;
    auto initial = StreamingSource(fixture, 710);
    auto source = std::make_shared<practice::MapAnalysis>(*initial);
    source->session = g_session;
    auto request = StreamingRequest(*source, practice::SeedSpace, 2);
    g_seedSearch.Begin(request);
    g_seedSearch.Update(source);
    auto batch = StreamingBatch(request, 0, 20, fixture.samples);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!g_seedSearch.Submit(batch, request.session) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    while (g_seedSearch.Get()->top.empty() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(!g_seedSearch.Get()->top.empty());
    auto searchData = *source->data;
    g_mapWorker.Submit(EncodeFixture(searchData), g_session);
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto m = g_mapWorker.Get(0);
        if (m && m->data->requestSerial == 710 && !m->working)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    g_plannerOptions.level = 0;
    g_plannerOptions.filters[0].spawn = -1;
    suite_ui::navigation.Select(suite_ui::Page::SeedMap, ImGui::GetTime());
    for (int i = 0; i < 4; i++)
        menuFrame();
    ImGuiTabBar *seedTabs = nullptr;
    for (int i = 0; i < GImGui->TabBars.GetMapSize(); i++)
    {
        auto *b = GImGui->TabBars.TryGetMapData(i);
        if (b && b->CurrFrameVisible == ImGui::GetFrameCount() && b->Tabs.Size == 2 &&
            std::strcmp(ImGui::TabBarGetTabName(b, &b->Tabs[0]), "Preview & replay") == 0)
            seedTabs = b;
    }
    assert(seedTabs);
    seedTabs->NextSelectedTabId = seedTabs->Tabs[1].ID;
    for (int f = 0; f < 8; f++)
    {
        menuFrame();
        Dump(output + "/tour-02-search-" + std::to_string(f) + ".draw");
    }
    g_seedSearch.Stop();
    fixture.recipe = 1;
    fixture.seed = 482915720;
    fixture.requestSerial = 0;
    fixture.samples.clear();
    fixture.requested = fixture.completed = 0;
    auto captured = std::make_shared<practice::MapData>(fixture);
    practice::NavGraph graph(fixture);
    auto route = practice::AnalyzeMap(captured, graph).route;
    assert(route.complete && !route.estimated);
    auto run = std::make_shared<practice::RunReview>();
    run->id = 638961530000000000ll;
    run->outcome = 1;
    run->seconds = 28;
    run->validationChecked = run->valid = run->observedFromStart = run->seedKnown = true;
    run->seedKind = 1;
    run->validityReason = "Synthetic GUI test recording";
    run->collected = {0, 1, 2, 3};
    run->pickupSeconds = {7, 14, 21, 27};
    run->lightSources = 255;
    for (int i = 0; i <= 1400; i++)
    {
        run->trace.push_back({i * .02, practice::SuggestedSlice(route, i / 1400.).marker, true});
    }
    run->flashes = {{8, run->trace[400].point, 1, 0},
                    {14.5, run->trace[725].point, 2, 0},
                    {21, run->trace[1050].point, 1, 0}};
    practice::ReplayMonster monster;
    monster.id = 1;
    monster.kind = 1;
    for (int i = 0; i <= 280; i++)
        monster.trace.push_back(
            {i * .1, practice::SuggestedSlice(route, std::max(0., (i - 15) / 280.)).marker, true});
    monster.paths = {{0, route.line}};
    run->monsters = {monster};
    fixture.review = run;
    g_mapWorker.Submit(EncodeFixture(fixture, 10), g_session);
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto m = g_mapWorker.Get(0);
        if (m && m->data->review && m->data->review->id == run->id && !m->working)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto recordedMap = g_mapWorker.Get(0);
    if (!recordedMap || !recordedMap->data->review)
        std::cerr << "Gallery recording error: " << g_mapWorker.Error() << "\n";
    assert(recordedMap && recordedMap->data->review && recordedMap->data->review->id == run->id &&
           !recordedMap->working);
    data.reviewReady = 1;
    data.reviewRunId = run->id;
    data.reviewScene = 3;
    data.reviewRetained = 1;
    data.timerState = 2;
    g_runReviewOpen = true;
    g_reviewFromArchive = false;
    g_reviewTarget = run->id;
    g_reviewLevel = 0;
    g_reviewWide = true;
    g_reviewPlayback = {};
    g_reviewPlayback.run = run->id;
    g_reviewPlayback.playing = false;
    g_reviewPlayback.collectedOnly = false;
    g_speedrunMode = true;
    g_practiceMode = false;
    suite_ui::navigation.Select(suite_ui::Page::Replay, ImGui::GetTime());
    for (int i = 0; i < 4; i++)
        menuFrame();
    for (int f = 0; f < 12; f++)
    {
        g_reviewPlayback.seconds = 7.7 + f * .09;
        menuFrame();
        Dump(output + "/tour-03-replay-" + std::to_string(f) + ".draw");
    }
    g_runReviewOpen = g_reviewWide = false;
    g_speedrunMode = false;
    suite_ui::navigation.Select(suite_ui::Page::Settings, ImGui::GetTime());
    for (int f = 0; f < 5; f++)
        menuFrame();
    ImGui::SetWindowPos(root, {100, 80});
    ImGui::SetWindowSize(root, {1140, 920});
    for (int f = 0; f < 4; f++)
        menuFrame();
    Dump(output + "/tour-04-settings.draw");
    suite_ui::navigation.Select(suite_ui::Page::Diagnostics, ImGui::GetTime());
    for (int f = 0; f < 5; f++)
        menuFrame();
    Dump(output + "/tour-05-diagnostics.draw");
    // Passive overlays: same compass projection, custom palette and opacity.
    g_mapOptions = {};
    g_mapOptions.follow = false;
    g_mapOptions.anchor = 0;
    g_showMinimapWindow = true;
    g_minimapWidth = g_minimapHeight = 360;
    g_minimapOpacity = .92f;
    data.timerState = 1;
    data.speedrunModeActive = 1;
    data.currentRunTimeSeconds = 12.456;
    data.runValidity = 1;
    data.seedKnown = data.seedSourcesVerified = data.rngPatchComplete = data.observedFromStart =
        data.externalScanStatus = 1;
    data.seedKind = 1;
    data.gameFps = 190;
    data.smoothedSpeed = 9.7f;
    data.runProgress = .65f;
    data.runSeed = 482915720;
    data.playerX = 17;
    data.playerY = 1;
    data.playerZ = 12;
    data.playerRotY = 35;
    data.crystalCount = 4;
    data.monsterActive = 1;
    data.wallCount = 2;
    std::strcpy(data.gameAssemblyHash, "ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8");
    float walls[] = {10, 5, 15, 25, 20, 5, 25, 25};
    std::copy(walls, walls + 8, data.wallBoxes);
    for (int i = 0; i < 4; i++)
    {
        data.crystalPositions[i * 2] = fixture.points[i].x;
        data.crystalPositions[i * 2 + 1] = fixture.points[i].z;
    }
    for (int f = 0; f < 4; f++)
    {
        ImGui::NewFrame();
        suite_ui::BeginFrame(false, true);
        RenderMinimapWindow(17, 12, 35, 28, 24, 4, 2, 0, nullptr, 0, 35, 0, 30, true, true, false);
        OverlayLayout speed(true);
        speed.anchor = 1;
        speed.width = 370;
        speed.opacity = .92f;
        RenderOverlayPanel("Tour speedrun", data, speed, true, "Classic", .01);
        OverlayLayout compact(false);
        compact.anchor = 3;
        compact.width = 390;
        compact.opacity = .92f;
        RenderOverlayPanel("Tour compact", data, compact, false, "Classic", .01);
        ImGui::Render();
        CheckHudRegions();
    }
    Dump(output + "/tour-06-huds-map.draw");
    data.timerState = 2;
    data.runOutcome = 1;
    data.currentRunTimeSeconds = 28;
    data.previousBestSeconds = 29.35;
    data.lastRunPb = 1;
    data.reviewSpawnSeed = 482915720;
    data.reviewSamples = 1401;
    data.runStartedUtc = run->id;
    data.runEndedUtc = run->id + 280000000;
    data.runSceneIndex = 3;
    data.gameVersion[0] = '9';
    std::strcpy(data.unityVersion, "2021.2.19f1");
    auto &p = suite_run::presentation;
    p.Reset();
    p.hasResult = true;
    p.completed = data;
    p.completedSession = g_session;
    p.scene = 3;
    p.epoch = 710;
    p.ended = 100;
    suite_ui::lifecycleTime = 106;
    p.scan.elapsed = suite_run::ScanTimeline::Duration;
    suite_ui::ConsumeTone();
    g_speedrunMode = true;
    g_overlayVisible = false;
    g_practiceMode = false;
    for (int f = 0; f < 10; f++)
    {
        suite_ui::lifecycleTime = 106 + f * .12;
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        RenderRunExperience();
        ImGui::Render();
        CheckHudRegions();
        assert(suite_run::PersonalBest(p.completed) && p.awardAt == 106);
        auto tone = suite_ui::ConsumeTone();
        assert(tone == (f == 0 ? suite_ui::Tone::PersonalBest : suite_ui::Tone::None));
        Dump(output + "/tour-07-pb-" + std::to_string(f) + ".draw");
    }
    p.completed.runWarnings = 8192;
    p.evidenceBuilt = false;
    p.scan.notified = false;
    p.awardAt = -1;
    ImGui::NewFrame();
    suite_ui::BeginFrame(true, false);
    RenderRunExperience();
    ImGui::Render();
    assert(!suite_run::PersonalBest(p.completed) && p.awardAt == -1 &&
           suite_ui::ConsumeTone() == suite_ui::Tone::Complete);
    Dump(output + "/tour-07-invalid.draw");
    p.Reset();
    suite_ui::lifecycleTime = -1;
    g_speedrunMode = false;
    for (int f = 0; f < 8; f++)
    {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        ImGui::SetNextWindowPos({100, 90}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({960, 640}, ImGuiCond_Always);
        ImGui::Begin("Interface states / native GUI");
        suite_ui::PageHeading(suite_ui::Page::Replay);
        suite_ui::LoadingState("Gallery load", "Reading replay library", 180);
        suite_ui::EmptyState(
            "Gallery empty", suite_ui::Icon::Replay, "Your next run starts here",
            "Record a Speedrun or Practice attempt. Runs meeting the recording rule are kept automatically.",
            200);
        ImGui::End();
        ImGui::Render();
        CheckHudRegions();
        Dump(output + "/tour-08-states-" + std::to_string(f) + ".draw");
    }
    suite_ui::preferences.motion = false;
    p.Reset();
    assert(!suite_run::PersonalBest(data) || data.lastRunPb);
    io.DeltaTime = 1.f / 60;
    suite_ui::preferences = {};
    std::cout
        << "7.1 native gallery: actual menu/search/replay/compass/HUD/personal-best/loading drawing, synthetic fixtures labelled in preview documentation; one gated PB cue / no invalid-PB celebration: PASS\n";
}
