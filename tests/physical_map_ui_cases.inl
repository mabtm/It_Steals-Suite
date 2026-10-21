void PhysicalMapUICases(const std::string &output)
{
    auto fixture = SeedFixture();
    fixture.scene = 5;
    fixture.epoch = 710;
    fixture.agentRadius = .4f;
    fixture.agentHeight = .8f;
    fixture.agentSlope = 45;
    fixture.physicalCaptured = true;
    fixture.samples.clear();
    fixture.requested = fixture.completed = 0;
    fixture.physicalParts = {{3, {{10, 0, 5}, {15, 0, 5}, {15, 0, 25}, {10, 0, 25}}},
                             {3, {{20, 0, 5}, {25, 0, 5}, {25, 0, 25}, {20, 0, 25}}},
                             {2, {{5, 0, 9}, {6.3f, 0, 10}, {5.3f, 0, 11.3f}, {4, 0, 10.3f}}}};
    for (auto point : fixture.points)
        fixture.pickupRegions.push_back({true, point, 1.35f});
    fixture.CalculateBounds();
    g_mapWorker.Submit(EncodeFixture(fixture), g_session);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::shared_ptr<const practice::MapAnalysis> map;
    while (std::chrono::steady_clock::now() < deadline)
    {
        map = g_mapWorker.Get(2);
        if (map && !map->working && map->data && map->data->epoch == 710)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(map && map->data->physicalCaptured && map->floor && map->route.pickupOptimized);
    auto &io = ImGui::GetIO();
    auto display = io.DisplaySize;
    auto savedData = data;
    bool speedrun = g_speedrunMode, practice = g_practiceMode, show = g_showMinimapWindow;
    auto options = g_mapOptions;
    float mw = g_minimapWidth, mh = g_minimapHeight;
    data.pid = fixture.pid;
    data.currentSceneIndex = 5;
    data.sceneEpoch = 710;
    data.hideAndSeekMode = 1;
    g_speedrunMode = false;
    g_practiceMode = false;
    g_showMinimapWindow = true;
    g_mapOptions.walls = true;
    g_mapOptions.anchor = 0;
    g_mapOptions.playerUp = false;
    g_minimapWidth = g_minimapHeight = 340;
    for (auto size : {ImVec2(1920, 1080), ImVec2(480, 360)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = size;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            for (int frame = 0; frame < 3; frame++)
            {
                ImGui::NewFrame();
                suite_ui::BeginFrame(true, false);
                ImGui::SetNextWindowPos({24, 24}, ImGuiCond_Always);
                ImGui::SetNextWindowSize({std::min(1200.f, size.x - 48), std::min(900.f, size.y - 48)},
                                         ImGuiCond_Always);
                ImGui::Begin("PHYSICAL MAP / PICKUP CONTACTS");
                ImGui::TextDisabled("v%s / mab | synthetic QA layout", suite_brand::Version);
                ImGui::TextUnformatted("Solid contours / continuous pickup route");
                RenderSeedCanvas(*map, std::max(100.f, ImGui::GetContentRegionAvail().y - 15));
                ImGui::End();
                ImGui::Render();
                CheckHudRegions();
            }
            if (!output.empty())
                Dump(output + "/710-physical-map-" + std::to_string(int(size.x)) + "-dpi-" +
                     std::to_string(int(dpi * 100)) + ".draw");
            for (int frame = 0; frame < 3; frame++)
            {
                ImGui::NewFrame();
                RenderMinimapWindow(2, 2, 30, 0, 0, 0, 0, 0, nullptr, 0, 35, 0, 30, true, false, false);
                ImGui::Render();
                CheckHudRegions();
            }
            if (!output.empty() && size.x == 1920 && dpi == 1)
                Dump(output + "/710-physical-radar.draw");
        }
    data = savedData;
    g_speedrunMode = speedrun;
    g_practiceMode = practice;
    g_showMinimapWindow = show;
    g_mapOptions = options;
    g_minimapWidth = mw;
    g_minimapHeight = mh;
    io.DisplaySize = display;
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    std::cout
        << "7.10 actual ImGui: physical polygon map/radar, shared floor, pickup route at 480/1920 and 100/150 DPI: PASS\n";
}
