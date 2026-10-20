#include "seed_batch_fixture.h"
#include <cassert>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <limits>
#include <random>

int main(int argc, char **argv)
{
    assert(PlannerSeedFromBits(0xffffffffu) == -1 &&
           PlannerSeedFromBits(0x80000000u) == std::numeric_limits<int>::min());
    std::string error;
    int randomSeed = 0;
    uint64_t randomKey = 0;
    for (int i = 0; i < 64; i++)
        assert(GeneratePlannerSeed(randomSeed, error) && GenerateSearchKey(randomKey, error) &&
               error.empty());
    // Independent Python vectors and inverse-round checks cover both halves,
    // signed boundaries and the final slot of the full seed space.
    constexpr std::array<uint32_t, 8> slots = {0u,     1u,          2u,          65535u,
                                               65536u, 0x7fffffffu, 0x80000000u, 0xffffffffu};
    constexpr std::array<int, 8> expectedSeeds = {-1979949308, -2081764548, -2127917975, 1038828463,
                                                  -1174749596, 904229284,   -947813596,  2114310595};
    practice::RandomSeedOrder order(FixtureSearchKey);
    for (size_t i = 0; i < slots.size(); i++)
        assert(order.SeedAt(slots[i]) == expectedSeeds[i]);
    auto mix = [](uint32_t v) {
        v = (v ^ (v >> 16)) * 0x7feb352du;
        v = (v ^ (v >> 15)) * 0x846ca68bu;
        return v ^ (v >> 16);
    };
    std::array<uint32_t, 8> inverseKeys{};
    uint32_t lo = uint32_t(FixtureSearchKey), hi = uint32_t(FixtureSearchKey >> 32);
    for (int i = 0; i < 8; i++)
    {
        unsigned shift = 7 + i;
        inverseKeys[i] = mix(lo ^ ((hi << shift) | (hi >> (32 - shift))) ^ (0x9e3779b9u * uint32_t(i + 1)));
    }
    std::vector<uint32_t> generated;
    generated.reserve(1000000);
    std::array<int, 16> bins{};
    for (uint32_t i = 0; i < 1000000; i++)
    {
        uint32_t bits = order.BitsAt(i), left = bits >> 16, right = bits & 65535;
        generated.push_back(bits);
        bins[bits >> 28]++;
        for (int round = 7; round >= 0; round--)
        {
            uint32_t previousRight = left;
            left = right ^ (mix(left ^ inverseKeys[round]) & 65535u);
            right = previousRight;
        }
        assert(((left << 16) | right) == i);
    }
    std::sort(generated.begin(), generated.end());
    assert(std::adjacent_find(generated.begin(), generated.end()) == generated.end());
    for (int count : bins)
        assert(count > 55000 && count < 70000);
    practice::RandomSeedOrder different(FixtureSearchKey ^ 0xfeedbeef12345678ULL);
    int same = 0;
    for (uint32_t i = 0; i < 10000; i++)
        same += order.SeedAt(i) == different.SeedAt(i);
    assert(same < 10);
    if (argc > 2)
    {
        std::ifstream in(argv[2], std::ios::binary);
        int count = 0;
        while (true)
        {
            uint64_t key;
            uint32_t slot;
            int32_t actual;
            in.read(reinterpret_cast<char *>(&key), 8);
            if (!in)
                break;
            in.read(reinterpret_cast<char *>(&slot), 4);
            in.read(reinterpret_cast<char *>(&actual), 4);
            assert(in && practice::RandomSeedOrder(key).SeedAt(slot) == actual);
            count++;
        }
        assert(count == 1024);
    }
    auto fixture = SeedFixture();
    fixture.seed = std::numeric_limits<int>::max() - 5;
    auto choices = fixture.samples;
    auto source = StreamingSource(fixture);
    auto request = StreamingRequest(*source, 20);
    auto packet = StreamingBatch(request, 0, 20, choices);
    auto encoded = EncodeBatchFixture(*packet);
    auto decoded = practice::DecodeSeedBatch(encoded, error);
    assert(decoded && decoded->total == 20 && decoded->layouts.size() == 20 && decoded->status == 1);
    std::mt19937 random(456);
    for (int i = 0; i < 10000; i++)
    {
        auto bad = encoded;
        if (i % 3 == 0)
            bad.resize(random() % (bad.size() + 1));
        else
            bad[random() % bad.size()] = uint8_t(random());
        auto result = practice::DecodeSeedBatch(bad, error);
        if (result)
            assert(result->layouts.size() <= practice::SeedBatchLimit &&
                   result->total <= practice::SeedSpace);
    }
    if (argc > 1)
    {
        std::ifstream in(argv[1], std::ios::binary);
        std::vector<uint8_t> wire{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        auto batch = practice::DecodeSeedBatch(wire, error);
        assert(batch && batch->pid == 6789 && batch->total == practice::SeedSpace && batch->offset == 0 &&
               batch->layouts.size() == 1 && batch->serial == 99 && batch->key == FixtureSearchKey &&
               batch->layouts[0].seed == practice::RandomSeedOrder(FixtureSearchKey).SeedAt(0));
    }
    // Independent full map analysis for every layout, including asymmetric
    // corridor approximation and crystal-array order. No search cache reused.
    practice::NavGraph graph(*source->data);
    std::vector<practice::SeedResult> expected;
    for (size_t i = 0; i < choices.size(); i++)
    {
        auto candidate = std::make_shared<practice::MapData>(*source->data);
        candidate->spawn = choices[i].spawn;
        candidate->selected = choices[i].crystals;
        auto analysis = practice::AnalyzeMap(candidate, graph);
        if (!analysis.route.complete)
            continue;
        practice::SeedResult result;
        result.index = i;
        result.seed = practice::RandomSeedOrder(request.key).SeedAt(uint32_t(i));
        result.selection = choices[i];
        result.route = analysis.route;
        practice::RankSeed(expected, std::move(result));
    }
    practice::SeedSearch search;
    for (int workers : {1, std::min(4, SeedWorkerLimit())})
    {
        request.workers = workers;
        search.Begin(request);
        assert(!search.Submit(packet, 77)); // Wait for the matching catalog before accepting any stream.
        auto oldSource = std::make_shared<practice::MapAnalysis>(*source);
        auto oldData = std::make_shared<practice::MapData>(*source->data);
        oldData->requestSerial = 99;
        oldSource->data = oldData;
        search.Update(oldSource);
        assert(search.Get()->phase == practice::SearchPhase::Generating);
        search.Update(source);
        FeedStreaming(search, request, choices);
        auto completed = AwaitStreaming(search);
        assert(completed->phase == practice::SearchPhase::Completed && completed->generated == 20 &&
               completed->evaluated == 20 && completed->estimated == 0);
        assert(completed->top.size() == expected.size() && completed->preview &&
               completed->preview->route.line.size() > 1);
        for (size_t i = 0; i < expected.size(); i++)
            assert(completed->top[i].seed == expected[i].seed &&
                   completed->top[i].index == expected[i].index &&
                   std::abs(completed->top[i].route.length - expected[i].route.length) < .00001 &&
                   completed->top[i].route.exact);
        assert(completed->source->data->samples.empty());
    }
    // A displayed result remains exact while a continuous search publishes
    // fresh rankings. Pinning does not stop the coordinator or alter the row.
    auto selected = search.Get()->top.back();
    search.Pin(*source, selected);
    auto pinned = search.Pinned();
    assert(pinned && pinned->data->seed == selected.seed && pinned->data->spawn == selected.selection.spawn &&
           pinned->data->selected == selected.selection.crystals);
    auto continuous = request;
    continuous.count = practice::SeedSpace;
    search.Begin(continuous);
    search.Update(source);
    auto firstBatch = StreamingBatch(continuous, 0, 20, choices);
    assert(search.Submit(firstBatch, continuous.session));
    auto pinDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (search.Get()->evaluated < 20 && std::chrono::steady_clock::now() < pinDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(search.Busy() && search.Get()->evaluated == 20 && search.Pinned() == pinned &&
           pinned->data->seed == selected.seed);
    search.Stop();
    assert(search.Pinned() == pinned);
    search.HidePreview();
    assert(!search.Pinned());
    std::cout
        << "Continuous search: selected seed/spawn/crystals stay pinned through new rankings and stop; explicit preview reset clears selection: PASS\n";
    // Independent filtering/ranking of every sampled layout. The actual
    // worker pipeline must match it, with identical counters on 1/4 workers.
    for (int goal = 0; goal < 2; goal++)
        for (int filteredMode = 0; filteredMode < 2; filteredMode++)
            for (int workers : {1, std::min(4, SeedWorkerLimit())})
            {
                auto custom = StreamingRequest(*source, 257, workers, false);
                auto &criteria = custom.criteria;
                criteria.catalog = practice::CatalogKey(*source->data);
                criteria.objective = static_cast<practice::SearchObjective>(goal);
                if (filteredMode)
                    criteria.spawn = 1;
                uint64_t filtered = 0;
                std::vector<practice::SeedResult> wanted;
                for (uint64_t i = 0; i < custom.count; i++)
                {
                    const auto &selection = fixture.samples.at(i % fixture.samples.size());
                    if (criteria.spawn >= 0 && selection.spawn != criteria.spawn)
                    {
                        filtered++;
                        continue;
                    }
                    auto d = std::make_shared<practice::MapData>(*source->data);
                    d->spawn = selection.spawn;
                    d->selected = selection.crystals;
                    auto analysis = practice::AnalyzeMap(d, graph);
                    assert(analysis.route.complete);
                    practice::SeedResult row;
                    row.seed = practice::RandomSeedOrder(custom.key).SeedAt(uint32_t(i));
                    row.index = i;
                    row.selection = selection;
                    row.route = analysis.route;
                    practice::RankSeed(wanted, std::move(row), criteria.objective);
                }
                search.Begin(custom);
                search.Update(source);
                FeedStreaming(search, custom, fixture.samples);
                auto result = AwaitStreaming(search);
                assert(result->phase == practice::SearchPhase::Completed &&
                       result->evaluated == custom.count && result->filtered == filtered &&
                       result->scored == custom.count - filtered && result->top.size() == wanted.size());
                for (size_t i = 0; i < wanted.size(); i++)
                    assert(result->top[i].seed == wanted[i].seed &&
                           result->top[i].selection.spawn == wanted[i].selection.spawn &&
                           std::abs(result->top[i].route.length - wanted[i].route.length) < 1e-8);
                auto text = practice::SeedResultsText(*result);
                assert(text.find("SET PRACTICE research") != std::string::npos &&
                       text.find("spawn seed") != std::string::npos &&
                       text.find("crystal_visit_order") != std::string::npos);
            }
    for (int problem = 0; problem < 2; problem++)
    {
        auto invalid = StreamingRequest(*source, 20);
        invalid.criteria.catalog = practice::CatalogKey(*source->data);
        invalid.criteria.spawn = problem ? 127 : 1;
        if (!problem)
            invalid.criteria.catalog ^= 1;
        search.Begin(invalid);
        search.Update(source);
        auto result = AwaitStreaming(search);
        assert(result->phase == practice::SearchPhase::Failed && !result->error.empty());
    }
    auto forced = *source->data;
    forced.flags |= 16;
    while (forced.spawns.size() < 5)
        forced.spawns.push_back({1, 1, 1});
    practice::SearchCriteria timer;
    timer.catalog = practice::CatalogKey(forced);
    timer.spawn = 1;
    assert(timer.CheckCatalog(forced).empty() &&
           !timer.CheckCatalog(forced, true).empty()); // Captured first-run evidence is not a new override.
    timer.spawn = 4;
    assert(timer.CheckCatalog(forced, true).empty());
    auto invalid = StreamingRequest(*source, 20);
    invalid.criteria.objective = static_cast<practice::SearchObjective>(99);
    search.Begin(invalid);
    assert(search.Get()->phase == practice::SearchPhase::Failed);
    practice::SearchMapFilter saved;
    saved.catalog = practice::CatalogKey(*source->data);
    saved.spawn = 1;
    assert(!saved.Bind(practice::CatalogKey(*source->data)) && saved.spawn == 1);
    auto changed = *source->data;
    std::swap(changed.points[0], changed.points[1]);
    assert(saved.Bind(practice::CatalogKey(changed)) && !saved.HasRules());
    if (argc > 1)
    {
        auto largePath = std::filesystem::path(argv[1]).parent_path() / "seed-large-wire.bin";
        std::ifstream in(largePath, std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
        auto batch = practice::DecodeSeedBatch(bytes, error);
        assert(batch && batch->layouts.size() == 2048 &&
               batch->layouts.back().selection.crystals.size() == 64 &&
               batch->offset == practice::SeedSpace - 2048 && batch->status == 1 &&
               batch->generationMicros == 1234567 && batch->generationFrames == 99);
    }
    search.Begin(request);
    search.Update(source);
    FeedStreaming(search, request, fixture.samples);
    assert(AwaitStreaming(search)->phase == practice::SearchPhase::Completed);
    search.Select(0);
    assert(search.Get()->preview);
    search.HidePreview();
    assert(!search.Get()->preview && !search.Get()->top.empty());
    // Neither the old minimum of ten nor the old maximum of 2000 applies.
    for (uint64_t count : {uint64_t{1}, uint64_t{2057}})
    {
        request.count = count;
        request.fast = true;
        search.Begin(request);
        search.Update(source);
        FeedStreaming(search, request, choices);
        auto completed = AwaitStreaming(search);
        assert(completed->phase == practice::SearchPhase::Completed && completed->evaluated == count &&
               !completed->top.front().route.exact);
    }
    request.count = practice::SeedSpace;
    search.Begin(request);
    assert(search.Get()->request.count == practice::SeedSpace && search.Busy());
    search.Stop();
    request.count = practice::SeedSpace + 1;
    search.Begin(request);
    assert(search.Get()->phase == practice::SearchPhase::Failed);
    request.count = 20;
    search.Begin(request);
    search.Update(source);
    auto foreign = std::make_shared<practice::SeedBatch>(*packet);
    foreign->serial = 99;
    assert(search.Submit(foreign, 77) && search.Busy()); // old search ignored
    foreign->serial = request.serial;
    foreign->offset = 1;
    assert(search.Submit(foreign, 77) && !search.Busy()); // gap stops the search
    search.Begin(request);
    search.Update(source);
    assert(search.Submit(packet, 78) && !search.Busy()); // session mismatch
    search.Begin(request);
    search.Update(source);
    auto wrongSeed = std::make_shared<practice::SeedBatch>(*packet);
    wrongSeed->layouts[0].seed ^= 1;
    assert(search.Submit(wrongSeed, 77) && !search.Busy());
    search.Begin(request);
    search.Update(source);
    auto wrongKey = std::make_shared<practice::SeedBatch>(*packet);
    wrongKey->key ^= 1;
    assert(search.Submit(wrongKey, 77) && !search.Busy());

    search.Begin(request);
    search.Update(source);
    auto repeated = std::make_shared<practice::SeedBatch>(*packet);
    repeated->layouts[0].selection.crystals[1] = repeated->layouts[0].selection.crystals[0];
    assert(search.Submit(repeated, 77) && !search.Busy());
    search.Begin(request);
    search.Update(source);
    auto negativeStatus = std::make_shared<practice::SeedBatch>(*packet);
    negativeStatus->status = -1;
    assert(search.Submit(negativeStatus, 77) && !search.Busy());
    search.Begin(request);
    auto lost = std::make_shared<practice::MapAnalysis>(*source);
    auto lostData = std::make_shared<practice::MapData>(*source->data);
    lostData->flags &= ~1;
    lost->data = lostData;
    search.Update(lost);
    assert(!search.Busy());
    // Missing AI geometry is not evidence that a valid game seed is unplayable.
    auto noNav = fixture;
    noNav.vertices.clear();
    noNav.indices.clear();
    auto missing = StreamingSource(noNav);
    request = StreamingRequest(*missing, 20);
    search.Begin(request);
    search.Update(missing);
    FeedStreaming(search, request, fixture.samples);
    auto noGeometry = AwaitStreaming(search);
    assert(noGeometry->phase == practice::SearchPhase::Completed && noGeometry->evaluated == 20 &&
           noGeometry->top.size() == expected.size() && noGeometry->estimated == 20);
    assert(noGeometry->top[0].route.complete && noGeometry->top[0].route.estimated &&
           noGeometry->top[0].route.line.size() > 1);
    request.layoutOnly = true;
    search.Begin(request);
    search.Update(missing);
    FeedStreaming(search, request, fixture.samples);
    auto layouts = AwaitStreaming(search);
    assert(layouts->phase == practice::SearchPhase::Completed && layouts->evaluated == 20 &&
           !layouts->top.empty() && !layouts->top[0].route.complete && layouts->request.workers == 1);
    practice::SearchTiming eta;
    assert(std::isnan(eta.Remaining(0, 1000, 0)));
    assert(std::isnan(eta.Remaining(100, 1000, 1)));
    assert(std::abs(eta.Remaining(200, 1000, 2) - 8) < 1e-8);
    assert(std::isnan(eta.Remaining(200, 1000, 6)));
    assert(eta.Remaining(1000, 1000, 7) == 0);
    assert(practice::DurationEstimate(90).find("min") != std::string::npos &&
           practice::DurationEstimate(90000).find("d") != std::string::npos);
    // Independent exhaustive ranking of 180 unique layouts checks MST pruning
    // and cross-worker cutoffs against the unpruned analyzer (both goals).
    auto varied = SeedFixture();
    varied.vertices.clear();
    varied.indices.clear();
    varied.walls.clear();
    FixtureTile(varied, 0, 0, 200, 200);
    varied.points.clear();
    for (int i = 0; i < 40; i++)
        varied.points.push_back({float(2 + i % 10 * (i < 10 ? 2 : 18)), 1, float(2 + i / 10 * 30)});
    varied.spawns = {{1, 1, 1}, {195, 1, 195}};
    varied.keep = 8;
    varied.selected = {0, 1, 2, 3, 4, 5, 6, 7};
    varied.liveSelected = varied.selected;
    std::vector<practice::Selection> variedChoices;
    for (int i = 0; i < 180; i++)
    {
        std::vector<int> candidates(i < 20 ? 10 : 40);
        std::iota(candidates.begin(), candidates.end(), 0);
        std::shuffle(candidates.begin(), candidates.end(), random);
        practice::Selection selection;
        selection.spawn = i < 20 ? 0 : i % 2;
        selection.crystals.assign(candidates.begin(), candidates.begin() + 8);
        variedChoices.push_back(std::move(selection));
    }
    auto variedSource = StreamingSource(varied);
    practice::NavGraph variedGraph(*variedSource->data);
    for (bool fast : {false, true})
        for (int goal = 0; goal < 2; goal++)
        {
            std::vector<practice::SeedResult> oracle;
            for (size_t i = 0; i < variedChoices.size(); i++)
            {
                auto candidate = std::make_shared<practice::MapData>(*variedSource->data);
                candidate->spawn = variedChoices[i].spawn;
                candidate->selected = variedChoices[i].crystals;
                practice::SearchDistances matrix(
                    *candidate, variedGraph, [] { return false; }, [](int, int) {});
                practice::RouteWorkspace workspace;
                practice::SeedResult row;
                row.index = i;
                row.seed = order.SeedAt(static_cast<uint32_t>(i));
                row.selection = variedChoices[i];
                row.route = matrix.Score(row.selection, fast, [] { return false; }, workspace);
                practice::RankSeed(oracle, std::move(row), static_cast<practice::SearchObjective>(goal));
            }
            for (int workers : {1, std::min(4, SeedWorkerLimit())})
            {
                auto scan = StreamingRequest(*variedSource, variedChoices.size(), workers, fast);
                scan.criteria.objective = static_cast<practice::SearchObjective>(goal);
                search.Begin(scan);
                search.Update(variedSource);
                FeedStreaming(search, scan, variedChoices);
                auto result = AwaitStreaming(search);
                assert(result->phase == practice::SearchPhase::Completed && result->top.size() == 10 &&
                       result->evaluated == scan.count);
                for (size_t i = 0; i < oracle.size(); i++)
                    assert(result->top[i].index == oracle[i].index &&
                           std::abs(result->top[i].route.length - oracle[i].route.length) < 1e-8);
                if (goal == 0)
                    assert(result->pruned > 0);
                else
                    assert(result->pruned == 0);
            }
        }
    auto native = SeedFixture();
    native.scene = 7;
    native.vertices.clear();
    native.indices.clear();
    native.walls.clear();
    native.spawns = {{2, 1, 2}, {2, 1, 3}};
    native.points = {{8, 1, 2}, {18, 1, 2}};
    native.keep = 2;
    native.selected = native.liveSelected = {0, 1};
    native.samples.clear();
    native.navigationStatus = 2;
    native.navigationNodes = {{2, 1, 2}, {2, 1, 3}, {8, 1, 2}, {18, 1, 2}};
    native.navigationEdges = {0, 2, 1, 2, 2, 3};
    native.navigationAnchors = {0, 1, 2, 3};
    native.navigationQueries = native.navigationTotal = 5;
    auto nativeSource = StreamingSource(native);
    auto nativeRequest = StreamingRequest(*nativeSource, 20, 2, false);
    auto pendingNative = native;
    pendingNative.navigationStatus = 1;
    pendingNative.flags |= 128;
    pendingNative.navigationNodes.clear();
    pendingNative.navigationEdges.clear();
    pendingNative.navigationAnchors.clear();
    search.Begin(nativeRequest);
    search.Update(StreamingSource(pendingNative));
    assert(search.Get()->phase == practice::SearchPhase::Generating &&
           search.Get()->source->data->navigationStatus == 1);
    search.Update(nativeSource);
    std::vector<practice::Selection> nativeChoices = {practice::Selection{0, {0, 1}},
                                                      practice::Selection{1, {0, 1}}};
    FeedStreaming(search, nativeRequest, nativeChoices);
    auto nativeDone = AwaitStreaming(search);
    assert(nativeDone->phase == practice::SearchPhase::Completed && nativeDone->top.size() == 2 &&
           nativeDone->top[0].route.complete && nativeDone->top[0].route.line.size() >= 3 &&
           nativeDone->estimated == 0);
    assert(nativeDone->projectedSpawns == 2 && nativeDone->projectedPoints == 2);
    // Production regression: all catalog anchors were present, but an
    // incomplete captured AI graph excluded every playable seed. Ranking must
    // match an independently built finite matrix, with no implicit filtering.
    for (int defect = 0; defect < 4; defect++)
    {
        auto gaps = SeedFixture();
        gaps.vertices.clear();
        gaps.indices.clear();
        gaps.walls.clear();
        gaps.navigationStatus = 2;
        gaps.navigationNodes = gaps.spawns;
        gaps.navigationNodes.insert(gaps.navigationNodes.end(), gaps.points.begin(), gaps.points.end());
        gaps.navigationAnchors.resize(gaps.navigationNodes.size());
        std::iota(gaps.navigationAnchors.begin(), gaps.navigationAnchors.end(), 0);
        gaps.navigationQueries = gaps.navigationTotal = static_cast<int>(
            gaps.spawns.size() * gaps.points.size() + gaps.points.size() * (gaps.points.size() - 1) / 2);
        if (defect == 1)
            gaps.navigationAnchors.assign(gaps.navigationAnchors.size(), -1);
        if (defect == 2)
            gaps.navigationEdges = {0, 2}; // A valid pair amid missing connections.
        if (defect == 3)
        {
            gaps.navigationStatus = 0;
            gaps.navigationNodes.clear();
            gaps.navigationAnchors.clear();
            gaps.navigationQueries = gaps.navigationTotal = 0;
        }
        auto gapsSource = StreamingSource(gaps);
        for (bool fast : {false, true})
            for (int goal = 0; goal < 2; goal++)
                for (int workers : {1, std::min(4, SeedWorkerLimit())})
                    for (int spawn : {-1, 1})
                    {
                        auto scan = StreamingRequest(*gapsSource, 257, workers, fast);
                        scan.criteria.catalog = practice::CatalogKey(*gapsSource->data);
                        scan.criteria.objective = static_cast<practice::SearchObjective>(goal);
                        scan.criteria.spawn = spawn;
                        std::vector<practice::SeedResult> wanted;
                        uint64_t filtered = 0;
                        for (uint64_t i = 0; i < scan.count; i++)
                        {
                            const auto &selection = gaps.samples.at(i % gaps.samples.size());
                            if (spawn >= 0 && selection.spawn != spawn)
                            {
                                filtered++;
                                continue;
                            }
                            std::vector<practice::Vec3> positions = {gaps.spawns.at(selection.spawn)};
                            for (int id : selection.crystals)
                                positions.push_back(gaps.points.at(id));
                            std::vector<std::vector<double>> matrix(positions.size(),
                                                                    std::vector<double>(positions.size()));
                            for (size_t a = 0; a < positions.size(); a++)
                                for (size_t b = 0; b < positions.size(); b++)
                                    matrix[a][b] = practice::Distance(positions[a], positions[b]);
                            practice::SeedResult row;
                            row.index = i;
                            row.seed = order.SeedAt(static_cast<uint32_t>(i));
                            row.selection = selection;
                            row.route = practice::SolveOrder(matrix, [] { return false; }, !fast);
                            practice::RankSeed(wanted, std::move(row), scan.criteria.objective);
                        }
                        search.Begin(scan);
                        search.Update(gapsSource);
                        FeedStreaming(search, scan, gaps.samples);
                        auto result = AwaitStreaming(search);
                        assert(result->phase == practice::SearchPhase::Completed &&
                               result->generated == scan.count && result->evaluated == scan.count &&
                               result->filtered == filtered);
                        assert(result->filtered + result->scored + result->pruned == scan.count &&
                               result->estimated == scan.count - filtered && result->estimatedPairs > 0);
                        assert(result->top.size() == wanted.size() && result->preview &&
                               result->preview->route.line.size() > 1);
                        for (size_t i = 0; i < wanted.size(); i++)
                        {
                            const auto &row = result->top[i];
                            assert(row.index == wanted[i].index && row.route.complete &&
                                   row.route.estimated &&
                                   std::abs(row.route.length - wanted[i].route.length) < 1e-8);
                            assert(row.route.estimatedLine.size() + 1 == row.route.line.size());
                            assert(std::count(row.route.estimatedLine.begin(), row.route.estimatedLine.end(),
                                              1) > 0);
                        }
                        assert(practice::SeedResultsText(*result).find("estimated (navigation gaps)") !=
                               std::string::npos);
                    }
    }
    // A broken new capture cannot change established, complete triangle routes.
    auto restored = SeedFixture();
    restored.navigationStatus = 2;
    restored.navigationNodes = restored.spawns;
    restored.navigationNodes.insert(restored.navigationNodes.end(), restored.points.begin(),
                                    restored.points.end());
    restored.navigationAnchors.resize(restored.navigationNodes.size());
    std::iota(restored.navigationAnchors.begin(), restored.navigationAnchors.end(), 0);
    restored.navigationQueries = restored.navigationTotal = 27;
    auto restoredSource = StreamingSource(restored);
    practice::NavGraph restoredGraph(*restoredSource->data);
    assert(!restoredGraph.UsesUnityCorridors());
    auto restoredRequest = StreamingRequest(*restoredSource, 20, 2, false);
    search.Begin(restoredRequest);
    search.Update(restoredSource);
    FeedStreaming(search, restoredRequest, restored.samples);
    auto restoredDone = AwaitStreaming(search);
    assert(restoredDone->phase == practice::SearchPhase::Completed && restoredDone->estimated == 0 &&
           restoredDone->estimatedPairs == 0 && restoredDone->top.size() == expected.size());
    for (size_t i = 0; i < expected.size(); i++)
        assert(restoredDone->top[i].index == expected[i].index &&
               restoredDone->top[i].route.length == expected[i].route.length &&
               !restoredDone->top[i].route.estimated);
    std::cout
        << "Disconnected/missing navigation: every seed accounted for, independent Top 10 on both goals/modes, 1/4 workers, explicit spawn filters and estimated path markings; established routes preserved: PASS\n";
    auto pendingLayout = StreamingRequest(*nativeSource, 20, 1, true);
    pendingLayout.layoutOnly = true;
    search.Begin(pendingLayout);
    search.Update(StreamingSource(pendingNative));
    FeedStreaming(search, pendingLayout, nativeChoices);
    assert(AwaitStreaming(search)->phase == practice::SearchPhase::Completed);
    // Timer-sized (19 crystals) captured navigation must produce ten unique
    // candidates and a drawn route in both accuracy modes, without raw triangles.
    auto timerSized = SeedFixture();
    timerSized.scene = 7;
    timerSized.flags |= 32;
    timerSized.vertices.clear();
    timerSized.indices.clear();
    timerSized.walls.clear();
    timerSized.spawns = {{1, 0, 5}};
    timerSized.points.clear();
    timerSized.selected.clear();
    timerSized.navigationNodes = {{1, 0, 5}};
    timerSized.navigationEdges.clear();
    timerSized.navigationAnchors = {0};
    for (int i = 0; i < 20; i++)
    {
        timerSized.points.push_back({float(3 + i * 2), 0, 5});
        timerSized.navigationNodes.push_back(timerSized.points.back());
        timerSized.navigationAnchors.push_back(i + 1);
        timerSized.navigationEdges.push_back(i);
        timerSized.navigationEdges.push_back(i + 1);
        if (i < 19)
            timerSized.selected.push_back(i);
    }
    timerSized.keep = 19;
    timerSized.liveSelected = timerSized.selected;
    timerSized.navigationStatus = 2;
    timerSized.navigationQueries = timerSized.navigationTotal = 210;
    auto timerSource = StreamingSource(timerSized);
    std::vector<practice::Selection> timerChoices;
    for (int missing = 0; missing < 20; missing++)
    {
        practice::Selection s;
        s.spawn = 0;
        for (int i = 0; i < 20; i++)
            if (i != missing)
                s.crystals.push_back(i);
        timerChoices.push_back(std::move(s));
    }
    for (bool fast : {false, true})
    {
        auto scan = StreamingRequest(*timerSource, 2000, 2, fast);
        search.Begin(scan);
        search.Update(timerSource);
        FeedStreaming(search, scan, timerChoices);
        auto finished = AwaitStreaming(search);
        assert(finished->phase == practice::SearchPhase::Completed && finished->top.size() == 10 &&
               finished->estimated == 0 && finished->top[0].route.complete && !finished->top[0].route.exact &&
               finished->top[0].route.line.size() > 10);
    }
    // Screenshot-sized catalog: nine fully projected spawns, 34 fully
    // projected candidate points, 18 crystals and zero graph connections.
    auto screenshot = timerSized;
    screenshot.scene = 12;
    screenshot.flags &= ~32;
    screenshot.keep = 18;
    screenshot.spawns.clear();
    screenshot.points.clear();
    screenshot.selected.clear();
    screenshot.navigationEdges.clear();
    for (int i = 0; i < 9; i++)
        screenshot.spawns.push_back({float(i * 4 + 1), 0, 1});
    for (int i = 0; i < 34; i++)
        screenshot.points.push_back({float((i % 9) * 4 + 1), 0, float(5 + (i / 9) * 7)});
    for (int i = 0; i < 18; i++)
        screenshot.selected.push_back(i);
    screenshot.liveSelected = screenshot.selected;
    screenshot.navigationNodes = screenshot.spawns;
    screenshot.navigationNodes.insert(screenshot.navigationNodes.end(), screenshot.points.begin(),
                                      screenshot.points.end());
    screenshot.navigationAnchors.resize(43);
    std::iota(screenshot.navigationAnchors.begin(), screenshot.navigationAnchors.end(), 0);
    screenshot.navigationQueries = screenshot.navigationTotal = 867;
    auto screenshotSource = StreamingSource(screenshot);
    std::vector<practice::Selection> screenshotChoices;
    for (int i = 0; i < 128; i++)
    {
        practice::Selection s;
        s.spawn = i % 9;
        for (int j = 0; j < 18; j++)
            s.crystals.push_back((i + j) % 34);
        screenshotChoices.push_back(std::move(s));
    }
    for (bool fast : {false, true})
        for (int workers : {1, std::min(4, SeedWorkerLimit())})
        {
            auto scan = StreamingRequest(*screenshotSource, screenshotChoices.size(), workers, fast);
            search.Begin(scan);
            search.Update(screenshotSource);
            FeedStreaming(search, scan, screenshotChoices);
            auto result = AwaitStreaming(search);
            assert(result->phase == practice::SearchPhase::Completed && result->top.size() == 10 &&
                   result->generated == 128 && result->evaluated == 128 && result->filtered == 0 &&
                   result->estimated == 128);
            assert(result->scored + result->pruned == 128 && result->projectedSpawns == 9 &&
                   result->projectedPoints == 34);
            assert(result->preview && result->preview->route.line.size() == 19 &&
                   result->top.front().route.estimated && !result->top.front().route.exact);
        }
    // Exact fourteen-crystal DP saturates the bounded input queue. Stopping
    // while workers run is prompt and cannot be undone by late publication.
    auto large = SeedFixture();
    large.vertices.clear();
    large.indices.clear();
    large.walls.clear();
    FixtureTile(large, 0, 0, 50, 50);
    large.points.clear();
    for (int i = 0; i < 18; i++)
        large.points.push_back({float(2 + (i % 6) * 7), 1, float(3 + (i / 6) * 15)});
    large.keep = 14;
    large.selected.clear();
    for (int i = 0; i < 14; i++)
        large.selected.push_back(i);
    large.liveSelected = large.selected;
    choices.clear();
    for (int i = 0; i < 18; i++)
    {
        practice::Selection selection;
        selection.spawn = i % 2;
        for (int j = 0; j < 14; j++)
            selection.crystals.push_back((i + j) % 18);
        choices.push_back(std::move(selection));
    }
    auto largeSource = StreamingSource(large);
    request = StreamingRequest(*largeSource, 1000000, 1, false);
    search.Begin(request);
    search.Update(largeSource);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    uint64_t offset = 0;
    bool pressure = false;
    while (offset < 8192 && std::chrono::steady_clock::now() < deadline)
    {
        auto batch = StreamingBatch(request, offset, 256, choices);
        if (!search.Submit(batch, 77))
        {
            pressure = true;
            break;
        }
        offset += 256;
    }
    assert(pressure);
    assert(search.Get()->request.count == 1000000);
    while (search.Get()->top.empty() && search.Busy() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(!search.Get()->top.empty());
    auto before = std::chrono::steady_clock::now();
    search.Stop();
    assert(std::chrono::steady_clock::now() - before < std::chrono::milliseconds(500) && !search.Busy() &&
           !search.Get()->top.empty());
    for (int i = 0; i < 20; i++)
    {
        auto fresh = StreamingRequest(*source, 20);
        fresh.serial += i + 1;
        search.Begin(fresh);
        search.Stop();
        assert(search.Get()->phase == practice::SearchPhase::Stopped);
    }
    // A fresh job after canceling heavy work must complete with only new data.
    request = StreamingRequest(*source, 20, 2, true);
    search.Begin(request);
    search.Update(source);
    FeedStreaming(search, request, fixture.samples);
    assert(AwaitStreaming(search)->phase == practice::SearchPhase::Completed);
    {
        practice::SeedSearch cachedSearch;
        auto catalog = SeedFixture();
        catalog.agentRadius = .35f;
        catalog.agentHeight = 1.8f;
        catalog.agentStep = .3f;
        catalog.agentSlope = 45;
        practice::WallOutline outline;
        outline.corners = {practice::Vec3{10, 0, 5}, practice::Vec3{15, 0, 5}, practice::Vec3{15, 0, 25},
                           practice::Vec3{10, 0, 25}};
        catalog.wallOutlines = {outline};
        uint32_t ticket = 600;
        auto runCached = [&](practice::MapData map, bool expectedReuse) {
            auto src = StreamingSource(map, ticket++);
            auto req = StreamingRequest(*src, 20, 1);
            cachedSearch.Begin(req);
            cachedSearch.Update(src);
            FeedStreaming(cachedSearch, req, catalog.samples);
            auto state = AwaitStreaming(cachedSearch);
            assert(state->phase == practice::SearchPhase::Completed &&
                   state->distanceCacheReused == expectedReuse && state->preparationMs >= 0);
            return state;
        };
        auto first = runCached(catalog, false);
        auto changed = catalog;
        changed.epoch++;
        changed.seed++;
        changed.selected = {1, 2, 3, 4};
        auto second = runCached(changed, true);
        assert(first->top.size() == second->top.size());
        for (size_t i = 0; i < first->top.size(); i++)
            assert(first->top[i].route.length == second->top[i].route.length &&
                   first->top[i].seed == second->top[i].seed);
        auto mutation = catalog;
        mutation.points[0].x += .1f;
        runCached(mutation, false);
        runCached(catalog, true);
        mutation = catalog;
        mutation.vertices[0].x += .05f;
        runCached(mutation, false);
        mutation = catalog;
        mutation.agentRadius = .4f;
        runCached(mutation, false);
        runCached(catalog, false); // Third distinct catalog evicts the oldest entry.
        mutation = catalog;
        mutation.agentHeight = 2;
        runCached(mutation, false);
        mutation = catalog;
        mutation.hash = std::string(64, 'b');
        runCached(mutation, false);
        mutation = catalog;
        mutation.navigationStatus = 3;
        runCached(mutation, false);
        mutation = catalog;
        mutation.scene = 4;
        runCached(mutation, false);
    }
    std::cout
        << "Bounded two-catalog distance cache: unchanged request/epoch/selection reuse with identical ranking; points, mesh, radius, height, hash, navigation status, scene invalidate; LRU eviction: PASS\n";
    std::cout
        << "Custom goals/filters, independent match counters, catalog ownership, large direct C# packet and research TSV: PASS\n";
    std::cout
        << "One million unique random seeds, inverse permutation, independent vectors / C# agreement, broad coverage and streaming decode/fuzz, managed wire, random seed ownership, 64-bit counts, independent route ranking, parallel determinism, bounded backpressure, context guards and exact-DP cancellation: PASS\n";
}
