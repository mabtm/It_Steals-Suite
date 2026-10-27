#pragma once
#include "GpuDevSession.h"
#include "GpuDevPlan.h"

namespace gpu_dev
{
inline practice::SeedSearchRequest RequestFor(const practice::MapAnalysis &source, const SuiteOptions &options, const Experiment &e)
{
    practice::SeedSearchRequest r;
    const auto &d = *source.data;
    r.pid = d.pid; r.scene = d.scene; r.epoch = d.epoch; r.session = source.session; r.hash = d.hash;
    r.count = e.count; r.key = SearchKey(options, e.keyIndex); r.recipe = options.recipe; r.workers = e.workers;
    r.criteria.spawn = e.spawn; r.criteria.catalog = practice::CatalogKey(d);
    r.exhaustive = e.reference || (!e.gpu && e.bounds == BoundsMode::Disabled);
    return r;
}

// Shared by the live automation and the Windows integration probe.
inline void RunMap(SuiteState &suite, std::shared_ptr<const practice::MapAnalysis> source,
    const practice::Cancel &canceled, const std::function<void(uint64_t)> &progress,
    const std::function<void(const SuiteState &)> &checkpoint, bool allowSoftware = false)
{
    auto check = [&] { if (canceled()) throw std::runtime_error("Benchmark stopped"); };
    check();
    if (!source || !source->data || !source->data->Verified() || source->working || (source->data->flags & 128) ||
        source->data->keep < 16 || source->data->keep > 52 || source->data->rngStatus != 2 || source->data->rngProbes != 4160)
        throw std::runtime_error("A verified complete 16-52 crystal map is required");
    const auto &data = *source->data;
    Report metadata;
    metadata.source = source->data; metadata.liveCapture = source->live;
    int level = MapLevelIndex(data.scene), maximumCount = std::max(suite.options.screenCount, StressCount(suite.options, level));
    metadata.options.adapter = suite.options.adapter; metadata.options.seeds = maximumCount;
    metadata.options.batch = 1024; metadata.options.key = suite.options.key; metadata.options.fixture = !source->live;
    metadata.options.recipe = suite.options.recipe;
    metadata.options.pairs = suite.options.rounds;
    metadata.logicalCpus = std::thread::hardware_concurrency();
    metadata.startedUtcMs = UtcMs();
    int brand[13]{}; for (int i = 0; i < 3; i++) __cpuid(brand + i * 4, 0x80000002 + i);
    metadata.cpu = reinterpret_cast<const char *>(brand);
    suite.maps.push_back(metadata);
    suite.active = std::string(ModeName(data.scene)) + " / shared preparation and GPU warmup";
    checkpoint(suite);
    try
    {
    auto at = Clock::now();
    practice::NavGraph graph(data, canceled);
    practice::SearchDistances distances(data, graph, canceled, [](int, int) {});
    metadata.preparationMs = Ms(at); metadata.model = distances.ModelName();
    at = Clock::now();
    Device device(metadata, allowSoftware); metadata.deviceMs = Ms(at);
    at = Clock::now();
    auto catalog = std::make_shared<Catalog>(int(data.spawns.size()), int(data.points.size()),
        [&](int from, int to) { return distances.CenterDistance(from, to); });
    device.SetCatalog(std::move(catalog), metadata); metadata.catalogSetupMs = Ms(at);
    for (auto kind : ShaderKinds)
    {
        check();
        double compileMs = device.PrepareShader(kind, data.keep);
        suite.shaders.push_back({data.scene, data.keep, kind, ShapeFor(kind, data.keep), compileMs});
    }
    // Warm both scorers and validate a short real batch before timing searches.
    at = Clock::now();
    int warmCount = std::min(64, suite.options.screenCount);
    practice::PortableSelectionGenerator generator(data); practice::RandomSeedOrder seedOrder(suite.options.key);
    std::vector<practice::Selection> warm(warmCount);
    std::vector<practice::RouteResult> cpuWarm(warmCount);
    for (int i = 0; i < warmCount; i++)
    {
        check(); generator.Generate(seedOrder.SeedAt(uint32_t(i)), suite.options.recipe, warm[i]);
        cpuWarm[i] = distances.Score(warm[i], false, canceled);
    }
    for (auto kind : ShaderKinds)
    {
        device.SelectShader(kind, data.keep);
        for (int i = 0; i < warmCount; i++) device.PackSelection(i, warm[i]);
        auto routes = device.Execute(warmCount);
        for (int i = 0; i < warmCount; i++)
        {
            check();
            auto gpu = distances.FinishOrder(warm[i], std::move(routes[i]), canceled);
            if (!SameScore(cpuWarm[i], gpu)) throw std::runtime_error("GPU shader warmup differs from CPU reference");
        }
    }
    metadata.warmupMs = Ms(at); metadata.warmupChecked = int(ShaderKinds.size()) * warmCount; metadata.phase = Phase::Completed;
    suite.warmupChecked += metadata.warmupChecked; suite.maps.back() = metadata;
    std::vector<practice::RouteResult> referenceScores(static_cast<size_t>(maximumCount));
    std::vector<practice::Selection> referenceLayouts(static_cast<size_t>(maximumCount));
    std::vector<practice::SeedResult> referenceTop, filteredTop;
    for (auto stage : {Stage::Screen, Stage::Stress})
    {
        ShaderKind selected = ShaderKind::Compact;
        if (stage == Stage::Stress)
        {
            check();
            auto choice = Calibrate(suite, data.scene); selected = choice.shader;
            suite.calibration.push_back(choice); checkpoint(suite);
        }
        for (const auto &e : Experiments(suite.options, level, stage, selected))
        {
            check();
            suite.active = std::string(ModeName(data.scene)) + " / " + e.Name();
            checkpoint(suite); progress(0);
            SearchRun run; run.experiment = e;
            auto request = RequestFor(*source, suite.options, e);
            practice::RandomSeedOrder runOrder(request.key);
            std::atomic<uint64_t> audited{0};
            bool collecting = e.reference && e.spawn < 0;
            auto audit = [&](uint64_t index, const practice::Selection &selection, const practice::RouteResult &route) {
                if (index >= referenceScores.size()) throw std::runtime_error("Audit index exceeds reference set");
                if (collecting) { referenceScores[size_t(index)] = route; referenceLayouts[size_t(index)] = selection; }
                else if (!practice::SameLayout(referenceLayouts[size_t(index)], selection) || !SameScore(referenceScores[size_t(index)], route))
                    throw std::runtime_error("Score audit mismatch at seed " + std::to_string(runOrder.SeedAt(uint32_t(index))));
                audited.fetch_add(1, std::memory_order_relaxed);
            };
            if (e.gpu)
            {
                auto setting = Clock::now();
                device.SelectShader(e.shader, data.keep); run.shape = ShapeFor(e.shader, data.keep);
                device.Resize(e.batch, metadata);
                device.BeginPass();
                run.bufferSetupMs = Ms(setting);
                run.state = SearchGpu(source, distances, request, e.batch, [&](std::span<const practice::SeedLayout> layouts) {
                    check();
                    for (size_t i = 0; i < layouts.size(); i++) device.PackSelection(int(i), layouts[i].selection);
                    return device.Execute(int(layouts.size()), &run.gpu);
                }, canceled, progress, audit, e.bounds, &run.bounds);
                auto finishing = Clock::now();
                try { device.EndPass(run.gpu, canceled); }
                catch (const std::exception &error) { run.state.phase = practice::SearchPhase::Failed; run.state.error = error.what(); }
                run.state.elapsed += (run.bufferSetupMs + Ms(finishing)) / 1000.;
            }
            else run.state = practice::SeedSearch::RunFrozen(source, distances, request, canceled, progress, audit);
            run.audited = audited.load();
            if (run.state.phase == practice::SearchPhase::Completed)
            {
                if (e.reference)
                {
                    if (collecting) referenceTop = run.state.top;
                    else filteredTop = run.state.top;
                    run.topMatch = run.state.pruned == 0 && run.audited == run.state.scored;
                }
                else run.topMatch = SameTop(e.spawn < 0 ? referenceTop : filteredTop, run.state.top) && run.audited == run.state.scored;
            }
            suite.runs.push_back(std::move(run)); suite.completedTests++;
            suite.maps.back() = metadata;
            checkpoint(suite);
            const auto &last = suite.runs.back();
            if (last.state.phase != practice::SearchPhase::Completed)
                throw std::runtime_error(last.state.error.empty() ? "Search did not complete" : last.state.error);
            if (!last.topMatch) throw std::runtime_error("Top10 differs from the exhaustive CPU reference");
        }
    }
    suite.completedMaps++;
    }
    catch (const std::exception &error)
    {
        metadata.phase = canceled() ? Phase::Stopped : Phase::Failed;
        metadata.error = error.what(); suite.maps.back() = metadata;
        throw;
    }
}

class Automation
{
    std::jthread worker;
    std::atomic<bool> busy{false};
    std::atomic<bool> detecting{false};
    std::atomic<int> wanted{-1};
    std::atomic<uint64_t> progress{0};
    std::shared_ptr<const SuiteState> current = std::make_shared<SuiteState>();
    std::shared_ptr<const AdapterList> adapters = std::make_shared<AdapterList>();
    std::mutex gate;
    std::condition_variable changed;
    std::shared_ptr<const practice::MapAnalysis> offered;
    int process = 0; uint32_t session = 0;
    bool fixtureOffers = false;
  public:
    std::filesystem::path directory;
    ~Automation() { Stop(); if (worker.joinable()) worker.join(); }
    bool Busy() const { return busy.load(std::memory_order_acquire); }
    int Wanted() const { return worker.get_stop_token().stop_requested() ? -1 : wanted.load(); }
    uint64_t Checked() const { return progress.load(); }
    std::shared_ptr<const SuiteState> Get() const { return std::atomic_load(&current); }
    std::shared_ptr<const AdapterList> GetAdapters() const { return std::atomic_load(&adapters); }
    void RefreshAdapters()
    {
        if (Busy()) return;
        if (worker.joinable()) worker.join();
        busy = detecting = true;
        try
        {
        worker = std::jthread([this] {
            auto found = std::make_shared<AdapterList>();
            try { found->entries = Adapters(); } catch (const std::exception &e) { found->error = e.what(); }
            std::atomic_store(&adapters, std::shared_ptr<const AdapterList>(found));
            detecting = false; busy = false;
        });
        }
        catch (const std::exception &e)
        {
            auto failed = std::make_shared<AdapterList>(); failed->error = e.what();
            std::atomic_store(&adapters, std::shared_ptr<const AdapterList>(failed));
            detecting = false; busy = false;
        }
    }
    void Stop() { if (worker.joinable()) worker.request_stop(); changed.notify_all(); }
    void CheckContext(int pid, int, int, uint32_t identity, bool restricted)
    {
        if (Busy() && !detecting && (pid != process || identity != session || restricted)) Stop();
    }
    bool Offer(std::shared_ptr<const practice::MapAnalysis> map, int pid, int scene, int epoch, uint32_t identity)
    {
        int level = Wanted();
        if (!Busy() || level < 0 || pid != process || identity != session || !map || !map->data || map->working ||
            (!map->live && !fixtureOffers)) return false;
        const auto &d = *map->data;
        if (d.pid != pid || map->session != identity || d.scene != kMapScenes[level] || d.scene != scene || d.epoch != epoch ||
            !d.Verified() || (d.flags & 128) || d.rngStatus != 2 || d.rngProbes != 4160) return false;
        { std::lock_guard lock(gate); if (wanted.load() != level) return false; offered = std::move(map); }
        changed.notify_all(); return true;
    }
    void Start(SuiteOptions options, int pid, uint32_t identity, bool fixturesForTesting = false)
    {
        if (Busy()) return;
        SuiteTestCount(options);
        if (worker.joinable()) worker.join();
        process = pid; session = identity;
        fixtureOffers = fixturesForTesting;
        { std::lock_guard lock(gate); offered.reset(); }
        busy = true; progress = 0;
        auto root = directory;
        try
        {
        worker = std::jthread([this, options, root, fixturesForTesting](std::stop_token token) {
            SuiteState state; state.options = options; state.status = "Running";
            state.startedUtcMs = UtcMs(); state.totalTests = SuiteTestCount(options);
            auto folder = root / ("Dev6-" + std::to_string(state.startedUtcMs)); state.directory = folder;
            auto publish = [&](const SuiteState &s) {
                SaveJson(folder / "Dev6-Summary.json", SuiteJson(s));
                std::atomic_store(&current, std::shared_ptr<const SuiteState>(std::make_shared<SuiteState>(s)));
            };
            try
            {
                publish(state);
                for (int level : options.levels)
                {
                    { std::lock_guard lock(gate); offered.reset(); wanted = level; }
                    state.active = "Waiting for verified " + std::string(kMapLevelNames[level]); publish(state);
                    auto deadline = Clock::now() + std::chrono::seconds(180);
                    std::shared_ptr<const practice::MapAnalysis> map;
                    {
                        std::unique_lock lock(gate);
                        while (!offered && !token.stop_requested() && Clock::now() < deadline)
                            changed.wait_for(lock, std::chrono::milliseconds(20));
                        if (token.stop_requested()) throw std::runtime_error("Benchmark stopped");
                        if (!offered) throw std::runtime_error("Timed out waiting for verified map capture");
                        map = std::move(offered); wanted = -1;
                    }
                    RunMap(state, map, [&] { return token.stop_requested(); }, [&](uint64_t n) { progress = n; }, publish, fixturesForTesting);
                    publish(state);
                }
                state.status = "Completed"; state.active = "All tests completed";
            }
            catch (const std::exception &e) { state.status = token.stop_requested() ? "Stopped" : "Failed"; state.error = e.what(); }
            wanted = -1;
            try { publish(state); } catch (const std::exception &e) { state.error += std::string(" / JSON export: ") + e.what(); }
            std::atomic_store(&current, std::shared_ptr<const SuiteState>(std::make_shared<SuiteState>(state)));
            busy.store(false, std::memory_order_release);
        });
        }
        catch (...) { busy = false; wanted = -1; throw; }
    }
};
} // namespace gpu_dev
