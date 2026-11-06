#pragma once
#include "PracticeMapData.h"
#include "ReplaySplitModel.h"
#include "ReplayPackage.h"
#include "ReplayAudioIO.h"
#include "ReplayHealth.h"
#include "ReplayTextValidation.h"
#include "SuiteUpdateModel.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <unordered_map>

namespace practice
{
inline const char *RecordedCategoryName(int category)
{
    return category == 0 ? "Random Seed" : category == 1 ? "Set-Seed" : category == 2 ? "Practice" : "Unknown";
}
inline constexpr const char *CaptureStages[] = {"colliders", "audio_sync", "scene_cameras_ui", "effects", "visual_poses", "view_frame", "capture_total"};
struct CaptureCost { uint64_t count = 0; double mean = 0, p95 = 0, maximum = 0; };
inline std::array<CaptureCost, 7> ParseCaptureProfile(std::string_view text)
{
    auto json = suite_update::JsonReader(text).Read();
    if (json.At("version").UInt() != 1 || json.At("unit").String() != "ms" || json.At("p95_window").UInt() != 128)
        throw std::runtime_error("Unsupported capture CPU profile");
    std::array<CaptureCost, 7> result;
    for (size_t i = 0; i < result.size(); i++)
    {
        const auto &stage = json.At("stages").At(CaptureStages[i]);
        auto number = [&](const char *key) {
            auto &j = stage.At(key); double n = 0;
            auto read = std::from_chars(j.text.data(), j.text.data() + j.text.size(), n);
            if (j.kind != suite_update::Json::Kind::Number || read.ec != std::errc{} ||
                read.ptr != j.text.data() + j.text.size() || !std::isfinite(n) || n < 0 || n > 315360000000.)
                throw std::runtime_error("Invalid capture CPU profile");
            return n;
        };
        double count = number("count");
        if (std::floor(count) != count) throw std::runtime_error("Invalid capture sample count");
        result[i] = {uint64_t(count), number("mean"), number("p95"), number("max")};
    }
    return result;
}
struct ReplayInfo
{
    int64_t id = 0;
    int scene = 0, seed = 0, outcome = 0, runCategory = -1, selectedSeed = 0;
    double seconds = 0;
    bool valid = false, saved = false, favorite = false, rawSeed = false, protectedRecording = false;
    std::string name, category, notes, reason;
    std::array<CaptureCost, 7> captureCpu{};
    bool hasCaptureCpu = false;
    std::filesystem::path file;
};

inline bool ReplayAborted(int outcome)
{
    return outcome != 1;
}

inline const char *ReplayStatusName(int outcome, bool valid)
{
    return ReplayAborted(outcome) ? "ABORTED" : valid ? "VALID" : "UNVALID";
}

inline std::string ReplayRecordedAt(int64_t id)
{
    constexpr int64_t epoch = 621355968000000000ll;
    if (id < epoch || id > 3155378975999999999ll)
        return {};
    std::time_t time = std::time_t((id - epoch) / 10000000);
    std::tm local{};
#ifdef _WIN32
    if (localtime_s(&local, &time) != 0)
        return {};
#else
    if (!localtime_r(&time, &local))
        return {};
#endif
    char stamp[32]{};
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M", &local);
    return stamp;
}

inline std::string ReplayName(const MapData &data)
{
    if (!data.review)
        return "Run";
    auto stamp = ReplayRecordedAt(data.review->id);
    if (stamp.empty())
        return "Run " + std::to_string(data.review->id);
    int level = MapLevelIndex(data.scene);
    return std::string(level >= 0 ? kMapLevelNames[level] : "Run") + " | " + stamp;
}

struct ReplayArchiveState
{
    std::vector<ReplayInfo> runs;
    std::unordered_map<int64_t, size_t> byId;
    std::shared_ptr<const ReplayRevalidation> revalidation;
    std::string error, motionError, exportPath;
    bool busy = false;
    uint64_t savedRevision = 0;
    int64_t annotationRun = 0;
    uint64_t annotationRevision = 0;
    std::string annotationError;
    int64_t savedRun = 0, motionRun = 0, exportedRun = 0, stagedRun = 0;
};

// All directory walks, bounded decoding, loads and atomic writes happen on a
// sleeping IO worker. The UI receives immutable snapshots, never file handles.
class ReplayArchive
{
    static constexpr const char *FavoriteWarning = "Replay favorites could not be loaded (favorites.json).";
    enum class JobKind
    {
        Initialize,
        Refresh,
        ChangeSource,
        LoadRecording,
        LoadReference,
        MatchReference,
        SaveRecording,
        ExportPackage,
        LoadMotion,
        ImportPackage,
        Stage3D,
        LoadAnnotations,
        SaveAnnotations,
        Revalidate,
        SaveVerification,
        Favorite,
    };

    struct Job
    {
        JobKind kind = JobKind::Initialize;
        int64_t id = 0;
        uint64_t request = 0;
        std::filesystem::path path;
        std::shared_ptr<const MapData> data;
        std::string name, category, notes;
        bool favorite = false;
        std::shared_ptr<const ReplayAnnotations> annotations;
        std::shared_ptr<const suite_run::Verification> verification;
    };

    struct Cached
    {
        uintmax_t size = 0;
        std::filesystem::file_time_type modified;
        ReplayInfo info;
    };

    std::filesystem::path root, source;
    std::map<std::filesystem::path, Cached> cache;
    std::set<int64_t> favorites;
    std::shared_ptr<const ReplayArchiveState> state = std::make_shared<ReplayArchiveState>();
    std::shared_ptr<const MapData> loaded;
    std::shared_ptr<const MapData> splitReference;
    struct MatchedReference { int64_t target = 0; std::shared_ptr<const MapData> data; bool pending = false; };
    std::shared_ptr<const MatchedReference> matchedReference = std::make_shared<MatchedReference>();
    std::shared_ptr<const ReplayMotion> motion;
    std::shared_ptr<const ReplayRevalidation> revalidation;
    std::atomic<uint64_t> nextRevalidation{0};
    std::shared_ptr<const ReplayAnnotations> annotations;
    int64_t annotationRun = 0;
    uint64_t annotationRevision = 0;
    std::string annotationError;
    int64_t motionRun = 0, exportedRun = 0, stagedRun = 0;
    std::string motionError, exportPath;
    std::deque<Job> jobs;
    std::mutex gate;
    std::condition_variable wake;
    std::thread worker;
    bool stopping = false, scanQueued = false;
    std::string problem;

    static std::vector<uint8_t> Read(const std::filesystem::path &p, size_t limit)
    {
        std::error_code ec;
        auto n = std::filesystem::file_size(p, ec);
        if (ec || n > limit)
            throw std::runtime_error("Replay file missing or too large");
        std::ifstream in(p, std::ios::binary);
        std::vector<uint8_t> b(static_cast<size_t>(n));
        if (n)
            in.read(reinterpret_cast<char *>(b.data()), static_cast<std::streamsize>(n));
        if (!in)
            throw std::runtime_error("Replay file unreadable");
        return b;
    }

    static void Atomic(const std::filesystem::path &p, const std::vector<uint8_t> &bytes, bool replace,
                       bool backupExisting = true)
    {
        std::filesystem::create_directories(p.parent_path());
        auto temp = p;
        temp += ".tmp";
        if (!replace && std::filesystem::exists(p))
        {
            if (Read(p, std::max(MapCapacity, bytes.size())) != bytes)
                throw std::runtime_error("Existing replay preserved; identity conflict");
            return;
        }
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
            out.close();
            if (!out)
                throw std::runtime_error("Replay could not be saved");
        }
        if (replace && backupExisting && std::filesystem::exists(p))
        {
            auto backup = p;
            backup += ".bak";
            std::filesystem::copy_file(p, backup, std::filesystem::copy_options::overwrite_existing);
        }
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Replay save could not be committed");
#else
        std::filesystem::rename(temp, p);
#endif
    }

    static std::vector<uint8_t> Meta(const ReplayInfo &row)
    {
        auto single = [](const std::string &s) {
            return std::none_of(s.begin(), s.end(), [](unsigned char c) { return c < 32 || c == 127; });
        };
        if (row.name.empty() || !single(row.name) || !single(row.category) || !ReplayUtf8(row.name, 160) ||
            !ReplayUtf8(row.category, 128) || !ReplayUtf8(row.notes, 8192))
            throw std::runtime_error("Replay text is invalid or too long");
        std::vector<uint8_t> b;
        auto put = [&](auto v) {
            auto *p = reinterpret_cast<const uint8_t *>(&v);
            b.insert(b.end(), p, p + sizeof(v));
        };
        put(uint32_t{0x524d4554});
        put(int32_t{1});
        put(row.id);
        for (const auto &s : {row.name, row.category, row.notes})
        {
            put(int32_t(s.size()));
            b.insert(b.end(), s.begin(), s.end());
        }
        put(ReplayHash(b, b.size()));
        return b;
    }

    static void ReadMeta(const std::filesystem::path &p, ReplayInfo &row)
    {
        auto b = Read(p, 16384);
        MapReader r(b);
        if (r.Read<uint32_t>() != 0x524d4554 || r.Read<int32_t>() != 1 || r.Read<int64_t>() != row.id)
            throw std::runtime_error("Replay notes identity mismatch");
        auto name = r.Text(160), category = r.Text(128), notes = r.Text(8192);
        if (!ReplayUtf8(name, 160) || !ReplayUtf8(category, 128) || !ReplayUtf8(notes, 8192) ||
            b.size() < 8 || r.Read<uint64_t>() != ReplayHash(b, b.size() - 8) || !r.Done())
            throw std::runtime_error("Replay notes are damaged");
        row.name = name;
        row.category = category;
        row.notes = notes;
    }

    std::filesystem::path File(int64_t id, const char *extension) const
    {
        return root / ("run-" + std::to_string(id) + extension);
    }

    uint64_t savedRevision = 0;
    int64_t savedRun = 0;

    void Publish(bool busy = false)
    {
        auto snapshot = std::make_shared<ReplayArchiveState>();
        snapshot->revalidation = revalidation;
        snapshot->annotationRun = annotationRun;
        snapshot->annotationRevision = annotationRevision;
        snapshot->annotationError = annotationError;
        snapshot->error = problem;
        snapshot->busy = busy;
        snapshot->savedRevision = savedRevision;
        snapshot->savedRun = savedRun;
        snapshot->motionError = motionError;
        snapshot->motionRun = motionRun;
        snapshot->exportedRun = exportedRun;
        snapshot->exportPath = exportPath;
        snapshot->stagedRun = stagedRun;
        std::map<int64_t, ReplayInfo> rows;
        for (const auto &[path, item] : cache)
        {
            (void)path;
            auto old = rows.find(item.info.id);
            if (old == rows.end()) rows[item.info.id] = item.info;
            else if (item.info.saved)
            {
                bool keep = old->second.protectedRecording;
                rows[item.info.id] = item.info;
                rows[item.info.id].protectedRecording |= keep;
            }
            else old->second.protectedRecording |= item.info.protectedRecording;
        }
        for (auto it = rows.rbegin(); it != rows.rend(); ++it)
        {
            auto row = it->second;
            row.favorite = favorites.contains(row.id);
            auto metadata = File(row.id, ".meta");
            std::error_code metaError;
            if (std::filesystem::exists(metadata, metaError))
                try
                {
                    ReadMeta(metadata, row);
                }
                catch (const std::exception &e)
                {
                    snapshot->error = e.what();
                    auto backup = metadata;
                    backup += ".bak";
                    try
                    {
                        ReadMeta(backup, row);
                    }
                    catch (...)
                    {
                    }
                }
            snapshot->byId.emplace(row.id, snapshot->runs.size());
            snapshot->runs.push_back(std::move(row));
        }
        std::atomic_store(&state, std::shared_ptr<const ReplayArchiveState>(snapshot));
    }

    void Scan()
    {
        Publish(true);
        std::map<std::filesystem::path, Cached> next;
        for (const auto &directory : {source, root})
        {
            if (directory.empty() || !std::filesystem::exists(directory))
                continue;
            for (const auto &entry : std::filesystem::directory_iterator(directory))
            {
                if (!entry.is_regular_file() || entry.path().extension() != ".isrun")
                    continue;
                try
                {
                    auto modified = entry.last_write_time();
                    auto size = entry.file_size();
                    auto old = cache.find(entry.path());
                    if (old != cache.end() && old->second.size == size && old->second.modified == modified)
                    {
                        next.emplace(entry.path(), old->second);
                        continue;
                    }
                    std::string error;
                    auto data = DecodeMap(Read(entry.path(), MapCapacity - 64), error);
                    if (!data || !data->review)
                        throw std::runtime_error(error.empty() ? "File contains no replay" : error);
                    const auto &r = *data->review;
                    auto expected = "run-" + std::to_string(r.id) + ".isrun";
                    if (entry.path().filename() != expected)
                        throw std::runtime_error("Replay filename and identity differ");
                    ReplayInfo info;
                    info.id = r.id;
                    info.scene = data->scene;
                    info.seed = data->seed;
                    info.runCategory = r.category;
                    info.selectedSeed = r.selectedSeed;
                    info.rawSeed = r.rawSpawnSeed;
                    info.protectedRecording = std::filesystem::exists(entry.path().parent_path() /
                        (entry.path().stem().string() + ".keep"));
                    info.outcome = r.outcome;
                    info.seconds = r.seconds;
                    info.valid = r.valid;
                    info.reason = r.validityReason;
                    info.saved = directory == root;
                    info.file = entry.path();
                    info.name = ReplayName(*data);
                    auto profile = entry.path(); profile.replace_extension(".capture.json");
                    if (std::filesystem::exists(profile))
                        try
                        {
                            auto bytes = Read(profile, 16384);
                            info.captureCpu = ParseCaptureProfile(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
                            info.hasCaptureCpu = true;
                        }
                        catch (const std::exception &e) { problem = "Capture CPU diagnostic: " + std::string(e.what()); }
                    next.emplace(entry.path(), Cached{size, modified, std::move(info)});
                }
                catch (const std::exception &e)
                {
                    problem = entry.path().filename().string() + ": " + std::string(e.what());
                }
            }
        }
        cache = std::move(next);
        Publish();
    }

    static bool SameRecording(const MapData &a, const MapData &b)
    {
        if (!a.review || !b.review || a.scene != b.scene || a.seed != b.seed || a.hash != b.hash ||
            a.spawn != b.spawn || a.keep != b.keep || a.selected != b.selected ||
            a.capturedUtc != b.capturedUtc || a.spawns.size() != b.spawns.size() ||
            a.points.size() != b.points.size())
            return false;
        auto vectors = [](const auto &x, const auto &y) {
            return x.size() == y.size() &&
                   (x.empty() || std::memcmp(x.data(), y.data(), x.size() * sizeof(Vec3)) == 0);
        };
        if (!vectors(a.spawns, b.spawns) || !vectors(a.points, b.points))
            return false;
        const auto &x = *a.review;
        const auto &y = *b.review;
        if (x.id != y.id || x.outcome != y.outcome || x.seconds != y.seconds || x.valid != y.valid ||
            x.validationChecked != y.validationChecked || x.observedFromStart != y.observedFromStart ||
            x.seedKnown != y.seedKnown || x.seedKind != y.seedKind || x.aids != y.aids ||
            x.category != y.category || x.selectedSeed != y.selectedSeed || x.runSeed != y.runSeed ||
            x.rawSpawnSeed != y.rawSpawnSeed || x.initialAids != y.initialAids ||
            x.warnings != y.warnings || x.validityReason != y.validityReason || x.reduced != y.reduced ||
            x.monstersReduced != y.monstersReduced || x.lightSources != y.lightSources ||
            x.lightRejected != y.lightRejected || x.collected != y.collected ||
            x.pickupSeconds != y.pickupSeconds || x.trace.size() != y.trace.size() ||
            x.monsters.size() != y.monsters.size() || x.flashes.size() != y.flashes.size() ||
            bool(x.motion) != bool(y.motion))
            return false;
        for (size_t i = 0; i < x.trace.size(); i++)
            if (x.trace[i].seconds != y.trace[i].seconds || Distance(x.trace[i].point, y.trace[i].point) != 0)
                return false;
        for (size_t i = 0; i < x.monsters.size(); i++)
        {
            const auto &m = x.monsters[i];
            const auto &n = y.monsters[i];
            if (m.id != n.id || m.kind != n.kind || m.trace.size() != n.trace.size() ||
                m.paths.size() != n.paths.size())
                return false;
            for (size_t j = 0; j < m.trace.size(); j++)
                if (m.trace[j].seconds != n.trace[j].seconds ||
                    m.trace[j].continuous != n.trace[j].continuous ||
                    Distance(m.trace[j].point, n.trace[j].point) != 0)
                    return false;
            for (size_t j = 0; j < m.paths.size(); j++)
                if (m.paths[j].seconds != n.paths[j].seconds ||
                    !vectors(m.paths[j].corners, n.paths[j].corners))
                    return false;
        }
        for (size_t i = 0; i < x.flashes.size(); i++)
        {
            const auto &m = x.flashes[i];
            const auto &n = y.flashes[i];
            if (m.seconds != n.seconds || m.kind != n.kind || m.actor != n.actor ||
                Distance(m.player, n.player) != 0)
                return false;
        }
        if (x.motion)
        {
            const auto &m = *x.motion;
            const auto &n = *y.motion;
            if (m.frames != n.frames || m.ticks != n.ticks || m.events != n.events || m.flags != n.flags ||
                m.dropped != n.dropped || m.tool != n.tool || m.game != n.game || m.unity != n.unity)
                return false;
        }
        return true;
    }

    std::shared_ptr<const MapData> Canonical(const MapData &d) const
    {
        auto name = "run-" + std::to_string(d.review->id) + ".isrun";
        for (const auto &folder : {root, source})
            if (!folder.empty() && std::filesystem::exists(folder / name))
            {
                std::string error;
                auto original = DecodeMap(Read(folder / name, MapCapacity - 64), error);
                if (original && SameRecording(d, *original))
                    return original;
            }
        return {};
    }

    std::filesystem::path Companion(const MapData &d, const char *extension) const
    {
        std::string name = "run-" + std::to_string(d.review->id) + extension;
        for (const auto &folder : {root, source})
            if (!folder.empty())
            {
                auto map = folder / ("run-" + std::to_string(d.review->id) + ".isrun");
                auto p = folder / name;
                if (!std::filesystem::exists(map) || !std::filesystem::exists(p))
                    continue;
                std::string error;
                auto original = DecodeMap(Read(map, MapCapacity - 64), error);
                if (original && SameRecording(d, *original))
                    return p;
            }
        return {};
    }

    std::shared_ptr<const ReplayMotion> ReadMotion(const MapData &d, bool fresh = false)
    {
        if (!d.review || !d.review->motion)
            return {};
        auto current = std::atomic_load(&motion);
        if (!fresh && current && current->Matches(d) &&
            (!current->experience.audio.recorded || !current->audioFile.empty()))
            return current;
        auto file = Companion(d, ".isinput");
        if (file.empty())
            throw std::runtime_error("Input recording is missing or still being saved");
        std::string error;
        auto value = DecodeMotion(Read(file, MotionCapacity), error);
        if (!value || !value->Matches(d))
            throw std::runtime_error(value ? "Input recording belongs to a different run" : error);
        // Bind optional audio only on the archive worker, before publishing.
        if (value->experience.audio.recorded)
        {
            auto writable = std::const_pointer_cast<ReplayMotion>(value);
            try
            {
                auto audio = Companion(d, ".isaudio");
                if (audio.empty())
                    throw std::runtime_error("Recorded audio is missing");
                writable->experience.audio = VerifyRecordedAudio(audio, value->id, value->scene, value->seed,
                                                                 value->hash, &value->experience.audio);
                writable->audioFile = audio;
            }
            catch (const std::exception &e)
            {
                writable->audioError = e.what();
            }
        }
        RecordingCheckFor(*value);
        return value;
    }

    static void CopyAudio(const ReplayMotion &input, const std::filesystem::path &destination)
    {
        if (!input.experience.audio.recorded)
            return;
        std::filesystem::create_directories(destination.parent_path());
        if (input.audioFile.empty())
            throw std::runtime_error("Recorded audio companion is missing or damaged");
        if (std::filesystem::exists(destination))
        {
            VerifyRecordedAudio(destination, input.id, input.scene, input.seed, input.hash,
                                &input.experience.audio);
            return;
        }
        auto temporary = destination;
        temporary += ".tmp-audio";
        bool own = false;
        try
        {
            std::filesystem::copy_file(input.audioFile, temporary);
            own = true;
            VerifyRecordedAudio(temporary, input.id, input.scene, input.seed, input.hash,
                                &input.experience.audio);
#ifdef _WIN32
            if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("Cannot commit replay audio; existing file preserved");
#else
            std::filesystem::create_hard_link(temporary, destination);
            std::filesystem::remove(temporary);
#endif
        }
        catch (...)
        {
            if (own)
            {
                std::error_code ec;
                std::filesystem::remove(temporary, ec);
            }
            throw;
        }
    }

    ReplayFiles Package(const MapData &d)
    {
        if (!d.review || d.bytes.empty())
            throw std::runtime_error("No recorded run to export");
        auto prefix = "run-" + std::to_string(d.review->id);
        ReplayFiles files;
        auto originalMap = Canonical(d);
        files[prefix + ".isrun"] = originalMap ? originalMap->bytes : d.bytes;
        auto input = ReadMotion(d, true);
        if (input)
            files[prefix + ".isinput"] = input->bytes;
        if (input && input->experience.audio.recorded)
        {
            if (input->audioFile.empty())
                throw std::runtime_error("Recorded audio companion is missing or damaged");
            files[prefix + ".isaudio"] = Read(input->audioFile, AudioCapacity);
        }
        auto original = Companion(d, ".validation.json");
        bool report = !original.empty();
        if (report)
            files[prefix + ".validation.json"] = Read(original, 65536);
        auto verification = Companion(d, ".verification.json");
        if (!verification.empty())
        {
            auto bytes = Read(verification, 16384);
            auto snapshot = suite_run::DecodeVerification(
                std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
            if (!snapshot.Matches(d))
                throw std::runtime_error("Run verification identity mismatch");
            files[prefix + ".verification.json"] = std::move(bytes);
        }
        auto note = File(d.review->id, ".meta");
        if (std::filesystem::exists(note))
        {
            ReplayInfo row;
            row.id = d.review->id;
            ReadMeta(note, row);
            files[prefix + ".meta"] = Read(note, 16384);
        }
        auto marks = File(d.review->id, ".ismarks");
        bool hasMarks = std::filesystem::exists(marks);
        if (hasMarks)
        {
            auto bytes = Read(marks, 65536);
            auto a = DecodeAnnotations(bytes);
            if (!a->Matches(d))
                throw std::runtime_error("Annotation identity mismatch");
            files[prefix + ".ismarks"] = std::move(bytes);
        }
        files["manifest.json"] = ReplayText(ReplayManifest(originalMap ? *originalMap : d, bool(input),
                                                           report, suite_brand::Version, hasMarks));
        files["README.txt"] = ReplayText(
            "PracticeSuite run package / mab\nImport this ZIP in Replay > Import package. Keep the package intact.\nInput/camera data is available only for recordings made with 7.2.0 or later.\nSHA256SUMS.txt verifies consistency, not origin or physical keyboard input.\nABORTED is an outcome; local validity is a separate observation.\nOptional .isaudio contains the recorded game mix and is bound to the input recording. New input formats include observed animation states and real game probes; missing historical channels are labelled.\n");
        std::string sums;
        for (const auto &[name, bytes] : files)
            sums += ReplaySha256(bytes) + "  " + name + "\n";
        files["SHA256SUMS.txt"] = ReplayText(sums);
        return files;
    }

    static void VerifyPackageChecksums(const ReplayFiles &files)
    {
        auto sums = files.find("SHA256SUMS.txt");
        if (sums == files.end() || sums->second.size() > 4096)
            throw std::runtime_error("Package has no checksum manifest");
        std::istringstream lines(std::string(sums->second.begin(), sums->second.end()));
        std::string line;
        std::map<std::string, bool> verified;
        while (std::getline(lines, line))
        {
            if (line.size() < 67 || line.substr(64, 2) != "  ")
                throw std::runtime_error("Invalid package checksum list");
            auto name = line.substr(66);
            auto f = files.find(name);
            if (f == files.end() || name == "SHA256SUMS.txt" || verified[name] ||
                ReplaySha256(f->second) != line.substr(0, 64))
                throw std::runtime_error("Package SHA256 mismatch");
            verified[name] = true;
        }
        if (verified.size() + 1 != files.size())
            throw std::runtime_error("Unverified package entry");
    }

    void Import(const std::filesystem::path &path)
    {
        auto files = DecodeReplayZip(Read(path, ReplayPackageCapacity));
        VerifyPackageChecksums(files);
        std::shared_ptr<const MapData> d;
        std::string error;
        for (const auto &[name, bytes] : files)
            if (std::filesystem::path(name).extension() == ".isrun")
            {
                if (d)
                    throw std::runtime_error("Package contains multiple runs");
                d = DecodeMap(bytes, error);
                if (!d || !d->review || name != "run-" + std::to_string(d->review->id) + ".isrun")
                    throw std::runtime_error("Invalid package run identity: " + error);
            }
        if (!d)
            throw std::runtime_error("Package contains no run");
        auto prefix = "run-" + std::to_string(d->review->id);
        bool hasInput = files.count(prefix + ".isinput") != 0,
             hasReport = files.count(prefix + ".validation.json") != 0;
        if (bool(d->review->motion) != hasInput)
            throw std::runtime_error("Package input recording is missing");
        if (!hasInput && files.count(prefix + ".isaudio"))
            throw std::runtime_error("Orphaned audio companion");
        if (hasInput)
        {
            auto input = DecodeMotion(std::move(files[prefix + ".isinput"]), error);
            if (!input || !input->Matches(*d))
                throw std::runtime_error("Package input identity mismatch: " + error);
            bool hasAudio = files.count(prefix + ".isaudio") != 0;
            if (hasAudio != input->experience.audio.recorded)
                throw std::runtime_error("Package audio companion does not match recording");
            if (hasAudio)
                VerifyRecordedAudio(files[prefix + ".isaudio"], input->id, input->scene, input->seed,
                                    input->hash, &input->experience.audio);
            files[prefix + ".isinput"] = input->bytes;
        }
        if (files.count(prefix + ".verification.json"))
        {
            const auto &bytes = files.at(prefix + ".verification.json");
            auto snapshot = suite_run::DecodeVerification(
                std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
            if (!snapshot.Matches(*d))
                throw std::runtime_error("Package verification identity mismatch");
        }
        bool hasMarks = files.count(prefix + ".ismarks") != 0;
        if (hasMarks)
        {
            auto a = DecodeAnnotations(files[prefix + ".ismarks"]);
            if (!a->Matches(*d))
                throw std::runtime_error("Package annotation identity mismatch");
        }
        auto manifest = files.find("manifest.json");
        if (manifest == files.end() ||
            !ReplayManifestMatches(manifest->second, *d, hasInput, hasReport, hasMarks))
            throw std::runtime_error("Package manifest identity mismatch");
        for (const auto &[name, bytes] : files)
        {
            (void)bytes;
            if (name != prefix + ".isrun" && name != prefix + ".isinput" && name != prefix + ".isaudio" &&
                name != prefix + ".validation.json" && name != prefix + ".verification.json" &&
                name != prefix + ".meta" && name != prefix + ".ismarks" && name != "manifest.json" &&
                name != "SHA256SUMS.txt" && name != "README.txt")
                throw std::runtime_error("Unexpected package file");
        }
        // Preflight identity conflicts before committing any file.
        for (const auto &[name, bytes] : files)
            if (name.rfind(prefix + ".", 0) == 0)
            {
                auto dest = root / name;
                if (std::filesystem::exists(dest) &&
                    Read(dest, name.ends_with(".isaudio") ? AudioCapacity : MotionCapacity) != bytes)
                    throw std::runtime_error("Existing recording preserved; package identity conflict");
            }
        if (files.count(prefix + ".meta"))
        {
            ReplayInfo row;
            row.id = d->review->id;
            auto temp = root / "import-notes.tmp";
            Atomic(temp, files[prefix + ".meta"], true, false);
            try
            {
                ReadMeta(temp, row);
            }
            catch (...)
            {
                std::filesystem::remove(temp);
                throw;
            }
            std::filesystem::remove(temp);
        }
        for (const auto &[name, bytes] : files)
            if (name.rfind(prefix + ".", 0) == 0 && name != prefix + ".isrun")
                Atomic(root / name, bytes, false);
        Atomic(File(d->review->id, ".isrun"), d->bytes, false);
        savedRun = d->review->id;
        savedRevision++;
        Scan();
    }

    void InitializeLibrary(const Job &job)
    {
        root = job.path;
        std::filesystem::create_directories(root);
        auto config = root / "source.txt";
        if (std::filesystem::exists(config))
        {
            auto b = Read(config, 32768);
            source = std::filesystem::path(std::u8string(b.begin(), b.end()));
        }
        favorites.clear();
        auto favoriteFile = root / "favorites.json";
        if (std::filesystem::exists(favoriteFile))
            try
            {
                auto bytes = Read(favoriteFile, 1024 * 1024);
                auto json = suite_update::JsonReader(
                                std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()))
                                .Read();
                if (json.kind != suite_update::Json::Kind::Array)
                    throw std::runtime_error("Replay favorites are unreadable");
                for (const auto &value : json.items)
                {
                    auto id = value.UInt();
                    if (!id || id > uint64_t(INT64_MAX))
                        throw std::runtime_error("Replay favorite identity is invalid");
                    favorites.insert(int64_t(id));
                }
            }
            catch (const std::exception &)
            {
                favorites.clear();
                problem = FavoriteWarning;
            }
        Scan();
    }

    void SetFavorite(const Job &job)
    {
        auto snapshot = Get();
        if (std::none_of(snapshot->runs.begin(), snapshot->runs.end(),
                         [&](const auto &row) { return row.id == job.id; }))
            throw std::runtime_error("Replay no longer available");
        auto next = favorites;
        if (job.favorite)
            next.insert(job.id);
        else
            next.erase(job.id);
        std::string json = "[";
        for (auto id : next)
        {
            if (json.size() > 1)
                json += ',';
            json += std::to_string(id);
        }
        json += "]\n";
        if (json.size() > 1024 * 1024)
            throw std::runtime_error("Replay favorites exceed the library limit");
        Atomic(root / "favorites.json", ReplayText(json), true);
        favorites.swap(next);
        // A preference change does not need to reread every replay's metadata.
        auto updated = std::make_shared<ReplayArchiveState>(*snapshot);
        for (auto &row : updated->runs)
            if (row.id == job.id)
                row.favorite = job.favorite;
        if (updated->error == FavoriteWarning)
            updated->error.clear();
        std::atomic_store(&state, std::shared_ptr<const ReplayArchiveState>(updated));
    }

    void ChangeSourceDirectory(const Job &job)
    {
        source = job.path;
        stagedRun = 0;
        auto text = source.u8string();
        Atomic(root / "source.txt", std::vector<uint8_t>(text.begin(), text.end()), true);
        Scan();
    }

    void LoadRecording(const Job &job)
    {
        auto snapshot = Get();
        auto row = std::find_if(snapshot->runs.begin(), snapshot->runs.end(),
                                [&](const auto &r) { return r.id == job.id; });
        if (row == snapshot->runs.end())
            throw std::runtime_error("Replay no longer available");
        std::string error;
        auto data = DecodeMap(Read(row->file, MapCapacity - 64), error);
        if (!data || !data->review || data->review->id != job.id)
            throw std::runtime_error(error);
        std::atomic_store(&loaded, data);
    }

    void SaveRecording(const Job &job)
    {
        if (!job.data || !job.data->review || job.data->bytes.empty())
            throw std::runtime_error("Replay contains no saved recording");
        ReplayInfo row;
        row.id = job.data->review->id;
        row.name = job.name;
        row.category = job.category;
        row.notes = job.notes;
        auto metadata = Meta(row);
        if (root.empty())
            throw std::runtime_error("Replay directory unavailable");
        auto original = Canonical(*job.data);
        const auto &bytes = original ? original->bytes : job.data->bytes;
        if (std::filesystem::exists(File(row.id, ".isrun")) &&
            Read(File(row.id, ".isrun"), MapCapacity - 64) != bytes)
            throw std::runtime_error("Existing replay preserved; identity conflict");
        auto input = ReadMotion(*job.data);
        if (input)
        {
            CopyAudio(*input, File(row.id, ".isaudio"));
            Atomic(File(row.id, ".isinput"), input->bytes, false);
        }
        auto report = Companion(*job.data, ".validation.json");
        if (!report.empty())
            Atomic(File(row.id, ".validation.json"), Read(report, 65536), false);
        for (auto extension : {".keep", ".capture.json"})
        {
            auto companion = Companion(*job.data, extension);
            if (!companion.empty()) Atomic(File(row.id, extension), Read(companion, 16384), false);
        }
        auto verification = Companion(*job.data, ".verification.json");
        if (!verification.empty())
            Atomic(File(row.id, ".verification.json"), Read(verification, 16384), false);
        Atomic(File(row.id, ".isrun"), bytes, false);
        auto target = File(row.id, ".meta");
        bool preserve = false;
        if (std::filesystem::exists(target))
            try
            {
                auto previous = row;
                ReadMeta(target, previous);
            }
            catch (...)
            {
                preserve = true;
                auto damaged = target;
                auto damagedBytes = Read(target, 16384);
                damaged += ".corrupt-" + std::to_string(ReplayHash(damagedBytes, damagedBytes.size()));
                std::filesystem::copy_file(target, damaged, std::filesystem::copy_options::skip_existing);
            }
        Atomic(target, metadata, true, !preserve);
        savedRun = row.id;
        savedRevision++;
        Scan();
    }

    void ExportRecording(const Job &job)
    {
        auto files = Package(*job.data);
        auto p = root / "Exports" / ("run-" + std::to_string(job.data->review->id) + ".zip");
        auto bytes = EncodeReplayZip(files);
        if (std::filesystem::exists(p) && Read(p, ReplayPackageCapacity) != bytes)
        {
            auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
            p = root / "Exports" /
                ("run-" + std::to_string(job.data->review->id) + "-" + std::to_string(stamp) + ".zip");
        }
        Atomic(p, bytes, false);
        exportedRun = job.data->review->id;
        exportPath = p.string();
        Publish();
    }

    void LoadRecordedMotion(const Job &job)
    {
        motionRun = job.data->review->id;
        motionError.clear();
        std::atomic_store(&motion, std::shared_ptr<const ReplayMotion>{});
        Publish(true);
        auto value = ReadMotion(*job.data);
        std::atomic_store(&motion, value);
        Publish();
    }

    void RevalidateRecording(const Job &job)
    {
        auto result = std::make_shared<ReplayRevalidation>();
        result->request = job.request;
        result->run = job.data->review->id;
        result->checkedUtc = int64_t(std::time(nullptr));
        result->originalChecked = job.data->review->validationChecked;
        result->originalValid = job.data->review->valid;
        result->originalAids = job.data->review->aids;
        result->originalWarnings = job.data->review->warnings;
        result->originalReason = job.data->review->validityReason;
        result->verification =
            std::make_shared<suite_run::Verification>(suite_run::HistoricalVerification(*job.data));
        Publish(true);
        try
        {
            auto original = Canonical(*job.data);
            if (!original)
                throw std::runtime_error(
                    "Original replay is missing, damaged or differs from the selected run");
            result->mapSha256 = ReplaySha256(original->bytes);
            auto snapshotFile = Companion(*original, ".verification.json");
            if (!snapshotFile.empty())
            {
                auto bytes = Read(snapshotFile, 16384);
                auto snapshot = suite_run::DecodeVerification(
                    std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
                if (!snapshot.Matches(*original))
                    throw std::runtime_error("Verification snapshot disagrees with replay");
                result->verificationSha256 = ReplaySha256(bytes);
                result->verification = std::make_shared<suite_run::Verification>(std::move(snapshot));
            }
            auto report = Companion(*original, ".validation.json");
            if (!report.empty())
            {
                auto bytes = Read(report, 65536);
                result->reportIncluded = true;
                result->reportSha256 = ReplaySha256(bytes);
                suite_update::Json json;
                try
                {
                    json = suite_update::JsonReader(
                               std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()))
                               .Read();
                }
                catch (const std::exception &)
                {
                    throw std::runtime_error("Original validation report contains invalid JSON");
                }
                if (json.At("runId").UInt() != uint64_t(result->run))
                    throw std::runtime_error("Original validation report belongs to a different run");
                if (json.At("reportFormat").UInt() == 1)
                {
                    const auto &run = *original->review;
                    double seconds = std::stod(json.At("observedSeconds").text);
                    if (json.At("scene").UInt() != uint64_t(original->scene) || !std::isfinite(seconds) ||
                        std::abs(seconds - run.seconds) > .000001 ||
                        !json.At("validationChecked").IsBool(run.validationChecked) ||
                        !json.At("observedFromStart").IsBool(run.observedFromStart) ||
                        json.At("gameAssemblyFileSha256").String() != original->hash ||
                        json.At("integrity").String() != (run.valid ? "VALID (LOCAL)" : "UNVALID"))
                        throw std::runtime_error(
                            "Original report disagrees with the recorded run observations");
                    // Older format-1 reports have no category declaration.
                    // Every v15 recording, or report with new category fields,
                    // requires the whole declaration and the recorded latch.
                    int32_t mapFormat = 0;
                    if (original->bytes.size() >= 8)
                        std::memcpy(&mapFormat, original->bytes.data() + 4, sizeof(mapFormat));
                    bool categoryMetadata = json.fields.count("category") ||
                                            json.fields.count("selectedSeed") ||
                                            json.fields.count("seedRecipe");
                    if (mapFormat >= 15 || categoryMetadata)
                    {
                        auto integerMatches = [&](const char *field, int expected) {
                            const auto &value = json.At(field);
                            int decoded = 0;
                            auto [end, error] = std::from_chars(
                                value.text.data(), value.text.data() + value.text.size(), decoded);
                            return value.kind == suite_update::Json::Kind::Number &&
                                   error == std::errc{} && end == value.text.data() + value.text.size() &&
                                   decoded == expected;
                        };
                        const char *category = run.category == 1 ? "SET-SEED" :
                                               run.category == 2 ? "PRACTICE" : "RANDOM SEED";
                        const char *recipe = run.rawSpawnSeed ? "RAW-FIRST-SPAWN-V1" : "DERIVED-V1";
                        const char *seedType = run.seedKind == 2 ? "SET" :
                                               run.seedKind == 1 ? "RANDOM" : "UNKNOWN";
                        bool seedMatches = run.seedKnown ? integerMatches("seed", run.runSeed) :
                                           json.fields.count("seed") &&
                                               json.At("seed").kind == suite_update::Json::Kind::Null;
                        if (mapFormat < 15 || json.At("category").String() != category ||
                            !integerMatches("selectedSeed", run.selectedSeed) ||
                            json.At("seedRecipe").String() != recipe ||
                            json.At("seedType").String() != seedType || !seedMatches)
                            throw std::runtime_error(
                                "Original report disagrees with the recorded category or seed recipe");
                    }
                    result->reportChecked = true;
                    if (!result->verification->full)
                        result->verification = std::make_shared<suite_run::Verification>(
                            suite_run::HistoricalVerification(*original, &json));
                }
            }
            auto input = ReadMotion(*original, true);
            if (!input)
                throw std::runtime_error("This replay has no input recording to recheck");
            result->inputSha256 = ReplaySha256(input->bytes);
            result->coverage = RecordingCheckFor(*input);
            if (input->experience.audio.recorded)
            {
                if (!input->audioError.empty())
                    throw std::runtime_error(input->audioError);
                result->audioSha256 = ReplayFileSha256(input->audioFile, AudioCapacity);
            }
            // Replace cached motion only after all companions have been read.
            // This never rewrites the run's original verdict or evidence.
            std::atomic_store(&motion, input);
            motionRun = result->run;
            motionError.clear();
            result->filesOk = true;
        }
        catch (const std::exception &e)
        {
            result->error = e.what();
        }
        revalidation = result;
        Publish();
    }

    void StageRecording3D(const Job &job)
    {
        stagedRun = 0;
        Publish(true);
        if (source.empty())
            throw std::runtime_error("Connect to the game before opening 3D replay");
        auto input = ReadMotion(*job.data);
        if (!input || input->frames.empty())
            throw std::runtime_error("This replay has no camera recording");
        auto folder = source / "ReplayInbox";
        auto prefix = "run-" + std::to_string(input->id);
        auto original = Canonical(*job.data);
        if (!input->audioFile.empty())
            CopyAudio(*input, folder / (prefix + ".isaudio"));
        Atomic(folder / (prefix + ".isinput"), input->bytes, false);
        Atomic(folder / (prefix + ".isrun"), original ? original->bytes : job.data->bytes, false);
        stagedRun = input->id;
        Publish();
    }

    void LoadRecordedAnnotations(const Job &job)
    {
        annotationRun = job.data->review->id;
        annotationError.clear();
        std::atomic_store(&annotations, std::shared_ptr<const ReplayAnnotations>{});
        Publish(true);
        auto p = File(annotationRun, ".ismarks");
        std::shared_ptr<const ReplayAnnotations> a;
        if (std::filesystem::exists(p))
        {
            try
            {
                a = DecodeAnnotations(Read(p, 65536));
            }
            catch (const std::exception &e)
            {
                annotationError = e.what();
                auto backup = p;
                backup += ".bak";
                a = DecodeAnnotations(Read(backup, 65536));
            }
        }
        else
        {
            auto fresh = std::make_shared<ReplayAnnotations>();
            fresh->run = annotationRun;
            fresh->duration = job.data->review->seconds;
            a = fresh;
        }
        if (!a->Matches(*job.data))
            throw std::runtime_error("Annotation identity mismatch");
        std::atomic_store(&annotations, a);
        annotationRevision++;
        Publish();
    }

    void SaveRecordedAnnotations(const Job &job)
    {
        if (!job.annotations || !job.annotations->Matches(*job.data))
            throw std::runtime_error("Annotation identity mismatch");
        auto original = Canonical(*job.data);
        if (!original)
            throw std::runtime_error("Retain the recording before saving markers");
        auto bytes = EncodeAnnotations(*job.annotations);
        auto p = File(job.annotations->run, ".ismarks");
        bool backup = true;
        if (std::filesystem::exists(p))
            try
            {
                DecodeAnnotations(Read(p, 65536));
            }
            catch (...)
            {
                backup = false;
                auto damaged = p;
                damaged += ".corrupt";
                std::filesystem::copy_file(p, damaged, std::filesystem::copy_options::overwrite_existing);
            }
        Atomic(p, bytes, true, backup);
        annotationRun = job.annotations->run;
        annotationError.clear();
        std::atomic_store(&annotations, job.annotations);
        annotationRevision++;
        Publish();
    }

    void Process(const Job &job)
    {
        switch (job.kind)
        {
        case JobKind::Initialize:
            InitializeLibrary(job);
            break;
        case JobKind::Refresh:
            Scan();
            break;
        case JobKind::ChangeSource:
            ChangeSourceDirectory(job);
            break;
        case JobKind::LoadRecording:
            LoadRecording(job);
            break;
        case JobKind::MatchReference:
        {
            auto answer = std::make_shared<MatchedReference>();
            answer->target = job.data->review->id;
            auto snapshot = Get();
            std::vector<const ReplayInfo *> candidates;
            for (const auto &row : snapshot->runs)
                if (row.id != answer->target && row.outcome == 1 && row.runCategory == 1 &&
                    row.scene == job.data->scene && row.seed == job.data->seed &&
                    row.selectedSeed == job.data->review->selectedSeed && row.rawSeed == job.data->review->rawSpawnSeed)
                    candidates.push_back(&row);
            std::sort(candidates.begin(), candidates.end(), [](auto a, auto b) {
                if (a->valid != b->valid) return a->valid;
                return a->seconds != b->seconds ? a->seconds < b->seconds : a->id < b->id;
            });
            for (const auto *row : candidates)
            {
                try
                {
                    std::string error;
                    auto data = DecodeMap(Read(row->file, MapCapacity - 64), error);
                    if (data && data->review && data->review->id == row->id &&
                        data->review->outcome == 1 && !data->review->collected.empty() &&
                        data->review->collected.size() == data->review->pickupSeconds.size() &&
                        MatchingSplitLayout(*job.data, *data))
                    { answer->data = std::move(data); break; }
                }
                catch (const std::exception &) { /* Next intact compatible recording. */ }
            }
            auto wanted = std::atomic_load(&matchedReference);
            if (wanted->target == answer->target)
                std::atomic_store(&matchedReference, std::shared_ptr<const MatchedReference>(answer));
            break;
        }
        case JobKind::LoadReference:
        {
            if (!job.id) { std::atomic_store(&splitReference, std::shared_ptr<const MapData>{}); break; }
            auto snapshot = Get(); auto row = snapshot->byId.find(job.id);
            if (row == snapshot->byId.end()) throw std::runtime_error("Split reference recording is unavailable");
            std::string error;
            auto data = DecodeMap(Read(snapshot->runs[row->second].file, MapCapacity - 64), error);
            if (!data || !data->review || data->review->id != job.id)
                throw std::runtime_error("Split reference could not be read: " + error);
            std::atomic_store(&splitReference, data);
            break;
        }
        case JobKind::SaveRecording:
            SaveRecording(job);
            break;
        case JobKind::Favorite:
            SetFavorite(job);
            break;
        case JobKind::ExportPackage:
            ExportRecording(job);
            break;
        case JobKind::SaveVerification:
            if (job.verification && job.verification->data.reviewRetained)
            {
                auto path = File(job.verification->data.reviewRunId, ".verification.json");
                if (!std::filesystem::exists(path))
                    Atomic(path, ReplayText(suite_run::EncodeVerification(*job.verification)), false);
            }
            break;
        case JobKind::Revalidate:
            RevalidateRecording(job);
            break;
        case JobKind::LoadMotion:
            LoadRecordedMotion(job);
            break;
        case JobKind::ImportPackage:
            Import(job.path);
            break;
        case JobKind::Stage3D:
            StageRecording3D(job);
            break;
        case JobKind::LoadAnnotations:
            LoadRecordedAnnotations(job);
            break;
        case JobKind::SaveAnnotations:
            SaveRecordedAnnotations(job);
            break;
        }
    }

    void Work()
    {
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif
        while (true)
        {
            Job job;
            {
                std::unique_lock lock(gate);
                wake.wait(lock, [&] { return stopping || !jobs.empty(); });
                if (jobs.empty() && stopping)
                    return;
                job = std::move(jobs.front());
                jobs.pop_front();
                if (job.kind == JobKind::Refresh)
                    scanQueued = false;
            }
            try
            {
                problem.clear();
                Process(job);
            }
            catch (const std::exception &e)
            {
                if (job.kind == JobKind::LoadAnnotations || job.kind == JobKind::SaveAnnotations)
                    annotationError = e.what();
                else if (job.kind == JobKind::LoadMotion)
                    motionError = e.what();
                else
                    problem = e.what();
                Publish();
            }
        }
    }

    void Queue(Job job)
    {
        std::lock_guard lock(gate);
        if (stopping)
            return;
        if (job.kind == JobKind::Refresh && scanQueued)
            return;
        if (job.kind == JobKind::Refresh)
            scanQueued = true;
        jobs.push_back(std::move(job));
        if (!worker.joinable())
            worker = std::thread([this] { Work(); });
        wake.notify_one();
    }

  public:
    ReplayArchive() = default;
    ReplayArchive(const ReplayArchive &) = delete;

    ~ReplayArchive()
    {
        Shutdown();
    }

    void Shutdown()
    {
        {
            std::lock_guard lock(gate);
            stopping = true;
            wake.notify_one();
        }
        if (worker.joinable())
            worker.join();
    }

    void SetDirectory(std::filesystem::path path)
    {
        Queue({.kind = JobKind::Initialize, .path = std::move(path)});
    }

    void SetSourceDirectory(std::filesystem::path path)
    {
        Queue({.kind = JobKind::ChangeSource, .path = std::move(path)});
    }

    void Refresh()
    {
        Queue({.kind = JobKind::Refresh});
    }

    void Favorite(int64_t id, bool value)
    {
        Queue({.kind = JobKind::Favorite, .id = id, .favorite = value});
    }

    void Load(int64_t id)
    {
        Queue({.kind = JobKind::LoadRecording, .id = id});
    }
    void FindMatchingReference(std::shared_ptr<const MapData> data)
    {
        if (!data || !data->review || data->review->category != 1) return;
        auto answer = std::make_shared<MatchedReference>();
        answer->target = data->review->id; answer->pending = true;
        std::atomic_store(&matchedReference, std::shared_ptr<const MatchedReference>(answer));
        Queue({.kind = JobKind::MatchReference, .data = std::move(data)});
    }
    bool ReferenceLoading(int64_t target) const
    {
        auto answer = std::atomic_load(&matchedReference);
        return answer->target == target && answer->pending;
    }
    std::shared_ptr<const MapData> MatchingReference(int64_t target) const
    {
        auto answer = std::atomic_load(&matchedReference);
        return answer->target == target ? answer->data : nullptr;
    }
    void SetReference(int64_t id) { Queue({.kind = JobKind::LoadReference, .id = id}); }
    std::shared_ptr<const MapData> Reference() const { return std::atomic_load(&splitReference); }

    void RecordVerification(const GameData &data, uint32_t session)
    {
        if (!data.reviewReady || !data.reviewRetained || !suite_run::ReplayIdentityUsable(data))
            return;
        auto snapshot = std::make_shared<suite_run::Verification>();
        snapshot->data = data;
        snapshot->session = session;
        snapshot->full = true;
        Queue({.kind = JobKind::SaveVerification, .verification = std::move(snapshot)});
    }

    uint64_t Revalidate(std::shared_ptr<const MapData> data)
    {
        if (!data || !data->review)
            return 0;
        uint64_t request = ++nextRevalidation;
        Queue({.kind = JobKind::Revalidate, .request = request, .data = std::move(data)});
        return request;
    }

    void LoadMotion(std::shared_ptr<const MapData> data)
    {
        Queue({.kind = JobKind::LoadMotion, .data = std::move(data)});
    }

    void Export(std::shared_ptr<const MapData> data)
    {
        Queue({.kind = JobKind::ExportPackage, .data = std::move(data)});
    }

    void ImportPackage(std::filesystem::path path)
    {
        Queue({.kind = JobKind::ImportPackage, .path = std::move(path)});
    }

    void Stage3D(std::shared_ptr<const MapData> data)
    {
        Queue({.kind = JobKind::Stage3D, .data = std::move(data)});
    }

    void Save(std::shared_ptr<const MapData> data, std::string name, std::string category, std::string notes)
    {
        Queue({.kind = JobKind::SaveRecording,
               .data = std::move(data),
               .name = std::move(name),
               .category = std::move(category),
               .notes = std::move(notes)});
    }

    void LoadAnnotations(std::shared_ptr<const MapData> data)
    {
        Queue({.kind = JobKind::LoadAnnotations, .data = std::move(data)});
    }

    void SaveAnnotations(std::shared_ptr<const MapData> data, std::shared_ptr<const ReplayAnnotations> value)
    {
        Queue({.kind = JobKind::SaveAnnotations, .data = std::move(data), .annotations = std::move(value)});
    }

    std::shared_ptr<const ReplayAnnotations> Annotations() const
    {
        return std::atomic_load(&annotations);
    }

    std::shared_ptr<const ReplayArchiveState> Get() const
    {
        return std::atomic_load(&state);
    }

    std::shared_ptr<const MapData> Loaded() const
    {
        return std::atomic_load(&loaded);
    }

    std::shared_ptr<const ReplayMotion> Motion() const
    {
        return std::atomic_load(&motion);
    }
};

inline ReplayArchive g_replayArchive;
} // namespace practice
