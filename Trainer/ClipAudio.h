#pragma once
#include "ExportReport.h"
#include <atomic>

namespace suite_clip
{
inline uint64_t AudioFrames(const Options &o)
{
    return uint64_t(std::llround(EncodedSeconds(o) * 48000));
}

inline void RenderRecordedAudio(const std::filesystem::path &path, const Options &o,
                                const ExportSource &source, const std::atomic<bool> &cancel)
{
    practice::RecordedAudioReader reader(source.audioFile, source.audio, source.run, source.scene,
                                         source.seed, source.hash);
    uint64_t frames = AudioFrames(o), bytes = frames * 8;
    if (bytes > UINT32_MAX - 36)
        throw std::runtime_error("Recorded audio export exceeds the WAV size limit");
    std::ofstream file(path, std::ios::binary);
    if (!file)
        throw std::runtime_error("Cannot create temporary audio");
    auto put = [&]<class T>(T v) { file.write(reinterpret_cast<const char *>(&v), sizeof(v)); };
    file.write("RIFF", 4);
    put(uint32_t(bytes + 36));
    file.write("WAVEfmt ", 8);
    put(uint32_t(16));
    put(uint16_t(3));
    put(uint16_t(2));
    put(uint32_t(48000));
    put(uint32_t(384000));
    put(uint16_t(8));
    put(uint16_t(32));
    file.write("data", 4);
    put(uint32_t(bytes));
    std::array<float, 8192> buffer{};
    for (uint64_t n = 0; n < frames;)
    {
        if (cancel)
            throw std::runtime_error("Export cancelled");
        size_t count = size_t(std::min<uint64_t>(buffer.size() / 2, frames - n));
        for (size_t k = 0; k < count; k++)
        {
            double seconds = o.Advance(o.a, double(n + k) / 48000);
            auto stereo = seconds < o.b ? reader.Stereo(seconds * source.audio.rate) : std::array<float, 2>{};
            buffer[k * 2] = stereo[0] * o.audioVolume;
            buffer[k * 2 + 1] = stereo[1] * o.audioVolume;
        }
        file.write(reinterpret_cast<const char *>(buffer.data()), std::streamsize(count * 8));
        if (!file)
            throw std::runtime_error("Temporary audio write failed");
        n += count;
    }
}

inline std::wstring AudioCodecArguments(int format)
{
    return format == 0 || format == 6 ? L"-c:a aac -b:a 128k " : L"-c:a pcm_f32le ";
}
} // namespace suite_clip
