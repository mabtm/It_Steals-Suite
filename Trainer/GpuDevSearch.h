#pragma once
#include "GpuDevReport.h"
#include <span>

namespace gpu_dev
{
using SearchAudit = std::function<void(uint64_t, const practice::Selection &, const practice::RouteResult &)>;
using BatchScorer = std::function<std::vector<practice::RouteResult>(std::span<const practice::SeedLayout>)>;

enum class BoundsMode { Enabled, Disabled, Adaptive };
inline const char *BoundsName(BoundsMode mode)
{
    return mode == BoundsMode::Enabled ? "on" : mode == BoundsMode::Disabled ? "off" : "adaptive";
}
struct BoundsStats { uint64_t checked = 0, skipped = 0; };
struct BoundsBudget
{
    BoundsMode mode;
    int tested = 0, rejected = 0, cooldown = 0;
    bool Use()
    {
        if (mode == BoundsMode::Disabled) return false;
        if (mode == BoundsMode::Adaptive && cooldown) { cooldown--; return false; }
        return true;
    }
    void Result(bool reject)
    {
        if (mode != BoundsMode::Adaptive) return;
        rejected += reject;
        if (++tested == 64)
        {
            if (rejected < 2) cooldown = 512;
            tested = rejected = 0;
        }
    }
};

inline bool SameTop(const std::vector<practice::SeedResult> &a, const std::vector<practice::SeedResult> &b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (a[i].index != b[i].index || a[i].seed != b[i].seed ||
            !practice::SameLayout(a[i].selection, b[i].selection) || !SameScore(a[i].route, b[i].route)) return false;
    return true;
}

// One bounded packet per CPU worker. The caller owns the GPU context; workers
// generate/filter/bound candidates and finish/rank returned routes in parallel.
inline practice::SeedSearchState SearchGpu(std::shared_ptr<const practice::MapAnalysis> source,
    const practice::SearchDistances &distances, practice::SeedSearchRequest request, int batch,
    const BatchScorer &score, const practice::Cancel &canceled,
    const std::function<void(uint64_t)> &progress = {}, const SearchAudit &audit = {},
    BoundsMode bounds = BoundsMode::Enabled, BoundsStats *boundsStats = nullptr)
{
    using namespace practice;
    using Timer = std::chrono::steady_clock;
    struct Packet { std::vector<SeedLayout> layouts; std::vector<uint64_t> indices; std::vector<RouteResult> routes; bool ready = false; };
    SeedSearchState state;
    if (boundsStats) *boundsStats = {};
    state.source = std::move(source); state.request = request; state.portable = true;
    state.phase = SearchPhase::Searching; state.navigationModel = distances.ModelName();
    std::mutex gate;
    std::condition_variable changed;
    std::deque<std::shared_ptr<Packet>> queue;
    std::atomic<uint64_t> next{0};
    std::atomic<int> finished{0};
    std::atomic<bool> failed{false};
    auto start = Timer::now();
    std::vector<std::thread> workers;
    auto stop = [&] { return failed.load() || canceled(); };
    auto fail = [&](const std::string &error) {
        { std::lock_guard lock(gate); if (state.error.empty()) state.error = error; }
        failed = true; changed.notify_all();
    };
    auto finishPacket = [&](const std::shared_ptr<Packet> &packet, uint64_t begin, uint64_t end,
                    uint64_t filtered, uint64_t pruned, BoundsStats measured = {}) {
        std::vector<SeedResult> localTop;
        for (size_t i = 0; i < packet->layouts.size(); i++)
        {
            if (stop()) break;
            auto &layout = packet->layouts[i]; auto route = std::move(packet->routes[i]);
            if (!route.complete || !ValidOrder(route.order, layout.selection.crystals.size()))
                throw std::runtime_error("GPU returned an incomplete route");
            double center = 0; int previous = layout.selection.spawn;
            for (int id : route.order)
            {
                int node = distances.Node(layout.selection, id + 1);
                center += distances.CenterDistance(previous, node); previous = node;
            }
            if (center != route.length) throw std::runtime_error("GPU center-length mismatch at seed " + std::to_string(layout.seed));
            route = distances.FinishOrder(layout.selection, std::move(route), stop);
            if (audit) audit(packet->indices[i], layout.selection, route);
            if (WouldRank(localTop, layout.selection, route.length, packet->indices[i], request.criteria.objective, route.estimated))
                RankSeed(localTop, {layout.seed, packet->indices[i], layout.selection, std::move(route)}, request.criteria.objective);
        }
        if (stop()) return;
        {
            std::lock_guard lock(gate);
            state.evaluated += end - begin; state.filtered += filtered; state.pruned += pruned;
            state.scored += packet->layouts.size(); state.matched += packet->layouts.size();
            if (boundsStats) { boundsStats->checked += measured.checked; boundsStats->skipped += measured.skipped; }
            for (auto &row : localTop) RankSeed(state.top, std::move(row), request.criteria.objective);
        }
    };
    auto worker = [&] {
        try
        {
            PortableSelectionGenerator generator(*state.source->data);
            RandomSeedOrder order(request.key);
            RouteWorkspace workspace;
            int misses = 0, cooldown = 0;
            BoundsBudget budget{bounds};
            while (!stop())
            {
                uint64_t begin = next.fetch_add(uint64_t(batch));
                if (begin >= request.count) break;
                uint64_t end = std::min(begin + uint64_t(batch), request.count), filtered = 0, pruned = 0;
                auto packet = std::make_shared<Packet>();
                BoundsStats measured;
                double cutoff = Unreachable;
                {
                    std::lock_guard lock(gate);
                    if (state.top.size() == 10 && !state.top.back().route.estimated) cutoff = state.top.back().route.length;
                }
                for (uint64_t index = begin; index < end; index++)
                {
                    if (stop()) break;
                    SeedLayout layout; layout.seed = order.SeedAt(uint32_t(index));
                    generator.Generate(layout.seed, request.recipe, layout.selection);
                    if (request.criteria.spawn >= 0 && layout.selection.spawn != request.criteria.spawn) { filtered++; continue; }
                    if (cooldown) cooldown--;
                    bool reject = false;
                    if (!request.exhaustive && request.criteria.objective == SearchObjective::ShortestRoute && std::isfinite(cutoff))
                    {
                        if (!budget.Use()) measured.skipped++;
                        else
                        {
                            measured.checked++;
                            int count = distances.FillBoundMatrix(layout.selection, workspace);
                            double lower = MstRouteLowerBound(workspace.matrix.data(), count, stop);
                            if (!std::isfinite(lower)) throw std::runtime_error("Invalid prepared route distances");
                            if (!cooldown && count > 16 && lower <= cutoff + 1e-6 && lower >= .75 * cutoff)
                            {
                                lower = TightenRouteLowerBound(workspace.matrix.data(), count, lower, cutoff, stop);
                                if (lower > cutoff + 1e-6) misses = 0;
                                else if (++misses == 16) { misses = 0; cooldown = 256; }
                            }
                            reject = lower > cutoff + 1e-6;
                            budget.Result(reject);
                        }
                    }
                    if (reject) { pruned++; continue; }
                    packet->indices.push_back(index); packet->layouts.push_back(std::move(layout));
                }
                if (stop()) break;
                {
                    std::unique_lock lock(gate);
                    state.generated += end - begin;
                    if (!packet->layouts.empty())
                    {
                        queue.push_back(packet); changed.notify_all();
                        changed.wait(lock, [&] { return packet->ready || stop(); });
                        if (stop()) break;
                    }
                }
                finishPacket(packet, begin, end, filtered, pruned, measured);
                changed.notify_all();
            }
        }
        catch (const std::exception &e) { if (!canceled()) fail(e.what()); }
        finished.fetch_add(1); changed.notify_all();
    };
    try
    {
        if (!state.source || !state.source->data || batch < 1 || batch > 8192 || request.workers < 1 || request.workers > 6 ||
            !request.count || !request.criteria.Valid() || int(bounds) < 0 || int(bounds) > 2)
            throw std::runtime_error("Invalid GPU search request");
        auto error = request.criteria.CheckCatalog(*state.source->data, true);
        if (!error.empty()) throw std::runtime_error(error);
        // Establish a real cutoff from the first 64 candidates before large
        // parallel packets; the exhaustive reference is never used for pruning.
        auto first = std::make_shared<Packet>();
        PortableSelectionGenerator generator(*state.source->data);
        RandomSeedOrder order(request.key);
        uint64_t bootstrap = std::min(uint64_t{64}, request.count), filtered = 0;
        for (uint64_t i = 0; i < bootstrap; i++)
        {
            if (stop()) break;
            SeedLayout row; row.seed = order.SeedAt(uint32_t(i));
            generator.Generate(row.seed, request.recipe, row.selection);
            if (request.criteria.spawn >= 0 && row.selection.spawn != request.criteria.spawn) { filtered++; continue; }
            first->layouts.push_back(std::move(row)); first->indices.push_back(i);
        }
        if (!stop())
        {
            if (!first->layouts.empty()) first->routes = score(first->layouts);
            if (first->routes.size() != first->layouts.size()) throw std::runtime_error("GPU bootstrap result count mismatch");
            state.generated = bootstrap;
            finishPacket(first, 0, bootstrap, filtered, 0);
        }
        next.store(bootstrap);
        for (int i = 0; i < request.workers && !stop(); i++) workers.emplace_back(worker);
        while (!stop())
        {
            std::shared_ptr<Packet> packet;
            uint64_t evaluated;
            {
                std::unique_lock lock(gate);
                changed.wait_for(lock, std::chrono::milliseconds(20), [&] { return stop() || !queue.empty() || finished.load() == request.workers; });
                evaluated = state.evaluated;
                if (!queue.empty()) { packet = queue.front(); queue.pop_front(); }
                else if (finished.load() == request.workers) break;
            }
            if (progress) progress(evaluated);
            if (stop()) break;
            if (!packet) continue;
            auto routes = score(packet->layouts);
            if (routes.size() != packet->layouts.size()) throw std::runtime_error("GPU packet result count mismatch");
            { std::lock_guard lock(gate); packet->routes = std::move(routes); packet->ready = true; }
            changed.notify_all();
        }
    }
    catch (const std::exception &e) { if (!canceled()) fail(e.what()); }
    if (canceled()) changed.notify_all();
    for (auto &thread : workers) if (thread.joinable()) thread.join();
    state.elapsed = std::chrono::duration<double>(Timer::now() - start).count();
    state.phase = failed ? SearchPhase::Failed : canceled() ? SearchPhase::Stopped
        : state.evaluated == request.count ? SearchPhase::Completed : SearchPhase::Failed;
    if (progress) progress(state.evaluated);
    return state;
}
} // namespace gpu_dev
