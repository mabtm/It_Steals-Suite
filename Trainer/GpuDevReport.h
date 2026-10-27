#pragma once
#include "SeedSearch.h"
#include <bit>
#include <filesystem>
#include <fstream>
#include <iomanip>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace gpu_dev
{
enum class Phase { Idle, Detecting, Preparing, Baseline, Warming, Cpu, Gpu, Verifying, Completed, Stopped, Failed };

inline const char *PhaseName(Phase phase)
{
    constexpr const char *names[]{"Idle", "Detecting adapters", "Preparing captured map", "Idle baseline",
                                  "Warming scoring paths", "CPU reference", "GPU hybrid", "Checking results / saving", "Completed", "Stopped", "Failed"};
    return names[int(phase)];
}

struct Adapter
{
    int index = 0;
    std::string name, driver;
    uint32_t vendor = 0, device = 0;
    uint64_t dedicatedBytes = 0;
    bool software = false;
};

struct Options
{
    int adapter = 0, seeds = 128, batch = 16, pairs = 3, recipe = 1, baselineSeconds = 0;
    uint64_t key = 0;
    bool fixture = false, sweep = false, gpuTiming = true;
};

inline std::vector<int> Batches(const Options &o) { return o.sweep ? std::vector<int>{16, 64, 128, 256} : std::vector<int>{o.batch}; }
inline int TotalChecks(const Options &o) { return o.seeds * o.pairs * int(Batches(o).size()); }
inline bool ValidOptions(const Options &o)
{
    return o.seeds >= 1 && o.seeds <= 16384 && o.batch >= 1 && o.batch <= 256 &&
           o.pairs >= 1 && o.pairs <= 12 && o.recipe >= 1 && o.recipe <= 2 &&
           o.baselineSeconds >= 0 && o.baselineSeconds <= 30;
}

inline Options Preset(int index)
{
    Options o;
    o.seeds = index == 0 ? 1024 : index == 1 ? 4096 : 8192;
    o.pairs = index == 0 ? 4 : index == 1 ? 6 : 12;
    o.sweep = index != 0;
    o.baselineSeconds = index == 2 ? 10 : 5;
    return o;
}

struct PairTiming
{
    int pair = 0, batch = 0, sequence = 0, checked = 0, cpuScored = 0, gpuScored = 0, mismatches = 0;
    bool gpuFirst = false, complete = false;
    double cpuMs = 0, gpuMs = 0, packMs = 0, dispatchMs = 0, contactsMs = 0;
    double cpuThreadMs = 0, gpuThreadMs = 0, hostUploadMs = 0, hostReadbackMs = 0, queryCollectMs = 0;
    double bufferSetupMs = 0, verificationMs = 0;
    double gpuUploadMs = 0, gpuKernelMs = 0, gpuCopyMs = 0;
    int timestampBatches = 0, timestampInvalid = 0;
    uint64_t cpuDigest = 14695981039346656037ull, gpuDigest = 14695981039346656037ull;
};

struct Case
{
    int pair = 0, batch = 0, seed = 0, spawn = -1;
    bool orderMatch = false, scoreMatch = false, centerLengthMatch = false;
    double cpuCenterLength = 0, gpuCenterLength = 0;
    std::vector<int> crystals;
    practice::RouteResult cpu, gpu;
    std::vector<double> matrix;
};

// Fixed-memory histogram over the whole run. Quantiles are upper bucket bounds
// at 0.1 ms resolution; slow frames retain their exact maximum separately.
struct FrameStats
{
    std::array<uint64_t, 2048> bins{};
    uint64_t count = 0, over16 = 0, over33 = 0;
    double sum = 0, maximum = 0;
    void Observe(double ms)
    {
        if (!std::isfinite(ms) || ms < 0) return;
        bins[size_t(std::min(ms * 10., double(bins.size() - 1)))]++;
        count++; sum += ms; maximum = std::max(maximum, ms);
        over16 += ms > 1000. / 60.; over33 += ms > 1000. / 30.;
    }
    double Mean() const { return count ? sum / count : 0; }
    double Quantile(double fraction) const
    {
        uint64_t target = uint64_t(std::ceil(count * std::clamp(fraction, 0., 1.))), seen = 0;
        if (!target) return 0;
        for (size_t i = 0; i < bins.size(); i++)
            if ((seen += bins[i]) >= target) return i + 1 == bins.size() ? maximum : (i + 1) / 10.;
        return maximum;
    }
    double P95() const { return Quantile(.95); }
};

struct PhaseFrames
{
    Phase phase = Phase::Idle;
    int batch = 0;
    FrameStats menuCpu, hudCpu, intervals;
    uint64_t gameSamples = 0;
    double gameFpsSum = 0;
};

struct Observations
{
    double baselineGameFps = 0, baselineMenuCpuP95 = 0;
    FrameStats menuCpu, hudCpu, intervals;
    uint64_t menuFrames = 0, hudFrames = 0, gameSamples = 0;
    double gameFpsSum = 0;
    std::vector<PhaseFrames> phases;
    Phase previous = Phase::Idle;
    int previousBatch = -1;

    void Record(double cpuMs, double intervalMs, bool menu, double gameFps,
                Phase phase = Phase::Idle, int batch = 0)
    {
        (menu ? menuCpu : hudCpu).Observe(cpuMs);
        (menu ? menuFrames : hudFrames)++;
        intervals.Observe(intervalMs);
        if (std::isfinite(gameFps) && gameFps > 0)
        {
            gameFpsSum += gameFps;
            gameSamples++;
        }
        // Discard the interval straddling a phase transition from phase statistics.
        bool stable = previous == phase && previousBatch == batch;
        previous = phase; previousBatch = batch;
        if (!stable || (phase != Phase::Baseline && phase != Phase::Cpu && phase != Phase::Gpu)) return;
        auto it = std::find_if(phases.begin(), phases.end(), [&](const auto &p) { return p.phase == phase && p.batch == batch; });
        if (it == phases.end()) { phases.push_back({}); it = std::prev(phases.end()); it->phase = phase; it->batch = batch; }
        (menu ? it->menuCpu : it->hudCpu).Observe(cpuMs);
        it->intervals.Observe(intervalMs);
        if (std::isfinite(gameFps) && gameFps > 0) { it->gameFpsSum += gameFps; it->gameSamples++; }
    }
};

struct Report
{
    Options options;
    Adapter adapter;
    Phase phase = Phase::Idle;
    int64_t startedUtcMs = 0;
    int generated = 0, checked = 0, mismatches = 0, warmupChecked = 0, successRows = 0, mismatchRows = 0;
    bool fp64 = false, liveCapture = false, timestampsSupported = false;
    uint64_t gpuBufferBytes = 0;
    unsigned logicalCpus = 0;
    double preparationMs = 0, deviceMs = 0, generationMs = 0, warmupMs = 0, baselineMs = 0, elapsedMs = 0;
    std::string error, model, cpu;
    std::shared_ptr<const practice::MapData> source;
    std::vector<PairTiming> timings;
    std::vector<Case> cases;
    Observations observations;
    std::filesystem::path savedPath;
    std::string exportError;
};

inline void Digest(uint64_t &hash, uint64_t value)
{
    for (int i = 0; i < 8; i++) { hash = (hash ^ uint8_t(value)) * 1099511628211ull; value >>= 8; }
}
inline void DigestRoute(uint64_t &hash, const practice::RouteResult &r)
{
    Digest(hash, std::bit_cast<uint64_t>(r.length));
    Digest(hash, r.complete | (r.exact << 1) | (r.pickupOptimized << 2) | (r.estimated << 3));
    Digest(hash, r.order.size()); for (int id : r.order) Digest(hash, uint64_t(id));
    Digest(hash, r.contacts.size()); for (int id : r.contacts) Digest(hash, uint64_t(id));
}

inline bool KeepCase(Report &r, bool mismatch, bool warmup)
{
    // Every layout is checked; only successful examples and bounded failures
    // retain the expensive vectors/matrices in the JSON report.
    if (mismatch) { if (r.mismatchRows >= 16) return false; r.mismatchRows++; }
    else { if (r.successRows >= 64 && !warmup) return false; r.successRows++; }
    return true;
}

inline bool ValidOrder(const std::vector<int> &order, size_t count)
{
    if (count < 1 || count > 52 || order.size() != count)
        return false;
    uint64_t used = 0;
    for (int id : order)
    {
        if (id < 0 || size_t(id) >= count || (used & (uint64_t{1} << id)))
            return false;
        used |= uint64_t{1} << id;
    }
    return true;
}

inline bool SameScore(const practice::RouteResult &a, const practice::RouteResult &b)
{
    return a.complete && b.complete && std::isfinite(a.length) && a.length == b.length &&
           a.order == b.order && a.contacts == b.contacts && a.pickupOptimized == b.pickupOptimized &&
           a.estimated == b.estimated && a.exact == b.exact;
}

inline void Quote(std::ostream &out, std::string_view value)
{
    constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (unsigned char c : value)
        if (c == '"' || c == '\\')
            out << '\\' << char(c);
        else if (c < 32)
            out << "\\u00" << hex[c >> 4] << hex[c & 15];
        else
            out << char(c);
    out << '"';
}

inline void Number(std::ostream &out, double value)
{
    if (std::isfinite(value))
        out << value;
    else
        out << "null";
}

template <class T> inline void Array(std::ostream &out, const std::vector<T> &values)
{
    out << '[';
    for (size_t i = 0; i < values.size(); i++)
    {
        if (i) out << ',';
        if constexpr (std::is_floating_point_v<T>) Number(out, values[i]);
        else out << values[i];
    }
    out << ']';
}

inline void RouteJson(std::ostream &out, const practice::RouteResult &route)
{
    out << "{\"complete\":" << route.complete << ",\"exact\":" << route.exact
        << ",\"pickupOptimized\":" << route.pickupOptimized << ",\"estimated\":" << route.estimated
        << ",\"length\":";
    Number(out, route.length);
    out << ",\"order\":";
    Array(out, route.order);
    out << ",\"contacts\":";
    Array(out, route.contacts);
    out << '}';
}

inline void FramesJson(std::ostream &out, const FrameStats &f)
{
    out << "{\"count\":" << f.count << ",\"meanMs\":" << f.Mean()
        << ",\"p50Ms\":" << f.Quantile(.5) << ",\"p95Ms\":" << f.P95()
        << ",\"p99Ms\":" << f.Quantile(.99) << ",\"maxMs\":" << f.maximum
        << ",\"over16_67Ms\":" << f.over16 << ",\"over33_33Ms\":" << f.over33 << '}';
}

inline std::string Json(const Report &r)
{
    const auto &frames = r.observations;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::boolalpha << std::setprecision(17);
    out << "{\n\"schema\":\"practice-suite.gpu-dev.v2\",\n\"version\":";
    Quote(out, suite_brand::Version);
    out << ",\"channel\":\"GPU Dev 2\",\"sourceCommit\":";
#ifdef SUITE_DEV_SOURCE_COMMIT
    Quote(out, SUITE_DEV_SOURCE_COMMIT);
#else
    Quote(out, "local");
#endif
    out << ",\"status\":"; Quote(out, PhaseName(r.phase));
    out << ",\"startedUtcMs\":" << r.startedUtcMs << ",\"error\":"; Quote(out, r.error);
    out << ",\n\"scope\":\"Frozen captured layouts; CPU full score versus GPU order plus CPU pickup scoring; no pruning or Unity generation timed\""
        << ",\"sourceKind\":"; Quote(out, r.options.fixture ? "asset_fixture" : "live_game_capture");
    out << ",\"cpu\":"; Quote(out, r.cpu);
    out << ",\"liveCapture\":" << r.liveCapture << ",\"logicalCpus\":" << r.logicalCpus
        << ",\n\"adapter\":{\"index\":" << r.adapter.index << ",\"name\":";
    Quote(out, r.adapter.name);
    out << ",\"driver\":"; Quote(out, r.adapter.driver);
    out << ",\"vendorId\":" << r.adapter.vendor << ",\"deviceId\":" << r.adapter.device
        << ",\"software\":" << r.adapter.software << ",\"fp64\":" << r.fp64
        << ",\"dedicatedBytes\":" << r.adapter.dedicatedBytes << ",\"peakTestBufferBytes\":" << r.gpuBufferBytes
        << ",\"timestampsSupported\":" << r.timestampsSupported << '}'
        << ",\n\"request\":{\"seeds\":" << r.options.seeds << ",\"pairs\":" << r.options.pairs
        << ",\"batch\":" << r.options.batch << ",\"recipe\":" << r.options.recipe << ",\"key\":";
    Quote(out, std::to_string(r.options.key));
    out << ",\"batches\":"; Array(out, Batches(r.options));
    out << ",\"baselineSeconds\":" << r.options.baselineSeconds
        << ",\"gpuTiming\":" << r.options.gpuTiming
        << ",\"exactWhenPossible\":true,\"cpuWorkers\":1,\"execution\":\"synchronous readback; rotating batch order; alternating full CPU/GPU passes\"}"
        << ",\"generated\":" << r.generated << ",\"checked\":" << r.checked
        << ",\"mismatches\":" << r.mismatches << ",\"warmupChecked\":" << r.warmupChecked
        << ",\"navigationModel\":"; Quote(out, r.model);
    out << ",\"preparationMs\":"; Number(out, r.preparationMs);
    out << ",\"deviceSetupMs\":"; Number(out, r.deviceMs);
    out << ",\"generationMs\":"; Number(out, r.generationMs);
    out << ",\"warmupMs\":"; Number(out, r.warmupMs);
    out << ",\"baselineMs\":"; Number(out, r.baselineMs);
    out << ",\"elapsedMs\":"; Number(out, r.elapsedMs);
    if (r.source)
    {
        const auto &d = *r.source;
        out << ",\n\"capture\":{\"scene\":" << d.scene << ",\"epoch\":" << d.epoch << ",\"gameHash\":";
        Quote(out, d.hash);
        out << ",\"flags\":" << d.flags << ",\"crystals\":" << d.keep << ",\"catalogPoints\":" << d.points.size()
            << ",\"spawns\":" << d.spawns.size() << ",\"physicalCaptured\":" << d.physicalCaptured
            << ",\"physicalOmitted\":" << d.physicalOmitted << ",\"pickupRegions\":" << d.pickupRegions.size()
            << ",\"rng\":{\"status\":" << d.rngStatus << ",\"probes\":" << d.rngProbes
            << ",\"init\":" << d.rngInit << ",\"range\":" << d.rngRange
            << ",\"advance\":" << d.rngAdvance << ",\"site\":" << d.rngSite << "}}";
    }
    out << ",\n\"uiObservations\":{\"baselineGameFps\":"; Number(out, frames.baselineGameFps);
    out << ",\"baselineMenuCpuP95Ms\":"; Number(out, frames.baselineMenuCpuP95);
    out << ",\"menuFrames\":" << frames.menuFrames << ",\"hudFrames\":" << frames.hudFrames
        << ",\"menuCpuMeanMs\":" << frames.menuCpu.Mean() << ",\"menuCpuP95Ms\":" << frames.menuCpu.P95()
        << ",\"hudCpuMeanMs\":" << frames.hudCpu.Mean() << ",\"hudCpuP95Ms\":" << frames.hudCpu.P95()
        << ",\"intervalP95Ms\":" << frames.intervals.P95() << ",\"gameFpsSamples\":" << frames.gameSamples
        << ",\"meanGameFpsSnapshots\":";
    Number(out, frames.gameSamples ? frames.gameFpsSum / frames.gameSamples : std::numeric_limits<double>::quiet_NaN());
    out << ",\"note\":\"Whole-run fixed-memory histograms, 0.1ms upper-bound quantiles (overflow uses max). Phase intervals omit the first transition frame. Game FPS are shared snapshots, not independent frame samples. No controlled OBS benchmark.\",\"phases\":[";
    for (size_t i = 0; i < frames.phases.size(); i++)
    {
        const auto &p = frames.phases[i];
        if (i) out << ',';
        out << "{\"phase\":"; Quote(out, PhaseName(p.phase));
        out << ",\"batch\":" << p.batch << ",\"menuCpu\":"; FramesJson(out, p.menuCpu);
        out << ",\"hudCpu\":"; FramesJson(out, p.hudCpu);
        out << ",\"intervals\":"; FramesJson(out, p.intervals);
        out << ",\"gameFpsSamples\":" << p.gameSamples << ",\"meanGameFpsSnapshots\":";
        Number(out, p.gameSamples ? p.gameFpsSum / p.gameSamples : std::numeric_limits<double>::quiet_NaN());
        out << '}';
    }
    out << "]},\n\"timingNotes\":\"Host wall times include instrumentation. GPU timestamps bracket input updates, Dispatch, and output copies on the GPU timeline; contention can be included. Invalid/disjoint timestamps are null and never used as zero-time results. Query collection is included in hybrid wall time.\",\n\"pairs\":[";
    for (size_t i = 0; i < r.timings.size(); i++)
    {
        const auto &p = r.timings[i];
        if (i) out << ',';
        out << "{\"pair\":" << p.pair << ",\"batch\":" << p.batch << ",\"sequence\":" << p.sequence
            << ",\"complete\":" << p.complete << ",\"checked\":" << p.checked
            << ",\"cpuScored\":" << p.cpuScored << ",\"gpuScored\":" << p.gpuScored
            << ",\"mismatches\":" << p.mismatches << ",\"gpuFirst\":" << p.gpuFirst
            << ",\"cpuMs\":" << p.cpuMs << ",\"gpuHybridMs\":" << p.gpuMs
            << ",\"packMs\":" << p.packMs << ",\"uploadDispatchReadbackMs\":" << p.dispatchMs
            << ",\"cpuPickupMs\":" << p.contactsMs << ",\"cpuThreadMs\":" << p.cpuThreadMs
            << ",\"gpuThreadMs\":" << p.gpuThreadMs << ",\"hostUploadMs\":" << p.hostUploadMs
            << ",\"hostReadbackMs\":" << p.hostReadbackMs << ",\"queryCollectMs\":" << p.queryCollectMs
            << ",\"bufferSetupMs\":" << p.bufferSetupMs << ",\"verificationMs\":" << p.verificationMs
            << ",\"timestampBatches\":" << p.timestampBatches << ",\"timestampInvalid\":" << p.timestampInvalid;
        out << ",\"gpuUploadMs\":"; Number(out, p.timestampBatches ? p.gpuUploadMs : NAN);
        out << ",\"gpuKernelMs\":"; Number(out, p.timestampBatches ? p.gpuKernelMs : NAN);
        out << ",\"gpuCopyMs\":"; Number(out, p.timestampBatches ? p.gpuCopyMs : NAN);
        out << ",\"cpuDigest\":"; Quote(out, std::to_string(p.cpuDigest));
        out << ",\"gpuDigest\":"; Quote(out, std::to_string(p.gpuDigest));
        out << '}';
    }
    out << "],\n\"caseRetention\":{\"compared\":" << r.checked + r.warmupChecked
        << ",\"stored\":" << r.cases.size() << ",\"successLimit\":64,\"failureLimit\":16,\"matrixLimit\":8,"
        << "\"note\":\"Every layout is strictly compared. Successful examples and first failures are retained; per-pass digests cover all compared final routes (FNV-1a, not cryptographic). Warmup may exceed the success limit.\"},\n\"cases\":[";
    for (size_t i = 0; i < r.cases.size(); i++)
    {
        const auto &c = r.cases[i];
        if (i) out << ',';
        out << "\n{\"pair\":" << c.pair << ",\"batch\":" << c.batch << ",\"seed\":" << c.seed << ",\"spawn\":" << c.spawn
            << ",\"orderMatch\":" << c.orderMatch << ",\"scoreMatch\":" << c.scoreMatch
            << ",\"centerLengthMatch\":" << c.centerLengthMatch << ",\"cpuCenterLength\":";
        Number(out, c.cpuCenterLength);
        out << ",\"gpuCenterLength\":"; Number(out, c.gpuCenterLength);
        out << ",\"crystals\":";
        Array(out, c.crystals);
        out << ",\"cpu\":"; RouteJson(out, c.cpu);
        out << ",\"gpu\":"; RouteJson(out, c.gpu);
        if (!c.matrix.empty()) { out << ",\"centerMatrix\":"; Array(out, c.matrix); }
        out << '}';
    }
    out << "\n]}\n";
    return out.str();
}

inline void Save(const std::filesystem::path &path, const Report &report)
{
    std::filesystem::create_directories(path.parent_path());
    auto temporary = path;
    temporary += ".tmp";
    try
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << Json(report);
        out.close();
        if (!out) throw std::runtime_error("Could not write the GPU JSON report");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace the GPU JSON checkpoint");
#else
        std::filesystem::rename(temporary, path);
#endif
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
} // namespace gpu_dev
