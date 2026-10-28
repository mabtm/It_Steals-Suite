#pragma once
#include "SeedGpuDevice.h"
#include "SeedGpuSearch.h"

namespace seed_gpu
{
// Session-local profiles include the geometry, generation recipe, filter, goal
// and CPU budget. A new capture cannot silently inherit an unrelated decision.
class Engine
{
    using Clock = std::chrono::steady_clock;
    struct Profile
    {
        std::shared_ptr<const practice::MapData> map;
        practice::SeedSearchRequest request;
        bool gpu = false;
        double cpuMs = 0, gpuMs = 0;
    };
    std::unique_ptr<Device> device;
    std::shared_ptr<const practice::MapData> uploaded;
    std::deque<Profile> profiles;
    std::string name, unavailable;
    bool software;
    int adapterIndex;
    static double Ms(Clock::time_point at) { return std::chrono::duration<double, std::milli>(Clock::now() - at).count(); }
    static ShaderKind Shader(int scene) { return scene == 13 ? ShaderKind::Compact : ShaderKind::CompactFixed; }
    static int Helpers(const practice::SeedSearchRequest &r) { return std::min(r.workers, r.scene == 5 ? 4 : 2); }
    static bool Matches(const Profile &p, const practice::MapData &d, const practice::SeedSearchRequest &r)
    {
        const auto &a = *p.map; const auto &b = p.request;
        return a.keep == d.keep && a.rngInit == d.rngInit && a.rngRange == d.rngRange && a.rngAdvance == d.rngAdvance &&
            a.rngSite == d.rngSite && (a.flags & 16) == (d.flags & 16) &&
            b.recipe == r.recipe && b.fast == r.fast && b.workers == r.workers &&
            b.criteria.spawn == r.criteria.spawn && b.criteria.objective == r.criteria.objective && practice::SameNavigationCatalog(a, d);
    }
    static std::string ProfileText(const Profile &p)
    {
        std::ostringstream note; note.imbue(std::locale::classic());
        note << "Automatic calibration: CPU " << uint64_t(1024000. / p.cpuMs) << "/s; GPU "
             << uint64_t(1024000. / p.gpuMs) << "/s. Cached for this map/settings.";
        return note.str();
    }
    void Prepare(std::shared_ptr<const practice::MapData> data, const practice::SearchDistances &distances)
    {
        if (!device)
        {
            std::string error = "No compatible hardware GPU was found";
            for (const auto &adapter : Adapters())
            {
                if ((adapterIndex >= 0 && adapter.index != adapterIndex) || (adapter.software && !software)) continue;
                try { device = std::make_unique<Device>(adapter.index, software); name = adapter.name; break; }
                catch (const std::exception &e) { error = e.what(); }
            }
            if (!device) throw std::runtime_error(error);
        }
        if (!uploaded || !practice::SameNavigationCatalog(*uploaded, *data))
        {
            auto catalog = std::make_shared<Catalog>(int(data->spawns.size()), int(data->points.size()),
                [&](int from, int to) { return distances.CenterDistance(from, to); });
            device->SetCatalog(std::move(catalog)); uploaded = data;
        }
        device->Select(Shader(data->scene), data->keep);
    }
    static bool SameTop(const practice::SeedSearchState &a, const practice::SeedSearchState &b)
    {
        if (a.top.size() != b.top.size() || a.evaluated != b.evaluated || a.filtered != b.filtered) return false;
        for (size_t i = 0; i < a.top.size(); i++)
            if (a.top[i].index != b.top[i].index || a.top[i].seed != b.top[i].seed ||
                !practice::SameLayout(a.top[i].selection, b.top[i].selection) || !SameScore(a.top[i].route, b.top[i].route)) return false;
        return true;
    }
  public:
    // Software adapters are admitted only by the Windows integration probe.
    explicit Engine(bool allowSoftware = false, int selectedAdapter = -1) : software(allowSoftware), adapterIndex(selectedAdapter) {}

    bool Run(practice::SeedSearchState &state, const practice::SearchDistances &distances,
             const practice::Cancel &canceled, const Progress &progress)
    {
        using namespace practice;
        const auto original = state;
        const auto &r = original.request; const auto &data = *original.source->data;
        if (r.backend == SearchBackend::Cpu) { state.backendNote = "CPU selected."; return false; }
        if (!Eligible(data, r))
        {
            state.backendNote = "CPU: GPU scoring requires a complete verified 16-52 crystal Outside, Classic Bonus or Hide & Seek map.";
            return false;
        }
        if (r.backend == SearchBackend::Automatic && data.scene == 5)
        {
            state.backendNote = "Automatic: CPU bounds accelerate long Hide & Seek searches."; return false;
        }
        if (r.backend == SearchBackend::Automatic && r.count < 4096)
        {
            state.backendNote = "Automatic: CPU avoids GPU setup for a short search."; return false;
        }
        state.request.prune = data.scene == 5 ? r.prune : false;
        if (!unavailable.empty()) { state.backendNote = unavailable; return false; }
        if (r.backend == SearchBackend::Automatic)
            for (const auto &p : profiles) if (!p.gpu && Matches(p, data, r))
            {
                state.backendNote = "CPU selected. " + ProfileText(p); state.accelerator = name; return false;
            }
        auto started = Clock::now();
        try
        {
            auto check = [&] { if (canceled()) throw std::runtime_error("Seed search stopped"); };
            auto preparing = state; preparing.phase = SearchPhase::Preparing;
            preparing.backendNote = "Preparing and verifying GPU search..."; progress(preparing);
            check(); Prepare(original.source->data, distances); check();
            PortableSelectionGenerator generator(data); RandomSeedOrder order(r.key);
            // Validate real layouts and pickup-aware scores before every search,
            // including when the geometry/profile is reused from this session.
            std::vector<SeedLayout> warm(64);
            for (size_t i = 0; i < warm.size(); i++)
            {
                check(); warm[i].seed = order.SeedAt(uint32_t(i)); generator.Generate(warm[i].seed, r.recipe, warm[i].selection);
            }
            auto routes = device->Execute(warm, canceled);
            for (size_t i = 0; i < warm.size(); i++)
            {
                check(); auto gpu = distances.FinishOrder(warm[i].selection, std::move(routes[i]), canceled);
                if (!SameScore(distances.Score(warm[i].selection, r.fast, canceled), gpu)) throw std::runtime_error("GPU warmup differs from CPU reference");
            }
            std::string detail = "GPU selected; CPU-verified startup and Top 10.";
            if (r.backend == SearchBackend::Automatic)
            {
                auto found = std::find_if(profiles.begin(), profiles.end(), [&](const auto &p) { return Matches(p, data, r); });
                if (found == profiles.end())
                {
                    preparing.backendNote = "Calibrating CPU and GPU search (two short paired trials)..."; progress(preparing);
                    Profile profile; profile.map = original.source->data; profile.request = r;
                    double cpuMs[2]{}, gpuMs[2]{};
                    for (int round = 0; round < 2; round++)
                    {
                        auto trial = r; trial.count = 1024; trial.prune = false;
                        trial.key ^= 0x9e3779b97f4a7c15ULL * uint64_t(round + 1);
                        SeedSearchState cpu, gpu;
                        auto runCpu = [&] { cpu = SeedSearch::RunFrozen(original.source, distances, trial, canceled); };
                        auto runGpu = [&] {
                            auto request = trial; request.workers = Helpers(r);
                            gpu = Search(original.source, distances, request, 4096,
                                [&](auto layouts) { return device->Execute(layouts, canceled); }, canceled);
                        };
                        if (round == 0) { runCpu(); check(); runGpu(); }
                        else { runGpu(); check(); runCpu(); }
                        check();
                        if (cpu.phase != SearchPhase::Completed || gpu.phase != SearchPhase::Completed || !SameTop(cpu, gpu))
                            throw std::runtime_error("GPU calibration differs from CPU reference");
                        cpuMs[round] = cpu.elapsed * 1000.; gpuMs[round] = gpu.elapsed * 1000.;
                    }
                    profile.cpuMs = (cpuMs[0] + cpuMs[1]) / 2; profile.gpuMs = (gpuMs[0] + gpuMs[1]) / 2;
                    profile.gpu = profile.gpuMs < .9 * profile.cpuMs;
                    profiles.push_front(profile); if (profiles.size() > 4) profiles.pop_back(); found = profiles.begin();
                }
                detail = ProfileText(*found); state.calibrationMs = Ms(started);
                if (!found->gpu) { state.backendNote = "CPU selected. " + detail; state.accelerator = name; return false; }
            }
            state.calibrationMs = Ms(started);
            const auto prepared = state;
            auto request = r; request.prune = false; request.workers = Helpers(r);
            std::map<uint64_t, RouteResult> verified;
            auto verifyTop = [&](const SeedSearchState &s) {
                std::map<uint64_t, RouteResult> next;
                for (const auto &row : s.top)
                {
                    check(); auto prior = verified.find(row.index);
                    auto cpu = prior == verified.end() ? distances.Score(row.selection, r.fast, canceled) : prior->second;
                    if (!SameScore(cpu, row.route)) throw std::runtime_error("GPU Top 10 differs from CPU reference");
                    next.emplace(row.index, std::move(cpu));
                }
                verified = std::move(next);
            };
            auto update = [&](SeedSearchState s) {
                verifyTop(s); s.accelerator = name; s.backendNote = detail;
                s.preparationMs = prepared.preparationMs; s.calibrationMs = prepared.calibrationMs;
                s.distanceCacheReused = prepared.distanceCacheReused;
                s.prepared = prepared.prepared; s.prepareTotal = prepared.prepareTotal;
                s.projectedSpawns = prepared.projectedSpawns; s.projectedPoints = prepared.projectedPoints;
                s.estimatedPairs = prepared.estimatedPairs; s.recoveredPairs = prepared.recoveredPairs; s.repairQueries = prepared.repairQueries;
                return s;
            };
            auto nextPublish = Clock::now(); uint64_t batchIndex = 0;
            auto result = Search(original.source, distances, request, 4096,
                [&](auto layouts) {
                    auto scores = device->Execute(layouts, canceled); check();
                    // One changing sample per dispatch detects errors beyond the
                    // visible winners without repeating every GPU solve on the CPU.
                    size_t sample = size_t(batchIndex++ % layouts.size());
                    auto gpu = distances.FinishOrder(layouts[sample].selection, scores[sample], canceled);
                    if (!SameScore(distances.Score(layouts[sample].selection, r.fast, canceled), gpu))
                        throw std::runtime_error("GPU batch sample differs from CPU reference");
                    return scores;
                }, canceled, [&](SeedSearchState s) {
                    if (Clock::now() >= nextPublish)
                    {
                        progress(update(std::move(s))); nextPublish = Clock::now() + std::chrono::milliseconds(200);
                    }
                });
            check();
            if (result.phase != SearchPhase::Completed) throw std::runtime_error(result.error.empty() ? "GPU search did not complete" : result.error);
            state = update(std::move(result)); return true;
        }
        catch (const std::exception &error)
        {
            double calibrationMs = state.calibrationMs;
            device.reset(); uploaded.reset(); profiles.clear();
            if (canceled()) { state.phase = SearchPhase::Stopped; return true; }
            unavailable = std::string("GPU disabled for this session: ") + error.what() + ". CPU restarted with the same search key.";
            state = original; state.gpu = false; state.accelerator.clear();
            state.request.prune = data.scene == 5 ? r.prune : false;
            state.calibrationMs = calibrationMs > 0 ? calibrationMs : Ms(started);
            state.backendNote = unavailable; state.phase = SearchPhase::Searching;
            progress(state); return false;
        }
    }
};
} // namespace seed_gpu
