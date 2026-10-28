#pragma once
#include "SuiteBranding.h"
#include "PracticeRouting.h"
#include "PlannerRandom.h"
#include "SeedSearchOptions.h"
#include "SeedBatch.h"
#include "PerformanceTiming.h"
#include "PortableRandom.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <sstream>
#include <locale>

namespace practice
{
// Geometry and pair distances are independent of the seed. Build them once,
// then share immutable tables with all route workers. Contact costs have a
// separate bounded budget and never trigger Unity queries per candidate seed.
class SearchDistances
{
    const MapData &data;
    const NavGraph &graph;
    int estimatedPairs = 0;
    std::unique_ptr<PickupRouting> pickups;
    std::shared_ptr<const PreparedRoutes> table;
    std::vector<std::vector<uint16_t>> nearest;

  public:
    SearchDistances(const MapData &catalog, const NavGraph &nav, const Cancel &canceled,
                    const std::function<void(int, int)> &progress,
                    const std::function<void(const char *)> &step = {})
        : data(catalog), graph(nav)
    {
        int nodes = int(data.spawns.size() + data.points.size());
        int pairs = int(data.spawns.size() * data.points.size() + data.points.size() * (data.points.size() - 1) / 2);
        int total = nodes * 4 + pairs;
        auto report = [&](int done) { if (progress) progress(done, total); };
        if (step) step("navigation paths");
        report(0);
        table = graph.Routes(canceled, [&](int done, int) { report(done); });
        int count = table->count;
        // Seed layouts retain descending catalog IDs. Equal-distance ties in
        // this shared order therefore match the solver's local-index ties.
        // Sparse catalogs use the original per-selection sort instead.
        if (data.points.size() <= 4 * 64)
        {
            nearest.resize(count);
            for (int from = int(data.spawns.size()); from < count; from++)
            {
                if (canceled())
                    throw std::runtime_error("Canceled");
                auto &row = nearest[from];
                row.resize(data.points.size());
                std::iota(row.begin(), row.end(), uint16_t(data.spawns.size()));
                std::sort(row.begin(), row.end(), [&](int a, int b) {
                    double x = table->lengths[from][a], y = table->lengths[from][b];
                    return x < y || (x == y && a > b);
                });
                report(2 * nodes + from + 1);
            }
        }
        report(3 * nodes);
        const auto &estimates = table->estimates;
        estimatedPairs = 0;
        for (int a = 0; a < count; a++)
            for (int b = std::max(a + 1, int(data.spawns.size())); b < count; b++)
                estimatedPairs += estimates[a][b] != 0;
        std::vector<int> ids(count);
        std::iota(ids.begin(), ids.end(), 0);
        if (step) step("pickup contacts");
        pickups = std::make_unique<PickupRouting>(data, graph, ids, canceled,
            [&](int done, int) { report(3 * nodes + done); });
        if (step) step("pickup distances");
        int prepared = 4 * nodes;
        if (pickups->Active())
            for (int from = 0; from < count; from++)
                for (int to = std::max(from + 1, int(data.spawns.size())); to < count; to++)
                {
                    pickups->SetLeg(from, to, table->Between(from, to), estimates[from][to] != 0, canceled);
                    report(++prepared);
                }
        report(total);
    }

    int ProjectedSpawns() const
    {
        const auto &projections = table->projected;
        return static_cast<int>(std::count_if(projections.begin(), projections.begin() + data.spawns.size(),
                                              [](auto p) { return p.triangle >= 0; }));
    }

    int ProjectedPoints() const
    {
        const auto &projections = table->projected;
        return static_cast<int>(std::count_if(projections.begin() + data.spawns.size(), projections.end(),
                                              [](auto p) { return p.triangle >= 0; }));
    }

    int EstimatedPairs() const
    {
        return estimatedPairs;
    }

    int RecoveredPairs() const
    {
        return table->recoveredPairs;
    }

    int RepairQueries() const
    {
        return table->repairAttempts;
    }

    const char *ModelName() const
    {
        return pickups->Active()            ? "physical clearance + pickup contacts"
               : data.physicalCaptured      ? "floor + physical clearance / shared paths"
               : graph.UsesUnityCorridors() ? "Unity corridors / shared paths"
                                            : "triangle corridors / shared paths";
    }

    int Node(const Selection &selection, int index) const
    {
        return index == 0 ? selection.spawn
                          : static_cast<int>(data.spawns.size()) + selection.crystals.at(index - 1);
    }

    double CenterDistance(int from, int to) const { return table->lengths.at(from).at(to); }

    int FillMatrix(const Selection &selection, RouteWorkspace &work, bool bound = false) const
    {
        int count = static_cast<int>(selection.crystals.size()) + 1;
        bool contacts = bound && pickups->Active();
        std::array<int, 65> nodes;
        for (int i = 0; i < count; i++)
            nodes[i] = Node(selection, i);
        work.estimated = false;
        for (int from = 0; from < count; from++)
        {
            const auto &lengths = table->lengths.at(nodes[from]);
            const auto &estimates = table->estimates.at(nodes[from]);
            work.matrix[from * count + from] = 0;
            for (int to = from + 1; to < count; to++)
            {
                int b = nodes[to];
                work.matrix[from * count + to] = work.matrix[to * count + from] =
                    contacts ? pickups->Lower(nodes[from], b) : lengths.at(b);
                if (estimatedPairs)
                    work.estimated |= estimates.at(b) != 0;
            }
        }
        return count;
    }

    int FillBoundMatrix(const Selection &selection, RouteWorkspace &work) const
    {
        return FillMatrix(selection, work, true);
    }

    RouteResult Score(const Selection &selection, bool fast, const Cancel &canceled, RouteWorkspace &work,
                      bool boundPrepared = false) const
    {
        // Contact lower bounds differ from center distances; only the latter
        // can serve both pruning and scoring without filling the matrix twice.
        int count = boundPrepared && !pickups->Active() ? static_cast<int>(selection.crystals.size()) + 1
                                                        : FillMatrix(selection, work);
        RouteNeighbors neighbors;
        const RouteNeighbors *prepared = nullptr;
        if (count > 16 && data.points.size() <= 4 * selection.crystals.size() &&
            std::adjacent_find(selection.crystals.begin(), selection.crystals.end(),
                               std::less_equal<int>()) == selection.crystals.end())
        {
            std::array<int16_t, 1152> local; // wire limits: 128 spawns + 1024 candidate points
            local.fill(-1);
            for (int i = 0; i < count - 1; i++)
                local[Node(selection, i + 1)] = int16_t(i);
            for (int i = 0; i < count - 1; i++)
            {
                int rank = 0;
                for (int node : nearest[Node(selection, i + 1)])
                    if (local[node] >= 0)
                        neighbors[i][rank++] = uint8_t(local[node]);
            }
            prepared = &neighbors;
        }
        auto result = SolveOrderFlat(work.matrix.data(), count, work, canceled, !fast, prepared);
        return FinishOrder(selection, std::move(result), canceled);
    }

    RouteResult FinishOrder(const Selection &selection, RouteResult result, const Cancel &canceled) const
    {
        int count = static_cast<int>(selection.crystals.size()) + 1;
        if (pickups->Active())
        {
            std::array<int, 65> nodes;
            for (int i = 0; i < count; i++)
                nodes[i] = Node(selection, i);
            pickups->Optimize(result, std::span<const int>(nodes.data(), size_t(count)), canceled);
        }
        bool estimated = false;
        if (result.pickupOptimized)
            estimated = result.estimated;
        else if (result.complete)
        {
            estimated = false;
            int previous = selection.spawn;
            for (int index : result.order)
            {
                int next = Node(selection, index + 1);
                estimated |= table->estimates[previous][next] != 0;
                previous = next;
            }
        }
        DescribeRouteEstimate(result, estimated);
        return result;
    }

    RouteResult Score(const Selection &selection, bool fast, const Cancel &canceled) const
    {
        RouteWorkspace work;
        return Score(selection, fast, canceled, work);
    }

    RouteResult Draw(const Selection &selection, RouteResult result, const Cancel &canceled) const
    {
        if (canceled())
            throw std::runtime_error("Canceled");
        if (result.pickupOptimized)
        {
            int from = selection.spawn, contact = 0;
            size_t layer = 0;
            for (int index : result.order)
            {
                int to = Node(selection, index + 1), next = result.contacts[layer++];
                bool estimated = pickups->Estimated(from, contact, to, next);
                for (Vec3 p : pickups->Line(from, contact, to, next))
                    AppendRoutePoint(result, p, estimated);
                from = to;
                contact = next;
            }
            for (int &id : result.order)
                id = selection.crystals.at(id);
            return result;
        }
        int previous = selection.spawn;
        for (int index : result.order)
        {
            if (canceled())
                throw std::runtime_error("Canceled");
            int next = Node(selection, index + 1);
            bool estimated = table->estimates.at(previous).at(next) != 0;
            for (auto point : table->Between(previous, next).points)
                AppendRoutePoint(result, point, estimated);
            previous = next;
        }
        for (int &id : result.order)
            id = selection.crystals.at(id);
        return result;
    }
};

struct SeedResult
{
    int seed = 0;
    uint64_t index = 0;
    Selection selection;
    RouteResult route;
};
enum class SearchPhase
{
    Idle,
    Generating,
    Preparing,
    Searching,
    Completed,
    Stopped,
    Failed
};

struct SeedSearchRequest
{
    int pid = 0, scene = 0, epoch = 0, seed = 0, recipe = 2, workers = 1;
    uint64_t count = 100, key = 0;
    uint32_t serial = 0, session = 0;
    std::string hash;
    bool fast = false, live = true, layoutOnly = false, allowPortable = true, prune = true;
    SearchBackend backend = SearchBackend::Automatic;
    SearchCriteria criteria;
};

struct SeedSearchState
{
    SeedSearchRequest request;
    SearchPhase phase = SearchPhase::Idle;
    uint64_t generated = 0, evaluated = 0, estimated = 0, filtered = 0, scored = 0, pruned = 0, matched = 0,
             cacheHits = 0;
    uint64_t generationMicros = 0, generationFrames = 0;
    int prepared = 0, prepareTotal = 0;
    size_t queued = 0;
    int projectedSpawns = 0, projectedPoints = 0;
    int estimatedPairs = 0, recoveredPairs = 0, repairQueries = 0;
    std::string navigationModel;
    std::string accelerator, backendNote;
    std::string preparationStep = "navigation geometry";
    bool gpu = false;
    double calibrationMs = 0;
    bool portable = false, distanceCacheReused = false;
    double preparationMs = 0;
    double elapsed = 0, recentGenerated = 0, recentChecked = 0,
           remainingSeconds = std::numeric_limits<double>::quiet_NaN();
    std::vector<SeedResult> top;
    std::shared_ptr<const MapAnalysis> source, preview;
    std::string error;

    bool Busy() const
    {
        return phase == SearchPhase::Generating || phase == SearchPhase::Preparing ||
               phase == SearchPhase::Searching;
    }
};

inline std::string SeedResultsText(const SeedSearchState &state)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out.precision(17);
    const auto &r = state.request;
    out << "# ItSteals PracticeSuite " << suite_brand::Version << " / " << suite_brand::Author
        << " | SET PRACTICE research | "
        << (r.layoutOnly ? "Spawn/layout scan" : ObjectiveName(r.criteria.objective))
        << "\n# Seed interpretation: " << (r.recipe == 1 ? "spawn seed" : "Practice base seed")
        << " | scene: " << r.scene << " | game hash: " << r.hash << "\n# Search key: " << std::hex << r.key
        << std::dec << " | requested: " << r.count << " | checked: " << state.evaluated
        << " | spawn filtered: " << state.filtered << " | proven outside Top 10: " << state.pruned
        << "\n# Search performance: backend: " << (state.gpu ? "GPU + CPU pickup" : state.portable ? "verified CPU" : "Unity game thread")
        << " | workers: " << r.workers << " | generated: " << state.generated << " | scored: " << state.scored
        << " | cache hits: " << state.cacheHits
        << "\n# Timing: elapsed_s (including preparation): " << state.elapsed
        << " | average_checked_per_s: " << (state.elapsed > 0 ? state.evaluated / state.elapsed : 0)
        << " | preparation_ms: " << state.preparationMs << " | calibration_ms: " << state.calibrationMs
        << "\n# Backend detail: " << state.accelerator << " | " << state.backendNote
        << "\n# Navigation: " << state.navigationModel
        << "\n# Navigation details: " << (state.source ? state.source->data->navigationNote : std::string())
        << "\n# Layouts with center navigation gaps: " << state.estimated
        << " | missing center pairs: " << state.estimatedPairs << "\n# Player spawn filter: "
        << (r.criteria.spawn < 0 ? "any" : std::to_string(r.criteria.spawn + 1))
        << "\n# Static modeled paths only; no proof of fastest real movement or a globally optimal seed.\n"
        << "rank\tseed\tspawn\troute_units\tmethod\tcrystal_visit_order\tdistance_model\n";
    for (size_t i = 0; i < state.top.size(); i++)
    {
        const auto &row = state.top[i];
        out << i + 1 << '\t' << row.seed << '\t' << row.selection.spawn + 1 << '\t';
        if (!r.layoutOnly)
            out << row.route.length;
        out << '\t' << (r.layoutOnly ? "unavailable" : row.route.exact ? "exact order" : "heuristic") << '\t';
        for (size_t j = 0; j < row.route.order.size(); j++)
        {
            if (j)
                out << ',';
            out << row.route.order[j] + 1;
        }
        out << '\t'
            << (r.layoutOnly          ? "unavailable"
                : row.route.estimated ? "estimated (navigation gaps)"
                                      : "navigation")
            << '\n';
    }
    return out.str();
}

inline bool SameLayout(const Selection &a, const Selection &b)
{
    return a.spawn == b.spawn && a.crystals == b.crystals;
}

inline bool BetterSeed(double length, uint64_t index, const SeedResult &other, SearchObjective objective,
                       bool estimated = false)
{
    if (estimated != other.route.estimated)
        return !estimated;
    double a = objective == SearchObjective::LongestRoute ? -length : length;
    double b = objective == SearchObjective::LongestRoute ? -other.route.length : other.route.length;
    return a < b ||
           (a == b && (length < other.route.length || (length == other.route.length && index < other.index)));
}

inline bool WouldRank(const std::vector<SeedResult> &top, const Selection &selection, double length,
                      uint64_t index, SearchObjective objective, bool estimated = false)
{
    for (const auto &row : top)
        if (SameLayout(row.selection, selection))
            return index < row.index;
    return top.size() < 10 || BetterSeed(length, index, top.back(), objective, estimated);
}

inline void RankSeed(std::vector<SeedResult> &top, SeedResult result,
                     SearchObjective objective = SearchObjective::ShortestRoute)
{
    for (auto it = top.begin(); it != top.end(); ++it)
        if (SameLayout(it->selection, result.selection))
        {
            if (it->index <= result.index)
                return;
            top.erase(it);
            break;
        }
    top.push_back(std::move(result));
    std::sort(top.begin(), top.end(), [&](const SeedResult &a, const SeedResult &b) {
        return BetterSeed(a.route.length, a.index, b, objective, a.route.estimated);
    });
    if (top.size() > 10)
        top.resize(10);
}

inline std::shared_ptr<const MapAnalysis> SearchPreview(const MapAnalysis &source, const SeedResult &result)
{
    auto analysis = std::make_shared<MapAnalysis>();
    auto d = std::make_shared<MapData>(*source.data);
    d->seed = result.seed;
    d->spawn = result.selection.spawn;
    d->selected = result.selection.crystals;
    d->flags &= ~8;
    if (source.live && d->spawn == d->liveSpawn)
    {
        auto selected = d->selected, live = d->liveSelected;
        std::sort(selected.begin(), selected.end());
        std::sort(live.begin(), live.end());
        if (selected == live)
            d->flags |= 8;
    }
    analysis->data = d;
    analysis->outline = source.outline;
    analysis->floor = source.floor;
    analysis->route = result.route;
    analysis->live = source.live;
    analysis->session = source.session;
    return analysis;
}

// One coordinator and a configurable, sleeping worker pool. Only eight small
// batches can be queued; acknowledging input pauses the game sampler naturally.
using SeedAcceleration = std::function<bool(SeedSearchState &, const SearchDistances &, const Cancel &,
                                           const std::function<void(SeedSearchState)> &)>;
class SeedSearch
{
    using Clock = std::chrono::steady_clock;

    struct Job
    {
        std::mutex gate;
        std::condition_variable changed;
        SeedSearchState state;
        std::deque<std::shared_ptr<const SeedBatch>> queue;
        std::atomic<bool> canceled{false};
        std::atomic<uint64_t> nextIndex{0};
        bool inputComplete = false, failed = false;
        uint64_t received = 0, token = 0;
        Clock::time_point started = Clock::now();
        std::function<void(uint64_t, const Selection &, const RouteResult &)> audit;
    };

    std::shared_ptr<const SeedSearchState> current = std::make_shared<SeedSearchState>();
    std::mutex gate;
    std::condition_variable ready;
    std::thread coordinator;
    std::atomic<uint64_t> generation{0};
    std::atomic<bool> closing{false};
    std::shared_ptr<Job> active, pending;

    struct Prepared
    {
        std::shared_ptr<const MapData> catalog;
        std::shared_ptr<NavGraph> graph;
        std::shared_ptr<SearchDistances> distances;
    };

    std::deque<Prepared> preparedCatalogs;
    SeedAcceleration accelerate;
    bool gameWasBusy = false, gameStopPending = false;
    uint32_t gameSerial = 0;

    bool Obsolete(uint64_t token) const
    {
        return closing || generation.load() != token;
    }

    void Publish(SeedSearchState state, uint64_t token)
    {
        std::lock_guard<std::mutex> lock(gate);
        if (!Obsolete(token))
            std::atomic_store(&current, std::shared_ptr<const SeedSearchState>(
                                            std::make_shared<SeedSearchState>(std::move(state))));
    }

    SeedSearchState Snapshot(const std::shared_ptr<Job> &job)
    {
        std::lock_guard<std::mutex> lock(job->gate);
        auto state = job->state;
        state.queued = job->queue.size();
        state.elapsed = std::chrono::duration<double>(Clock::now() - job->started).count();
        return state;
    }

    void RouteWorker(const std::shared_ptr<Job> &job, const SearchDistances *distances)
    {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
        auto canceled = [&] { return job->canceled || Obsolete(job->token); };

        struct CachedRoute
        {
            int spawn = -1;
            size_t count = 0;
            std::array<uint16_t, 64> crystals;
            RouteResult score;
            double lower = Unreachable;
            bool boundOnly = false, estimated = false;

            bool Matches(const Selection &x) const
            {
                return spawn == x.spawn && count == x.crystals.size() &&
                       std::equal(x.crystals.begin(), x.crystals.end(), crystals.begin());
            }
        };

        const auto &criteria = job->state.request.criteria;
        const size_t crystals = job->state.source->data->selected.size();
        // Cheap tiny tours/layout scans publish once per bounded packet. Heavy
        // exact tours publish smaller chunks; fast/large heuristics sit between.
        const uint64_t publishChunk = !distances || crystals <= 8                ? SeedBatchLimit
                                      : job->state.request.fast || crystals > 15 ? 256
                                                                                 : 32;
        try
        {
            std::vector<CachedRoute> cache(distances ? 4096 : 0);
            RouteWorkspace workspace;
            int refinementMisses = 0, refinementCooldown = 0;
            std::vector<SeedResult> top;
            top.reserve(11);
            std::unique_ptr<PortableSelectionGenerator> generator;
            std::shared_ptr<SeedBatch> localBuffer;
            RandomSeedOrder order(job->state.request.key);
            if (job->state.portable)
            {
                generator = std::make_unique<PortableSelectionGenerator>(*job->state.source->data);
                localBuffer = std::make_shared<SeedBatch>();
            }
            while (!canceled())
            {
                std::shared_ptr<const SeedBatch> batch;
                double cutoff = Unreachable;
                if (generator)
                {
                    const uint64_t chunk = distances ? 64 : 256;
                    uint64_t begin = job->nextIndex.fetch_add(chunk);
                    if (begin >= job->state.request.count)
                        return;
                    uint64_t end = std::min(begin + chunk, job->state.request.count);
                    auto &local = localBuffer;
                    local->offset = begin;
                    local->layouts.resize(static_cast<size_t>(end - begin));
                    auto start = Clock::now();
                    for (size_t i = 0; i < local->layouts.size(); i++)
                    {
                        if (canceled())
                            return;
                        auto &layout = local->layouts[i];
                        layout.seed = order.SeedAt(static_cast<uint32_t>(begin + i));
                        generator->Generate(layout.seed, job->state.request.recipe, layout.selection);
                    }
                    uint64_t micros = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start).count());
                    {
                        std::lock_guard<std::mutex> lock(job->gate);
                        job->state.generated += end - begin;
                        job->state.generationMicros += micros;
                        job->inputComplete = job->state.generated == job->state.request.count;
                        if (job->state.top.size() == 10 && !job->state.top.back().route.estimated)
                            cutoff = job->state.top.back().route.length;
                    }
                    batch = local;
                }
                else
                {
                    std::unique_lock<std::mutex> lock(job->gate);
                    job->changed.wait(
                        lock, [&] { return canceled() || !job->queue.empty() || job->inputComplete; });
                    if (canceled() || job->queue.empty())
                        return;
                    batch = std::move(job->queue.front());
                    job->queue.pop_front();
                    if (job->state.top.size() == 10 && !job->state.top.back().route.estimated)
                        cutoff = job->state.top.back().route.length;
                }
                top.clear();
                uint64_t done = 0, estimated = 0, filtered = 0, scored = 0, pruned = 0, matched = 0, hits = 0;
                auto flush = [&] {
                    std::lock_guard<std::mutex> lock(job->gate);
                    if (canceled())
                        return;
                    job->state.evaluated += done;
                    job->state.estimated += estimated;
                    job->state.filtered += filtered;
                    job->state.scored += scored;
                    job->state.pruned += pruned;
                    job->state.matched += matched;
                    job->state.cacheHits += hits;
                    for (const auto &row : top)
                        RankSeed(job->state.top, row, criteria.objective);
                    if (job->state.top.size() == 10 && !job->state.top.back().route.estimated)
                        cutoff = std::min(cutoff, job->state.top.back().route.length);
                    done = estimated = filtered = scored = pruned = matched = hits = 0;
                    job->changed.notify_all();
                };
                for (size_t i = 0; i < batch->layouts.size(); i++)
                {
                    if (canceled())
                        return;
                    const auto &layout = batch->layouts[i];
                    const auto &selection = layout.selection;
                    done++;
                    if (criteria.spawn >= 0 && selection.spawn != criteria.spawn)
                        filtered++;
                    else if (!distances)
                    {
                        matched++;
                        uint64_t index = batch->offset + i;
                        if (WouldRank(top, selection, 0, index, criteria.objective))
                        {
                            SeedResult row;
                            row.seed = layout.seed;
                            row.index = index;
                            row.selection = selection;
                            row.route.message = "Verified spawn layout; route length unavailable.";
                            RankSeed(top, std::move(row), criteria.objective);
                        }
                    }
                    else
                    {
                        if (refinementCooldown)
                            refinementCooldown--;
                        uint64_t hash = 14695981039346656037ULL ^ static_cast<uint64_t>(selection.spawn);
                        for (int id : selection.crystals)
                        {
                            hash ^= static_cast<uint64_t>(id) + 1;
                            hash *= 1099511628211ULL;
                        }
                        auto &entry = cache[(hash ^ (hash >> 32)) & (cache.size() - 1)];
                        bool keep = true;
                        double boundCutoff = cutoff;
                        if (top.size() == 10 && !top.back().route.estimated)
                            boundCutoff = std::min(boundCutoff, top.back().route.length);
                        bool cached = entry.Matches(selection);
                        if (cached)
                        {
                            hits++;
                            estimated += entry.estimated;
                        }
                        if (cached && entry.boundOnly && entry.lower > boundCutoff + 1e-6)
                        {
                            pruned++;
                            keep = false;
                        }
                        else if (!cached || entry.boundOnly)
                        {
                            int count = distances->FillBoundMatrix(selection, workspace);
                            double lower = Unreachable;
                            if (!cached)
                                estimated += workspace.estimated;
                            if (criteria.objective == SearchObjective::ShortestRoute &&
                                job->state.request.prune && std::isfinite(boundCutoff))
                            {
                                lower = cached ? entry.lower
                                               : MstRouteLowerBound(workspace.matrix.data(), count, canceled);
                                if (!std::isfinite(lower))
                                    throw std::runtime_error("Invalid prepared route distances");
                                // Spend the bounded refinement budget only on
                                // large tours already close to the cutoff.
                                if (!refinementCooldown && count > 16 && lower <= boundCutoff + 1e-6 &&
                                    lower >= .75 * boundCutoff)
                                {
                                    lower = TightenRouteLowerBound(workspace.matrix.data(), count, lower,
                                                                   boundCutoff, canceled);
                                    if (lower > boundCutoff + 1e-6)
                                        refinementMisses = 0;
                                    else if (++refinementMisses == 16)
                                    {
                                        refinementMisses = 0;
                                        refinementCooldown = 256;
                                    }
                                }
                                if (lower > boundCutoff + 1e-6)
                                {
                                    pruned++;
                                    keep = false;
                                }
                            }
                            if (keep)
                                entry.score = distances->Score(selection, job->state.request.fast, canceled,
                                                               workspace, true);
                            if (keep || std::isfinite(lower))
                            {
                                entry.spawn = selection.spawn;
                                entry.count = selection.crystals.size();
                                entry.lower = lower;
                                entry.boundOnly = !keep;
                                entry.estimated = workspace.estimated;
                                std::copy(selection.crystals.begin(), selection.crystals.end(),
                                          entry.crystals.begin());
                            }
                        }
                        if (keep)
                        {
                            const auto &score = entry.score;
                            scored++;
                            if (!score.complete)
                                throw std::runtime_error("Route solver failed for a finite distance matrix");
                            if (job->audit)
                                job->audit(batch->offset + i, selection, score);
                            {
                                matched++;
                                uint64_t index = batch->offset + i;
                                if (WouldRank(top, selection, score.length, index, criteria.objective,
                                              score.estimated))
                                {
                                    SeedResult row;
                                    row.index = index;
                                    row.seed = layout.seed;
                                    row.selection = selection;
                                    row.route = score;
                                    RankSeed(top, std::move(row), criteria.objective);
                                }
                            }
                        }
                    }
                    // Publish partial counters/results, so a large exact batch
                    // cannot make progress/ETA appear frozen or lose all work.
                    if (done >= publishChunk || i + 1 == batch->layouts.size())
                        flush();
                }
            }
        }
        catch (const std::exception &e)
        {
            if (!Obsolete(job->token))
            {
                std::lock_guard<std::mutex> lock(job->gate);
                job->failed = true;
                job->state.error = e.what();
                job->canceled = true;
            }
            job->changed.notify_all();
        }
    }

    void Evaluate(const std::shared_ptr<Job> &job)
    {
        auto canceled = [&] { return job->canceled || Obsolete(job->token); };
        std::vector<std::thread> workers;
        try
        {
            const auto &data = *job->state.source->data;
            if (data.spawns.empty() || data.spawns.size() > 128 || data.points.size() > 1024 ||
                data.selected.empty() || data.selected.size() > 64)
                throw std::runtime_error("Invalid or empty captured spawn/crystal catalog");
            auto filterError = job->state.request.criteria.CheckCatalog(data, true);
            if (!filterError.empty())
                throw std::runtime_error(filterError);
            if (job->state.portable &&
                (!job->state.request.allowPortable || data.rngStatus != 2 || data.rngProbes != 4160))
                throw std::runtime_error("CPU generator has not been verified for this game catalog");
            std::shared_ptr<NavGraph> graph;
            std::shared_ptr<SearchDistances> distances;
            auto nextProgress = Clock::now();
            auto prepareCancel = [&] {
                if (canceled()) return true;
                if (Clock::now() >= nextProgress)
                {
                    Publish(Snapshot(job), job->token);
                    nextProgress = Clock::now() + std::chrono::milliseconds(200);
                }
                return false;
            };
            if (!job->state.request.layoutOnly)
            {
                auto started = Clock::now();
                bool reused = false;
                for (auto i = preparedCatalogs.begin(); i != preparedCatalogs.end(); ++i)
                    if (SameNavigationCatalog(*i->catalog, data))
                    {
                        auto cached = *i;
                        preparedCatalogs.erase(i);
                        preparedCatalogs.push_front(cached);
                        graph = cached.graph;
                        distances = cached.distances;
                        reused = true;
                        break;
                    }
                if (!reused)
                {
                    graph = std::make_shared<NavGraph>(data, prepareCancel);
                    distances =
                        std::make_shared<SearchDistances>(data, *graph, prepareCancel, [&](int done, int total) {
                            {
                                std::lock_guard<std::mutex> lock(job->gate);
                                job->state.prepared = done;
                                job->state.prepareTotal = total;
                            }
                            if (done == total || Clock::now() >= nextProgress)
                            {
                                Publish(Snapshot(job), job->token);
                                nextProgress = Clock::now() + std::chrono::milliseconds(200);
                            }
                        }, [&](const char *step) {
                            { std::lock_guard lock(job->gate); job->state.preparationStep = step; }
                            Publish(Snapshot(job), job->token);
                        });
                    if (canceled())
                        return;
                    preparedCatalogs.push_front({job->state.source->data, graph, distances});
                    if (preparedCatalogs.size() > 2)
                        preparedCatalogs.pop_back();
                }
                {
                    std::lock_guard<std::mutex> lock(job->gate);
                    job->state.distanceCacheReused = reused;
                    job->state.preparationMs =
                        std::chrono::duration<double, std::milli>(Clock::now() - started).count();
                    if (reused)
                        job->state.prepared = job->state.prepareTotal =
                            int(data.spawns.size() + data.points.size());
                }
            }
            {
                std::lock_guard<std::mutex> lock(job->gate);
                job->state.phase = SearchPhase::Searching;
                if (distances)
                {
                    job->state.projectedSpawns = distances->ProjectedSpawns();
                    job->state.projectedPoints = distances->ProjectedPoints();
                    job->state.estimatedPairs = distances->EstimatedPairs();
                    job->state.recoveredPairs = distances->RecoveredPairs();
                    job->state.repairQueries = distances->RepairQueries();
                    job->state.navigationModel = distances->ModelName();
                }
            }
            std::map<uint64_t, RouteResult> drawn;
            std::shared_ptr<const MapAnalysis> preview;
            uint64_t previewIndex = SeedSpace;
            double previousElapsed = 0;
            uint64_t previousGenerated = 0, previousChecked = 0;
            SearchTiming timing;
            bool wasGpu = false;
            auto present = [&](SeedSearchState state) {
                if (canceled()) return;
                if (state.gpu != wasGpu || state.evaluated < previousChecked)
                {
                    drawn.clear(); preview.reset(); previewIndex = SeedSpace;
                    previousElapsed = 0; previousGenerated = previousChecked = 0; timing = {};
                }
                wasGpu = state.gpu;
                state.elapsed = std::chrono::duration<double>(Clock::now() - job->started).count();
                double interval = state.elapsed - previousElapsed;
                if (interval > 0)
                {
                    state.recentGenerated = (state.generated - previousGenerated) / interval;
                    state.recentChecked = (state.evaluated - previousChecked) / interval;
                }
                state.remainingSeconds =
                    timing.Remaining(state.evaluated, state.request.count, state.elapsed);
                previousElapsed = state.elapsed;
                previousGenerated = state.generated;
                previousChecked = state.evaluated;
                std::map<uint64_t, RouteResult> nextDrawn;
                for (auto &result : state.top)
                {
                    auto found = drawn.find(result.index);
                    if (found != drawn.end())
                        result.route = found->second;
                    else if (distances)
                        result.route = distances->Draw(result.selection, std::move(result.route), canceled);
                    nextDrawn.emplace(result.index, result.route);
                }
                drawn = std::move(nextDrawn);
                if (!state.top.empty() && state.top.front().index != previewIndex)
                {
                    preview = SearchPreview(*state.source, state.top.front());
                    previewIndex = state.top.front().index;
                }
                state.preview = preview;
                Publish(std::move(state), job->token);
            };
            if (accelerate && job->state.portable && distances)
            {
                auto accelerated = Snapshot(job);
                bool handled = accelerate(accelerated, *distances, canceled, present);
                if (canceled()) return;
                { std::lock_guard lock(job->gate); job->state = std::move(accelerated); }
                if (handled) { present(Snapshot(job)); return; }
            }
            workers.reserve(job->state.request.workers);
            for (int i = 0; i < job->state.request.workers && !canceled(); i++)
                workers.emplace_back([this, job, distances, graph] { RouteWorker(job, distances.get()); });
            while (!canceled())
            {
                auto state = Snapshot(job);
                bool finished = false;
                {
                    std::lock_guard<std::mutex> lock(job->gate);
                    finished = job->inputComplete && state.evaluated == state.request.count;
                }
                if (finished)
                    state.phase = SearchPhase::Completed;
                present(std::move(state));
                if (finished)
                    break;
                std::unique_lock<std::mutex> lock(job->gate);
                job->changed.wait_for(lock, std::chrono::milliseconds(200), [&] {
                    return canceled() ||
                           (job->inputComplete && job->state.evaluated == job->state.request.count);
                });
            }
            job->canceled = true;
            job->changed.notify_all();
            for (auto &worker : workers)
                if (worker.joinable())
                    worker.join();
            workers.clear();
            if (job->failed && !Obsolete(job->token))
            {
                auto state = Snapshot(job);
                state.phase = SearchPhase::Failed;
                Publish(std::move(state), job->token);
            }
        }
        catch (const std::exception &e)
        {
            job->canceled = true;
            job->changed.notify_all();
            for (auto &worker : workers)
                if (worker.joinable())
                    worker.join();
            if (!Obsolete(job->token))
            {
                auto state = Snapshot(job);
                state.phase = SearchPhase::Failed;
                state.error = e.what();
                Publish(std::move(state), job->token);
            }
        }
    }

    void Work()
    {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
        while (!closing)
        {
            std::shared_ptr<Job> job;
            {
                std::unique_lock<std::mutex> lock(gate);
                ready.wait(lock, [&] { return closing || pending; });
                if (closing)
                    return;
                job = std::move(pending);
            }
            Evaluate(job);
        }
    }

    void Queue(std::shared_ptr<const MapAnalysis> source)
    {
        std::lock_guard<std::mutex> lock(gate);
        auto state = *Get();
        if (state.phase != SearchPhase::Generating)
            return;
        auto job = std::make_shared<Job>();
        job->state = std::move(state);
        job->state.phase = SearchPhase::Preparing;
        job->state.portable = source->data->searchBackend == 1;
        job->state.source = std::move(source);
        job->token = generation;
        try
        {
            if (!coordinator.joinable())
                coordinator = std::thread([this] { Work(); });
        }
        catch (const std::exception &e)
        {
            job->state.phase = SearchPhase::Failed;
            job->state.error = e.what();
            std::atomic_store(&current, std::shared_ptr<const SeedSearchState>(
                                            std::make_shared<SeedSearchState>(job->state)));
            return;
        }
        active = pending = job;
        std::atomic_store(
            &current, std::shared_ptr<const SeedSearchState>(std::make_shared<SeedSearchState>(job->state)));
        ready.notify_one();
    }

  public:
    // Run the same production workers against prepared immutable distances.
    // Used for backend calibration and independent scorer verification.
    static SeedSearchState RunFrozen(std::shared_ptr<const MapAnalysis> source,
        const SearchDistances &distances, SeedSearchRequest request, const Cancel &canceled,
        const std::function<void(uint64_t)> &progress = {},
        const std::function<void(uint64_t, const Selection &, const RouteResult &)> &audit = {})
    {
        SeedSearch engine;
        auto job = std::make_shared<Job>();
        job->state.request = request;
        job->state.source = std::move(source);
        job->state.portable = true;
        job->state.phase = SearchPhase::Searching;
        job->state.navigationModel = distances.ModelName();
        job->audit = audit;
        std::vector<std::thread> workers;
        std::atomic<int> done{0};
        try
        {
            if (!job->state.source || !job->state.source->data || !request.count ||
                request.count > SeedSpace || request.workers < 1 || request.workers > 32 || !request.criteria.Valid())
                throw std::runtime_error("Invalid frozen search request");
            if (!job->state.source->data->Verified() || job->state.source->data->rngStatus != 2 ||
                job->state.source->data->rngProbes != 4160) throw std::runtime_error("Frozen search RNG is not verified");
            auto error = request.criteria.CheckCatalog(*job->state.source->data, true);
            if (!error.empty()) throw std::runtime_error(error);
            for (int i = 0; i < request.workers; i++)
                workers.emplace_back([&] {
                    engine.RouteWorker(job, &distances);
                    done.fetch_add(1);
                    job->changed.notify_all();
                });
            while (done.load() < request.workers)
            {
                if (canceled()) job->canceled = true;
                if (progress) progress(engine.Snapshot(job).evaluated);
                std::unique_lock lock(job->gate);
                job->changed.wait_for(lock, std::chrono::milliseconds(20), [&] {
                    return done.load() == request.workers;
                });
            }
        }
        catch (const std::exception &e)
        {
            job->canceled = true;
            job->changed.notify_all();
            std::lock_guard lock(job->gate);
            job->failed = true;
            job->state.error = e.what();
        }
        for (auto &worker : workers) if (worker.joinable()) worker.join();
        auto result = engine.Snapshot(job);
        result.phase = job->failed ? SearchPhase::Failed : canceled() ? SearchPhase::Stopped
            : result.evaluated == request.count ? SearchPhase::Completed : SearchPhase::Failed;
        if (progress) progress(result.evaluated);
        return result;
    }
    explicit SeedSearch(SeedAcceleration runner = {}) : accelerate(std::move(runner)) {}
    SeedSearch(const SeedSearch &) = delete;

    ~SeedSearch()
    {
        Stop();
        closing = true;
        ready.notify_all();
        if (coordinator.joinable())
            coordinator.join();
    }

    std::shared_ptr<const SeedSearchState> Get() const
    {
        return std::atomic_load(&current);
    }

    bool Busy() const
    {
        return Get()->Busy();
    }

    // Called by the command thread: retain a stop until the preceding game
    // command is acknowledged, and consume it exactly once.
    bool TakeGameStop(bool commandPending)
    {
        auto state = Get();
        bool busy = state->Busy();
        if (gameWasBusy && !busy && (state->phase != SearchPhase::Completed || state->portable))
            gameStopPending = true;
        if (busy && state->request.serial != gameSerial)
            gameStopPending = false;
        gameWasBusy = busy;
        gameSerial = state->request.serial;
        if (commandPending || !gameStopPending) return false;
        gameStopPending = false;
        return true;
    }

    void Begin(SeedSearchRequest request)
    {
        Stop();
        std::lock_guard<std::mutex> lock(gate);
        ++generation;
        request.workers = std::clamp(request.workers, 1, SeedWorkerLimit());
        auto state = std::make_shared<SeedSearchState>();
        state->request = std::move(request);
        state->phase = SearchPhase::Generating;
        if (!state->request.count || state->request.count > SeedSpace)
        {
            state->phase = SearchPhase::Failed;
            state->error = "Choose between 1 and 4294967296 distinct 32-bit seeds";
        }
        else if (!state->request.criteria.Valid() || int(state->request.backend) < 0 || int(state->request.backend) > 2)
        {
            state->phase = SearchPhase::Failed;
            state->error = "Invalid search criteria; check the goal and player spawn.";
        }
        std::atomic_store(&current, std::shared_ptr<const SeedSearchState>(state));
    }

    void Update(std::shared_ptr<const MapAnalysis> source)
    {
        auto state = Get();
        if (!state->Busy() || !source || !source->data)
            return;
        const auto &d = *source->data;
        const auto &r = state->request;
        if (!source->live || source->session != r.session || d.pid != r.pid || d.scene != r.scene ||
            d.epoch != r.epoch || d.hash != r.hash)
        {
            Stop("Map context changed; search stopped.");
            return;
        }
        if (d.requestSerial != r.serial)
            return;
        if (!d.Verified())
        {
            Stop("Seed verification was lost; search stopped.");
            return;
        }
        if (d.stage == 3)
        {
            Stop(d.message);
            return;
        }
        if (d.seed != r.seed || d.recipe != r.recipe)
        {
            Stop("Seed generation does not match this search.");
            return;
        }
        if ((d.flags & 128) && !r.layoutOnly)
        {
            if (state->phase == SearchPhase::Generating)
            {
                auto waiting = *state;
                waiting.source = source;
                waiting.prepared = d.navigationQueries;
                waiting.prepareTotal = d.navigationTotal;
                Publish(std::move(waiting), generation.load());
            }
            return;
        }
        if (d.stage == 1)
        {
            bool ended = false;
            {
                std::lock_guard<std::mutex> lock(gate);
                if (active)
                {
                    std::lock_guard<std::mutex> jobLock(active->gate);
                    ended = active->inputComplete;
                }
            }
            if (!ended)
                Stop("Seed generation stopped before the requested number of seeds was complete.");
            return;
        }
        if (state->phase == SearchPhase::Generating && d.stage == 2)
            Queue(std::move(source));
    }

    // False means backpressure: leave the shared packet unacknowledged and retry.
    // Stale packets are acknowledged and ignored, never applied to a new search.
    bool Submit(std::shared_ptr<const SeedBatch> batch, uint32_t session)
    {
        if (!batch)
            return true;
        auto state = Get();
        if (!state->Busy() || batch->serial != state->request.serial)
            return true;
        const auto &r = state->request;
        if (session != r.session || batch->pid != r.pid || batch->scene != r.scene ||
            batch->epoch != r.epoch || batch->hash != r.hash || batch->seed != r.seed ||
            batch->recipe != r.recipe || batch->total != r.count || batch->key != r.key)
        {
            Stop("Seed batch context mismatch; search stopped.");
            return true;
        }
        std::shared_ptr<Job> job;
        {
            std::lock_guard<std::mutex> lock(gate);
            job = active;
        }
        if (!job)
            return false;
        bool invalid = false;
        {
            std::lock_guard<std::mutex> lock(job->gate);
            if (job->canceled || Obsolete(job->token))
                return true;
            const auto &data = *job->state.source->data;
            if (job->state.portable)
                invalid = true;
            if (batch->offset < job->received && batch->offset + batch->layouts.size() <= job->received)
                return true;
            if (batch->offset != job->received)
                invalid = true;
            if (batch->status < 0 || batch->status > 1 || batch->layouts.size() > SeedBatchLimit ||
                batch->offset > r.count || batch->layouts.size() > r.count - batch->offset ||
                (batch->status == 0 && batch->layouts.empty()) ||
                (batch->status == 1 && batch->offset + batch->layouts.size() != r.count))
                invalid = true;
            RandomSeedOrder order(r.key);
            std::array<uint16_t, 1024> seen{};
            if (!invalid)
                for (size_t i = 0; i < batch->layouts.size(); i++)
                {
                    const auto &layout = batch->layouts[i];
                    const auto &selection = layout.selection;
                    if (layout.seed != order.SeedAt(static_cast<uint32_t>(batch->offset + i)) ||
                        selection.spawn < 0 || static_cast<size_t>(selection.spawn) >= data.spawns.size() ||
                        selection.crystals.size() != data.selected.size())
                    {
                        invalid = true;
                        break;
                    }
                    // One scratch table per packet, no visited-array allocation for
                    // each seed. Packet row stamps cannot wrap at the 2048 limit.
                    uint16_t stamp = static_cast<uint16_t>(i + 1);
                    for (int id : selection.crystals)
                    {
                        if (id < 0 || static_cast<size_t>(id) >= data.points.size() ||
                            static_cast<size_t>(id) >= seen.size() || seen[id] == stamp)
                        {
                            invalid = true;
                            break;
                        }
                        seen[id] = stamp;
                    }
                    if (invalid)
                        break;
                }
            if (!invalid)
            {
                if (job->queue.size() >= SeedQueueLimit)
                    return false;
                job->received += batch->layouts.size();
                job->state.generated = job->received;
                job->state.generationMicros = batch->generationMicros;
                job->state.generationFrames = batch->generationFrames;
                job->inputComplete = batch->status == 1;
                job->queue.push_back(std::move(batch));
                job->changed.notify_all();
                return true;
            }
        }
        Stop("Missing or invalid seed batch; partial results retained.");
        return true;
    }

    void Stop(std::string reason = {})
    {
        std::lock_guard<std::mutex> lock(gate);
        ++generation;
        if (active)
        {
            active->canceled = true;
            active->changed.notify_all();
        }
        active.reset();
        pending.reset();
        auto state = std::make_shared<SeedSearchState>(*Get());
        if (state->Busy())
            state->phase = SearchPhase::Stopped;
        if (!reason.empty())
            state->error = std::move(reason);
        std::atomic_store(&current, std::shared_ptr<const SeedSearchState>(state));
    }

    std::shared_ptr<const MapAnalysis> pinnedPreview;

    std::shared_ptr<const MapAnalysis> Pinned() const
    {
        return std::atomic_load(&pinnedPreview);
    }

    void Pin(const MapAnalysis &source, const SeedResult &row)
    {
        auto preview = SearchPreview(source, row);
        std::atomic_store(&pinnedPreview, std::shared_ptr<const MapAnalysis>(preview));
    }

    void HidePreview()
    {
        std::atomic_store(&pinnedPreview, std::shared_ptr<const MapAnalysis>{});
        std::lock_guard<std::mutex> lock(gate);
        auto state = std::make_shared<SeedSearchState>(*Get());
        state->preview.reset();
        std::atomic_store(&current, std::shared_ptr<const SeedSearchState>(state));
    }

    void Select(int rank)
    {
        Stop();
        std::lock_guard<std::mutex> lock(gate);
        auto state = std::make_shared<SeedSearchState>(*Get());
        if (rank < 0 || rank >= static_cast<int>(state->top.size()) || !state->source)
            return;
        state->preview = SearchPreview(*state->source, state->top[rank]);
        std::atomic_store(&current, std::shared_ptr<const SeedSearchState>(state));
    }
};
} // namespace practice
