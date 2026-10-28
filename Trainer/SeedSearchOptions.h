#pragma once
#include "PracticeMapData.h"
#include <array>
#include <bit>
#include <charconv>
#include <ostream>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <thread>

namespace practice
{
enum class SearchObjective
{
    ShortestRoute,
    LongestRoute
};

enum class SearchBackend { Automatic, Cpu, Gpu };

inline const char *ObjectiveName(SearchObjective value)
{
    return value == SearchObjective::LongestRoute ? "Longest route (practice)" : "Shortest complete route";
}

// Candidate IDs depend on capture order. Never silently reuse a filter after
// that order or its coordinates changed, including a different game build.
inline uint64_t CatalogKey(const MapData &data)
{
    uint64_t key = 14695981039346656037ULL;
    auto add = [&](uint32_t value) {
        for (int i = 0; i < 4; i++)
        {
            key ^= (value >> (8 * i)) & 255;
            key *= 1099511628211ULL;
        }
    };
    add(static_cast<uint32_t>(data.scene));
    for (unsigned char c : data.hash)
    {
        key ^= c;
        key *= 1099511628211ULL;
    }
    for (const auto *points : {&data.spawns, &data.points})
    {
        add(static_cast<uint32_t>(points->size()));
        for (auto p : *points)
            for (float value : {p.x, p.y, p.z})
                add(value == 0 ? 0 : std::bit_cast<uint32_t>(value));
    }
    return key ? key : 1;
}

struct SearchMapFilter
{
    uint64_t catalog = 0;
    int spawn = -1;

    bool HasRules() const
    {
        return spawn >= 0;
    }

    void Clear()
    {
        spawn = -1;
    }

    bool Bind(uint64_t key)
    {
        if (key == catalog)
            return false;
        bool discarded = HasRules();
        Clear();
        catalog = key;
        return discarded;
    }

    void Encode(std::ostream &out, int level) const
    {
        std::string prefix = "planner.filter" + std::to_string(level) + ".";
        out << prefix << "low=" << uint32_t(catalog) << '\n'
            << prefix << "high=" << uint32_t(catalog >> 32) << '\n'
            << prefix << "spawn=" << spawn << '\n';
    }

    void Decode(const std::string &key, double value)
    {
        if (!std::isfinite(value) || std::floor(value) != value)
            return;
        if (key == "low" && value >= 0 && value <= 4294967295.)
            catalog = (catalog & 0xffffffff00000000ULL) | static_cast<uint32_t>(value);
        else if (key == "high" && value >= 0 && value <= 4294967295.)
            catalog = (catalog & 0xffffffffULL) | (uint64_t(static_cast<uint32_t>(value)) << 32);
        else if (key == "spawn" && value >= -1 && value <= 127)
            spawn = static_cast<int>(value);
    }
};

struct SearchCriteria
{
    SearchObjective objective = SearchObjective::ShortestRoute;
    int spawn = -1;
    uint64_t catalog = 0;

    bool Valid() const
    {
        return int(objective) >= 0 && int(objective) <= 1 && spawn >= -1 && spawn < 128;
    }

    std::string CheckCatalog(const MapData &data, bool currentGeneration = false) const
    {
        if (spawn >= 0 && catalog != CatalogKey(data))
            return "The spawn filter belongs to a different catalog; select its spawn again.";
        if (spawn >= static_cast<int>(data.spawns.size()))
            return "The selected player spawn is not in this catalog.";
        if (currentGeneration && (data.flags & 16) && data.spawns.size() > 4 && spawn >= 0 && spawn != 4)
            return "The first Timer start forces spawn #5; select Any or spawn #5.";
        return {};
    }
};
} // namespace practice

inline int SeedWorkerLimit()
{
    unsigned cpus = std::thread::hardware_concurrency();
    return static_cast<int>(std::clamp(cpus > 1 ? cpus - 1 : 1u, 1u, 32u));
}

struct PlannerPreferences
{
    int level = 0, seed = 0, recipe = 2;
    uint64_t searchCount = 100;
    int searchWorkers = std::min(4, static_cast<int>(std::max(1u, std::thread::hardware_concurrency() / 2)));
    float generationMs = 2;
    bool automaticLoad = true, calculateRoutes = true;
    bool portableGeneration = true;
    bool searchFast = false;
    int searchObjective = 0, searchBackend = 0;
    std::array<practice::SearchMapFilter, kMapLevelCount> filters;
    bool candidates = true, minimapRoute = false;

    void Validate()
    {
        level = std::clamp(level, 0, kMapLevelCount - 1);
        recipe = std::clamp(recipe, 1, 2);
        searchCount = std::clamp(searchCount, uint64_t{1}, uint64_t{1} << 32);
        searchWorkers = std::clamp(searchWorkers, 1, SeedWorkerLimit());
        generationMs = std::isfinite(generationMs) ? std::clamp(generationMs, .25f, 8.f) : 2.f;
        searchObjective = std::clamp(searchObjective, 0, 1);
        searchBackend = std::clamp(searchBackend, 0, 2);
    }

    practice::SearchCriteria Criteria() const
    {
        practice::SearchCriteria value;
        const auto &filter = filters.at(level);
        value.objective = static_cast<practice::SearchObjective>(searchObjective);
        value.spawn = filter.spawn;
        value.catalog = filter.catalog;
        return value;
    }

    void Encode(std::ostream &out) const
    {
        out << "planner.level=" << level << "\nplanner.seed=" << seed << "\nplanner.recipe=" << recipe
            << "\nplanner.candidates=" << candidates << "\nplanner.minimapRoute=" << minimapRoute
            << "\nplanner.searchCount=" << searchCount << "\nplanner.searchFast=" << searchFast
            << "\nplanner.searchWorkers=" << searchWorkers << "\nplanner.generationMs=" << generationMs
            << "\nplanner.searchGoal=" << searchObjective << "\nplanner.searchBackend=" << searchBackend
            << "\nplanner.automaticLoad=" << automaticLoad
            << "\nplanner.calculateRoutes=" << calculateRoutes
            << "\nplanner.portableGeneration=" << portableGeneration << '\n';
        for (int i = 0; i < kMapLevelCount; i++)
            filters[i].Encode(out, i);
    }

    void Decode(const std::string &key, double value)
    {
        if (!std::isfinite(value))
            return;
        int n = static_cast<int>(std::clamp(value, -2147483648., 2147483647.));
        if (key == "level")
            level = n;
        else if (key == "seed")
            seed = n;
        else if (key == "recipe")
            recipe = n;
        else if (key == "searchCount")
            searchCount = static_cast<uint64_t>(std::clamp(value, 1., 4294967296.));
        else if (key == "searchWorkers")
            searchWorkers = n;
        else if (key == "generationMs")
            generationMs = static_cast<float>(value);
        else if (key == "searchObjective")
            searchObjective = n == 2 ? 1 : 0;
        else if (key == "searchGoal")
            searchObjective = n;
        else if (key == "searchBackend")
            searchBackend = n;
        else if (key == "calculateRoutes" && (value == 0 || value == 1))
            calculateRoutes = value == 1;
        else if (key == "portableGeneration" && (value == 0 || value == 1))
            portableGeneration = value == 1;
        else if (key == "automaticLoad" && (value == 0 || value == 1))
            automaticLoad = value == 1;
        else if (key.size() > 8 && key.rfind("filter", 0) == 0 && key[6] >= '0' &&
                 key[6] <= '0' + kMapLevelCount - 1 && key[7] == '.')
            filters[key[6] - '0'].Decode(key.substr(8), value);
        else if (key == "searchFast" && (value == 0 || value == 1))
            searchFast = value == 1;
        else if (key == "candidates" && (value == 0 || value == 1))
            candidates = value == 1;
        else if (key == "minimapRoute" && (value == 0 || value == 1))
            minimapRoute = value == 1;
    }
};
