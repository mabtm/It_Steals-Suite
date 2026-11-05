#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "RunDataModel.h"
#include <condition_variable>
#include <mutex>
#include <thread>

namespace suite_runs
{
class Service
{
    std::mutex gate;
    std::condition_variable wake;
    std::thread worker;
    std::filesystem::path requestedDirectory;
    Query requestedQuery;
    uint64_t nextRequest = 0;
    bool changed = false, stopping = false;
    std::shared_ptr<const View> published = std::make_shared<const View>();

    struct Reader
    {
        std::filesystem::path directory;
        std::vector<Record> records;
        std::unordered_map<int64_t, uint64_t> known;
        std::shared_ptr<const std::vector<Record>> immutable = std::make_shared<const std::vector<Record>>();
        Legacy counters;
        uintmax_t offset = 0, lastSize = uintmax_t(-1), statsSize = uintmax_t(-1);
        std::filesystem::file_time_type stamp{}, statsStamp{};
        std::string tail, error, counterError;
        bool header = false, broken = false, imported = false;
        int64_t session = 0, detailedSince = 0;
        uint64_t revision = 0;

        void Reset()
        {
            records.clear(); known.clear(); offset = 0; tail.clear(); header = false;
            session = detailedSince = 0; imported = broken = false;
        }
        void Line(const std::string &line)
        {
            if (!header)
            {
                if (line != "ELITE_RUN_LEDGER|1") throw std::runtime_error("Unsupported run archive version");
                header = true; return;
            }
            if (line.starts_with("SESSION|")) { session = ParseSession(line); return; }
            auto record = Parse(line);
            auto hash = Hash(line);
            auto found = known.find(record.id);
            if (found != known.end())
            {
                if (found->second != hash) throw std::runtime_error("Conflicting run archive identity");
                return;
            }
            if (records.size() >= 1000000)
                throw std::runtime_error("Run index safety limit reached; source archive remains intact");
            known.emplace(record.id, hash);
            imported |= record.legacy;
            if (!record.legacy && (!detailedSince || record.started < detailedSince)) detailedSince = record.started;
            records.push_back(record);
        }
        bool ReadJournal()
        {
            auto file = directory / "run-ledger-v1.tsv";
            if (!std::filesystem::exists(file)) return false;
            auto size = std::filesystem::file_size(file);
            auto time = std::filesystem::last_write_time(file);
            if (size == lastSize && time == stamp) return false;
            if (size > 256u * 1024 * 1024) throw std::runtime_error("Run archive exceeds safe index size");
            if (size < offset || broken || (size == offset && time != stamp)) Reset();
            lastSize = size; stamp = time;
            std::ifstream in(file, std::ios::binary);
            if (!in) throw std::runtime_error("Run archive cannot be opened");
            if (!tail.empty() && size >= offset)
            {
                // The game may replace an uncommitted crash tail with a new
                // session marker. Detect that even when the file grew overall.
                in.seekg(std::streamoff(offset - tail.size()));
                std::string observed(tail.size(), '\0');
                in.read(observed.data(), std::streamsize(observed.size()));
                if (observed != tail) Reset();
                in.clear();
            }
            in.seekg(std::streamoff(offset));
            std::array<char, 65536> buffer;
            bool added = false;
            try
            {
                while (offset < size && in)
                {
                    in.read(buffer.data(), std::streamsize(std::min<uintmax_t>(buffer.size(), size - offset)));
                    auto count = size_t(in.gcount());
                    offset += count;
                    size_t begin = 0;
                    for (size_t i = 0; i < count; i++)
                        if (buffer[i] == '\n')
                        {
                            tail.append(buffer.data() + begin, i - begin);
                            if (tail.size() > 1024) throw std::runtime_error("Run archive line exceeds limit");
                            Line(tail); tail.clear(); begin = i + 1; added = true;
                        }
                    tail.append(buffer.data() + begin, count - begin);
                    if (tail.size() > 1024) throw std::runtime_error("Incomplete run archive line exceeds limit");
                }
                if (offset != size) throw std::runtime_error("Run archive changed during read");
                error.clear();
            }
            catch (const std::exception &e)
            { broken = true; error = e.what(); }
            if (added || broken)
            {
                immutable = std::make_shared<const std::vector<Record>>(records);
                revision++;
            }
            return added || broken;
        }
        bool ReadCounters()
        {
            auto file = directory / "speedruns-v3.txt";
            if (!std::filesystem::exists(file)) return false;
            auto size = std::filesystem::file_size(file);
            auto time = std::filesystem::last_write_time(file);
            if (size == statsSize && time == statsStamp) return false;
            statsSize = size; statsStamp = time;
            try
            {
                if (size > 2u * 1024 * 1024) throw std::runtime_error("Counter snapshot exceeds size limit");
                std::string bytes(size, '\0');
#ifdef _WIN32
                // A read must not prevent the game's atomic snapshot replace.
                HANDLE input = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                if (input == INVALID_HANDLE_VALUE) throw std::runtime_error("Counter snapshot cannot be opened");
                DWORD read = 0;
                bool ok = ReadFile(input, bytes.data(), DWORD(size), &read, nullptr) != FALSE;
                CloseHandle(input);
                if (!ok || read != size) throw std::runtime_error("Counter snapshot changed during read");
#else
                std::ifstream in(file, std::ios::binary);
                if (!in.read(bytes.data(), std::streamsize(size))) throw std::runtime_error("Counter snapshot changed during read");
#endif
                counters = ParseLegacy(bytes); counterError.clear();
            }
            catch (const std::exception &e) { counterError = e.what(); }
            revision++; return true;
        }
        std::shared_ptr<const Store> Snapshot() const
        {
            auto result = std::make_shared<Store>();
            result->records = immutable; result->legacy = counters;
            result->session = session; result->detailedSince = detailedSince;
            result->imported = imported; result->revision = revision;
            result->error = error + (error.empty() || counterError.empty() ? "" : " / ") + counterError;
            return result;
        }
    };

    void Work()
    {
        Reader reader;
        std::shared_ptr<const Store> store;
        uint64_t served = uint64_t(-1);
        for (;;)
        {
            Query query; std::filesystem::path directory; uint64_t request;
            {
                std::unique_lock lock(gate);
                wake.wait_for(lock, std::chrono::seconds(2), [&] { return stopping || changed; });
                if (stopping) return;
                directory = requestedDirectory; query = requestedQuery; request = nextRequest; changed = false;
            }
            if (directory != reader.directory)
            { reader = Reader{}; reader.directory = directory; store.reset(); }
            bool update = !store;
            try
            {
                if (!directory.empty())
                { update |= reader.ReadJournal(); update |= reader.ReadCounters(); }
            }
            catch (const std::exception &e)
            { reader.error = e.what(); update = true; }
            if (update) store = reader.Snapshot();
            if (!update && request == served) continue;
            std::shared_ptr<View> result;
            try
            {
                result = std::make_shared<View>(Build(store, query, request, [&] {
                    std::lock_guard lock(gate); return stopping || nextRequest != request;
                }));
            }
            catch (const std::exception &e)
            {
                auto failed = std::make_shared<Store>(*store); failed->error = e.what();
                result = std::make_shared<View>(); result->source = failed;
                result->query = query; result->request = request;
            }
            {
                std::lock_guard lock(gate);
                // Never publish an old directory/category under a newer query.
                if (directory == requestedDirectory && request == nextRequest)
                { published = std::move(result); served = request; }
            }
        }
    }

  public:
    Service() = default;
    ~Service() { Shutdown(); }
    Service(const Service &) = delete;
    void SetDirectory(std::filesystem::path path)
    {
        std::lock_guard lock(gate);
        if (stopping || requestedDirectory == path) return;
        requestedDirectory = std::move(path); changed = true; nextRequest++;
        if (!worker.joinable()) worker = std::thread(&Service::Work, this);
        wake.notify_one();
    }
    uint64_t Submit(const Query &query)
    {
        std::lock_guard lock(gate);
        if (stopping) return nextRequest;
        if (!worker.joinable()) { changed = true; worker = std::thread(&Service::Work, this); wake.notify_one(); }
        if (!(requestedQuery == query)) { requestedQuery = query; changed = true; nextRequest++; wake.notify_one(); }
        return nextRequest;
    }
    std::shared_ptr<const View> Get()
    {
        std::lock_guard lock(gate); return published;
    }
    void Refresh()
    {
        std::lock_guard lock(gate); changed = true; nextRequest++; wake.notify_one();
    }
    void Shutdown()
    {
        { std::lock_guard lock(gate); stopping = true; wake.notify_all(); }
        if (worker.joinable()) worker.join();
    }
};
inline Service service;
}
