#pragma once
#include "SeedSearch.h"
#include "seed_map_fixture.h"

inline std::vector<uint8_t> EncodeBatchFixture(const practice::SeedBatch &batch)
{
    std::vector<uint8_t> bytes;
    auto value = [&]<class T>(T v) {
        auto start = bytes.size();
        bytes.resize(start + sizeof(T));
        std::memcpy(bytes.data() + start, &v, sizeof(T));
    };
    auto text = [&](const std::string &s) {
        value(int32_t(s.size()));
        bytes.insert(bytes.end(), s.begin(), s.end());
    };
    value(practice::SeedBatchMagic);
    value(int32_t{3});
    value(int32_t(batch.pid));
    value(int32_t(batch.scene));
    value(int32_t(batch.epoch));
    value(batch.serial);
    value(int32_t(batch.seed));
    value(int32_t(batch.recipe));
    value(batch.total);
    value(batch.offset);
    value(int32_t(batch.status));
    value(batch.key);
    value(batch.generationMicros);
    value(batch.generationFrames);
    text(batch.hash);
    text(batch.message);
    value(int32_t(batch.layouts.size()));
    for (const auto &layout : batch.layouts)
    {
        value(int32_t(layout.seed));
        value(int32_t(layout.selection.spawn));
        value(int32_t(layout.selection.crystals.size()));
        for (int id : layout.selection.crystals)
            value(int32_t(id));
    }
    return bytes;
}

inline constexpr uint64_t FixtureSearchKey = 0x0123456789abcdefULL;

inline std::shared_ptr<const practice::MapAnalysis> StreamingSource(practice::MapData fixture = SeedFixture(),
                                                                    uint32_t serial = 100)
{
    fixture.seed = practice::RandomSeedOrder(FixtureSearchKey).SeedAt(0);
    fixture.recipe = 1;
    fixture.requestSerial = serial;
    fixture.stage = 2;
    fixture.requested = fixture.completed = 0;
    fixture.samples.clear();
    std::string error;
    auto data = practice::DecodeMap(EncodeFixture(fixture), error);
    if (!data)
        throw std::runtime_error(error);
    auto result = std::make_shared<practice::MapAnalysis>();
    result->data = data;
    result->live = true;
    result->session = 77;
    return result;
}

inline practice::SeedSearchRequest StreamingRequest(const practice::MapAnalysis &source, uint64_t count,
                                                    int workers = 1, bool fast = false)
{
    const auto &d = *source.data;
    practice::SeedSearchRequest r;
    r.pid = d.pid;
    r.scene = d.scene;
    r.epoch = d.epoch;
    r.hash = d.hash;
    r.session = source.session;
    r.serial = d.requestSerial;
    r.key = FixtureSearchKey;
    r.seed = d.seed;
    r.recipe = d.recipe;
    r.count = count;
    r.workers = workers;
    r.fast = fast;
    return r;
}

inline std::shared_ptr<const practice::SeedBatch> StreamingBatch(
    const practice::SeedSearchRequest &r, uint64_t offset, size_t count,
    const std::vector<practice::Selection> &choices)
{
    auto batch = std::make_shared<practice::SeedBatch>();
    batch->pid = r.pid;
    batch->scene = r.scene;
    batch->epoch = r.epoch;
    batch->hash = r.hash;
    batch->serial = r.serial;
    batch->key = r.key;
    batch->seed = r.seed;
    batch->recipe = r.recipe;
    batch->total = r.count;
    batch->offset = offset;
    batch->status = offset + count == r.count ? 1 : 0;
    practice::RandomSeedOrder order(r.key);
    for (size_t i = 0; i < count; i++)
        batch->layouts.push_back(
            {order.SeedAt(static_cast<uint32_t>(offset + i)), choices.at((offset + i) % choices.size())});
    return batch;
}

inline void FeedStreaming(practice::SeedSearch &search, const practice::SeedSearchRequest &request,
                          const std::vector<practice::Selection> &choices)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    for (uint64_t offset = 0; offset < request.count;)
    {
        auto batch = StreamingBatch(
            request, offset,
            static_cast<size_t>(std::min(uint64_t{practice::SeedBatchLimit}, request.count - offset)),
            choices);
        while (!search.Submit(batch, request.session))
        {
            if (!search.Busy() || std::chrono::steady_clock::now() > deadline)
                throw std::runtime_error("Stream backpressure timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        offset += batch->layouts.size();
    }
}

inline std::shared_ptr<const practice::SeedSearchState> AwaitStreaming(practice::SeedSearch &search)
{
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto state = search.Get();
        if (!state->Busy())
            return state;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    throw std::runtime_error("Streaming route search timed out");
}
