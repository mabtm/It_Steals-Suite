#pragma once
#include "GpuDevSearch.h"

namespace gpu_dev
{
struct SuiteOptions
{
    int adapter = 0, count = 16384, rounds = 3, workers = 6, recipe = 1;
    uint64_t key = 0;
    std::vector<int> levels{7, 5};
};
struct Experiment
{
    bool gpu = false, reference = false;
    int workers = 1, batch = 1024, round = 0, spawn = -1;
    uint64_t count = 0;
    std::string Name() const
    {
        return std::string(reference ? "reference" : gpu ? "gpu" : "cpu") + "-w" + std::to_string(workers) +
            (gpu ? "-b" + std::to_string(batch) : "") + "-r" + std::to_string(round) +
            (spawn < 0 ? "-all-spawns" : "-spawn" + std::to_string(spawn)) + "-n" + std::to_string(count);
    }
};
inline std::vector<Experiment> Experiments(const SuiteOptions &o)
{
    if (o.count < 32 || o.count > 16384 || o.rounds < 1 || o.rounds > 3 || o.workers < 1 || o.workers > 6 ||
        o.recipe < 1 || o.recipe > 2 || o.levels.empty() || o.levels.size() > 2)
        throw std::runtime_error("Invalid automatic benchmark plan");
    if (o.levels.size() == 2 && o.levels[0] == o.levels[1]) throw std::runtime_error("Duplicate benchmark map");
    for (int level : o.levels) if (level != 5 && level != 7) throw std::runtime_error("Choose Outside or Classic Bonus");
    std::vector<Experiment> result{{false, true, 1, 1024, 0, -1, uint64_t(o.count)}};
    std::vector<Experiment> base;
    for (int w = 1; w <= o.workers; w++) base.push_back({false, false, w, 1024, 0, -1, uint64_t(o.count)});
    for (int batch : {512, 1024, 2048})
        for (int w = 1; w <= o.workers; w++) base.push_back({true, false, w, batch, 0, -1, uint64_t(o.count)});
    for (int round = 1; round <= o.rounds; round++)
        for (size_t i = 0; i < base.size(); i++)
        {
            auto e = base[(i + size_t((round - 1) * o.workers)) % base.size()];
            e.round = round; result.push_back(e);
        }
    uint64_t filtered = uint64_t(std::min(o.count, 1024));
    result.push_back({false, true, 1, 1024, 0, 0, filtered});
    result.push_back({false, false, o.workers, 1024, 0, 0, filtered});
    result.push_back({true, false, o.workers, 1024, 0, 0, filtered});
    return result;
}
inline const char *ModeSlug(int scene) { return scene == 13 ? "hide-seek-outside" : scene == 14 ? "classic-bonus" : "unknown"; }
inline const char *ModeName(int scene) { return scene == 13 ? "Hide & Seek Outside" : scene == 14 ? "Classic Bonus" : "Unknown"; }
struct SearchRun
{
    Experiment experiment;
    practice::SeedSearchState state;
    PairTiming gpu;
    uint64_t audited = 0;
    double bufferSetupMs = 0;
    bool topMatch = false;
    std::string file;
};
inline const char *SearchStatus(practice::SearchPhase phase)
{
    return phase == practice::SearchPhase::Completed ? "Completed" : phase == practice::SearchPhase::Stopped ? "Stopped" : "Failed";
}
inline std::string SearchJson(const SearchRun &r)
{
    const auto &s = r.state;
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::boolalpha << std::setprecision(17);
    out << "{\"schema\":\"practice-suite.gpu-search.v4\",\"version\":"; Quote(out, suite_brand::Version);
    out << ",\"channel\":\"GPU Dev 4\",\"sourceCommit\":";
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
    out << ",\"requested\":" << s.request.count << ",\"generated\":" << s.generated
        << ",\"evaluated\":" << s.evaluated << ",\"filtered\":" << s.filtered << ",\"pruned\":" << s.pruned
        << ",\"scored\":" << s.scored << ",\"cacheHits\":" << s.cacheHits
        << ",\"audited\":" << r.audited << ",\"top10Match\":" << r.topMatch
        << ",\"elapsedMs\":" << s.elapsed * 1000. << ",\"seedsPerSecond\":" << (s.elapsed > 0 ? s.evaluated / s.elapsed : 0)
        << ",\"bufferSetupMs\":" << r.bufferSetupMs << ",\"generationMicros\":" << s.generationMicros
        << ",\"gpuTimestampBatches\":" << r.gpu.timestampBatches << ",\"gpuTimestampInvalid\":" << r.gpu.timestampInvalid
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
};
inline std::string SuiteJson(const SuiteState &s)
{
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::boolalpha << std::setprecision(17);
    out << "{\"schema\":\"practice-suite.gpu-suite.v4\",\"version\":"; Quote(out, suite_brand::Version);
    out << ",\"channel\":\"GPU Dev 4\",\"sourceCommit\":";
#ifdef SUITE_DEV_SOURCE_COMMIT
    Quote(out, SUITE_DEV_SOURCE_COMMIT);
#else
    Quote(out, "local");
#endif
    out << ",\"plan\":{\"adapter\":" << s.options.adapter << ",\"seeds\":" << s.options.count
        << ",\"rounds\":" << s.options.rounds << ",\"maxWorkers\":" << s.options.workers
        << ",\"recipe\":" << s.options.recipe << ",\"key\":"; Quote(out, std::to_string(s.options.key));
    out << ",\"mapLevels\":"; Array(out, s.options.levels);
    out << "},\"mapMetadataAreSharedPreparationOnly\":true,\"status\":"; Quote(out, s.status);
    out << ",\"activeTest\":"; Quote(out, s.active); out << ",\"error\":"; Quote(out, s.error);
    out << ",\"startedUtcMs\":" << s.startedUtcMs << ",\"completedMaps\":" << s.completedMaps
        << ",\"completedTests\":" << s.completedTests << ",\"totalTests\":" << s.totalTests
        << ",\"warmupChecked\":" << s.warmupChecked << ",\"mapMetadata\":[";
    for (size_t i = 0; i < s.maps.size(); i++) { if (i) out << ','; out << Json(s.maps[i]); }
    out << "],\"scope\":\"Complete frozen-map search: verified portable RNG, spawn filter, route bounds, scoring and Top10 ranking. Shared cold preparation is reported per map, outside timed searches. Every scored route is audited against the exhaustive CPU reference. CPU uses production per-worker caches; GPU uses bounded packets without route caches. No live Unity generation or preview rendering is timed.\",\"runs\":[";
    for (size_t i = 0; i < s.runs.size(); i++) { if (i) out << ','; out << SearchJson(s.runs[i]); }
    out << "]}"; return out.str();
}
} // namespace gpu_dev
