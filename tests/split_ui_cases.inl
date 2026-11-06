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
    int first=data.splitPickupIds[1];data.splitPickupIds[1]=data.splitPickupIds[2];data.splitPickupIds[2]=first;rows=suite_splits::Live(data);
    assert(rows[1].comparable && !rows[1].sectorComparable && !suite_splits::Pace(data,rows,31) && !suite_splits::Theoretical(data,rows));
    data=suite_splits::PreviewData(GameData{});
    // The default screen renderer emits only the newest signed delta glyphs.
    // There are no background/outline primitives, labels or older crystals.
    {
        auto prefs=ReplayPreferences();
        assert(prefs.splitStyle==0 && !prefs.splitPace && !prefs.splitTheory && prefs.splitRows==1);
        assert(suite_splits::Delta(suite_splits::Live(data).back(),prefs)==-1);
        auto *font=ImGui::GetFont();
        auto draw=[&](double age) {
            ImDrawList list(ImGui::GetDrawListSharedData());
            list._ResetForNewFrame();
            list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1280,720});
            suite_splits::DrawDeltaText(&list,font,{100,100,480,90},data,prefs,false,age);
            return std::vector<ImDrawVert>(list.VtxBuffer.begin(),list.VtxBuffer.end());
        };
        auto glyphs=draw(.4);assert(glyphs.size()==24); // six glyphs in -1.000
        for(size_t i=0;i<glyphs.size();i+=4)
            assert(glyphs[i].uv.x<glyphs[i+1].uv.x && glyphs[i].uv.y<glyphs[i+2].uv.y);
        auto *one=font->FindGlyph('1');
        assert(one && glyphs[4].uv.x==one->U0 && glyphs[4].uv.y==one->V0);
        assert(draw(3.1).empty());
        // The actual clip composition emits exactly the live delta vertices,
        // with absolute source time for reverse seeks and no panel primitives.
        suite_clip::Content clip;
        auto map=std::make_shared<practice::MapData>(SeedFixture());
        auto review=std::make_shared<practice::RunReview>();review->id=725;review->seconds=50;review->category=1;
        map->review=review;
        auto analysis=std::make_shared<practice::MapAnalysis>();analysis->data=map;
        clip.analysis=analysis;clip.splitReference=map;clip.font=clip.splitFont=font;
        clip.splits=std::make_shared<const std::vector<practice::PickupSplit>>(suite_splits::Live(data));
        clip.splitPresentation=prefs;clip.splitMotion=false;
        clip.layout.blocks[suite_clip::Splits]={100.f/1280,100.f/720,480.f/1280,1};
        suite_clip::Options settings;settings.width=1280;settings.height=720;settings.transparent=true;settings.inputs=false;
        auto compose=[&](double at) {
            ImDrawList list(ImGui::GetDrawListSharedData());list._ResetForNewFrame();
            list.PushTextureID(io.Fonts->TexID);list.PushClipRect({0,0},{1280,720});
            suite_clip::Compose(&list,settings,clip,at);
            return std::vector<ImDrawVert>(list.VtxBuffer.begin(),list.VtxBuffer.end());
        };
        auto latest=clip.splits->back().at;auto exported=compose(latest+.4);
        assert(exported.size()==glyphs.size() && std::memcmp(exported.data(),glyphs.data(),glyphs.size()*sizeof(ImDrawVert))==0);
        assert(compose(0).empty() && compose(latest+3.1).empty());
        compose(latest-1);auto repeated=compose(latest+.4);
        assert(repeated.size()==exported.size() && std::memcmp(repeated.data(),exported.data(),exported.size()*sizeof(ImDrawVert))==0);
        auto frozen=suite_clip::CompositionSnapshot(clip);clip.splitPresentation.splitTextSize=90;
        assert(frozen.splitPresentation.splitTextSize==52);
        // A warmed live text frame performs no heap allocations. This is an
        // allocation contract, not a hardware-dependent FPS/time threshold.
        struct Allocations
        {
            ImGuiMemAllocFunc alloc;ImGuiMemFreeFunc free;void *user;size_t count=0;
            static void *Allocate(size_t n,void *p){auto &a=*static_cast<Allocations *>(p);a.count++;return a.alloc(n,a.user);}
            static void Release(void *p,void *context){auto &a=*static_cast<Allocations *>(context);a.free(p,a.user);}
        } meter{};
        ImGui::GetAllocatorFunctions(&meter.alloc,&meter.free,&meter.user);
        ImDrawList steady(ImGui::GetDrawListSharedData());
        auto drawSteady=[&] {
            steady._ResetForNewFrame();steady.PushTextureID(io.Fonts->TexID);steady.PushClipRect({0,0},{1280,720});
            suite_splits::DrawDeltaText(&steady,font,{100,100,480,90},data,prefs,false,.4);
        };
        drawSteady();drawSteady();
        ImGui::SetAllocatorFunctions(Allocations::Allocate,Allocations::Release,&meter);
        for(int frame=0;frame<10000;frame++)drawSteady();
        ImGui::SetAllocatorFunctions(meter.alloc,meter.free,meter.user);
        assert(meter.count==0);
        std::cout<<"Shared live/clip transparent latest delta: identical vertices, reverse seek, fade, frozen appearance and zero heap allocations in 10000 warmed live frames: PASS\n";
        prefs.splitDuration=0;assert(draw(99).size()==24);
        prefs.splitMetric=1;assert(std::abs(*suite_splits::Delta(suite_splits::Live(data).back(),prefs)+1.2)<1e-6);
        prefs.splitMetric=2;assert(std::abs(*suite_splits::Delta(suite_splits::Live(data).back(),prefs)+.2)<1e-6);
    }
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
