#pragma once
#include "GpuDevSearch.h"
#include "MapLevels.h"

namespace gpu_dev
{
struct SuiteOptions
{
    int adapter = 0, count = 16384, rounds = 3, workers = 2, recipe = 1, keys = 3;
    uint64_t key = 0;
    std::vector<int> levels{7, 5, 2};
};
inline uint64_t SearchKey(const SuiteOptions &o, int index) { return o.key + uint64_t(index) * 0x9e3779b97f4a7c15ULL; }
struct Experiment
{
    bool gpu = false, reference = false, specialized = false;
    int workers = 1, batch = 1024, round = 0, spawn = -1, keyIndex = 0;
    BoundsMode bounds = BoundsMode::Enabled;
    uint64_t count = 0;
    std::string Name() const
    {
        return "k" + std::to_string(keyIndex + 1) + "-" + (reference ? "reference" : gpu ? "gpu" : "cpu") +
            "-w" + std::to_string(workers) + (gpu ? "-b" + std::to_string(batch) +
            (specialized ? "-specialized" : "-generic") : "") + "-bounds-" + BoundsName(bounds) + "-r" + std::to_string(round) +
            (spawn < 0 ? "-all-spawns" : "-spawn" + std::to_string(spawn)) + "-n" + std::to_string(count);
    }
};
inline std::vector<Experiment> Experiments(const SuiteOptions &o)
{
    if (o.count < 32 || o.count > 16384 || o.rounds < 1 || o.rounds > 3 || o.workers < 1 || o.workers > 6 ||
        o.recipe < 1 || o.recipe > 2 || o.keys < 1 || o.keys > 3 || o.levels.empty() || o.levels.size() > 3)
        throw std::runtime_error("Invalid automatic benchmark plan");
    for (size_t i = 0; i < o.levels.size(); i++)
    {
        if (o.levels[i] != 2 && o.levels[i] != 5 && o.levels[i] != 7)
            throw std::runtime_error("Choose Hide & Seek, Outside or Classic Bonus");
        if (std::find(o.levels.begin(), o.levels.begin() + i, o.levels[i]) != o.levels.begin() + i)
            throw std::runtime_error("Duplicate benchmark map");
    }
    std::vector<Experiment> result;
    for (int key = 0; key < o.keys; key++)
    {
        Experiment reference; reference.reference = true; reference.bounds = BoundsMode::Disabled;
        reference.keyIndex = key; reference.count = uint64_t(o.count); result.push_back(reference);
        Experiment cpu = reference; cpu.reference = false; cpu.workers = 6; cpu.bounds = BoundsMode::Enabled;
        std::vector<Experiment> base{cpu};
        for (int batch : {512, 1024, 2048})
            for (bool specialized : {false, true})
                for (auto bounds : {BoundsMode::Enabled, BoundsMode::Disabled, BoundsMode::Adaptive})
                {
                    auto e = cpu; e.gpu = true; e.workers = o.workers; e.batch = batch;
                    e.specialized = specialized; e.bounds = bounds; base.push_back(e);
                }
        for (int round = 1; round <= o.rounds; round++)
            for (size_t i = 0; i < base.size(); i++)
            {
                auto e = base[(i + size_t((round - 1) * 5 + key * 7)) % base.size()];
                e.round = round; result.push_back(e);
            }
        reference.spawn = cpu.spawn = 0;
        reference.count = cpu.count = uint64_t(std::min(o.count, 1024));
        result.push_back(reference); result.push_back(cpu);
        auto filteredGpu = cpu; filteredGpu.gpu = filteredGpu.specialized = true;
        filteredGpu.workers = o.workers; filteredGpu.bounds = BoundsMode::Adaptive; result.push_back(filteredGpu);
    }
    return result;
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
    int shaderCrystals = 0, shaderThreads = 0, shaderSharedBytes = 0;
};
inline const char *SearchStatus(practice::SearchPhase phase)
{
    return phase == practice::SearchPhase::Completed ? "Completed" : phase == practice::SearchPhase::Stopped ? "Stopped" : "Failed";
}
inline std::string SearchJson(const SearchRun &r)
{
    const auto &s = r.state;
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::boolalpha << std::setprecision(17);
    out << "{\"schema\":\"practice-suite.gpu-search.v5\",\"version\":"; Quote(out, suite_brand::Version);
    out << ",\"channel\":\"GPU Dev 5\",\"sourceCommit\":";
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
    out << ",\"keyIndex\":" << r.experiment.keyIndex << ",\"boundsMode\":"; Quote(out, BoundsName(r.experiment.bounds));
    out << ",\"boundsChecked\":"; if (r.experiment.gpu) out << r.bounds.checked; else out << "null";
    out << ",\"boundsSkipped\":"; if (r.experiment.gpu) out << r.bounds.skipped; else out << "null";
    out << ",\"shader\":"; Quote(out, !r.experiment.gpu ? "cpu" : r.experiment.specialized ? "specialized" : "generic");
    out << ",\"shaderCrystals\":" << r.shaderCrystals << ",\"shaderThreads\":" << r.shaderThreads
        << ",\"shaderSharedBytes\":" << r.shaderSharedBytes;
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
    struct ShaderPreparation { int scene = 0, crystals = 0, threads = 0, sharedBytes = 0; double compileMs = 0; };
    std::vector<ShaderPreparation> shaders;
};
inline std::string SuiteJson(const SuiteState &s)
{
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::boolalpha << std::setprecision(17);
    out << "{\"schema\":\"practice-suite.gpu-suite.v5\",\"version\":"; Quote(out, suite_brand::Version);
    out << ",\"channel\":\"GPU Dev 5\",\"sourceCommit\":";
#ifdef SUITE_DEV_SOURCE_COMMIT
    Quote(out, SUITE_DEV_SOURCE_COMMIT);
#else
    Quote(out, "local");
#endif
    out << ",\"plan\":{\"adapter\":" << s.options.adapter << ",\"seeds\":" << s.options.count
        << ",\"rounds\":" << s.options.rounds << ",\"gpuCpuHelpers\":" << s.options.workers << ",\"cpuWorkers\":6"
        << ",\"recipe\":" << s.options.recipe << ",\"key\":"; Quote(out, std::to_string(s.options.key));
    out << ",\"mapLevels\":"; Array(out, s.options.levels);
    out << ",\"keys\":[";
    for (int i = 0; i < s.options.keys; i++) { if (i) out << ','; Quote(out, std::to_string(SearchKey(s.options, i))); }
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
        out << "{\"scene\":" << v.scene << ",\"crystals\":" << v.crystals << ",\"threads\":" << v.threads
            << ",\"sharedBytes\":" << v.sharedBytes << ",\"compileMs\":" << v.compileMs << '}';
    }
    out << "],\"scope\":\"Complete frozen-map search with strict per-seed CPU-reference audits and identical Top10. CPU uses production workers/caches. GPU compares generic and fixed-crystal FP64 shaders and on/off/adaptive bounds with bounded packets. Adaptive bounds sample 64 checks and skip 512 candidates when fewer than two checks reject. Disabling bounds fully scores candidates. Configurations share each map/key/recipe and rotate between repetitions. Shared geometry, device, shader compilation and warmup are outside search times; GPU buffer setup and query collection are included. No live Unity generation, scene loading or preview rendering is timed.\",\"runs\":[";
    for (size_t i = 0; i < s.runs.size(); i++) { if (i) out << ','; out << SearchJson(s.runs[i]); }
    out << "]}"; return out.str();
}
} // namespace gpu_dev
