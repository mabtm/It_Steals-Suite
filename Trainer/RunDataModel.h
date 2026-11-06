#pragma once
#include "MapLevels.h"
#include "ReplayPackage.h"
#include <charconv>
#include <functional>
#include <numeric>
#include <type_traits>
#include <unordered_map>
#include <string_view>

namespace suite_runs
{
inline constexpr int64_t MaxTicks = 3155378975999999999LL;
inline const char *CategoryName(int category)
{
    return category == 0 ? "Random Seed" : category == 1 ? "Set-Seed" : category == 2 ? "Practice" : "Unknown";
}
inline uint64_t Hash(std::string_view s)
{
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) h = (h ^ c) * 1099511628211ULL;
    return h;
}
inline std::vector<std::string_view> Fields(std::string_view line, char delimiter = '|')
{
    std::vector<std::string_view> result;
    size_t start = 0;
    for (size_t i = 0; i <= line.size(); i++)
        if (i == line.size() || line[i] == delimiter)
        { result.push_back(line.substr(start, i - start)); start = i + 1; }
    return result;
}
template<class T> inline T Number(std::string_view s, int base = 10)
{
    T v{};
    std::from_chars_result result;
    if constexpr (std::is_floating_point_v<T>) result = std::from_chars(s.data(), s.data() + s.size(), v);
    else result = std::from_chars(s.data(), s.data() + s.size(), v, base);
    if (result.ec != std::errc{} || result.ptr != s.data() + s.size() || s.empty())
        throw std::runtime_error("Invalid run archive number");
    if constexpr (std::is_floating_point_v<T>)
        if (!std::isfinite(v)) throw std::runtime_error("Invalid run archive value");
    return v;
}
inline bool Flag(std::string_view s)
{
    if (s != "0" && s != "1") throw std::runtime_error("Invalid run archive flag");
    return s == "1";
}
inline void VerifyLine(std::string_view line)
{
    auto pos = line.rfind('|');
    if (line.size() > 1024 || pos == std::string_view::npos || line.size() - pos != 17 ||
        Hash(line.substr(0, pos)) != Number<uint64_t>(line.substr(pos + 1), 16))
        throw std::runtime_error("Run archive checksum mismatch");
}
struct Record
{
    int64_t id = 0, started = 0, ended = 0, session = 0;
    int scene = -1, category = 0, seedKind = 0, selectedSeed = 0, seed = 0, outcome = 0;
    double seconds = 0, progress = 0;
    uint32_t aids = 0, warnings = 0, initialAids = 0;
    bool seedKnown = false, raw = false, eligible = false, quick = false, pb = false, legacy = false;
    bool Ranked() const { return outcome == 1 && eligible && category < 2 && seconds > 0; }
};
inline Record Parse(std::string_view line)
{
    VerifyLine(line);
    auto a = Fields(line);
    if (a.size() != 22) throw std::runtime_error("Unsupported run archive record");
    Record r;
    r.id = Number<int64_t>(a[0]); r.scene = Number<int>(a[1]); r.category = Number<int>(a[2]);
    r.started = Number<int64_t>(a[3]); r.ended = Number<int64_t>(a[4]); r.seconds = Number<double>(a[5]);
    r.seedKind = Number<int>(a[6]); r.seedKnown = Flag(a[7]); r.selectedSeed = Number<int>(a[8]);
    r.seed = Number<int>(a[9]); r.raw = Flag(a[10]); r.outcome = Number<int>(a[11]);
    r.eligible = Flag(a[12]); r.aids = Number<uint32_t>(a[13]); r.warnings = Number<uint32_t>(a[14]);
    r.quick = Flag(a[15]); r.session = Number<int64_t>(a[16]); r.pb = Flag(a[17]);
    r.progress = Number<double>(a[18]); r.initialAids = Number<uint32_t>(a[19]); r.legacy = Flag(a[20]);
    if (r.id <= 0 || r.id > MaxTicks || r.started <= 0 || r.started > MaxTicks || r.ended < 0 ||
        r.ended > MaxTicks || (r.ended && r.ended < r.started) || MapLevelIndex(r.scene) < 0 ||
        r.category < 0 || r.category > 2 || r.seedKind < 0 || r.seedKind > 2 || r.outcome < 1 || r.outcome > 5 ||
        r.session < 0 || r.session > MaxTicks || r.seconds < 0 || r.seconds > 315360000 ||
        r.progress < 0 || r.progress > 1)
        throw std::runtime_error("Invalid run archive record");
    return r;
}
inline int64_t ParseSession(std::string_view line)
{
    VerifyLine(line);
    auto a = Fields(line);
    if (a.size() != 3 || a[0] != "SESSION") throw std::runtime_error("Invalid archive session marker");
    auto id = Number<int64_t>(a[1]);
    if (id <= 0 || id > MaxTicks) throw std::runtime_error("Invalid archive session identity");
    return id;
}
struct Counters
{
    int64_t attempts = 0, finished = 0, resets = 0, deaths = 0;
    double finishSeconds = 0, playSeconds = 0;
};
struct Legacy
{
    bool available = false;
    int64_t since = 0;
    std::array<double, 16> pb{};
    std::array<double, 8> practicePb{};
    std::array<int64_t, 8> practiceDates{};
    std::array<int64_t, 16> dates{};
    std::array<Counters, 32> modes{};
};
inline Legacy ParseLegacy(std::string_view bytes)
{
    if (bytes.starts_with("\xef\xbb\xbf")) bytes.remove_prefix(3);
    auto checksum = bytes.rfind("checksum=");
    if (checksum == std::string_view::npos) throw std::runtime_error("Counter snapshot has no checksum");
    auto digest = bytes.substr(checksum + 9);
    while (!digest.empty() && (digest.back() == '\r' || digest.back() == '\n')) digest.remove_suffix(1);
    size_t offset = 0;
    auto expected = practice::ReplaySha256(checksum, [&](uint8_t *out, size_t n) {
        std::memcpy(out, bytes.data() + offset, n); offset += n;
    });
    if (digest != expected) throw std::runtime_error("Counter snapshot checksum mismatch");
    Legacy result;
    int version = 0;
    for (auto line : Fields(bytes.substr(0, checksum), '\n'))
    {
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        auto equals = line.find('=');
        if (equals == std::string_view::npos) continue;
        auto key = line.substr(0, equals), value = line.substr(equals + 1);
        if (key == "version") version = Number<int>(value);
        else if (key == "modeSince") result.since = Number<int64_t>(value);
        else if (key.starts_with("mode."))
        {
            int row = Number<int>(key.substr(5)); auto fields = Fields(value);
            if (row < 0 || row >= 32 || fields.size() != 6) throw std::runtime_error("Invalid mode counter snapshot");
            auto &c = result.modes[row];
            c.attempts = Number<int64_t>(fields[0]); c.finished = Number<int64_t>(fields[1]);
            c.resets = Number<int64_t>(fields[2]); c.deaths = Number<int64_t>(fields[3]);
            c.finishSeconds = Number<double>(fields[4]); c.playSeconds = Number<double>(fields[5]);
            if (c.attempts < 0 || c.finished < 0 || c.resets < 0 || c.deaths < 0 || c.finishSeconds < 0 ||
                c.playSeconds < 0 || c.finished > c.attempts || c.resets > c.attempts || c.deaths > c.resets)
                throw std::runtime_error("Inconsistent mode counter snapshot");
        }
        else if (key.starts_with("practicePb."))
        {
            int level = MapLevelIndex(Number<int>(key.substr(11)));
            double time = Number<double>(value);
            if (time < 0 || time > 315360000) throw std::runtime_error("Invalid Practice best");
            if (level >= 0) result.practicePb[level] = time;
        }
        else if (key.starts_with("practicePbAt."))
        {
            int level = Number<int>(key.substr(13)); auto date = Number<int64_t>(value);
            if (level < 0 || level >= 8 || date < 0 || date > MaxTicks) throw std::runtime_error("Invalid Practice best date");
            result.practiceDates[level] = date;
        }
        else if (key.starts_with("pb.") || key.starts_with("setSeedPb."))
        {
            bool set = key.starts_with("setSeedPb.");
            int level = MapLevelIndex(Number<int>(key.substr(set ? 10 : 3)));
            double time = Number<double>(value);
            if (time < 0 || time > 315360000) throw std::runtime_error("Invalid stored PB");
            if (level >= 0) result.pb[level * 2 + int(set)] = time;
        }
        else if (key.starts_with("pbAt.") || key.starts_with("setSeedPbAt."))
        {
            bool set = key.starts_with("setSeedPbAt.");
            int level = Number<int>(key.substr(set ? 12 : 5)); auto date = Number<int64_t>(value);
            if (level < 0 || level >= 8 || date < 0 || date > MaxTicks) throw std::runtime_error("Invalid PB date");
            result.dates[level * 2 + int(set)] = date;
        }
    }
    if (version < 6 || version > 9 || result.since < 0 || result.since > MaxTicks)
        throw std::runtime_error("Unsupported counter snapshot format");
    result.available = true;
    return result;
}

enum class Display { Bests, TopFive, History, Statistics };
enum class Sort { Date, Mode, Category, Time, Seed, Outcome };
struct Query
{
    Display display = Display::Bests;
    Sort sort = Sort::Mode;
    int level = -1, category = -1, status = 0, recipe = -1;
    bool ascending = true, seedFilter = false, pbOnly = false;
    int seed = 0;
    int64_t from = 0, through = MaxTicks, session = 0;
    bool operator==(const Query &) const = default;
};
struct Store
{
    std::shared_ptr<const std::vector<Record>> records = std::make_shared<const std::vector<Record>>();
    Legacy legacy;
    int64_t session = 0, detailedSince = 0;
    bool imported = false;
    uint64_t revision = 0;
    std::string error;
};
struct Totals
{
    int64_t attempts = 0, valid = 0, invalid = 0, finished = 0, resets = 0, deaths = 0,
            interrupted = 0, quick = 0;
    double attemptSeconds = 0, finishSeconds = 0, median = 0;
};
struct View
{
    std::shared_ptr<const Store> source;
    Query query;
    std::vector<Record> rows; // worker-built result; never sorted on render thread
    std::vector<int> ranks;
    std::array<Totals, 24> totals{};
    uint64_t request = 0;
};
inline bool Match(const Record &r, const Query &q)
{
    return (q.level < 0 || MapLevelIndex(r.scene) == q.level) &&
        (q.category < 0 || r.category == q.category) &&
        (!q.seedFilter || r.seedKnown && (r.category == 1 ? r.selectedSeed : r.seed) == q.seed) &&
        (q.recipe < 0 || int(r.raw) == q.recipe) &&
        r.started >= q.from && r.started <= q.through && (!q.session || r.session == q.session) &&
        (!q.pbOnly || r.pb) && (!q.status || (q.status == 1 ? r.Ranked() :
            q.status == 2 ? r.outcome == 1 && !r.eligible : r.outcome != 1));
}
inline View Build(std::shared_ptr<const Store> store, Query query, uint64_t request = 0,
                  const std::function<bool()> &canceled = {})
{
    View view; view.source = std::move(store); view.query = query; view.request = request;
    std::array<std::vector<double>, 24> finishTimes;
    size_t work = 0;
    auto check = [&] {
        if ((++work & 1023) == 0 && canceled && canceled()) throw std::runtime_error("Run query superseded");
    };
    for (const auto &r : *view.source->records)
    {
        check();
        if (!Match(r, query)) continue;
        auto &t = view.totals[MapLevelIndex(r.scene) * 3 + r.category];
        if (r.quick) t.quick++;
        else t.attempts++;
        t.attemptSeconds += r.seconds;
        if (r.outcome == 1)
        {
            t.finished++; t.finishSeconds += r.seconds;
            if (r.eligible) t.valid++; else t.invalid++;
            finishTimes[MapLevelIndex(r.scene) * 3 + r.category].push_back(r.seconds);
        }
        else if (!r.quick && r.outcome == 2) t.resets++;
        else if (r.outcome == 3) { t.resets++; t.deaths++; }
        else if (r.outcome >= 4) t.interrupted++;
        if (query.display == Display::TopFive && !r.Ranked() ||
            query.display == Display::Bests && !r.Ranked() && !(r.category == 2 && r.outcome == 1 && r.seconds > 0)) continue;
        if (query.display != Display::Statistics) view.rows.push_back(r);
    }
    for (size_t i = 0; i < finishTimes.size(); i++)
    {
        auto &times = finishTimes[i];
        if (times.empty()) continue;
        std::sort(times.begin(), times.end());
        auto n = times.size(); view.totals[i].median = n % 2 ? times[n / 2] : (times[n / 2 - 1] + times[n / 2]) * .5;
    }
    if (query.display == Display::Bests || query.display == Display::TopFive)
    std::sort(view.rows.begin(), view.rows.end(), [&](const Record &a, const Record &b) {
        check();
        int ma = MapLevelIndex(a.scene), mb = MapLevelIndex(b.scene);
        if (ma != mb) return ma < mb;
        if (a.category != b.category) return a.category < b.category;
        return a.seconds != b.seconds ? a.seconds < b.seconds : a.id < b.id;
    });
    if (query.display == Display::Bests || query.display == Display::TopFive)
    {
        std::vector<Record> selected;
        std::array<int, 24> count{};
        for (const auto &r : view.rows)
        {
            int key = MapLevelIndex(r.scene) * 3 + r.category;
            if (count[key]++ < (query.display == Display::Bests ? 1 : 5)) selected.push_back(r);
        }
        view.rows = std::move(selected);
        if (query.display == Display::Bests && !query.seedFilter && query.recipe < 0 && !query.from &&
            query.through == MaxTicks && !query.session && !query.pbOnly && !query.status)
            for (int level = 0; level < 8; level++)
                for (int cat = 0; cat < 3; cat++)
                {
                    double time = cat == 2 ? view.source->legacy.practicePb[level] : view.source->legacy.pb[level * 2 + cat];
                    if (time <= 0 || query.level >= 0 && query.level != level ||
                        query.category >= 0 && query.category != cat) continue;
                    auto it = std::find_if(view.rows.begin(), view.rows.end(), [&](const Record &r) {
                        return r.scene == kMapScenes[level] && r.category == cat;
                    });
                    if (it != view.rows.end() && it->seconds <= time) continue;
                    Record old;
                    old.scene = kMapScenes[level]; old.category = cat; old.seconds = time; old.outcome = 1;
                    old.started = cat == 2 ? view.source->legacy.practiceDates[level] : view.source->legacy.dates[level * 2 + cat]; old.legacy = true; old.pb = cat < 2;
                    if (it == view.rows.end()) view.rows.push_back(old); else *it = old;
                }
    }
    auto compare = [&](const Record &a, const Record &b) {
        check();
        auto cmp = [](auto x, auto y) { return x < y ? -1 : x > y ? 1 : 0; };
        int c = 0;
        switch (query.sort)
        {
        case Sort::Date: c = cmp(a.started, b.started); break;
        case Sort::Mode: c = cmp(MapLevelIndex(a.scene), MapLevelIndex(b.scene)); break;
        case Sort::Category: c = cmp(a.category, b.category); break;
        case Sort::Time: c = cmp(a.seconds, b.seconds); break;
        case Sort::Seed: c = cmp(a.category == 1 ? a.selectedSeed : a.seed, b.category == 1 ? b.selectedSeed : b.seed); break;
        case Sort::Outcome: c = cmp(a.outcome, b.outcome); break;
        }
        if (c) return query.ascending ? c < 0 : c > 0;
        if (a.scene != b.scene) return MapLevelIndex(a.scene) < MapLevelIndex(b.scene);
        if (a.category != b.category) return a.category < b.category;
        if (query.display == Display::TopFive && a.seconds != b.seconds) return a.seconds < b.seconds;
        return a.id < b.id;
    };
    std::sort(view.rows.begin(), view.rows.end(), compare);
    if (query.display == Display::TopFive) view.ranks.reserve(view.rows.size());
    for (const auto &r : view.rows)
    {
        if (query.display != Display::TopFive) break;
        int rank = 1;
        if (query.display == Display::TopFive)
            for (const auto &other : view.rows)
                if (other.scene == r.scene && other.category == r.category &&
                    (other.seconds < r.seconds || other.seconds == r.seconds && other.id < r.id)) rank++;
        view.ranks.push_back(rank);
    }
    return view;
}
}
