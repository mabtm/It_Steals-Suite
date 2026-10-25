#pragma once
#include "ClipModel.h"
#include "ReplayAudioIO.h"
#include "ReplayHealth.h"
#include "SuiteBranding.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <atomic>
#include <memory>
#include <functional>
#include <cstring>
#include <mutex>

namespace suite_clip
{
struct AviTiming
{
    uint32_t frames = 0, rate = 0, scale = 0;
    double audioStart = 0, audioSeconds = 0;
    bool audio = false;

    double Fps() const
    {
        return scale ? double(rate) / scale : 0;
    }

    double Seconds() const
    {
        return rate ? double(frames) * scale / rate : 0;
    }
};

inline AviTiming ReadAviTiming(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    std::vector<char> bytes(1024 * 1024);
    file.read(bytes.data(), std::streamsize(bytes.size()));
    bytes.resize(size_t(file.gcount()));
    auto word = [&](size_t at) {
        if (at + 4 > bytes.size())
            throw std::runtime_error("Incomplete AVI timing header");
        uint32_t value = 0;
        for (int i = 0; i < 4; i++)
            value |= uint32_t(uint8_t(bytes[at + i])) << (i * 8);
        return value;
    };
    auto tag = [&](size_t at, const char *value) {
        return at + 4 <= bytes.size() && std::memcmp(bytes.data() + at, value, 4) == 0;
    };
    if (!tag(0, "RIFF") || !tag(8, "AVI "))
        throw std::runtime_error("Invalid AVI container header");
    AviTiming result;
    uint32_t openDmlFrames = 0;
    std::function<void(size_t, size_t, int)> scan = [&](size_t begin, size_t end, int depth) {
        if (depth > 4)
            throw std::runtime_error("Invalid AVI header nesting");
        for (size_t at = begin; at + 8 <= end;)
        {
            uint32_t length = word(at + 4);
            size_t next = at + 8ull + length + (length & 1);
            if (tag(at, "LIST") && length >= 4)
            {
                if (tag(at + 8, "movi"))
                    return;
                scan(at + 12, std::min<size_t>(at + 8ull + length, end), depth + 1);
            }
            else if (tag(at, "strh") && length >= 56 && at + 64 <= end)
            {
                auto scale = word(at + 28), rate = word(at + 32), start = word(at + 36),
                     frames = word(at + 40);
                if (tag(at + 8, "vids"))
                {
                    if (result.rate || start)
                        throw std::runtime_error("Unexpected AVI video timeline");
                    result.scale = scale;
                    result.rate = rate;
                    result.frames = frames;
                }
                else if (tag(at + 8, "auds"))
                {
                    if (!rate || !scale)
                        throw std::runtime_error("Invalid AVI audio clock");
                    result.audio = true;
                    result.audioStart = double(start) * scale / rate;
                    result.audioSeconds = double(frames) * scale / rate;
                }
            }
            else if (tag(at, "dmlh") && length >= 4 && at + 12 <= end)
                openDmlFrames = word(at + 8);
            if (next <= at || next > end)
                break;
            at = next;
        }
    };
    scan(12, bytes.size(), 0);
    result.frames = std::max(result.frames, openDmlFrames);
    if (!result.rate || !result.scale || !result.frames)
        throw std::runtime_error("AVI video clock is missing");
    return result;
}

inline void VerifyAviTiming(const std::filesystem::path &path, const Options &options)
{
    auto timing = ReadAviTiming(path);
    if (timing.frames != options.Frames() || std::abs(timing.Fps() - options.fps) > 1e-6 ||
        std::abs(timing.Seconds() - EncodedSeconds(options)) > 1e-6)
        throw std::runtime_error("AVI frame count or frame rate differs from the export settings");
    if (options.AudioEnabled() && (!timing.audio || timing.audioStart > 1. / 48000 ||
                                   std::abs(timing.audioSeconds - timing.Seconds()) > 2. / 48000))
        throw std::runtime_error("AVI audio and video timelines do not match");
}

struct ExportProgress
{
    uint64_t frames = 0, outputBytes = 0, trainerWorkingSet = 0, encoderWorkingSet = 0, gameWorkingSet = 0;
    double wallSeconds = 0, encoderMilliseconds = 0, inputWaitMilliseconds = 0, sceneRenderMilliseconds = 0,
           sceneTransferMilliseconds = 0;
};

struct ExportPipelineStats
{
    std::atomic<uint64_t> sceneFrames{0}, asyncSceneFrames{0}, compositionFrames{0}, peakInFlight{0};
    std::atomic<uint64_t> sceneRenderMicros{0}, sceneReadbackMicros{0}, sceneTransferMicros{0},
        composeMicros{0}, compositionReadbackMicros{0};

    std::atomic<uint64_t> gameWorkingSet{0};
    mutable std::mutex progressGate;
    std::vector<ExportProgress> progress;

    void Mark(ExportProgress point)
    {
        point.gameWorkingSet = gameWorkingSet.load();
        point.sceneRenderMilliseconds = sceneRenderMicros.load() / 1000.;
        point.sceneTransferMilliseconds = sceneTransferMicros.load() / 1000.;
        std::lock_guard lock(progressGate);
        if (progress.size() < 1024)
            progress.push_back(point);
    }

    std::vector<ExportProgress> Progress() const
    {
        std::lock_guard lock(progressGate);
        return progress;
    }

    static uint64_t Micros(double ms)
    {
        return std::isfinite(ms) && ms > 0 ? uint64_t(std::min(ms, 60000.) * 1000) : 0;
    }
};

struct ExportSource
{
    int64_t run = 0;
    int seed = 0, scene = 0;
    std::string hash;
    std::filesystem::path audioFile;
    practice::ReplayAudioInfo audio;
    std::shared_ptr<ExportPipelineStats> pipeline;
    std::shared_ptr<const practice::ReplayRevalidation> revalidation;
    bool recordedValid = false;
    uint32_t recordedWarnings = 0;
};

inline void WriteExportReport(const std::filesystem::path &output, const Options &o, ExportSource source,
                              double wallSeconds)
{
    auto path = output;
    path += L".json";
    // The video is already complete. Never replace an existing sidecar.
    if (std::filesystem::exists(path))
        return;
    std::ofstream file(path);
    if (!file)
        return;
    file.imbue(std::locale::classic());
    file << std::setprecision(17);
    const auto bytes = o.format == 3 ? uint64_t(0) : std::filesystem::file_size(output);
    file << "{\n  \"version\": \"" << suite_brand::Version
         << "\",\n  \"captureType\": \"renderedReplay\",\n  \"run\": " << source.run
         << ",\n  \"seed\": " << source.seed << ",\n  \"format\": " << o.format
         << ",\n  \"preset\": " << o.preset << ",\n  \"width\": " << o.width
         << ",\n  \"height\": " << o.height << ",\n  \"outputFps\": " << o.fps
         << ",\n  \"frames\": " << o.Frames() << ",\n  \"expectedEncodedSeconds\": " << EncodedSeconds(o)
         << ",\n  \"sourceStart\": " << o.a << ",\n  \"sourceEnd\": " << o.b
         << ",\n  \"defaultPlaybackSpeed\": " << o.rate
         << ",\n  \"speedKeypointsApplied\": " << ((o.speedKeys && o.timing) ? "true" : "false")
         << ",\n  \"firstSourceTime\": " << o.At(0) << ",\n  \"lastSourceTime\": " << o.At(o.Frames() - 1)
         << ",\n  \"sizeLimitBytes\": " << o.maxFileBytes << ",\n  \"actualBytes\": " << bytes
         << ",\n  \"averageDataMegabytesPerSecond\": " << double(bytes) / EncodedSeconds(o) / 1000000.
         << ",\n  \"exportWallSeconds\": " << wallSeconds
         << ",\n  \"audio\": " << (o.AudioEnabled() ? "true" : "false")
         << ",\n  \"audioSourceFlags\": " << source.audio.flags
         << ",\n  \"audioSourceGaps\": " << source.audio.gaps
         << ",\n  \"audioOutputRate\": " << (o.AudioEnabled() ? 48000 : 0);
    file << ",\n  \"verificationCardSeconds\": " << (o.verification ? Options::VerificationSeconds : 0)
         << ",\n  \"sourceFrames\": " << o.SourceFrames();
    if (o.verification && source.revalidation)
    {
        const auto &r = *source.revalidation;
        file
            << ",\n  \"recheckUtcUnix\": " << r.checkedUtc
            << ",\n  \"replayFilesChecked\": " << (r.filesOk ? "true" : "false")
            << ",\n  \"recordedValidLocal\": " << (source.recordedValid ? "true" : "false")
            << ",\n  \"recordedWarnings\": " << source.recordedWarnings << ",\n  \"runSha256\": \""
            << r.mapSha256 << "\""
            << ",\n  \"inputSha256\": \"" << r.inputSha256 << "\""
            << ",\n  \"audioSha256\": \"" << r.audioSha256 << "\""
            << ",\n  \"originalReportSha256\": \"" << r.reportSha256 << "\""
            << ",\n  \"verificationSnapshotSha256\": \"" << r.verificationSha256 << "\""
            << ",\n  \"recheckScope\": \"Local unsigned file checks; stored run observations stay unchanged; no authenticity or leaderboard certification\"";
        if (r.coverage)
            file << ",\n  \"coveragePartial\": " << r.coverage->partial
                 << ",\n  \"coverageMissing\": " << r.coverage->missing
                 << ",\n  \"coverageUnknown\": " << r.coverage->unknown;
    }
    if (o.format == 4)
    {
        auto timing = ReadAviTiming(output);
        file << ",\n  \"containerVideoFps\": " << timing.Fps()
             << ",\n  \"containerVideoFrames\": " << timing.frames
             << ",\n  \"containerVideoSeconds\": " << timing.Seconds()
             << ",\n  \"containerAudioSeconds\": " << timing.audioSeconds;
    }
    if (source.pipeline)
    {
        const auto &p = *source.pipeline;
        file << ",\n  \"pipelineSlots\": 3,\n  \"peakFramesInFlight\": " << p.peakInFlight.load()
             << ",\n  \"sceneFrames\": " << p.sceneFrames.load()
             << ",\n  \"asyncSceneFrames\": " << p.asyncSceneFrames.load()
             << ",\n  \"sceneRenderMilliseconds\": " << p.sceneRenderMicros.load() / 1000.
             << ",\n  \"sceneReadbackLatencyMilliseconds\": " << p.sceneReadbackMicros.load() / 1000.
             << ",\n  \"sceneTransferMilliseconds\": " << p.sceneTransferMicros.load() / 1000.
             << ",\n  \"compositionFrames\": " << p.compositionFrames.load()
             << ",\n  \"compositionMilliseconds\": " << p.composeMicros.load() / 1000.
             << ",\n  \"compositionReadbackLatencyMilliseconds\": "
             << p.compositionReadbackMicros.load() / 1000.;
        file << ",\n  \"exportProgress\": [";
        bool comma = false;
        for (const auto &point : p.Progress())
        {
            if (comma)
                file << ',';
            comma = true;
            file << "\n    {\"frames\":" << point.frames << ",\"wallSeconds\":" << point.wallSeconds
                 << ",\"encoderMilliseconds\":" << point.encoderMilliseconds
                 << ",\"inputWaitMilliseconds\":" << point.inputWaitMilliseconds
                 << ",\"sceneRenderMilliseconds\":" << point.sceneRenderMilliseconds
                 << ",\"sceneTransferMilliseconds\":" << point.sceneTransferMilliseconds
                 << ",\"trainerWorkingSetBytes\":" << point.trainerWorkingSet
                 << ",\"encoderWorkingSetBytes\":" << point.encoderWorkingSet
                 << ",\"gameWorkingSetBytes\":" << point.gameWorkingSet
                 << ",\"outputBytes\":" << point.outputBytes << '}';
        }
        file << "\n  ]";
    }
    file << "\n}\n";
}
} // namespace suite_clip
