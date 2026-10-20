void ReplayExperienceUICases(const std::string& output){
    auto folder=std::filesystem::path(output).parent_path()/"experience";auto read=[](const auto& p){std::ifstream f(p,std::ios::binary);assert(f);return std::vector<uint8_t>{std::istreambuf_iterator<char>(f),{}};};
    std::string error;auto motion=practice::DecodeMotion(read(folder/"input-v6.isinput"),error);auto map=practice::DecodeMap(read(folder/"input-map.isrun"),error);assert(motion&&map&&motion->Matches(*map));auto mutableMotion=std::const_pointer_cast<practice::ReplayMotion>(motion);mutableMotion->audioFile=folder/("run-"+std::to_string(motion->id)+".isaudio");
    auto& io=ImGui::GetIO();auto saved=io.DisplaySize;practice::ReviewPlayback playback;playback.Bind(motion->id);playback.seconds=.01;
    for(auto size:{ImVec2(1920,1080),ImVec2(480,360)})for(float dpi:{1.f,1.5f}){
        io.DisplaySize=size;ApplyTheme(dpi);io.Fonts->Build();io.Fonts->SetTexID(1);
        for(int frame=0;frame<3;frame++){ImGui::NewFrame();suite_ui::BeginFrame(true,false);ImGui::SetNextWindowPos({24,24},ImGuiCond_Always);ImGui::SetNextWindowSize({std::min(1120.f,size.x-48),std::min(820.f,size.y-48)},ImGuiCond_Always);ImGui::Begin("REPLAY / GAME OBSERVATIONS");ImGui::TextDisabled("v%s / mab",suite_brand::Version);RenderReplayTimeline(*map->review,playback,map->review->seconds,nullptr,motion.get());ImGui::SetNextItemOpen(true,ImGuiCond_Always);RenderReplayExperience(*motion,playback);ImGui::Separator();RenderReplay3DSettings(motion.get());ImGui::End();ImGui::Render();CheckHudRegions();}
        if(dpi==1)Dump(output+"/replay-experience-"+std::to_string(int(size.x))+".draw");
    }
    io.DisplaySize=saved;ApplyTheme(1);io.Fonts->Build();io.Fonts->SetTexID(1);
    std::cout<<"7.8 UI: actual observation timeline, event seek list, audio and probe controls at 480/1920 pixels and 100/150% DPI: PASS\n";
}
