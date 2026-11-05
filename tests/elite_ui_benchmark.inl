// Optional measurement of production ImGui submission and mesh construction.
// Fixture preparation, file writes and native/GPU presentation are excluded.
void EliteUiBenchmark()
{
    auto &io = ImGui::GetIO();
    io.DisplaySize = {1280, 720};
    io.DeltaTime = 1.f / 60;
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    suite_ui::preferences = {};
    suite_ui::preferences.launchGraphic = false;
    suite_ui::appearance = {};
    suite_run::presentation.Reset();
    data = {};
    data.pid = 1234;
    data.currentSceneIndex = 3;
    data.sceneEpoch = 722;
    data.playerReady = data.health = data.rngPatchComplete = 1;
    g_session = 722;
    g_pShared = &data;
    g_allOverlayEnabled = g_overlayVisible = true;
    g_speedrunMode = g_practiceMode = g_compactMode = g_runReviewOpen = g_selectRunReview = false;
    pendingRequest = false;
    auto frame = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        RenderMainWindow("Classic", data.runSeed);
        ImGui::Render();
    };
    g_resetLayout = true;
    frame();
    g_resetLayout = false;
    auto measure = [&](const char *name, suite_ui::Page page, auto configure) {
        suite_ui::navigation.Select(page, ImGui::GetTime());
        frame();
        configure();
        for (int i = 0; i < 240; ++i)
            frame();
        std::vector<double> milliseconds;
        milliseconds.reserve(240);
        for (int i = 0; i < 240; ++i)
        {
            auto start = std::chrono::steady_clock::now();
            frame();
            milliseconds.push_back(std::chrono::duration<double, std::milli>(
                                       std::chrono::steady_clock::now() - start).count());
        }
        std::sort(milliseconds.begin(), milliseconds.end());
        std::cout << "UI CPU 1280x720 " << name << ": median " << milliseconds[120]
                  << " ms; p95 " << milliseconds[228] << " ms; ImGui vertices "
                  << ImGui::GetDrawData()->TotalVtxCount << '\n';
    };
    measure("mode dashboard", suite_ui::Page::Speedrun, [] {});
    auto fixture = SeedFixture();
    fixture.epoch = 722;
    fixture.agentRadius = .35f;
    fixture.agentHeight = 1.8f;
    fixture.movementTick = .02f;
    fixture.movementSprintStep = .14f;
    auto initial = StreamingSource(fixture, 722);
    auto source = std::make_shared<practice::MapAnalysis>(*initial);
    source->session = g_session;
    auto request = StreamingRequest(*source, practice::SeedSpace, 2);
    g_seedSearch.Begin(request);
    g_seedSearch.Update(source);
    auto batch = StreamingBatch(request, 0, 20, fixture.samples);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!g_seedSearch.Submit(batch, request.session) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    g_mapWorker.Submit(EncodeFixture(*source->data), g_session);
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto map = g_mapWorker.Get(0);
        if (map && !map->working && !g_seedSearch.Get()->top.empty())
            break;
        std::this_thread::yield();
    }
    assert(g_seedSearch.Busy() && !g_seedSearch.Get()->top.empty());
    g_plannerOptions.level = 0;
    measure("seed search with results / busy", suite_ui::Page::SeedMap, [&] {
        for (int i = 0; i < GImGui->TabBars.GetMapSize(); ++i)
        {
            auto *tabs = GImGui->TabBars.TryGetMapData(i);
            if (tabs && tabs->CurrFrameVisible == ImGui::GetFrameCount() && tabs->Tabs.Size == 2 &&
                std::strcmp(ImGui::TabBarGetTabName(tabs, &tabs->Tabs[0]), "Preview & replay") == 0)
                tabs->NextSelectedTabId = tabs->Tabs[1].ID;
        }
    });
    g_seedSearch.Stop();
    fixture.samples.clear();
    fixture.requested = fixture.completed = 0;
    auto run = std::make_shared<practice::RunReview>();
    run->id = 638961530000000722ll;
    run->outcome = 1;
    run->seconds = 28;
    run->validationChecked = run->valid = run->observedFromStart = run->seedKnown = true;
    run->seedKind = 1;
    run->validityReason = "Synthetic GUI benchmark recording";
    run->collected = {0, 1, 2, 3};
    run->pickupSeconds = {7, 14, 21, 27};
    for (int i = 0; i <= 1400; ++i)
        run->trace.push_back({i * .02, fixture.points[size_t(i / 350) % fixture.points.size()], true});
    fixture.review = run;
    g_mapWorker.Submit(EncodeFixture(fixture, 10), g_session);
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto map = g_mapWorker.Get(0);
        if (map && !map->working && map->data->review && map->data->review->id == run->id)
            break;
        std::this_thread::yield();
    }
    data.reviewReady = data.reviewRetained = 1;
    data.reviewRunId = run->id;
    data.reviewScene = 3;
    data.timerState = 2;
    g_runReviewOpen = true;
    g_reviewFromArchive = false;
    g_reviewTarget = run->id;
    g_reviewLevel = 0;
    g_reviewWide = true;
    g_reviewPlayback = {};
    g_reviewPlayback.run = run->id;
    g_reviewPlayback.seconds = 12;
    g_speedrunMode = true;
    measure("replay / 1401 samples", suite_ui::Page::Replay, [] {});
    g_runReviewOpen = g_reviewWide = g_speedrunMode = false;
    measure("appearance settings", suite_ui::Page::Settings, [] {});
    std::cout << "Host CPU measurements only; GPU submission, presentation, game load and monitor cadence are not measured.\n";
}
