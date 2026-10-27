#pragma once
#include "SeedSearch.h"
#include <bit>
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace gpu_dev
{
enum class Phase { Idle, Detecting, Preparing, Cpu, Gpu, Completed, Stopped, Failed };

inline const char *PhaseName(Phase phase)
{
    constexpr const char *names[]{"Idle", "Detecting adapters", "Preparing captured map", "CPU reference",
                                  "GPU hybrid", "Completed", "Stopped", "Failed"};
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
    int adapter = 0, seeds = 128, batch = 16, pairs = 3, recipe = 1;
    uint64_t key = 0;
    bool fixture = false;
};

struct PairTiming
{
    int pair = 0, checked = 0;
    bool gpuFirst = false;
    double cpuMs = 0, gpuMs = 0, packMs = 0, dispatchMs = 0, contactsMs = 0;
};

struct Case
{
    int pair = 0, seed = 0, spawn = -1;
    bool orderMatch = false, scoreMatch = false;
    std::vector<int> crystals;
    practice::RouteResult cpu, gpu;
    std::vector<double> matrix;
};

struct Observations
{
    double baselineGameFps = 0, baselineMenuCpuP95 = 0;
    FrameCost menuCpu, hudCpu, intervals;
    uint64_t menuFrames = 0, hudFrames = 0, gameSamples = 0;
    double gameFpsSum = 0;

    void Record(double cpuMs, double intervalMs, bool menu, double gameFps)
    {
        (menu ? menuCpu : hudCpu).Observe(cpuMs);
        (menu ? menuFrames : hudFrames)++;
        intervals.Observe(intervalMs);
        if (std::isfinite(gameFps) && gameFps > 0)
        {
            gameFpsSum += gameFps;
            gameSamples++;
        }
    }
};

struct Report
{
    Options options;
    Adapter adapter;
    Phase phase = Phase::Idle;
    int64_t startedUtcMs = 0;
    int generated = 0, checked = 0, mismatches = 0, warmupChecked = 0;
    bool fp64 = false, liveCapture = false;
    uint64_t gpuBufferBytes = 0;
    unsigned logicalCpus = 0;
    double preparationMs = 0, deviceMs = 0, elapsedMs = 0;
    std::string error, model, cpu;
    std::shared_ptr<const practice::MapData> source;
    std::vector<PairTiming> timings;
    std::vector<Case> cases;
    Observations observations;
    std::filesystem::path savedPath;
    std::string exportError;
};

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

inline std::string Json(const Report &r)
{
    const auto &frames = r.observations;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::boolalpha << std::setprecision(17);
    out << "{\n\"schema\":\"practice-suite.gpu-dev.v1\",\n\"version\":";
    Quote(out, suite_brand::Version);
    out << ",\"channel\":\"GPU Dev 1\",\"sourceCommit\":";
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
        << ",\"dedicatedBytes\":" << r.adapter.dedicatedBytes << ",\"testBufferBytes\":" << r.gpuBufferBytes << '}'
        << ",\n\"request\":{\"seeds\":" << r.options.seeds << ",\"pairs\":" << r.options.pairs
        << ",\"batch\":" << r.options.batch << ",\"recipe\":" << r.options.recipe << ",\"key\":";
    Quote(out, std::to_string(r.options.key));
    out << ",\"exactWhenPossible\":true,\"cpuWorkers\":1}"
        << ",\"generated\":" << r.generated << ",\"checked\":" << r.checked
        << ",\"mismatches\":" << r.mismatches << ",\"warmupChecked\":" << r.warmupChecked
        << ",\"navigationModel\":"; Quote(out, r.model);
    out << ",\"preparationMs\":"; Number(out, r.preparationMs);
    out << ",\"deviceSetupMs\":"; Number(out, r.deviceMs);
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
    out << ",\"note\":\"Rendered-frame observations during the whole test; not a controlled game/OBS FPS benchmark\"}"
        << ",\n\"pairs\":[";
    for (size_t i = 0; i < r.timings.size(); i++)
    {
        const auto &p = r.timings[i];
        if (i) out << ',';
        out << "{\"pair\":" << p.pair << ",\"checked\":" << p.checked << ",\"gpuFirst\":" << p.gpuFirst
            << ",\"cpuMs\":" << p.cpuMs << ",\"gpuHybridMs\":" << p.gpuMs
            << ",\"packMs\":" << p.packMs << ",\"uploadDispatchReadbackMs\":" << p.dispatchMs
            << ",\"cpuPickupMs\":" << p.contactsMs << '}';
    }
    out << "],\n\"cases\":[";
    for (size_t i = 0; i < r.cases.size(); i++)
    {
        const auto &c = r.cases[i];
        if (i) out << ',';
        out << "\n{\"pair\":" << c.pair << ",\"seed\":" << c.seed << ",\"spawn\":" << c.spawn
            << ",\"orderMatch\":" << c.orderMatch << ",\"scoreMatch\":" << c.scoreMatch << ",\"crystals\":";
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
        std::filesystem::rename(temporary, path);
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
} // namespace gpu_dev
