#pragma once
#include "PracticeMapData.h"
#include "RandomSeedOrder.h"

namespace practice
{
inline constexpr uint64_t SeedSpace = uint64_t{1} << 32;
inline constexpr size_t SeedBatchLimit = 2048, SeedQueueLimit = 8, SeedBatchByteLimit = 600000;
inline constexpr uint32_t SeedBatchMagic = 0x42535349u;

struct SeedLayout
{
    int seed = 0;
    Selection selection;
};

struct SeedBatch
{
    int pid = 0, scene = 0, epoch = 0, seed = 0, recipe = 0, status = 0;
    uint32_t serial = 0;
    uint64_t total = 0, offset = 0, key = 0, generationMicros = 0, generationFrames = 0;
    std::string hash, message;
    std::vector<SeedLayout> layouts;
};

inline std::shared_ptr<const SeedBatch> DecodeSeedBatch(const std::vector<uint8_t> &bytes, std::string &error)
{
    try
    {
        if (bytes.size() > SeedBatchByteLimit)
            throw std::runtime_error("Seed batch exceeds limit");
        MapReader r(bytes);
        if (r.Read<uint32_t>() != SeedBatchMagic || r.Read<int32_t>() != 3)
            throw std::runtime_error("Unsupported seed batch");
        auto b = std::make_shared<SeedBatch>();
        b->pid = r.Read<int32_t>();
        b->scene = r.Read<int32_t>();
        b->epoch = r.Read<int32_t>();
        b->serial = r.Read<uint32_t>();
        b->seed = r.Read<int32_t>();
        b->recipe = r.Read<int32_t>();
        b->total = r.Read<uint64_t>();
        b->offset = r.Read<uint64_t>();
        b->status = r.Read<int32_t>();
        b->key = r.Read<uint64_t>();
        b->generationMicros = r.Read<uint64_t>();
        b->generationFrames = r.Read<uint64_t>();
        b->hash = r.Text(65);
        b->message = r.Text(1024);
        int count = r.Count(static_cast<int>(SeedBatchLimit));
        b->layouts.reserve(count);
        for (int i = 0; i < count; i++)
        {
            SeedLayout layout;
            layout.seed = r.Read<int32_t>();
            layout.selection.spawn = r.Read<int32_t>();
            layout.selection.crystals = r.Ints(64);
            b->layouts.push_back(std::move(layout));
        }
        if (!r.Done() || b->pid < 1 || MapLevelIndex(b->scene) < 0 || b->epoch < 0 || b->recipe < 1 ||
            b->recipe > 2 || !b->total || b->total > SeedSpace || b->offset > b->total ||
            static_cast<uint64_t>(count) > b->total - b->offset || b->status < 0 || b->status > 3 ||
            (b->status == 0 && !count) || (b->status == 1 && b->offset + count != b->total))
            throw std::runtime_error("Invalid seed batch metadata");
        RandomSeedOrder order(b->key);
        if (b->seed != order.SeedAt(0))
            throw std::runtime_error("Invalid random search identity");
        for (size_t i = 0; i < b->layouts.size(); i++)
            if (b->layouts[i].seed != order.SeedAt(static_cast<uint32_t>(b->offset + i)))
                throw std::runtime_error("Layout seed does not match random search order");
        error.clear();
        return b;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return {};
    }
}
} // namespace practice
