#pragma once
#include "GpuDevSearch.h"
#include "MapLevels.h"
#include "GpuDevCatalog.h"

namespace gpu_dev
{
enum class Stage { Screen, Stress };
inline const char *StageName(Stage stage) { return stage == Stage::Screen ? "screen" : "stress"; }
struct SuiteOptions
{
    int adapter = 0, screenCount = 16384, count = 65536, hideSeekCount = 262144, filterCount = 65536;
    int rounds = 3, recipe = 1, keys = 3;
    uint64_t key = 0;
    std::vector<int> levels{7, 5, 2};
};
inline uint64_t SearchKey(const SuiteOptions &o, int index) { return o.key + uint64_t(index) * 0x9e3779b97f4a7c15ULL; }
inline ShaderKind BaselineShader(int level) { return level == 7 ? ShaderKind::Generic : ShaderKind::Fixed; }
inline int StressCount(const SuiteOptions &o, int level) { return level == 2 ? o.hideSeekCount : o.count; }
struct Experiment
{
    bool gpu = false, reference = false;
    Stage stage = Stage::Screen;
    ShaderKind shader = ShaderKind::Generic;
    int workers = 1, batch = 2048, round = 0, spawn = -1, keyIndex = 0;
    BoundsMode bounds = BoundsMode::Enabled;
    uint64_t count = 0;
    std::string Name() const
    {
        return std::string(StageName(stage)) + "-k" + std::to_string(keyIndex + 1) + "-" +
            (reference ? "reference" : gpu ? "gpu" : "cpu") + "-w" + std::to_string(workers) +
            (gpu ? "-b" + std::to_string(batch) + "-" + ShaderName(shader) : "") +
            "-bounds-" + BoundsName(bounds) + "-r" + std::to_string(round) +
            (spawn < 0 ? "-all-spawns" : "-spawn" + std::to_string(spawn)) + "-n" + std::to_string(count);
    }
};
inline std::vector<Experiment> Experiments(const SuiteOptions &o, int level, Stage stage, ShaderKind candidate = ShaderKind::Compact)
{
    if (o.screenCount < 32 || o.screenCount > 65536 || o.count < o.screenCount || o.count > 262144 ||
        o.hideSeekCount < o.screenCount || o.hideSeekCount > 262144 || o.filterCount < 32 || o.filterCount > 65536 ||
        o.rounds < 1 || o.rounds > 5 || o.recipe < 1 || o.recipe > 2 || o.keys < 1 || o.keys > 5 ||
        o.levels.empty() || o.levels.size() > 3 || int(stage) < 0 || int(stage) > 1 || int(candidate) < 2 || int(candidate) > 4)
        throw std::runtime_error("Invalid automatic benchmark plan");
    for (size_t i = 0; i < o.levels.size(); i++)
    {
        if (o.levels[i] != 2 && o.levels[i] != 5 && o.levels[i] != 7)
            throw std::runtime_error("Choose Hide & Seek, Outside or Classic Bonus");
        if (std::find(o.levels.begin(), o.levels.begin() + i, o.levels[i]) != o.levels.begin() + i)
            throw std::runtime_error("Duplicate benchmark map");
    }
    if (std::find(o.levels.begin(), o.levels.end(), level) == o.levels.end()) throw std::runtime_error("Map is outside benchmark plan");
    std::vector<Experiment> result;
    for (int key = 0; key < o.keys; key++)
    {
        Experiment reference; reference.reference = true; reference.bounds = BoundsMode::Disabled; reference.stage = stage;
        reference.keyIndex = key + (stage == Stage::Stress ? o.keys : 0);
        reference.count = uint64_t(stage == Stage::Screen ? o.screenCount : StressCount(o, level)); result.push_back(reference);
        Experiment cpu = reference; cpu.reference = false; cpu.workers = 6; cpu.bounds = BoundsMode::Enabled;
        std::vector<Experiment> base{cpu};
        auto cpuOff = cpu; cpuOff.bounds = BoundsMode::Disabled; base.push_back(cpuOff);
        auto gpu = cpu; gpu.gpu = true; gpu.bounds = BoundsMode::Disabled; gpu.workers = 2;
        if (stage == Stage::Screen)
            for (auto shader : ShaderKinds) { auto e = gpu; e.shader = shader; base.push_back(e); }
        else
            for (auto shader : {BaselineShader(level), candidate})
                for (int workers : {2, 4, 6}) for (int batch : {1024, 2048, 4096, 8192})
                {
                    auto e = gpu; e.shader = shader; e.workers = workers; e.batch = batch; base.push_back(e);
                }
        for (auto shader : stage == Stage::Screen ? std::vector<ShaderKind>{BaselineShader(level)} :
                                                   std::vector<ShaderKind>{BaselineShader(level), candidate})
            for (auto bounds : {BoundsMode::Enabled, BoundsMode::Adaptive})
            {
                auto e = gpu; e.shader = shader; e.bounds = bounds; base.push_back(e);
            }
        for (int round = 1; round <= o.rounds; round++) for (size_t i = 0; i < base.size(); i++)
        {
            size_t at = (i + size_t((round - 1) * 7 + key * 11)) % base.size();
            if (round % 2 == 0) at = base.size() - 1 - at;
            auto e = base[at]; e.round = round; result.push_back(e);
        }
        if (stage == Stage::Stress)
        {
            reference.spawn = cpu.spawn = gpu.spawn = 0;
            reference.count = cpu.count = gpu.count = std::min(reference.count, uint64_t(o.filterCount));
            result.push_back(reference); result.push_back(cpu);
            gpu.shader = BaselineShader(level); result.push_back(gpu);
            gpu.shader = candidate; gpu.workers = 6; gpu.batch = 4096; result.push_back(gpu);
            gpu.bounds = BoundsMode::Enabled; result.push_back(gpu);
        }
    }
    return result;
}
inline int SuiteTestCount(const SuiteOptions &o)
{
    int total = 0;
    for (int level : o.levels) for (auto stage : {Stage::Screen, Stage::Stress}) total += int(Experiments(o, level, stage).size());
    if (!total) throw std::runtime_error("Empty benchmark plan");
    return total;
}
inline const char *ModeName(int scene)
{
    int level = MapLevelIndex(scene);
    return level < 0 ? "Unknown" : kMapLevelNames[level];
}
struct SearchRun
{
    Experiment experiment;
    practice::SeedSearchState state;
    PairTiming gpu;
    BoundsStats bounds;
    uint64_t audited = 0;
    double bufferSetupMs = 0;
    bool topMatch = false;
    ShaderShape shape;
};
inline const char *SearchStatus(practice::SearchPhase phase)
{
    return phase == practice::SearchPhase::Completed ? "Completed" : phase == practice::SearchPhase::Stopped ? "Stopped" : "Failed";
}
inline std::string SearchJson(const SearchRun &r)
{
    const auto &s = r.state;
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::boolalpha << std::setprecision(17);
    out << "{\"schema\":\"practice-suite.gpu-search.v6\",\"version\":"; Quote(out, suite_brand::Version);
    out << ",\"channel\":\"GPU Dev 6\",\"sourceCommit\":";
#ifdef SUITE_DEV_SOURCE_COMMIT
    Quote(out, SUITE_DEV_SOURCE_COMMIT);
#else
    Quote(out, "local");
#endif
    out << ",\"catalogKey\":"; Quote(out, s.source && s.source->data ? std::to_string(practice::CatalogKey(*s.source->data)) : "");
    out << ",\"test\":"; Quote(out, r.experiment.Name());
    out << ",\"mode\":"; Quote(out, ModeName(s.request.scene));
    out << ",\"scene\":" << s.request.scene << ",\"status\":"; Quote(out, SearchStatus(s.phase));
    out << ",\"error\":"; Quote(out, s.error);
    out << ",\"backend\":"; Quote(out, r.experiment.gpu ? "GPU pipeline" : "production CPU worker");
    out << ",\"workers\":" << s.request.workers << ",\"batch\":" << (r.experiment.gpu ? r.experiment.batch : 64)
        << ",\"round\":" << r.experiment.round << ",\"reference\":" << r.experiment.reference
        << ",\"spawnFilter\":" << s.request.criteria.spawn << ",\"objective\":\"shortest route\",\"recipe\":" << s.request.recipe
        << ",\"key\":"; Quote(out, std::to_string(s.request.key));
    out << ",\"stage\":"; Quote(out, StageName(r.experiment.stage));
    out << ",\"keyIndex\":" << r.experiment.keyIndex << ",\"boundsMode\":"; Quote(out, BoundsName(r.experiment.bounds));
    out << ",\"boundsChecked\":"; if (r.experiment.gpu) out << r.bounds.checked; else out << "null";
    out << ",\"boundsSkipped\":"; if (r.experiment.gpu) out << r.bounds.skipped; else out << "null";
    out << ",\"shader\":"; Quote(out, r.experiment.gpu ? ShaderName(r.experiment.shader) : "cpu");
    out << ",\"shaderCrystals\":" << (r.experiment.gpu && r.shape.fixed ? r.shape.capacity : 0)
        << ",\"shaderCapacity\":" << (r.experiment.gpu ? r.shape.capacity : 0)
        << ",\"shaderThreads\":" << (r.experiment.gpu ? r.shape.threads : 0)
        << ",\"shaderNeighborBits\":" << (r.experiment.gpu ? r.shape.neighborBits : 0)
        << ",\"shaderTriangular\":" << (r.experiment.gpu && r.shape.triangular ? "true" : "false")
        << ",\"shaderSharedBytes\":" << (r.experiment.gpu ? r.shape.SharedBytes() : 0);
    out << ",\"requested\":" << s.request.count << ",\"generated\":" << s.generated
        << ",\"evaluated\":" << s.evaluated << ",\"filtered\":" << s.filtered << ",\"pruned\":" << s.pruned
        << ",\"scored\":" << s.scored << ",\"cacheHits\":" << s.cacheHits
        << ",\"audited\":" << r.audited << ",\"top10Match\":" << r.topMatch
        << ",\"elapsedMs\":" << s.elapsed * 1000. << ",\"seedsPerSecond\":" << (s.elapsed > 0 ? s.evaluated / s.elapsed : 0)
        << ",\"bufferSetupMs\":" << r.bufferSetupMs << ",\"generationMicros\":";
    if (r.experiment.gpu) out << "null"; else out << s.generationMicros;
    out << ",\"gpuTimestampBatches\":" << r.gpu.timestampBatches << ",\"gpuTimestampInvalid\":" << r.gpu.timestampInvalid
        << ",\"gpuKernelMs\":"; Number(out, r.gpu.timestampBatches ? r.gpu.gpuKernelMs : NAN);
    out << ",\"gpuHostUploadMs\":" << r.gpu.hostUploadMs << ",\"gpuHostReadbackMs\":" << r.gpu.hostReadbackMs
        << ",\"gpuQueryCollectMs\":" << r.gpu.queryCollectMs << ",\"top10\":[";
    for (size_t i = 0; i < s.top.size(); i++)
    {
        const auto &row = s.top[i]; if (i) out << ',';
        out << "{\"rank\":" << i + 1 << ",\"seed\":" << row.seed << ",\"index\":" << row.index
            << ",\"spawn\":" << row.selection.spawn << ",\"crystals\":"; Array(out, row.selection.crystals);
        out << ",\"route\":"; RouteJson(out, row.route); out << '}';
    }
    out << "]}"; return out.str();
}
struct SuiteState
{
    SuiteOptions options;
    std::string status = "Idle", active, error;
    std::filesystem::path directory;
    int completedMaps = 0, completedTests = 0, totalTests = 0, warmupChecked = 0;
    int64_t startedUtcMs = 0;
    std::vector<SearchRun> runs;
    std::vector<Report> maps;
    struct ShaderPreparation { int scene = 0, crystals = 0; ShaderKind shader{}; ShaderShape shape; double compileMs = 0; };
    std::vector<ShaderPreparation> shaders;
    struct Calibration
    {
        int scene = 0, samples = 0;
        ShaderKind shader = ShaderKind::Compact;
        BoundsMode bounds = BoundsMode::Disabled;
        bool gpuTime = false;
        double baselineMs = 0, candidateMs = 0;
        std::array<double, 3> boundsMs{};
    };
    std::vector<Calibration> calibration;
};
inline double Median(std::vector<double> values)
{
    if (values.empty()) throw std::runtime_error("Missing shader calibration samples");
    std::sort(values.begin(), values.end());
    return (values[(values.size() - 1) / 2] + values[values.size() / 2]) / 2;
}
inline SuiteState::Calibration Calibrate(const SuiteState &s, int scene)
{
    SuiteState::Calibration chosen; chosen.scene = scene; chosen.samples = s.options.keys * s.options.rounds;
    auto baseline = BaselineShader(MapLevelIndex(scene));
    std::vector<const SearchRun *> trials;
    for (const auto &r : s.runs)
        if (r.state.request.scene == scene && r.experiment.stage == Stage::Screen && r.experiment.gpu)
            trials.push_back(&r);
    chosen.gpuTime = std::all_of(trials.begin(), trials.end(), [](const auto *r) {
        if (r->experiment.bounds != BoundsMode::Disabled) return true;
        auto count = r->state.request.count, batch = uint64_t(r->experiment.batch);
        auto batches = 1 + (count > 64 ? (count - 64 + batch - 1) / batch : 0);
        return !r->gpu.timestampInvalid && r->gpu.timestampBatches == batches && r->gpu.gpuKernelMs > 0;
    });
    auto measure = [&](ShaderKind shader, BoundsMode bounds, bool gpuTime) {
        std::vector<double> samples;
        for (const auto *r : trials) if (r->experiment.shader == shader && r->experiment.bounds == bounds)
        {
            if (r->state.phase != practice::SearchPhase::Completed || !r->topMatch) throw std::runtime_error("Invalid shader calibration result");
            samples.push_back(gpuTime ? r->gpu.gpuKernelMs : r->state.elapsed * 1000);
        }
        if (int(samples.size()) != chosen.samples) throw std::runtime_error("Incomplete shader calibration");
        return Median(std::move(samples));
    };
    chosen.baselineMs = measure(baseline, BoundsMode::Disabled, chosen.gpuTime);
    chosen.candidateMs = std::numeric_limits<double>::infinity();
    for (auto kind : {ShaderKind::Triangle, ShaderKind::Compact, ShaderKind::CompactFixed})
    {
        double ms = measure(kind, BoundsMode::Disabled, chosen.gpuTime);
        if (ms < chosen.candidateMs) { chosen.candidateMs = ms; chosen.shader = kind; }
    }
    for (int i = 0; i < 3; i++) chosen.boundsMs[i] = measure(baseline, BoundsMode(i), false);
    chosen.bounds = BoundsMode(std::min_element(chosen.boundsMs.begin(), chosen.boundsMs.end()) - chosen.boundsMs.begin());
    return chosen;
}
inline std::string SuiteJson(const SuiteState &s)
{
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::boolalpha << std::setprecision(17);
    out << "{\"schema\":\"practice-suite.gpu-suite.v6\",\"version\":"; Quote(out, suite_brand::Version);
    out << ",\"channel\":\"GPU Dev 6\",\"sourceCommit\":";
#ifdef SUITE_DEV_SOURCE_COMMIT
    Quote(out, SUITE_DEV_SOURCE_COMMIT);
#else
    Quote(out, "local");
#endif
    out << ",\"plan\":{\"adapter\":" << s.options.adapter << ",\"screenSeeds\":" << s.options.screenCount
        << ",\"stressSeeds\":" << s.options.count << ",\"hideSeekStressSeeds\":" << s.options.hideSeekCount
        << ",\"filterSeeds\":" << s.options.filterCount << ",\"rounds\":" << s.options.rounds
        << ",\"keysPerStage\":" << s.options.keys << ",\"screenGpuHelpers\":2,\"stressGpuHelpers\":[2,4,6],\"cpuWorkers\":6"
        << ",\"stressBatches\":[1024,2048,4096,8192],\"recipe\":" << s.options.recipe << ",\"key\":";
    Quote(out, std::to_string(s.options.key));
    out << ",\"mapLevels\":"; Array(out, s.options.levels);
    out << ",\"keys\":[";
    for (int i = 0; i < 2 * s.options.keys; i++) { if (i) out << ','; Quote(out, std::to_string(SearchKey(s.options, i))); }
    out << ']';
    out << "},\"mapMetadataAreSharedPreparationOnly\":true,\"status\":"; Quote(out, s.status);
    out << ",\"activeTest\":"; Quote(out, s.active); out << ",\"error\":"; Quote(out, s.error);
    out << ",\"startedUtcMs\":" << s.startedUtcMs << ",\"completedMaps\":" << s.completedMaps
        << ",\"completedTests\":" << s.completedTests << ",\"totalTests\":" << s.totalTests
        << ",\"warmupChecked\":" << s.warmupChecked << ",\"mapMetadata\":[";
    for (size_t i = 0; i < s.maps.size(); i++) { if (i) out << ','; out << Json(s.maps[i]); }
    out << "],\"shaderPreparation\":[";
    for (size_t i = 0; i < s.shaders.size(); i++)
    {
        if (i) out << ',';
        const auto &v = s.shaders[i];
        out << "{\"scene\":" << v.scene << ",\"crystals\":" << v.crystals << ",\"shader\":"; Quote(out, ShaderName(v.shader));
        out << ",\"capacity\":" << v.shape.capacity << ",\"threads\":" << v.shape.threads << ",\"neighborBits\":" << v.shape.neighborBits
            << ",\"triangular\":" << (v.shape.triangular ? "true" : "false")
            << ",\"sharedBytes\":" << v.shape.SharedBytes() << ",\"compileMs\":" << v.compileMs << '}';
    }
    out << "],\"calibration\":[";
    for (size_t i = 0; i < s.calibration.size(); i++)
    {
        if (i) out << ',';
        const auto &c = s.calibration[i];
        out << "{\"scene\":" << c.scene << ",\"baselineShader\":"; Quote(out, ShaderName(BaselineShader(MapLevelIndex(c.scene))));
        out << ",\"stressShader\":"; Quote(out, ShaderName(c.shader));
        out << ",\"metric\":"; Quote(out, c.gpuTime ? "median complete GPU kernel time" : "median full search wall time (timestamp fallback)");
        out << ",\"samplesPerVariant\":" << c.samples << ",\"baselineMs\":" << c.baselineMs << ",\"candidateMs\":" << c.candidateMs;
        out << ",\"screenBoundsRecommendation\":"; Quote(out, BoundsName(c.bounds));
        out << ",\"boundsWallMsOnOffAdaptive\":"; Array(out, c.boundsMs); out << '}';
    }
    out << "],\"scope\":\"Complete frozen-map searches, strict per-seed CPU score/selection audits and identical Top10. Screen compares all five FP64 shader variants and bounds policies. Stress retains the Dev5 winner and selects the fastest compressed kernel by complete GPU timestamps, falling back to full wall time if coverage is incomplete. Stress uses separate held-out search keys, 2/4/6 CPU helpers and batches 1024-8192. On/off/count-adaptive bounds controls are timed separately; screen bounds recommendations use actual wall time, not rejection counts, and are not applied automatically to production. CPU uses production workers/caches; CPU bounds-off uses its existing dev exhaustive flag. Disabling bounds scores every unfiltered candidate. Geometry, device, shader compilation and warmup are shared outside timed searches; buffer setup, strict audits, worker startup/join and query collection are included. No live Unity generation, scene loading or preview rendering is timed.\",\"runs\":[";
    for (size_t i = 0; i < s.runs.size(); i++) { if (i) out << ','; out << SearchJson(s.runs[i]); }
    out << "]}"; return out.str();
}
} // namespace gpu_dev
