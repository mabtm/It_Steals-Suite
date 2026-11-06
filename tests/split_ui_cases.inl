void SetSeedSplitUICases(const std::string &output)
{
    auto saved = data;
    auto &io = ImGui::GetIO();
    io.DisplaySize = {1280,720};
    ApplyTheme(1); io.Fonts->Build(); io.Fonts->SetTexID(1);
    data = {};
    data.pid = 13; data.runCategory = 1; data.timerState = 1;
    data.splitRunId = data.reviewRunId = 100; data.splitReferenceId = 90; data.splitReferenceValid = 1;
    data.splitPickupCount = data.splitReferenceCount = 3;
    data.splitPickupIds[0] = 7; data.splitPickupIds[1] = 2; data.splitPickupIds[2] = 9;
    data.splitPickupSeconds[0] = 2; data.splitPickupSeconds[1] = 5; data.splitPickupSeconds[2] = 9;
    data.splitReferenceIds[0] = 2; data.splitReferenceIds[1] = 7; data.splitReferenceIds[2] = 9;
    data.splitReferenceSeconds[0] = 4; data.splitReferenceSeconds[1] = 3; data.splitReferenceSeconds[2] = 10;
    auto splits = suite_splits::Live(data);
    assert(splits.size() == 3 && splits[0].delta == -1 && splits[1].delta == 1 && splits[2].delta == -1);
    auto render = [&](double at, bool motion) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({80,80},ImGuiCond_Always);
        ImGui::SetNextWindowSize({420,230},ImGuiCond_Always);
        ImGui::Begin("Set-Seed split renderer QA", nullptr, ImGuiWindowFlags_NoSavedSettings);
        suite_splits::Draw(ImGui::GetWindowDrawList(), ImGui::GetFont(), {100,110}, 320,145,splits,at,true,motion);
        ImGui::End(); ImGui::Render();
        auto *window = ImGui::FindWindowByName("Set-Seed split renderer QA");
        return std::vector<ImDrawVert>(window->DrawList->VtxBuffer.begin(),window->DrawList->VtxBuffer.end());
    };
    render(9.3,true); auto first=render(9.3,true);
    render(3.5,true); auto repeated=render(9.3,true);
    assert(first.size()==repeated.size() && std::memcmp(first.data(),repeated.data(),first.size()*sizeof(ImDrawVert))==0);
    bool green=false,red=false;
    for (auto vertex:first) { green |= vertex.col==IM_COL32(107,235,162,255); red |= vertex.col==IM_COL32(255,112,124,255); }
    assert(green&&red);
    if (!output.empty()) Dump(output+"/set-seed-splits.draw");
    auto reduced=render(9.0,false); auto steady=render(9.3,false);
    assert(reduced.size()==steady.size() && std::memcmp(reduced.data(),steady.data(),steady.size()*sizeof(ImDrawVert))==0);
    // New run identity invalidates the cached live pickup sequence.
    data.splitRunId++; data.splitPickupCount=0;
    assert(suite_splits::Live(data).empty());
    data=saved;
    std::cout << "Actual split renderer: per-crystal deltas across changed order, green/red, absolute-time/reverse-seek determinism, reduced motion and run cache isolation: PASS\n";
}

void IndependentSplitPanelCases(const std::string &output)
{
    auto savedData=data; auto *savedShared=g_pShared;
    auto savedPrefs=suite_ui::replayPreferences; auto savedHud=g_speedrunLayout;
    bool savedSpeedrun=g_speedrunMode,savedPractice=g_practiceMode;
    bool savedMotion=suite_ui::preferences.motion;
    auto &io=ImGui::GetIO();
    data=suite_splits::PreviewData(GameData{}); g_pShared=&data;
    g_speedrunMode=true;g_practiceMode=false;suite_clip::open=false;suite_hud::active=false;
    suite_ui::replayPreferences=ReplayPreferences();
    g_speedrunLayout.enabled=false; // The detached panel has its own visibility.
    auto rows=suite_splits::Live(data);
    assert(rows.size()==3 && rows[0].sectorComparable && std::abs(rows[1].sectorDelta-.7)<1e-6 && rows[2].gold);
    assert(suite_splits::Pace(data,rows,31)==44 && suite_splits::Pace(data,rows,39)==46);
    assert(std::abs(*suite_splits::Theoretical(data,rows)-40.3)<1e-6);
    data.runValidity=2; rows=suite_splits::Live(data);
    assert(!rows[2].gold && suite_splits::Theoretical(data,rows)==40.5);
    data.runValidity=1;data.splitRunId++;
    std::swap(data.splitPickupIds[1],data.splitPickupIds[2]);rows=suite_splits::Live(data);
    assert(rows[1].comparable && !rows[1].sectorComparable && !suite_splits::Pace(data,rows,31) && !suite_splits::Theoretical(data,rows));
    data=suite_splits::PreviewData(GameData{});
    auto frame=[&](double age=0) {
        ImGui::NewFrame();
        g_validityBadgeDrawn = false;
        if (age == 0) RenderTimerOverlay("Classic");
        else suite_splits::RenderPanel(data,suite_ui::replayPreferences,age);
        ImGui::Render();CheckHudRegions();
    };
    auto visible=[] {
        auto *w=ImGui::FindWindowByName("Set-Seed splits");
        return w && w->LastFrameActive==ImGui::GetFrameCount() && !w->Hidden;
    };
    for (ImVec2 viewport : {ImVec2(480,360),ImVec2(800,600),ImVec2(1280,720),ImVec2(1920,1080)})
        for(float dpi:{1.f,1.5f})
        {
            io.DisplaySize=viewport;ApplyTheme(dpi);io.Fonts->Build();io.Fonts->SetTexID(1);
            frame();frame();assert(visible());
            auto b=suite_splits::PanelBounds(suite_ui::replayPreferences);
            assert(b.x>=0 && b.y>=0 && b.x+b.w<=viewport.x && b.y+b.h<=viewport.y);
            assert(std::abs(b.x-(viewport.x-b.w)*.5f)<.1f && std::abs(b.y+b.h-(viewport.y-12))<.1f);
            if(!output.empty()) Dump(output+"/independent-splits-"+std::to_string(int(viewport.x))+"-"+std::to_string(int(dpi*100))+".draw");
            frame(1);assert(!visible());
            suite_ui::replayPreferences.liveSplits=false;frame();assert(!visible());
            suite_ui::replayPreferences.liveSplits=true;suite_ui::replayPreferences.splitPlacement=0;frame();assert(!visible());
            suite_ui::replayPreferences.splitPlacement=1;data.runCategory=2;frame();assert(!visible());data.runCategory=1;
        }
    io.DisplaySize={1920,1080};ApplyTheme(1);io.Fonts->Build();io.Fonts->SetTexID(1);
    data.timerState=0;
    suite_hud::Open();suite_hud::selected=3;
    auto edit=[&] { ImGui::NewFrame();suite_hud::Render();ImGui::Render();CheckHudRegions(); };
    edit();edit();edit();
    auto b=suite_hud::bounds[3];ImVec2 click{b.x+22,b.y+22};
    auto before=suite_ui::replayPreferences;
    io.AddMousePosEvent(click.x,click.y);edit();io.AddMouseButtonEvent(0,true);edit();
    io.AddMousePosEvent(click.x+75,click.y-65);edit();io.AddMouseButtonEvent(0,false);edit();
    assert(suite_hud::draft.replay.splitX!=before.splitX && suite_ui::replayPreferences.splitX==before.splitX);
    assert(!suite_hud::undo.empty());
    auto *workspace=ImGui::FindWindowByName("HUD workspace");
    ImGui::ActivateItemByID(workspace->GetID("Undo"));edit();
    assert(suite_hud::draft.replay.splitX==before.splitX && suite_hud::draft.replay.splitY==before.splitY);
    suite_hud::draft.replay.splitX=.25f;edit();
    io.AddKeyEvent(ImGuiKey_Escape,true);edit();io.AddKeyEvent(ImGuiKey_Escape,false);edit();
    assert(!suite_hud::active && suite_ui::replayPreferences.splitX==before.splitX);
    suite_hud::Open();suite_hud::selected=3;suite_hud::draft.replay.splitX=.3f;
    edit();edit();
    if(!output.empty()) Dump(output+"/independent-splits-editor.draw");
    workspace=ImGui::FindWindowByName("HUD workspace");ImGui::ActivateItemByID(workspace->GetID("Apply layout"));edit();
    assert(!suite_hud::active && suite_ui::replayPreferences.splitX==.3f);
    TrainerPreferences prefs;prefs.replay=suite_ui::replayPreferences;
    auto decoded=TrainerPreferences::Decode(prefs.Encode());
    assert(decoded.replay.splitX==.3f && decoded.replay.splitY==1 && decoded.replay.splitPlacement==1);
    data=savedData;g_pShared=savedShared;g_speedrunLayout=savedHud;suite_ui::replayPreferences=savedPrefs;
    g_speedrunMode=savedSpeedrun;g_practiceMode=savedPractice;suite_ui::preferences.motion=savedMotion;
    io.AddMousePosEvent(-1000,-1000);
    std::cout << "Independent live splits: sector/total/gold, projected and theoretical timing, route changes, timer-independent placement, visibility, 8 viewport/DPI cases, real dragging/undo/cancel/apply and preference persistence: PASS\n";
}
