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
