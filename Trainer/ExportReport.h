#pragma once
#include "ClipModel.h"
#include "ReplayAudioIO.h"
#include "SuiteBranding.h"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <atomic>
#include <memory>
namespace suite_clip {
struct ExportPipelineStats {
    std::atomic<uint64_t> sceneFrames{0},asyncSceneFrames{0},compositionFrames{0},peakInFlight{0};
    std::atomic<uint64_t> sceneRenderMicros{0},sceneReadbackMicros{0},sceneTransferMicros{0},composeMicros{0},compositionReadbackMicros{0};
    static uint64_t Micros(double ms){return std::isfinite(ms)&&ms>0?uint64_t(std::min(ms,60000.)*1000):0;}
};
struct ExportSource {
    int64_t run = 0;
    int seed = 0, scene = 0;
    std::string hash;
    std::filesystem::path audioFile;
    practice::ReplayAudioInfo audio;
    std::shared_ptr<ExportPipelineStats> pipeline;
};
inline void WriteExportReport(const std::filesystem::path &output, const Options &o, ExportSource source,
                              double wallSeconds) {
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
    file << "{\n  \"version\": \"" << suite_brand::Version << "\",\n  \"run\": " << source.run
         << ",\n  \"seed\": " << source.seed << ",\n  \"format\": " << o.format
         << ",\n  \"preset\": " << o.preset << ",\n  \"width\": " << o.width
         << ",\n  \"height\": " << o.height << ",\n  \"outputFps\": " << o.fps
         << ",\n  \"frames\": " << o.Frames() << ",\n  \"expectedEncodedSeconds\": " << EncodedSeconds(o)
         << ",\n  \"sourceStart\": " << o.a << ",\n  \"sourceEnd\": " << o.b
         << ",\n  \"defaultPlaybackSpeed\": " << o.rate
         << ",\n  \"speedKeypointsApplied\": " << ((o.speedKeys && o.timing) ? "true" : "false")
         << ",\n  \"firstSourceTime\": " << o.At(0) << ",\n  \"lastSourceTime\": " << o.At(o.Frames() - 1)
         << ",\n  \"sizeLimitBytes\": " << o.maxFileBytes
         << ",\n  \"actualBytes\": " << (o.format == 3 ? 0 : std::filesystem::file_size(output))
         << ",\n  \"exportWallSeconds\": " << wallSeconds << ",\n  \"audio\": " << (o.AudioEnabled()?"true":"false") << ",\n  \"audioSourceFlags\": " << source.audio.flags << ",\n  \"audioSourceGaps\": " << source.audio.gaps << ",\n  \"audioOutputRate\": " << (o.AudioEnabled()?48000:0);
    if(source.pipeline){const auto& p=*source.pipeline;file<<",\n  \"pipelineSlots\": 3,\n  \"peakFramesInFlight\": "<<p.peakInFlight.load()
        <<",\n  \"sceneFrames\": "<<p.sceneFrames.load()<<",\n  \"asyncSceneFrames\": "<<p.asyncSceneFrames.load()
        <<",\n  \"sceneRenderMilliseconds\": "<<p.sceneRenderMicros.load()/1000.<<",\n  \"sceneReadbackLatencyMilliseconds\": "<<p.sceneReadbackMicros.load()/1000.
        <<",\n  \"sceneTransferMilliseconds\": "<<p.sceneTransferMicros.load()/1000.<<",\n  \"compositionFrames\": "<<p.compositionFrames.load()
        <<",\n  \"compositionMilliseconds\": "<<p.composeMicros.load()/1000.<<",\n  \"compositionReadbackLatencyMilliseconds\": "<<p.compositionReadbackMicros.load()/1000.;}
    file<<"\n}\n";
}
} // namespace suite_clip
