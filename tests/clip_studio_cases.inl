// Rebuild old-format fixtures without passing new fields to the legacy reader.
std::string LegacyProjectBytes(const suite_clip::Project &p, int version) {
    using namespace suite_clip;
    auto bytes = EncodeProject(p);
    auto last = bytes.rfind('\n', bytes.size() - 2);
    std::string body = bytes.substr(0, last + 1);
    auto flagsStart = body.find('\n', body.find('\n', body.find('\n') + 1) + 1) + 1;
    auto flagsEnd = body.find('\n', flagsStart);
    auto flags = body.substr(flagsStart, flagsEnd - flagsStart);
    for (int count = version < 4 ? 2 : version < 5 ? 1 : 0; count > 0; count--) {
        auto lastFlag = flags.find_last_not_of(' ');
        auto flagBegin = flags.find_last_of(' ', lastFlag);
        flags.erase(flagBegin + 1);
    }
    body.replace(flagsStart, flagsEnd - flagsStart, flags);
    auto exportStart = body.find('\n', flagsStart) + 1;
    auto audioStart=body.find('\n',exportStart)+1;
    if(version<7)body.erase(audioStart,body.find('\n',audioStart)-audioStart+1);
    if(version<6)body.erase(exportStart,body.find('\n',exportStart)-exportStart+1);

    for (auto it = p.content.track.keys.rbegin(); it != p.content.track.keys.rend(); ++it) {
        // The simpler row scan below matches each quoted key name and exact pose prefix.
        std::ostringstream row;
        row.imbue(std::locale::classic());
        row << std::setprecision(17) << it->seconds << ' ' << int(it->next) << ' ' << std::quoted(it->name)
            << ' ';
        WritePose(row, it->pose);
        if (version >= 3)
            row << it->mode << ' ' << it->layerMask << ' ' << it->layers << ' ' << it->follow.yaw << ' '
                << it->follow.pitch << ' ' << it->follow.radius << ' ' << it->follow.pan.x << ' '
                << it->follow.pan.y << ' ' << it->follow.pan.z << ' ';
        if (version >= 4)
            row << int(it->speedOverride) << ' ' << it->playbackSpeed << ' ' << int(it->speedCurve);
        auto prefix = row.str();
        auto start = body.rfind(prefix);
        assert(start != std::string::npos);
        auto lineEnd = body.find('\n', start);
        body.erase(start + prefix.size(), lineEnd - start - prefix.size());
    }
    body.replace(7, 1, std::to_string(version));
    return body + std::to_string(ProjectChecksum(body)) + "\n";
}
void Workshop751Cases(const std::string &output) {
    using namespace suite_clip;
    auto saved = data;
    auto &io = ImGui::GetIO();
    io.DisplaySize = {1920, 1080};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    g_pShared = &data;
    data.timerState = 0;
    pendingRequest = false;
    auto map = std::make_shared<practice::MapData>(SeedFixture());
    map->scene = 3;
    map->minX = map->minZ = -20;
    map->maxX = map->maxZ = 20;
    map->spawn = 0;
    map->spawns = {{-15, 0, -15}};
    map->points = {{3, 0, 4}, {10, 0, 10}, {-7, 0, 12}};
    map->selected = {0, 1, 2};
    map->walls = {{-19, -19, -18, 19}, {18, -19, 19, 19}, {-18, 18, 18, 19}, {-18, -19, 18, -18},
                  {-10, -10, -9, 8},   {-9, 7, 7, 8},     {5, -10, 6, 0},    {-2, -18, -1, -8}};
    auto run = std::make_shared<practice::RunReview>();
    run->id = 751;
    run->seconds = 12;
    run->valid = run->validationChecked = true;
    run->outcome = 1;
    for (int i = 0; i <= 240; i++)
        run->trace.push_back({i * .05, {float(-15 + i * .12), 0, float(-15 + i * .11)}, true});
    map->review = run;
    auto analysis = std::make_shared<practice::MapAnalysis>();
    analysis->data = map;
    auto motion = std::make_shared<practice::ReplayMotion>();
    motion->id = 751;
    motion->seconds = motion->deviceThrough = 12;
    for (int i = 0; i <= 720; i++) {
        double t = i / 60.;
        practice::ReplayViewFrame v;
        v.seconds = t;
        v.camera = {float(-15 + t * 2.4), 1.7f, float(-15 + t * 2.2)};
        v.body = v.camera;
        v.cameraRotation = {0, 0, 0, 1};
        motion->frames.push_back(v);
        suite_input::Frame f;
        f.seconds = t;
        f.frame = i;
        f.tick = unsigned(t * 50);
        f.flags = 256;
        if (i < 300 || i % 36 < 16) {
            f.Key('W');
            f.Key(160);
            f.state |= 4;
        }
        if (i % 60 < 20)
            f.Key('A');
        else if (i % 60 > 40)
            f.Key('D');
        if (i % 120 >= 70 && i % 120 < 100) {
            f.Key('R');
            f.state |= 8;
        }
        if (i % 90 > 60) {
            f.Key(162);
            f.state |= 512;
        }
        if (i % 100 > 60)
            f.mouse = 1;
        f.lookX = std::sin(t * 8) * 4;
        f.lookY = std::cos(t * 6) * 2;
        motion->devices.push_back(f);
        if (i == 0 || f.state != motion->devices[i - 1].state) {
            practice::ReplayInputEvent e;
            e.seconds = t;
            e.frame = i;
            e.tick = f.tick;
            e.state = f.state;
            motion->events.push_back(e);
        }
    }
    Open(analysis, motion);
    options.scene3d = true;
    SetCameraMode(1);
    previewAt = 2;
    // Rapid mode changes must use the canonical pose and keep the clock running.
    playPreview = true;
    for (int n = 0; n < 50; n++) {
        double at = 2 + n * .1;
        previewAt = at;
        auto expected = CameraAt(options, content, at);
        assert(expected);
        shownPose = {{999, 999, 999}, Angles(99, 20), 90}; // Delayed image acknowledgement.
        auto epoch = cameraEpoch;
        SetCameraMode(2);
        assert(playPreview && cameraEpoch > epoch);
        assert(std::abs(content.freePose.position.x - expected->position.x) < .0001f);
        SetCameraMode(1);
        assert(playPreview);
    }
    SetCameraMode(0);
    previewAt = 4;
    auto recorded = RecordedCamera(content, previewAt);
    assert(recorded);
    shownPose = {{999, 999, 999}, Angles(99, 20), 90};
    SetCameraMode(2);
    assert(std::abs(content.freePose.position.x - recorded->position.x) < .0001f);
    SetCameraMode(1);
    TrainerPreferences previous;
    previous.input.states = false;
    previous.input.events = true;
    auto legacySettings = previous.Encode();
    auto versionAt = legacySettings.find("version=18");
    assert(versionAt != std::string::npos);
    legacySettings.replace(versionAt, 10, "version=17");
    assert(TrainerPreferences::Decode(legacySettings).input.states);
    playPreview = false;
    for (int n = 0; n < 3; n++) {
        previewAt = 1 + n * 4;
        SaveKey(false);
        assert(options.camera == 3 && content.track.keys.back().mode == 1);
    }
    assert(content.track.keys.size() == 3);
    EditKey(1);
    assert(options.camera == 3 && cameraEditing && previewAt == 5);
    SetCameraMode(2);
    content.freePose.position.y = 9;
    UpdateEditingKey();
    assert(content.track.keys[1].pose.position.y == 9);
    TogglePlayback();
    assert(playPreview && options.camera == 3 && !cameraEditing);
    SetCameraMode(1);
    assert(playPreview);
    playPreview = false;
    options.a = 1;
    options.b = 11;
    previewAt = 0;
    TogglePlayback();
    assert(previewAt == 1);
    TogglePlayback();
    // Legacy project activity slots migrate while camera/identity metadata stay unchanged.
    Project project;
    project.run = 751;
    project.duration = 12;
    project.gameHash = map->hash;
    project.options = options;
    project.content = content;
    project.content.input.states = false;
    project.content.input.events = true;
    auto bytes = EncodeProject(project);
    auto old = LegacyProjectBytes(project, 1);
    auto migrated = DecodeProject(old, 751, 12, map->hash);
    assert(migrated.content.input.states && migrated.content.input.events &&
           migrated.content.track.keys.size() == 3);
    assert(EncodeProject(migrated).rfind("ISCLIP 7", 0) == 0);
    for (int version : {2, 3}) {
        auto previous = DecodeProject(LegacyProjectBytes(project, version), 751, 12, map->hash);
        assert(previous.content.track.keys.size() == 3 && !previous.content.track.keys[0].speedOverride &&
               !previous.options.simplified);
    }
    project.options.camera = 3;
    project.options.simplified = true;
    project.content.track.keys[0].speedOverride = true;
    project.content.track.keys[0].playbackSpeed = 1;
    project.content.track.keys[0].speedCurve = Transition::Smooth;
    project.content.track.keys[1].speedOverride = true;
    project.content.track.keys[1].playbackSpeed = .1;
    project.options.fullbright = true;
    project.content.track.keys[0].layerMask = Fullbright;
    project.content.track.keys[0].layers = Fullbright;
    project.content.track.keys[1].layerMask = Fullbright;
    project.content.track.keys[1].layers = 0;
    auto speedProject = DecodeProject(EncodeProject(project), 751, 12, map->hash);
    assert(speedProject.options.simplified && speedProject.content.track.keys[1].playbackSpeed == .1 &&
           speedProject.content.track.keys[0].speedCurve == Transition::Smooth);
    assert(speedProject.options.OutputDuration() > project.options.b - project.options.a);
    assert(speedProject.options.fullbright);
    assert(ResolveFrame(speedProject.options, speedProject.content, 1).options.fullbright);
    assert(!ResolveFrame(speedProject.options, speedProject.content, 5).options.fullbright);
    auto legacyBright = project;
    legacyBright.content.track.keys[0].layerMask = legacyBright.content.track.keys[0].layers = 0;
    legacyBright.content.track.keys[1].layerMask = legacyBright.content.track.keys[1].layers = 0;
    auto oldV4 = DecodeProject(LegacyProjectBytes(legacyBright, 4), 751, 12, map->hash);
    assert(!oldV4.options.fullbright && oldV4.content.track.keys[1].playbackSpeed == .1 &&
           oldV4.content.track.keys[0].speedOverride);
    auto oldV5 = DecodeProject(LegacyProjectBytes(project, 5), 751, 12, map->hash);
    assert(oldV5.options.fullbright && oldV5.options.maxFileBytes == 0 && oldV5.options.speedKeys);
    auto modern = project;
    modern.options.format = 6;
    modern.options.maxFileBytes = 8000000;
    modern.options.quality = 2;
    modern.options.preset = 4;
    modern.options.speedKeys = false;
    modern.options.audio=true;modern.options.audioVolume=.65f;
    auto oldV6=DecodeProject(LegacyProjectBytes(modern,6),751,12,map->hash);assert(!oldV6.options.audio&&oldV6.options.audioVolume==1);
    auto exported = DecodeProject(EncodeProject(modern), 751, 12, map->hash);
    assert(exported.options.audio&&exported.options.audioVolume==.65f);
    assert(exported.options.maxFileBytes == 8000000 && exported.options.quality == 2 &&
           !exported.options.speedKeys && !exported.options.timing &&
           exported.content.track.keys[0].speedOverride);
    std::cout << "7.7.0: fullbright v5 migration, v6 compression/preset fields, ignored speed keys with "
                 "camera track retained: PASS\n";
    std::cout << "7.6.1: fullbright v5 project roundtrip, exact point on/off boundaries and v4 speed "
                 "migration: PASS\n";
    content.track = project.content.track;
    options.camera = 3;
    options.scene3d = true;
    EditKey(0);
    Changed();
    for (int frame = 0; frame < 4; frame++) {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        Render();
        ImGui::Render();
        CheckHudRegions();
        for (int j = 0; j < GImGui->TabBars.GetMapSize(); j++) {
            auto *bar = GImGui->TabBars.TryGetMapData(j);
            if (bar && bar->Tabs.Size == 3 &&
                std::string(ImGui::TabBarGetTabName(bar, &bar->Tabs[0])) == "Camera")
                bar->NextSelectedTabId = bar->Tabs[0].ID;
        }
    }
    if (!output.empty())
        Dump(output + "/760-speed-keypoint.draw");
    std::cout
        << "7.6.0: speed-keypoint project v4 roundtrip, v1/v2/v3 migration, original/simplified profile "
           "and native speed editor rendering: PASS\n";
    // Exercise the actual 1x action without removing camera/layer/speed keypoints.
    auto savedOutput = options;
    auto savedTrack = content.track;
    options.rate = .25;
    options.speedKeys = true;
    auto outputFrame = [&] {
        bool changed = false;
        ImGui::NewFrame();
        ImGui::SetNextWindowSize({700, 1050}, ImGuiCond_Always);
        ImGui::Begin("Export timing action QA");
        OutputPanel(changed);
        ImGui::End();
        ImGui::Render();
        if (changed) Changed();
    };
    outputFrame();
    auto *timingPanel = ImGui::FindWindowByName("Export timing action QA");
    assert(timingPanel);
    ImGui::ActivateItemByID(timingPanel->GetID("Normal speed (1x)"));
    outputFrame();
    outputFrame();
    assert(options.rate == 1 && !options.speedKeys && !options.timing &&
           content.track.keys.size() == savedTrack.keys.size() &&
           content.track.keys[1].speedOverride && content.track.keys[1].playbackSpeed == .1);
    options = savedOutput;
    std::cout << "7.7.0 actual Output panel: Normal speed ignores speed keys, preserves camera/layers and recomputes preview timing: PASS\n";
    // A long held key must not glow forever; only the real release gets an afterglow.
    practice::ReplayMotion held;
    suite_input::Frame down;
    down.seconds = 0;
    down.flags = 256;
    down.Key('W');
    down.Key(160);
    auto up = down;
    up.seconds = 4;
    up.keys = {};
    held.devices = {down, up};
    assert(suite_input::ReplayGlow(held, 3)['W'] == 0);
    auto release = suite_input::ReplayGlow(held, 4.05);
    assert(release['W'] > .6f && release[16] > .6f && suite_input::ReplayGlow(held, 4.2)['W'] == 0);
    assert(suite_input::ModuleVisible(3, content.input) && !suite_input::ModuleVisible(4, content.input));
    // Real composition in a sub-viewport must not move previous vertices or change its parent's clip stack.
    options.scene3d = false;
    options.width = 1920;
    options.height = 1080;
    previewAt = 6.3;
    content.input.controller = false;
    content.input.mouseMask = 7;
    content.input.mouseScale = 1;
    content.layout = {};
    content.palette = suite_input::Colors(1);
    content.font = suite_ui::hudFont;
    ImGui::NewFrame();
    ImGui::Begin("Viewport composition QA");
    auto *draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled({20, 20}, {40, 40}, IM_COL32_WHITE);
    int count = draw->VtxBuffer.Size;
    auto original = draw->VtxBuffer[count - 1].pos;
    int stack = draw->_ClipRectStack.Size;
    ComposeViewport(draw, options, content, previewAt, {50, 80}, {800, 450});
    assert(draw->VtxBuffer[count - 1].pos.x == original.x && draw->VtxBuffer[count - 1].pos.y == original.y &&
           draw->_ClipRectStack.Size == stack);
    for (int i = count; i < draw->VtxBuffer.Size; i++)
        assert(std::isfinite(draw->VtxBuffer[i].pos.x));
    ImGui::TextUnformatted("Parent layout still valid");
    auto *background = ImGui::GetBackgroundDrawList();
    background->AddRectFilled({1, 1}, {20, 20}, IM_COL32_WHITE);
    auto previousClip = background->CmdBuffer.back().ClipRect;
    ComposeViewport(background, options, content, previewAt, {100, 200}, {800, 450});
    assert(background->CmdBuffer[0].ClipRect.x == previousClip.x &&
           background->CmdBuffer[0].ClipRect.z == previousClip.z);
    ImGui::End();
    ImGui::Render();
    CheckHudRegions();
    backendAvailable = true;
    freeDiskGiB = 48.2;
    editLayout = false;
    selectedBlock = Keyboard;
    // Force the new studio to its normal desktop size after the previous small-screen regression.
    if (auto *w = ImGui::FindWindowByName(WindowTitle().c_str())) {
        w->Pos = {12, 12};
        w->SizeFull = {1896, 1056};
    }
    for (int tab = 0; tab < 3; tab++) {
        options.scene3d = tab == 0;
        options.format = 2;
        if (tab == 2) ApplyExportPreset(options, 1);
        for (int frame = 0; frame < 4; frame++) {
            ImGui::NewFrame();
            suite_ui::BeginFrame(true, false);
            Render();
            ImGui::GetForegroundDrawList()->AddText(suite_ui::hudFont, 13, {1260, 60},
                                                    IM_COL32(150, 173, 184, 255),
                                                    "Synthetic replay / native GUI preview");
            ImGui::Render();
            CheckHudRegions();
            if (frame == 0)
                for (int j = 0; j < GImGui->TabBars.GetMapSize(); j++) {
                    auto *bar = GImGui->TabBars.TryGetMapData(j);
                    if (bar && bar->Tabs.Size == 3 &&
                        std::string(ImGui::TabBarGetTabName(bar, &bar->Tabs[0])) == "Camera")
                        bar->NextSelectedTabId = bar->Tabs[tab].ID;
                }
        }
        if (!output.empty())
            Dump(output + "/751-studio-" + std::to_string(tab) + ".draw");
        if (tab == 2 && !output.empty()) Dump(output + "/770-best-quality.draw");
    }
    if (!output.empty()) {
        std::filesystem::create_directories(std::filesystem::path(output) / "input-animation");
        suite_input::Preferences p;
        p.controller = false;
        p.mouseMask = 7;
        p.background = true;
        p.opacity = .85;
        p.scale = 1;
        for (int i = 0; i < 60; i++) {
            double t = 5 + i / 20.;
            auto input = suite_input::ReplayFrame(*motion, t);
            auto changes = suite_input::ReplayChanges(*motion, t);
            auto glow = suite_input::ReplayGlow(*motion, t);
            io.DisplaySize = {1000, 520};
            ImGui::NewFrame();
            auto *d = ImGui::GetBackgroundDrawList();
            d->AddText(suite_ui::hudFont, 24, {24, 18}, IM_COL32(100, 220, 201, 255),
                       "INPUT HUD   /   PracticeSuite 7.5.1   /   mab");
            d->AddText(suite_ui::hudFont, 15, {24, 52}, IM_COL32(150, 173, 184, 255),
                       "Native UI preview / synthetic replay inputs");
            auto base = suite_input::Size(p);
            float scale = 1.07f;
            ImVec2 lo{(1000 - base.x * scale) * .5f, 98};
            suite_input::DrawModules(d, lo, {lo.x + base.x * scale, lo.y + base.y * scale},
                                     input ? &*input : nullptr, p, changes, t, suite_ui::hudFont,
                                     suite_input::Colors(1), &glow);
            ImGui::Render();
            char filename[64];
            std::snprintf(filename, sizeof(filename), "/input-animation/frame-%03d.draw", i);
            Dump(output + filename);
        }
    }
    Close();
    data = saved;
    io.DisplaySize = {1920, 1080};
    std::cout
        << "7.5.1: 100 camera switches while playing, capture/edit/play track, range restart, project v1 "
           "migration, real key release glow, unified activity and parent-clipped native preview: PASS\n";
}
