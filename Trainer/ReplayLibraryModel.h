#pragma once
#include "ReplayArchive.h"
#include <cctype>

namespace practice
{
enum class ReplaySort
{
    Date,
    Name,
    Mode,
    Time,
    Seed,
    Status,
    Category
};

struct ReplayLibraryFilter
{
    std::string query;
    int level = -1, status = 0, category = -1;
    bool favorites = false, ascending = false, includeFullGameLevels = false;
    ReplaySort sort = ReplaySort::Date;
    bool operator==(const ReplayLibraryFilter &) const = default;
};

// The archive publishes immutable snapshots. Index display/search text once per
// snapshot and rebuild visible indices only when data, filters or sorting change.
class ReplayLibraryView
{
    std::shared_ptr<const ReplayArchiveState> source;
    ReplayLibraryFilter previous;

    static std::string Lower(std::string value)
    {
        for (auto &c : value)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        return value;
    }

    static int Status(const ReplayInfo &row)
    {
        return ReplayAborted(row.outcome) ? 2 : row.valid ? 0 : 1;
    }

    static bool NotesMatch(const std::string &notes, const std::string &query)
    {
        return std::search(notes.begin(), notes.end(), query.begin(), query.end(),
                           [](unsigned char a, unsigned char b) { return std::tolower(a) == b; }) !=
               notes.end();
    }

  public:
    struct Text
    {
        std::string id, label, name, search, date;
    };

    std::vector<Text> text;
    std::vector<size_t> shown;

    bool Update(std::shared_ptr<const ReplayArchiveState> state, const ReplayLibraryFilter &filter)
    {
        bool changed = state != source;
        if (!changed && filter == previous)
            return false;
        if (changed)
        {
            source = std::move(state);
            text.clear();
            text.reserve(source->runs.size());
            for (const auto &row : source->runs)
            {
                int level = MapLevelIndex(row.scene);
                std::string label = row.name + (row.category.empty() ? "" : " / " + row.category);
                text.push_back({std::to_string(row.id),
                                label,
                                Lower(label),
                                Lower(row.name + " " + row.category + " " + RecordedCategoryName(row.runCategory) + " " +
                                      std::to_string(row.runCategory == 1 ? row.selectedSeed : row.seed) + " " +
                                      (level >= 0 ? kMapLevelNames[level] : "Unknown")),
                                {}});
            }
        }
        previous = filter;
        auto query = Lower(filter.query);
        shown.clear();
        for (size_t i = 0; i < source->runs.size(); i++)
        {
            const auto &row = source->runs[i];
            bool status = filter.status == 0 || (filter.status == 1 && !ReplayAborted(row.outcome)) ||
                          (filter.status == 2 && !ReplayAborted(row.outcome) && row.valid) ||
                          (filter.status == 3 && !ReplayAborted(row.outcome) && !row.valid) ||
                          (filter.status == 4 && ReplayAborted(row.outcome));
            if ((filter.includeFullGameLevels || !source->fullGameChildren.contains(row.id)) && (filter.level < 0 || MapLevelIndex(row.scene) == filter.level) && status &&
                (filter.category < 0 || row.runCategory == filter.category) &&
                (!filter.favorites || row.favorite) &&
                (text[i].search.find(query) != std::string::npos || NotesMatch(row.notes, query)))
                shown.push_back(i);
        }
        std::sort(shown.begin(), shown.end(), [&](size_t a, size_t b) {
            const auto &x = source->runs[a], &y = source->runs[b];
            int comparison = 0;
            auto compare = [](const auto &x, const auto &y) { return x < y ? -1 : x > y ? 1 : 0; };
            switch (filter.sort)
            {
            case ReplaySort::Date:
                comparison = compare(x.id, y.id);
                break;
            case ReplaySort::Name:
                comparison = compare(text[a].name, text[b].name);
                break;
            case ReplaySort::Mode:
                comparison = compare(MapLevelIndex(x.scene), MapLevelIndex(y.scene));
                break;
            case ReplaySort::Time:
                comparison = compare(x.seconds, y.seconds);
                break;
            case ReplaySort::Seed:
                comparison = compare(x.runCategory == 1 ? x.selectedSeed : x.seed, y.runCategory == 1 ? y.selectedSeed : y.seed);
                break;
            case ReplaySort::Status:
                comparison = compare(Status(x), Status(y));
                break;
            case ReplaySort::Category:
                comparison = compare(x.runCategory, y.runCategory);
                break;
            }
            return comparison ? (filter.ascending ? comparison < 0 : comparison > 0) : x.id > y.id;
        });
        return true;
    }
};
} // namespace practice
