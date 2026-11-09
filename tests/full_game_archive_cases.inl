void FullGameArchiveCases(const std::filesystem::path &root,std::shared_ptr<const practice::MapData> wire)
{
    using namespace practice;
    auto folder=root/"full-game"/"game";auto source=folder/"RunReviews";
    std::filesystem::create_directories(source);std::filesystem::create_directories(folder/"FullGame");
    auto fixture=std::filesystem::path(__FILE__).parent_path()/"fixtures/full-game-wire.isfg";
    std::ifstream in(fixture,std::ios::binary);std::string metadata{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
    auto group=suite_full::Decode(metadata);
    auto write=[&](const std::filesystem::path &path,const std::vector<uint8_t> &bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char *>(bytes.data()),std::streamsize(bytes.size()));assert(f);};
    for(int i=0;i<8;i++)
    {
        auto map=*wire;map.scene=kMapScenes[i];auto review=std::make_shared<RunReview>(*wire->review);
        review->id=group.replays[i];review->seconds=group.attempts[i];review->outcome=1;
        review->category=1;review->selectedSeed=review->runSeed=group.seeds[i];review->rawSpawnSeed=bool(group.raw&(1u<<i));
        review->aids=review->initialAids=2;review->warnings=0;review->seedKind=2;
        review->valid=review->validationChecked=review->observedFromStart=review->seedKnown=true;
        review->trace.clear();review->monsters.clear();review->flashes.clear();review->motion.reset();
        review->collected=map.selected;review->pickupSeconds.clear();
        for(size_t n=0;n<review->collected.size();n++)review->pickupSeconds.push_back(review->seconds*double(n+1)/double(review->collected.size()+1));
        map.review=review;auto bytes=EncodeFixture(map,15);std::string error;
        if(!DecodeMap(bytes,error))throw std::runtime_error("Full-game level fixture: "+error);
        write(source/("run-"+std::to_string(review->id)+".isrun"),bytes);
        if(i==0)
        {
            auto failed=std::make_shared<RunReview>(*review);failed->id=group.id+9999;failed->outcome=3;map.review=failed;
            write(source/("run-"+std::to_string(failed->id)+".isrun"),EncodeFixture(map,15));
        }
    }
    write(folder/"FullGame"/("full-"+std::to_string(group.id)+".isfg"),std::vector<uint8_t>(metadata.begin(),metadata.end()));
    auto saved=root/"full-game"/"saved";
    std::filesystem::path exported;
    {
        ReplayArchive archive;archive.SetDirectory(saved);archive.SetSourceDirectory(source);
        Await([&]{return archive.Get()->runs.size()==9 && archive.Get()->fullGames.size()==1;});
        auto state=archive.Get();assert(state->fullGames[0].Ranked() && state->fullGameChildren.size()==8);
        for(int i=0;i<8;i++)assert(state->fullGameChildren.contains(group.replays[i]));
        assert(!state->fullGameChildren.contains(group.id+9999));
        ReplayLibraryView view;view.Update(state,ReplayLibraryFilter{});
        assert(view.shown.size()==1 && state->runs[view.shown[0]].outcome==3);
        archive.ExportFullGame(group.id);
        Await([&]{auto s=archive.Get();return s->exportedFullGame==group.id || !s->error.empty();});
        state=archive.Get();if(state->exportedFullGame!=group.id)throw std::runtime_error(state->error);
        exported=state->exportPath;
        assert(std::filesystem::is_directory(exported));
        for(int i=0;i<8;i++)
        {
            auto file=exported/("0"+std::to_string(i+1)+" - "+kMapLevelNames[i]+".zip");
            std::ifstream input(file,std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
            auto files=DecodeReplayZip(bytes);assert(files.contains("run-"+std::to_string(group.replays[i])+".isrun"));
        }
        // Preserve a damaged primary and an earlier interrupted export. Recovery
        // must package the good backup, without reusing the partial directory.
        auto primary=folder/"FullGame"/("full-"+std::to_string(group.id)+".isfg");
        auto backup=primary;backup+=".bak";
        write(backup,std::vector<uint8_t>(metadata.begin(),metadata.end()));
        write(primary,std::vector<uint8_t>{'b','a','d'});
        auto partial=exported;partial+=".partial";std::filesystem::rename(exported,partial);
        archive.Refresh();Await([&]{auto s=archive.Get();return !s->fullGames.empty() && s->fullGames[0].file==backup;});
        auto before=archive.Get()->exportPath;
        archive.ExportFullGame(group.id);
        Await([&]{auto s=archive.Get();return s->exportPath!=before || !s->error.empty();});
        state=archive.Get();if(state->exportPath==before)throw std::runtime_error(state->error);
        exported=state->exportPath;
        assert(std::filesystem::exists(partial) && std::filesystem::exists(exported));
        std::ifstream untouched(primary,std::ios::binary);std::string damaged{std::istreambuf_iterator<char>(untouched),std::istreambuf_iterator<char>()};
        assert(damaged=="bad");
    }
    {
        ReplayArchive restored;restored.SetDirectory(root/"full-game"/"restored");
        restored.ImportPackage(exported/("full-"+std::to_string(group.id)+".isfg"));
        Await([&]{auto s=restored.Get();return s->fullGames.size()==1 || !s->error.empty();});
        auto state=restored.Get();if(state->fullGames.empty())throw std::runtime_error(state->error);
        assert(state->runs.size()==8 && state->fullGameChildren.size()==8 && state->fullGames[0].Ranked());
        for(const auto &r:state->runs)assert(r.outcome==1);
        auto first=state->runs[0].file;std::ifstream beforeFile(first,std::ios::binary);
        std::vector<uint8_t> before{std::istreambuf_iterator<char>(beforeFile),std::istreambuf_iterator<char>()};beforeFile.close();
        restored.ImportPackage(exported/("full-"+std::to_string(group.id)+".isfg"));
        restored.Refresh();Await([&]{return !restored.Get()->busy;});
        std::ifstream afterFile(first,std::ios::binary);std::vector<uint8_t> after{std::istreambuf_iterator<char>(afterFile),std::istreambuf_iterator<char>()};
        assert(before==after);
    }
    std::cout<<"Actual full-game archive: eight scene/seed-bound successful replays, failed-attempt exclusion, clean individual list, bounded folder export, complete set import and existing-file preservation: PASS\n";
}
