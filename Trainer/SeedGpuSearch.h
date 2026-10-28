#pragma once
#include "SeedGpuCatalog.h"

namespace seed_gpu
{
using BatchScorer = std::function<std::vector<practice::RouteResult>(std::span<const practice::SeedLayout>)>;
using Progress = std::function<void(practice::SeedSearchState)>;
using Audit = std::function<void(uint64_t, const practice::Selection &, const practice::RouteResult &)>;

// One bounded packet per helper. The caller owns the GPU context; helpers
// generate/filter layouts and finish pickup scoring while the next batch runs.
inline practice::SeedSearchState Search(std::shared_ptr<const practice::MapAnalysis> source,
    const practice::SearchDistances &distances, practice::SeedSearchRequest request, int batch,
    const BatchScorer &score, const practice::Cancel &canceled, const Progress &progress = {}, const Audit &audit = {})
{
    using namespace practice;
    using Clock = std::chrono::steady_clock;
    struct Packet { std::vector<SeedLayout> layouts; std::vector<uint64_t> indices; std::vector<RouteResult> routes; bool ready = false; };
    SeedSearchState state; state.source = std::move(source); state.request = request;
    state.portable = state.gpu = true; state.phase = SearchPhase::Searching; state.navigationModel = distances.ModelName();
    std::mutex gate; std::condition_variable changed;
    std::deque<std::shared_ptr<Packet>> queue;
    std::atomic<uint64_t> next{0}; std::atomic<int> finished{0}; std::atomic<bool> failed{false};
    auto start = Clock::now(); std::vector<std::thread> workers;
    auto stop = [&] { return failed.load() || canceled(); };
    auto fail = [&](const std::string &error) {
        { std::lock_guard lock(gate); if (state.error.empty()) state.error = error; }
        failed = true; changed.notify_all();
    };
    auto publish = [&] {
        if (!progress || stop()) return;
        SeedSearchState snapshot;
        { std::lock_guard lock(gate); snapshot = state; snapshot.queued = queue.size(); }
        snapshot.elapsed = std::chrono::duration<double>(Clock::now() - start).count(); progress(std::move(snapshot));
    };
    auto finishPacket = [&](const std::shared_ptr<Packet> &packet, uint64_t begin, uint64_t end, uint64_t filtered) {
        std::vector<SeedResult> localTop; uint64_t estimated = 0;
        for (size_t i = 0; i < packet->layouts.size(); i++)
        {
            if (stop()) break;
            auto &layout = packet->layouts[i]; auto route = std::move(packet->routes[i]);
            if (!route.complete || !ValidOrder(route.order, layout.selection.crystals.size())) throw std::runtime_error("GPU returned an incomplete seed route");
            double center = 0; int previous = layout.selection.spawn;
            for (int id : route.order)
            {
                int node = distances.Node(layout.selection, id + 1);
                center += distances.CenterDistance(previous, node); previous = node;
            }
            if (center != route.length) throw std::runtime_error("GPU seed center-length mismatch");
            route = distances.FinishOrder(layout.selection, std::move(route), stop); estimated += route.estimated;
            if (audit) audit(packet->indices[i], layout.selection, route);
            if (WouldRank(localTop, layout.selection, route.length, packet->indices[i], request.criteria.objective, route.estimated))
                RankSeed(localTop, {layout.seed, packet->indices[i], layout.selection, std::move(route)}, request.criteria.objective);
        }
        if (stop()) return;
        { std::lock_guard lock(gate);
            state.evaluated += end - begin; state.filtered += filtered; state.estimated += estimated;
            state.scored += packet->layouts.size(); state.matched += packet->layouts.size();
            for (auto &row : localTop) RankSeed(state.top, std::move(row), request.criteria.objective);
        }
    };
    auto worker = [&] {
        try
        {
            PortableSelectionGenerator generator(*state.source->data); RandomSeedOrder order(request.key);
            while (!stop())
            {
                uint64_t begin = next.fetch_add(uint64_t(batch)); if (begin >= request.count) break;
                uint64_t end = std::min(begin + uint64_t(batch), request.count), filtered = 0;
                auto packet = std::make_shared<Packet>();
                for (uint64_t index = begin; index < end; index++)
                {
                    if (stop()) break;
                    SeedLayout layout; layout.seed = order.SeedAt(uint32_t(index));
                    generator.Generate(layout.seed, request.recipe, layout.selection);
                    if (request.criteria.spawn >= 0 && layout.selection.spawn != request.criteria.spawn) { filtered++; continue; }
                    packet->indices.push_back(index); packet->layouts.push_back(std::move(layout));
                }
                if (stop()) break;
                { std::unique_lock lock(gate);
                    state.generated += end - begin;
                    if (!packet->layouts.empty())
                    {
                        queue.push_back(packet); changed.notify_all();
                        changed.wait(lock, [&] { return packet->ready || stop(); }); if (stop()) break;
                    }
                }
                finishPacket(packet, begin, end, filtered); changed.notify_all();
            }
        }
        catch (const std::exception &error) { if (!canceled()) fail(error.what()); }
        finished.fetch_add(1); changed.notify_all();
    };
    try
    {
        if (!state.source || !state.source->data || batch < 1 || batch > 8192 || request.workers < 1 || request.workers > 6 ||
            !request.count || request.count > SeedSpace || !request.criteria.Valid()) throw std::runtime_error("Invalid GPU seed search request");
        auto error = request.criteria.CheckCatalog(*state.source->data, true); if (!error.empty()) throw std::runtime_error(error);
        auto first = std::make_shared<Packet>(); PortableSelectionGenerator generator(*state.source->data); RandomSeedOrder order(request.key);
        uint64_t bootstrap = std::min(uint64_t{64}, request.count), filtered = 0;
        for (uint64_t i = 0; i < bootstrap; i++)
        {
            if (stop()) break;
            SeedLayout row; row.seed = order.SeedAt(uint32_t(i)); generator.Generate(row.seed, request.recipe, row.selection);
            if (request.criteria.spawn >= 0 && row.selection.spawn != request.criteria.spawn) { filtered++; continue; }
            first->layouts.push_back(std::move(row)); first->indices.push_back(i);
        }
        if (!stop())
        {
            if (!first->layouts.empty()) first->routes = score(first->layouts);
            if (first->routes.size() != first->layouts.size()) throw std::runtime_error("GPU seed bootstrap count mismatch");
            state.generated = bootstrap; finishPacket(first, 0, bootstrap, filtered); publish();
        }
        next.store(bootstrap);
        for (int i = 0; i < request.workers && !stop(); i++) workers.emplace_back(worker);
        while (!stop())
        {
            std::shared_ptr<Packet> packet;
            { std::unique_lock lock(gate);
                changed.wait_for(lock, std::chrono::milliseconds(20), [&] { return stop() || !queue.empty() || finished.load() == request.workers; });
                if (!queue.empty()) { packet = queue.front(); queue.pop_front(); }
                else if (finished.load() == request.workers) break;
            }
            publish(); if (stop()) break; if (!packet) continue;
            auto routes = score(packet->layouts);
            if (routes.size() != packet->layouts.size()) throw std::runtime_error("GPU seed packet count mismatch");
            { std::lock_guard lock(gate); packet->routes = std::move(routes); packet->ready = true; }
            changed.notify_all();
        }
    }
    catch (const std::exception &error) { if (!canceled()) fail(error.what()); }
    changed.notify_all();
    for (auto &thread : workers) if (thread.joinable()) thread.join();
    state.elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    state.phase = failed ? SearchPhase::Failed : canceled() ? SearchPhase::Stopped : state.evaluated == request.count ? SearchPhase::Completed : SearchPhase::Failed;
    return state;
}
} // namespace seed_gpu
