#pragma once
#include "PracticeMapData.h"
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>

namespace suite_navigation_capture
{
enum class Phase { Idle, Pending, Saved, Failed };

struct Metadata
{
    std::string trainerVersion, modVersion, source, phase, step;
    int prepared = 0, total = 0;
};

struct Snapshot
{
    Phase phase = Phase::Idle;
    std::filesystem::path folder;
    std::string error;
};

// An active search's immutable source is the reproduction input. A cached map
// must never silently replace it, even while its first packet is still absent.
inline std::shared_ptr<const practice::MapData> Source(
    bool searching, std::shared_ptr<const practice::MapData> search,
    std::shared_ptr<const practice::MapData> selected)
{
    return searching ? std::move(search) : std::move(selected);
}

inline std::string OneLine(std::string text)
{
    text.resize(std::min<size_t>(text.size(), 512));
    for (char &c : text)
        if (static_cast<unsigned char>(c) < 32 || c == 127)
            c = ' ';
    return text;
}

inline std::string Report(const practice::MapData &data, const Metadata &m)
{
    std::ostringstream out;
    out << "Navigation capture\nTrainer: " << OneLine(m.trainerVersion)
        << "\nMod: " << OneLine(m.modVersion) << "\nSource: " << OneLine(m.source)
        << "\nSearch phase: " << OneLine(m.phase) << "\nPreparation: " << OneLine(m.step)
        << "\nProgress: " << m.prepared << " / " << m.total
        << "\nScene: " << data.scene << "\nScene epoch: " << data.epoch
        << "\nPacket stage: " << data.stage << "\nCapture UTC ticks: " << data.capturedUtc
        << "\nGame assembly SHA256: " << OneLine(data.hash)
        << "\nSpawns: " << data.spawns.size() << "\nCandidate points: " << data.points.size()
        << "\nMesh vertices: " << data.vertices.size() << "\nMesh triangles: " << data.indices.size() / 3
        << "\nPhysical parts: " << data.physicalParts.size()
        << "\nPhysical parts omitted: " << data.physicalOmitted
        << "\nNative navigation status: " << data.navigationStatus
        << "\nNative navigation nodes: " << data.navigationNodes.size()
        << "\nNative navigation edges: " << data.navigationEdges.size() / 2
        << "\nNative navigation anchors: " << data.navigationAnchors.size()
        << "\nPacket bytes: " << data.bytes.size()
        << "\n\nmap.ismap contains the original validated packet, including geometry and navigation.\n";
    return out.str();
}

// One bounded job, using an immutable shared packet. Validation, metadata
// formatting and all filesystem work stay off the input/render thread.
class Service
{
    struct Job
    {
        std::filesystem::path root;
        std::shared_ptr<const practice::MapData> data;
        Metadata metadata;
    };

    std::filesystem::path directory;
    std::mutex gate;
    std::condition_variable wake;
    std::thread worker;
    std::optional<Job> job;
    std::optional<Snapshot> completed;
    Snapshot state;
    bool stopping = false;

    static void Write(const std::filesystem::path &file, const char *bytes, size_t count)
    {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (!out || !out.write(bytes, std::streamsize(count)) || !out.flush())
            throw std::runtime_error("Navigation capture could not be written.");
        out.close();
        if (!out)
            throw std::runtime_error("Navigation capture could not be completed.");
    }

    static Snapshot Save(const Job &request)
    {
        Snapshot result;
        std::filesystem::path created;
        try
        {
            if (!request.data || request.data->bytes.empty())
                throw std::runtime_error("No navigation packet is available for this source.");
            std::string error;
            auto decoded = practice::DecodeMap(request.data->bytes, error);
            if (!decoded)
                throw std::runtime_error("Navigation packet validation failed: " + error);
            if (request.root.empty())
                throw std::runtime_error("Navigation capture folder is not configured.");
            std::filesystem::create_directories(request.root);
            auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count();
            auto name = "navigation-scene-" + std::to_string(decoded->scene) + "-" + std::to_string(stamp);
            for (int suffix = 0; suffix < 100; ++suffix)
            {
                auto candidate = request.root / (name + "-" + std::to_string(suffix));
                if (std::filesystem::create_directory(candidate))
                {
                    created = std::move(candidate);
                    break;
                }
            }
            if (created.empty())
                throw std::runtime_error("A unique navigation capture folder could not be created.");
            Write(created / "map.ismap", reinterpret_cast<const char *>(decoded->bytes.data()),
                  decoded->bytes.size());
            auto report = Report(*decoded, request.metadata);
            Write(created / "capture.txt", report.data(), report.size());
            result.phase = Phase::Saved;
            result.folder = created;
        }
        catch (const std::exception &error)
        {
            if (!created.empty())
            {
                std::error_code ignored;
                std::filesystem::remove_all(created, ignored);
            }
            result.phase = Phase::Failed;
            result.error = error.what();
        }
        return result;
    }

    void Work()
    {
        for (;;)
        {
            Job request;
            {
                std::unique_lock lock(gate);
                wake.wait(lock, [&] { return stopping || job.has_value(); });
                if (!job)
                    return;
                request = std::move(*job);
                job.reset();
            }
            auto result = Save(request);
            std::lock_guard lock(gate);
            completed = std::move(result);
        }
    }

  public:
    Service() = default;
    Service(const Service &) = delete;
    ~Service() { Shutdown(); }

    void SetDirectory(std::filesystem::path path)
    {
        std::lock_guard lock(gate);
        directory = std::move(path);
    }

    bool Begin(std::shared_ptr<const practice::MapData> data, Metadata metadata)
    {
        std::lock_guard lock(gate);
        if (stopping || state.phase == Phase::Pending)
            return false;
        state = {.phase = Phase::Pending};
        completed.reset();
        try
        {
            if (!worker.joinable())
                worker = std::thread(&Service::Work, this);
            job = Job{directory, std::move(data), std::move(metadata)};
            wake.notify_one();
            return true;
        }
        catch (const std::exception &error)
        {
            state = {.phase = Phase::Failed, .error = error.what()};
            return false;
        }
    }

    Snapshot View()
    {
        std::lock_guard lock(gate);
        return state;
    }

    Snapshot Poll()
    {
        std::lock_guard lock(gate);
        if (completed)
        {
            state = std::move(*completed);
            completed.reset();
        }
        return state;
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
};

inline Service service;
} // namespace suite_navigation_capture
