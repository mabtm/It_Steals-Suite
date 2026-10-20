void Workshop750Cases(const std::string &output)
{
    using namespace suite_clip;
    auto savedData = data;
    auto savedInput = suite_input::preferences;
    auto &io = ImGui::GetIO();
    io.DisplaySize = {1920, 1080};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    g_pShared = &data;
    data.timerState = 0;
    pendingRequest = false;
    auto map = std::make_shared<practice::MapData>(SeedFixture());
    map->minX = map->minZ = -20;
    map->maxX = map->maxZ = 20;
    map->spawn = 0;
    map->spawns = {{-15, 0, -15}};
    map->points = {{3, 0, 4}, {10, 0, 10}, {-7, 0, 12}};
    map->selected = {0, 1, 2};
    map->walls = {{-19, -19, -18, 19}, {18, -19, 19, 19}, {-18, 18, 18, 19}, {-18, -19, 18, -18},
                  {-10, -10, -9, 8},   {-9, 7, 7, 8},     {5, -10, 6, 0}};
    auto review = std::make_shared<practice::RunReview>();
    review->id = 750;
    review->seconds = 10;
    review->valid = review->validationChecked = true;
    review->outcome = 1;
    for (int i = 0; i <= 200; i++)
        review->trace.push_back({i * .05, {float(i * .04), 0, float(i * .1)}, true});
    map->review = review;
    auto analysis = std::make_shared<practice::MapAnalysis>();
    analysis->data = map;
    auto motion = std::make_shared<practice::ReplayMotion>();
    motion->id = 750;
    motion->seconds = motion->deviceThrough = 10;
    for (int i = 0; i <= 600; i++)
    {
        practice::ReplayViewFrame f;
        f.seconds = i / 60.;
        f.camera = {float(i * .02), 1.7f, float(i * .01)};
        f.body = f.camera;
        f.cameraRotation = {0, 0, 0, 1};
        motion->frames.push_back(f);
    }
    auto input = suite_input::Demo();
    input.state |= 512;
    motion->devices = {input};
    motion->events = {{0, 0, 0, 0, 0, 0, 0, 1}, {2, 120, 100, 0, 0, 0, 0, 1 | 4 | 512}};
    Open(analysis, motion);
    options.a = 0;
    options.b = 10;
    options.scene3d = true;
    options.camera = 1;
    previewAt = 1;
    shownPose = content.orbit.Pose({0, 2, 0});
    shownPoseReady = true;
    options.width = 1280;
    options.height = 720;
    auto cameraPanel = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({20, 20}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({450, 1000}, ImGuiCond_Always);
        ImGui::Begin("Camera controls QA");
        bool changed = false;
        CameraPanel(changed);
        ImGui::End();
        ImGui::Render();
    };
    for (int i = 0; i < 3; i++)
        cameraPanel();
    auto *panel = ImGui::FindWindowByName("Camera controls QA");
    assert(panel);
    ImGui::ActivateItemByID(panel->GetID("Add keypoint"));
    cameraPanel();
    if (content.track.keys.size() != 1 || options.camera != 3 || content.track.keys[0].mode != 1)
        std::cerr << "Add key diagnostic: count=" << content.track.keys.size() << " camera=" << options.camera
                  << " error=" << error << " pose=" << bool(CurrentCameraPose()) << "\n";
    assert(content.track.keys.size() == 1 && options.camera == 3 && content.track.keys[0].mode == 1);
    previewAt = 5;
    shownPose = {{10, 3, 9}, Angles(60, 15), 80};
    ImGui::ActivateItemByID(panel->GetID("Add keypoint"));
    cameraPanel();
    assert(content.track.keys.size() == 2 && content.track.keys[1].seconds == 5);
    EditKey(1);
    SetCameraMode(2);
    content.freePose = {{20, 6, 10}, Angles(100, 30), 70};
    UpdateEditingKey();
    ImGui::ActivateItemByID(panel->GetID("Update selected"));
    cameraPanel();
    assert(content.track.keys.size() == 2 && content.track.keys[1].pose.position.x == 20 && cameraEditing);
    auto folder = std::filesystem::path(output) / "clip-projects";
    std::filesystem::create_directories(folder);
    Project project;
    project.run = 750;
    project.duration = 10;
    project.gameHash = map->hash;
    project.name = "Cinematic / marker A";
    project.options = options;
    project.content = content;
    project.content.input.background = false;
    project.content.input.opacity = 0;
    project.content.input.customKeys = true;
    project.content.input.ShowKey(255, true);
    project.content.layout.blocks[Mouse] = {.4f, .2f, .08f, .55f};
    auto bytes = EncodeProject(project);
    auto decoded = DecodeProject(bytes, 750, 10, map->hash);
    assert(decoded.content.track.keys.size() == 2 && decoded.content.input.ShowKey(255) &&
           !decoded.content.input.background && decoded.content.input.opacity == 0 &&
           decoded.content.layout.blocks[Mouse].opacity == .55f);
    StoreProject(folder / "run.isclip", project);
    assert(ReadProjectFile(folder / "run.isclip") == bytes);
    for (int id = 0; id < 5; id++)
    {
        auto corrupt = bytes;
        bool rejected = false;
        try
        {
            if (id == 0)
                corrupt[35] ^= 1;
            if (id == 1)
                corrupt.resize(15);
            if (id == 2)
                DecodeProject(corrupt, 751, 10, map->hash);
            else if (id == 3)
                DecodeProject(corrupt, 750, 11, map->hash);
            else if (id == 4)
                DecodeProject(corrupt, 750, 10, "different assembly");
            else
                DecodeProject(corrupt, 750, 10, map->hash);
        }
        catch (...)
        {
            rejected = true;
        }
        assert(rejected);
    }
    {
        std::ofstream future(folder / "future.isclip");
        future << "ISCLIP 99\nfuture data\n";
    }
    bool preserved = false;
    try
    {
        StoreProject(folder / "future.isclip", project);
    }
    catch (...)
    {
        preserved = true;
    }
    assert(preserved && ReadProjectFile(folder / "future.isclip").find("future data") != std::string::npos);
    auto liveBefore = savedInput;
    auto temp = content;
    auto tempOptions = options;
    ApplyProject(decoded, tempOptions, temp);
    std::ostringstream unchanged, after;
    liveBefore.Encode(unchanged);
    savedInput.Encode(after);
    assert(unchanged.str() == after.str());
    auto gesture = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({490, 20}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({900, 650}, ImGuiCond_Always);
        ImGui::Begin("Export preview controls QA");
        auto origin = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("Canvas", {820, 500},
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                   ImGuiButtonFlags_MouseButtonMiddle);
        if (PreviewInteraction(origin, {820, 500}, false))
            Changed();
        ImGui::End();
        ImGui::Render();
    };
    cameraEditing = false;
    options.camera = 1;
    editLayout = false;
    io.AddMousePosEvent(800, 250);
    for (int i = 0; i < 3; i++)
        gesture();
    float yaw = content.orbit.yaw;
    io.AddMouseButtonEvent(1, true);
    gesture();
    io.AddMousePosEvent(880, 300);
    gesture();
    io.AddMouseButtonEvent(1, false);
    gesture();
    assert(content.orbit.yaw != yaw && content.orbit.pitch > 22);
    float distance = content.orbit.radius;
    io.AddMouseWheelEvent(0, 1);
    gesture();
    gesture();
    assert(content.orbit.radius < distance);
    options.camera = 2;
    content.freeReady = true;
    content.freePose = shownPose;
    fly.Align(content.freePose);
    io.AddMouseButtonEvent(1, true);
    gesture();
    io.AddMousePosEvent(920, 300);
    gesture();
    io.AddMouseButtonEvent(1, false);
    gesture();
    assert(content.freeReady && std::abs(content.freePose.rotation.w - shownPose.rotation.w) > .0001f);
    auto poseAt1 = CameraAt(options, content, 1), poseAt9 = CameraAt(options, content, 9);
    assert(poseAt1 && poseAt9 && poseAt1->position.x == poseAt9->position.x);
    editLayout = true;
    options.inputs = true;
    content.input.keyboard = true;
    selectedBlock = Keyboard;
    auto beforeMove = content.layout.blocks[Keyboard];
    auto r = OverlayBounds(Keyboard, options, content);
    auto *previewWindow = ImGui::FindWindowByName("Export preview controls QA");
    ImVec2 origin{previewWindow->Pos.x + previewWindow->WindowPadding.x,
                  previewWindow->Pos.y + previewWindow->WindowPadding.y + previewWindow->TitleBarHeight};
    io.AddMousePosEvent(origin.x + (r.x + r.w * .5f) / options.width * 820,
                        origin.y + (r.y + r.h * .5f) / options.height * 500);
    gesture();
    io.AddMouseButtonEvent(0, true);
    gesture();
    io.AddMousePosEvent(io.MousePos.x - 60, io.MousePos.y - 50);
    gesture();
    io.AddMouseButtonEvent(0, false);
    gesture();
    assert(content.layout.blocks[Keyboard].x < beforeMove.x &&
           content.layout.blocks[Keyboard].y < beforeMove.y);
    editLayout = false;
    dragBlock = -1;
    io.AddMousePosEvent(-1000, -1000);
    content.layout = {};
    options.scene3d = false;
    options.camera = 0;
    previewAt = 2.2;
    content.input.background = false;
    content.input.border = false;
    content.input.controller = false;
    content.input.mouseScale = 1.15f;
    content.input.events = true;
    content.palette = suite_input::Colors(1);
    content.font = suite_ui::hudFont;
    io.DisplaySize = {float(options.width), float(options.height)};
    ImGui::NewFrame();
    Compose(ImGui::GetBackgroundDrawList(), options, content, previewAt);
    ImGui::Render();
    if (!output.empty())
        Dump(output + "/750-export-composition.draw");
    io.DisplaySize = {1920, 1080};
    for (int tab = 0; tab < 3; tab++)
    {
        for (int i = 0; i < 4; i++)
        {
            ImGui::NewFrame();
            suite_ui::BeginFrame(true, false);
            Render();
            ImGui::Render();
            if (i == 0)
                for (int j = 0; j < GImGui->TabBars.GetMapSize(); j++)
                {
                    auto *bar = GImGui->TabBars.TryGetMapData(j);
                    if (bar && bar->Tabs.Size == 3 &&
                        std::string(ImGui::TabBarGetTabName(bar, &bar->Tabs[0])) == "Camera")
                        bar->NextSelectedTabId = bar->Tabs[tab].ID;
                }
        }
        if (!output.empty())
            Dump(output + "/750-workspace-" + std::to_string(tab) + ".draw");
    }
    for (auto view : {ImVec2(480, 360), ImVec2(1280, 720), ImVec2(1920, 1080)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = view;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            auto *dialog = ImGui::FindWindowByName(suite_clip::WindowTitle().c_str());
            dialog->Pos = {16, 16};
            dialog->SizeFull = {view.x - 32, view.y - 32};
            for (int i = 0; i < 3; i++)
            {
                ImGui::NewFrame();
                Render();
                ImGui::Render();
                CheckHudRegions();
            }
        }
    Close();
    data = savedData;
    suite_input::preferences = savedInput;
    shownPoseReady = cameraEditing = false;
    selectedKey = -1;
    std::cout
        << "7.5.x real ImGui: add/update keypoints, preview orbit/zoom/free-camera stability, overlay "
           "dragging, "
           "project identity/checksum/future-file guards, independent export settings and 3 workspace tabs at "
           "100/150 percent DPI: PASS\n";
}
