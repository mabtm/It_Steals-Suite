void Workshop752Cases(const std::string &output)
{
    using namespace suite_clip;
    auto savedData = data;
    auto &io = ImGui::GetIO();
    io.DisplaySize = {1920, 1080};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    data.timerState = 0;
    g_pShared = &data;
    auto map = std::make_shared<practice::MapData>(SeedFixture());
    map->minX = map->minZ = -20;
    map->maxX = map->maxZ = 20;
    map->walls = {{-19, -19, -18, 19}, {18, -19, 19, 19}, {-18, 18, 18, 19}, {-18, -19, 18, -18},
                  {-10, -10, -9, 8},   {-9, 7, 7, 8},     {5, -10, 6, 0}};
    auto run = std::make_shared<practice::RunReview>();
    run->id = 752;
    run->seconds = 12;
    run->outcome = 1;
    run->valid = run->validationChecked = true;
    for (int i = 0; i <= 240; i++)
        run->trace.push_back({i * .05, {float(-15 + i * .12), 0, float(-15 + i * .11)}, true});
    map->review = run;
    auto analysis = std::make_shared<practice::MapAnalysis>();
    analysis->data = map;
    auto motion = std::make_shared<practice::ReplayMotion>();
    motion->id = 752;
    motion->seconds = motion->deviceThrough = 12;
    for (int i = 0; i <= 720; i++)
    {
        practice::ReplayViewFrame frame;
        frame.seconds = i / 60.;
        frame.camera = {float(-15 + i * .04), 1.7f, float(-15 + i * .035)};
        frame.body = frame.camera;
        frame.cameraRotation = {0, 0, 0, 1};
        motion->frames.push_back(frame);
        auto input = suite_input::Demo();
        input.seconds = frame.seconds;
        input.frame = i;
        input.tick = unsigned(i * 5 / 6);
        motion->devices.push_back(input);
    }
    Open(analysis, motion);
    options.scene3d = true;
    options.camera = 0;
    options.colliders = false;
    Seek(1);
    SaveKey(false);
    assert(options.camera == 3 && content.track.keys[0].mode == 0);
    Seek(5.123456);
    SaveKey(false);
    SetCameraMode(2);
    content.freePose = {{-6, 8, -9}, Angles(35, 28), 70};
    fly.Align(content.freePose);
    UpdateEditingKey();
    content.track.keys[1].layerMask = Colliders;
    content.track.keys[1].layers = Colliders;
    Seek(9);
    SaveKey(false);
    SetCameraMode(1);
    content.track.keys[2].layerMask = Colliders;
    content.track.keys[2].layers = 0;
    content.track.keys[0].next = content.track.keys[1].next = Transition::Cut;
    assert(!ResolveFrame(options, content, 5.123455).options.colliders);
    assert(ResolveFrame(options, content, 5.123456).options.colliders);
    assert(ResolveFrame(options, content, 8.99999).options.colliders);
    assert(!ResolveFrame(options, content, 9).options.colliders);
    // Captures within the minimum separation never silently move an old point.
    auto beforeTrack = content.track;
    Seek(5.123956);
    SaveKey(false);
    assert(!error.empty() && content.track.keys.size() == 3 &&
           content.track.keys[1].seconds == beforeTrack.keys[1].seconds);
    error.clear();
    EditKey(1);
    auto panelFrame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({450, 1000}, ImGuiCond_Always);
        ImGui::Begin("752 camera QA");
        bool changed = false;
        CameraPanel(changed);
        if (changed)
            Changed();
        ImGui::End();
        ImGui::Render();
    };
    for (int i = 0; i < 3; i++)
        panelFrame();
    auto *panel = ImGui::FindWindowByName("752 camera QA");
    for (int mode : {0, 1, 2})
    {
        ImGui::ActivateItemByID(panel->GetID(ShotModeNames[mode]));
        panelFrame();
        assert(options.camera == 3 && content.track.keys[1].mode == mode && cameraEditing);
    }
    // Point-scoped scene switches use the same controls the user clicks.
    auto tree = panel->GetID("Scene layers");
    panel->StateStorage.SetInt(tree, 1);
    panelFrame();
    auto colliderControl = ImHashStr("Actual colliders", 0, tree);
    ImGui::ActivateItemByID(colliderControl);
    panelFrame();
    assert(content.track.keys[1].layerMask & Colliders);
    assert(!(content.track.keys[1].layers & Colliders) && !options.colliders);
    ImGui::ActivateItemByID(colliderControl);
    panelFrame();
    assert(content.track.keys[1].layers & Colliders);
    panel->StateStorage.SetInt(tree, 0);
    // Actual ImGui mouse/key events: rotate in place and fly without a pivot.
    editLayout = false;
    auto gesture = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({490, 20}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({900, 650}, ImGuiCond_Always);
        ImGui::Begin("752 fly QA");
        auto origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("Canvas", {820, 500},
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        if (PreviewInteraction(origin, {820, 500}, false))
            Changed();
        ImGui::End();
        ImGui::Render();
    };
    auto distance = [](Point a, Point b) {
        return std::sqrt(std::pow(a.x - b.x, 2) + std::pow(a.y - b.y, 2) + std::pow(a.z - b.z, 2));
    };
    io.AddMousePosEvent(800, 250);
    for (int i = 0; i < 3; i++)
        gesture();
    auto original = content.freePose;
    io.AddMouseButtonEvent(1, true);
    io.AddKeyEvent(ImGuiKey_W, true);
    for (int i = 0; i < 5; i++)
        gesture();
    assert(distance(content.freePose.position, original.position) > .3);
    io.AddKeyEvent(ImGuiKey_W, false);
    gesture();
    auto position = content.freePose.position;
    io.AddMousePosEvent(850, 280);
    gesture();
    assert(distance(position, content.freePose.position) < .00001);
    assert(std::abs(content.freePose.rotation.y - original.rotation.y) > .01);
    io.AddKeyEvent(ImGuiKey_E, true);
    gesture();
    gesture();
    assert(content.freePose.position.y > position.y);
    io.AddKeyEvent(ImGuiKey_E, false);
    gesture();
    float height = content.freePose.position.y;
    io.AddKeyEvent(ImGuiKey_Q, true);
    gesture();
    gesture();
    assert(content.freePose.position.y < height);
    io.AddKeyEvent(ImGuiKey_Q, false);
    io.AddMouseButtonEvent(1, false);
    gesture();
    assert(content.track.keys[1].pose.position.x == content.freePose.position.x);
    assert(options.camera == 3 && content.track.keys[1].mode == 2);
    // Exact point selection remains exact while the button is held and moved away.
    ImVec2 railOrigin;
    auto rail = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({500, 700}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({900, 200}, ImGuiCond_Always);
        ImGui::Begin("752 rail QA");
        railOrigin = ImGui::GetCursorScreenPos();
        TimeRail(false);
        ImGui::End();
        ImGui::Render();
    };
    for (int i = 0; i < 3; i++)
        rail();
    auto *railWindow = ImGui::FindWindowByName("752 rail QA");
    float railWidth = railWindow->WorkRect.GetWidth();
    float pointX = railOrigin.x + float(5.123456 / 12) * railWidth;
    io.AddMousePosEvent(pointX, railOrigin.y + 59);
    rail();
    io.AddMouseButtonEvent(0, true);
    rail();
    assert(std::abs(previewAt - 5.123456) < 1e-10 && cameraEditing && options.camera == 3);
    io.AddMousePosEvent(pointX + 40, railOrigin.y + 35);
    rail();
    assert(std::abs(previewAt - 5.123456) < 1e-10);
    io.AddMouseButtonEvent(0, false);
    rail();
    // A pending frame owns its composition settings, independent of further edits.
    auto resolved = ResolveFrame(options, content, 6);
    auto frozenOverlay = CompositionSnapshot(content);
    float opacity = frozenOverlay.input.opacity;
    content.input.opacity = .11f;
    content.track.keys[1].layers = 0;
    assert(frozenOverlay.track.keys.empty() && frozenOverlay.input.opacity == opacity &&
           resolved.options.colliders);
    content.track.keys[1].layers = Colliders;
    Project project;
    project.run = 752;
    project.duration = 12;
    project.gameHash = map->hash;
    project.options = options;
    project.content = content;
    auto decoded = DecodeProject(EncodeProject(project), 752, 12, map->hash);
    for (size_t i = 0; i < 3; i++)
    {
        assert(decoded.content.track.keys[i].mode == content.track.keys[i].mode);
        assert(decoded.content.track.keys[i].layerMask == content.track.keys[i].layerMask);
        assert(decoded.content.track.keys[i].layers == content.track.keys[i].layers);
        const auto &before = content.track.keys[i];
        const auto &after = decoded.content.track.keys[i];
        assert(after.seconds == before.seconds && after.pose.fov == before.pose.fov);
        assert(distance(after.pose.position, before.pose.position) < .00001);
        assert(after.follow.yaw == before.follow.yaw && after.follow.pitch == before.follow.pitch &&
               after.follow.radius == before.follow.radius &&
               distance(after.follow.pan, before.follow.pan) < .00001);
    }
    for (int version : {1, 2})
    {
        auto legacy = DecodeProject(LegacyProjectBytes(project, version), 752, 12, map->hash);
        assert(legacy.options.camera == 3);
        for (auto key : legacy.content.track.keys)
            assert(key.mode == 2 && key.layerMask == 0);
    }
    guideCache.Bind(content, revision + 1);
    assert(guideCache.anchors.size() == 3 && guideCache.lines.size() <= 1030);
    for (const auto &anchor : guideCache.anchors)
    {
        auto expected = CameraAt(options, content, anchor.seconds);
        assert(expected && distance(expected->position, anchor.pose.position) < .00001);
    }
    CameraPose camera{{0, 0, 0}, Angles(0, 0), 60};
    auto center = ProjectGuide({0, 0, 10}, camera, {0, 0}, {800, 450});
    assert(center && center->x == 400 && center->y == 225 &&
           !ProjectGuide({0, 0, -1}, camera, {0, 0}, {800, 450}));
    // Measure real text vertices, with all other mouse elements using different ink.
    ImGui::NewFrame();
    auto *draw = ImGui::GetBackgroundDrawList();
    auto prefs = content.input;
    prefs.background = prefs.border = prefs.shadow = false;
    prefs.mouseMask = 3;
    auto colors = content.palette;
    colors.ink = IM_COL32(17, 231, 123, 255);
    colors.edge = IM_COL32(111, 112, 113, 255);
    auto input = suite_input::Demo();
    input.mouse = 0;
    int start = draw->VtxBuffer.Size;
    suite_input::Module(draw, 1, {0, 0}, {170, 140}, &input, prefs, {}, 0, ImGui::GetFont(), colors);
    for (float expected : {64.f, 106.f})
    {
        float lo = FLT_MAX, hi = -FLT_MAX;
        auto target = suite_input::Fade(colors.ink, prefs.contentOpacity);
        for (int i = start; i < draw->VtxBuffer.Size; i++)
        {
            auto v = draw->VtxBuffer[i];
            if (v.col == target && v.pos.y < 66 && std::abs(v.pos.x - expected) < 18)
            {
                lo = std::min(lo, v.pos.x);
                hi = std::max(hi, v.pos.x);
            }
        }
        assert(lo < hi && std::abs((lo + hi) * .5f - expected) < 2);
    }
    ImGui::Render();
    assert(run->valid && run->validationChecked && motion->frames.size() == 721 &&
           motion->frames[0].camera.x == -15);
    // Real native UI with synthetic replay data, not a game-rendering screenshot.
    EditKey(1);
    pathOverview = true;
    showCameraPath = true;
    options.width = 1920;
    options.height = 1080;
    options.scene3d = true;
    backendAvailable = false;
    previewImage = {};
    io.AddMousePosEvent(-1000, -1000);
    for (int i = 0; i < 5; i++)
    {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        Render();
        if (i == 0)
            for (int j = 0; j < GImGui->TabBars.GetMapSize(); j++)
            {
                auto *bar = GImGui->TabBars.TryGetMapData(j);
                if (bar && bar->Tabs.Size == 3 &&
                    std::string(ImGui::TabBarGetTabName(bar, &bar->Tabs[0])) == "Camera")
                    bar->NextSelectedTabId = bar->Tabs[0].ID;
            }
        auto *d = ImGui::GetForegroundDrawList();
        d->AddText(ImGui::GetFont(), 13, {850, 66}, IM_COL32(255, 194, 109, 255),
                   "UI PREVIEW / synthetic replay data / live Unity scene unavailable");
        ImGui::Render();
    }
    for (auto *window : GImGui->Windows)
        if (std::strstr(window->Name, "/Camera keypoints_") &&
            window->LastFrameActive == ImGui::GetFrameCount())
            assert(window->ScrollMax.y < 1); // All three points fit at native 1080p.
    if (!output.empty())
        Dump(output + "/752-timeline-path.draw");
    Close();
    pathOverview = false;
    data = savedData;
    std::cout << "7.5.2 actual ImGui: per-point mode buttons, true RMB/WASD/QE flight, in-place look, exact "
                 "held timeline selection, layer boundaries, pending composition isolation, v1/v2/v3 project "
                 "migration, bounded path guides and centered L/R glyphs; original replay preserved: PASS\n";
}
