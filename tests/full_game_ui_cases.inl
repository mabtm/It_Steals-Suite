void FullGameUICases(const std::string &output)
{
    auto savedData=data;auto savedCommands=g_commands;auto savedPrefs=suite_full::preferences;
    bool savedSpeed=g_speedrunMode,savedPractice=g_practiceMode,savedSeed=g_seededPractice,savedOverlay=g_overlayVisible;
    auto *savedShared=g_pShared;auto &io=ImGui::GetIO();
    suite_hud::active=false;suite_clip::open=false;g_runReviewOpen=false;pendingRequest=false;
    g_pShared=&data;data={};data.pid=726;data.speedrunModeActive=1;data.timerState=1;data.runValidity=1;
    std::strcpy(data.version,"v7.26.0");
    data.fullGameRunId=639000000000000000ll;data.fullGameMode=2;data.fullGameState=2;data.fullGameStage=6;
    data.fullGameEligible=data.fullGameChecked=1;data.fullGameSeconds=100;data.fullGameSectorSeconds=6;data.fullGameReferenceTotal=140;
    for(int i=0;i<8;i++)
    {
        data.fullGameReferenceSectors[i]=17.5;
        data.fullGameSectors[i]=i<6?15:-1;data.fullGameCumulative[i]=i<6?(i+1)*15:-1;
        data.fullGameAttemptSeconds[i]=i<6?14:-1;data.fullGameRetries[i]=i==2?1:0;
        data.fullGameReplayIds[i]=i<6?72600+i:0;
    }
    for(int i=0;i<6;i++){data.fullGameSectors[i]=i==0?19:15;data.fullGameCumulative[i]=19+i*15;} // red first split, then green
    g_speedrunMode=true;g_practiceMode=false;g_seededPractice=true;g_commands={};g_commands.fullGameMode=2;
    suite_full::preferences={};suite_full::preferences.mode=2;
    auto hudFrame=[&] {
        ImGui::NewFrame();g_validityBadgeDrawn=false;g_validityBadgeBounds={};
        RenderTimerOverlay("Camera Bright");ImGui::Render();CheckHudRegions();
        auto *window=ImGui::FindWindowByName("Full-game timing");assert(window && !window->Hidden && g_validityBadgeDrawn);
        assert(window->Pos.x>=0 && window->Pos.y>=0 && window->Pos.x+window->Size.x<=io.DisplaySize.x+.1f && window->Pos.y+window->Size.y<=io.DisplaySize.y+.1f);
    };
    for(ImVec2 size:{ImVec2(480,360),ImVec2(800,600),ImVec2(1280,720),ImVec2(1920,1080)})
        for(float dpi:{1.f,1.5f})
        {
            io.DisplaySize=size;ApplyTheme(dpi);io.Fonts->Build();io.Fonts->SetTexID(1);
            hudFrame();hudFrame();
            if(!output.empty())Dump(output+"/full-game-hud-"+std::to_string(int(size.x))+"-"+std::to_string(int(dpi*100))+".draw");
        }
    data.fullGameState=1;assert(suite_full::Live(data,.15).seconds==100);hudFrame();
    suite_full::preferences.overlay=false;hudFrame();
    assert(ImGui::FindWindowByName("Full-game timing")->Size.y<180*g_dpiScale); // hiding rows keeps mandatory status
    suite_full::preferences.overlay=true;
    io.DisplaySize={1920,1080};ApplyTheme(1);io.Fonts->Build();io.Fonts->SetTexID(1);
    // Measure allocation behavior with the real eight-row overlay after its
    // draw buffers and animation entries are warm, without a hardware FPS gate.
    for(int frame=0;frame<64;frame++)hudFrame();
    struct Allocations
    {
        ImGuiMemAllocFunc alloc;ImGuiMemFreeFunc free;void *user;size_t count=0;
        static void *Allocate(size_t n,void *p){auto &a=*static_cast<Allocations *>(p);a.count++;return a.alloc(n,a.user);}
        static void Release(void *p,void *context){auto &a=*static_cast<Allocations *>(context);a.free(p,a.user);}
    } meter{};
    ImGui::GetAllocatorFunctions(&meter.alloc,&meter.free,&meter.user);
    ImGui::SetAllocatorFunctions(Allocations::Allocate,Allocations::Release,&meter);
    auto measured=std::chrono::steady_clock::now();
    for(int frame=0;frame<10000;frame++)hudFrame();
    auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-measured).count();
    ImGui::SetAllocatorFunctions(meter.alloc,meter.free,meter.user);
    assert(meter.count==0);
    std::cout<<"Actual eight-row full-game overlay: 10000 warm frames, zero ImGui heap allocations, "<<elapsed/10000<<" ms/frame in headless renderer (not in-game FPS): PASS\n";
    data.timerState=0;data.fullGameState=0;data.fullGameRunId=0;data.speedrunModeActive=0;
    g_speedrunMode=false;g_commands={};suite_full::preferences.raw=0b10101010;
    for(int i=0;i<8;i++)suite_full::preferences.seeds[i]=100+i;
    auto setupFrame=[&] {
        ImGui::NewFrame();ImGui::SetNextWindowPos({20,20},ImGuiCond_Always);ImGui::SetNextWindowSize({880,980},ImGuiCond_Always);
        ImGui::Begin("Full-game setup QA",nullptr,ImGuiWindowFlags_NoSavedSettings);suite_full::Setup();ImGui::End();ImGui::Render();
    };
    setupFrame();setupFrame();
    ImGuiWindow *child=nullptr;
    for(auto *window:GImGui->Windows)if(std::strstr(window->Name,"Full-game setup scroll"))child=window;
    assert(child);
    auto advanced=child->GetID("Seed types (advanced)");
    assert(child->StateStorage.GetInt(advanced,0)==0);
    ImGui::ActivateItemByID(advanced);setupFrame();setupFrame();
    assert(child->StateStorage.GetInt(advanced,0)!=0);
    int level=0;auto row=ImHashData(&level,sizeof(level),child->GetID("Full-game spawn recipes"));
    auto rawCheckbox=ImHashStr("Use run seed",0,row);
    auto originalMask=suite_full::preferences.raw;
    ImGui::ActivateItemByID(rawCheckbox);setupFrame();assert(suite_full::preferences.raw==(originalMask^1u));
    ImGui::ActivateItemByID(rawCheckbox);setupFrame();assert(suite_full::preferences.raw==originalMask);
    ImGui::ActivateItemByID(child->GetID("Start full game"));setupFrame();
    assert(g_commands.fullGameCmd==1 && g_commands.fullGameMode==2 && g_commands.speedrunLevelIndex==0 && g_commands.fullGameRawMask==0b10101010);
    for(int i=0;i<8;i++)assert(g_commands.fullGameSeeds[i]==100+i);
    g_commands.fullGameCmd=0;data.speedrunModeActive=1;data.fullGameRunId=726;data.fullGameMode=2;data.fullGameState=2;data.fullGameStage=6;
    ImGui::ActivateItemByID(rawCheckbox);setupFrame();assert(suite_full::preferences.raw==originalMask);
    setupFrame();ImGui::ActivateItemByID(child->GetID("Restart level"));setupFrame();
    assert(g_commands.speedrunLoadLevelCmd==1 && g_commands.fullGameCmd==0);
    g_commands.speedrunLoadLevelCmd=0;setupFrame();ImGui::ActivateItemByID(child->GetID("Full reset / Classic"));setupFrame();
    assert(g_commands.fullGameCmd==2 && g_commands.speedrunLevelIndex==0);
    if(!output.empty())Dump(output+"/full-game-setup.draw");
    data.fullGameState=3;data.fullGameStage=8;data.fullGameRunId=727;data.fullGameNewPb=1;data.timerState=2;
    data.fullGameSeconds=124;data.fullGameSectorSeconds=0;
    for(int i=6;i<8;i++)
    {data.fullGameSectors[i]=15;data.fullGameCumulative[i]=94+(i-5)*15;data.fullGameAttemptSeconds[i]=15;data.fullGameReplayIds[i]=72600+i;}
    suite_full::resultSeen=0;suite_full::ObserveResult(&data);assert(suite_full::resultOpen);
    std::strcpy(data.fullGameStorageError,"A completed level could not be saved");
    suite_full::ObserveResult(&data);assert(suite_full::resultStorageError==data.fullGameStorageError);
    data.fullGameStorageError[0]=0;suite_full::ObserveResult(&data);
    for(ImVec2 size:{ImVec2(1920,1080),ImVec2(480,360)})
    {
        io.DisplaySize=size;
        for(int frame=0;frame<3;frame++){ImGui::NewFrame();suite_full::RenderResult();ImGui::Render();}
        auto *window=ImGui::FindWindowByName("Full-game result");
        assert(window && window->Pos.x>=0 && window->Pos.y>=0 && window->Pos.x+window->Size.x<=size.x && window->Pos.y+window->Size.y<=size.y);
        if(!output.empty())Dump(output+"/full-game-result-"+std::to_string(int(size.x))+".draw");
    }
    suite_full::resultOpen=false;data.fullGameState=0;g_commands={};g_speedrunMode=false;
    io.DisplaySize={1280,900};
    auto archive=std::make_shared<practice::ReplayArchiveState>();
    auto fixture=std::filesystem::path(__FILE__).parent_path()/"fixtures/full-game-wire.isfg";
    std::ifstream input(fixture,std::ios::binary);std::string bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    auto group=suite_full::Decode(bytes);archive->fullGames.push_back(group);
    for(int i=0;i<8;i++)
    {
        practice::ReplayInfo row;row.id=group.replays[i];row.scene=kMapScenes[i];row.outcome=1;row.valid=true;row.runCategory=1;row.seconds=group.attempts[i];
        archive->byId[row.id]=archive->runs.size();archive->runs.push_back(row);archive->fullGameChildren.insert(row.id);
    }
    suite_full::focusReplayGroup=group.id;
    for(int frame=0;frame<3;frame++)
    {
        ImGui::NewFrame();ImGui::SetNextWindowPos({20,20},ImGuiCond_Always);ImGui::SetNextWindowSize({1240,860},ImGuiCond_Always);
        ImGui::Begin("Full-game replay groups QA",nullptr,ImGuiWindowFlags_NoSavedSettings);
        suite_full::ReplayGroups(archive,practice::ReplayLibraryFilter{});ImGui::End();ImGui::Render();
    }
    assert(suite_full::focusReplayGroup==0);
    if(!output.empty())Dump(output+"/full-game-replays.draw");
    // Show setup inside the actual Suite shell as well as testing its isolated
    // controls above. These images use synthetic data, not a gameplay capture.
    auto savedNavigation=suite_ui::navigation;int savedPage=suite_runs::page;
    bool savedAllOverlay=g_allOverlayEnabled,savedCompact=g_compactMode;
    data={};std::strcpy(data.version,"v7.26.0");data.pid=726;data.currentSceneIndex=3;
    g_commands={};g_speedrunMode=false;g_seededPractice=false;g_compactMode=false;
    g_allOverlayEnabled=g_overlayVisible=true;suite_run::presentation.Reset();g_runReviewOpen=false;
    io.DisplaySize={1280,900};suite_ui::navigation.Select(suite_ui::Page::Speedrun,ImGui::GetTime());suite_runs::page=0;
    for(ImVec2 viewport:{ImVec2(480,360),ImVec2(800,600),ImVec2(1280,900)})for(float dpi:{1.f,1.5f})
    {
        io.DisplaySize=viewport;ApplyTheme(dpi);io.Fonts->Build();io.Fonts->SetTexID(1);
        g_resetLayout=true;Frame();g_resetLayout=false;Frame();Frame();
    }
    io.DisplaySize={1280,900};ApplyTheme(1);io.Fonts->Build();io.Fonts->SetTexID(1);
    g_resetLayout=true;Frame();g_resetLayout=false;
    auto *root=ImGui::FindWindowByName("It Steals | Practice Suite");
    ImGui::SetWindowPos(root,{20,20});ImGui::SetWindowSize(root,{1020,860});
    for(int frame=0;frame<16;frame++)Frame();
    if(!output.empty())Dump(output+"/full-game-setup-shell.draw");
    suite_ui::navigation=savedNavigation;suite_runs::page=savedPage;g_allOverlayEnabled=savedAllOverlay;g_compactMode=savedCompact;
    suite_full::preferences=savedPrefs;data=savedData;g_pShared=savedShared;g_commands=savedCommands;
    g_speedrunMode=savedSpeed;g_practiceMode=savedPractice;g_seededPractice=savedSeed;g_overlayVisible=savedOverlay;
    io.DisplaySize={1920,1080};
    std::cout<<"Actual full-game UI: 8 viewport/DPI HUD cases, badge, load freeze, 8 seed inputs, real Start/level restart/full reset actions, responsive final result and selected replay folder: PASS\n";
}
