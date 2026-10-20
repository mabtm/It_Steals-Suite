#pragma once
#include "ClipCamera.h"
#include "ClipTiming.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <iomanip>
#include <locale>
#include <sstream>
namespace suite_clip {
struct Options {
    double a = 0, b = 0, rate = 1;
    int width = 1280, height = 720, fps = 30,
        format = 0; // Windows MP4 / GIF / FFV1 / PNG / UtVideo / ProRes / x264
    bool scene3d = false, inputs = true, speed = true, time = true, seed = true, events = true, route = true,
         monster = true, colliders = false;
    bool fullKeyboard = false, hideCeiling = false, fadeWalls = false;
    int camera = 0;
    bool transparent = false, simplified = false, fullbright = false;
    bool speedKeys = true, audio = false;
    float audioVolume = 1;
    bool AudioEnabled() const {return audio && !transparent && format!=1 && format!=3;}
    int quality = 1, preset = 9;
    uint64_t maxFileBytes = 0;
    std::shared_ptr<const SpeedMap> timing;
    void Validate(double duration) {
        if (!std::isfinite(duration) || duration <= 0 || !std::isfinite(a) || !std::isfinite(b) || a < 0 ||
            b > duration + .000001 || b - a < .02 || !std::isfinite(rate) || rate < .1 || rate > 8)
            throw std::runtime_error("Choose a valid clip start and end");
        if (width < 320 || width > 2560 || height < 180 || height > 1440 || width % 2 || height % 2 ||
            fps < 1 || fps > 120 || format < 0 || format > 6 || camera < 0 || camera > 3)
            throw std::runtime_error("Invalid clip output settings");
        if (transparent && format != 3)
            throw std::runtime_error("Transparent overlays require a PNG sequence");
        if (format == 1 && fps > 25)
            throw std::runtime_error("GIF supports up to 25 FPS");
        if (!std::isfinite(audioVolume) || audioVolume<0 || audioVolume>1)
            throw std::runtime_error("Invalid recorded audio volume");
        if (quality < 0 || quality > 2 || preset < 0 || preset > 9)
            throw std::runtime_error("Invalid export preset");
        if (maxFileBytes && (format != 6 || maxFileBytes < 256000 || maxFileBytes > 100000000000ull))
            throw std::runtime_error("File size limits require compressed MP4 / x264 "
                                     "(0.256 MB to 100 GB)");
        if (Frames() > 216000)
            throw std::runtime_error("Clip exceeds the 216,000-frame limit");
    }
    double OutputDuration() const {
        return timing && speedKeys ? timing->Output(b) - timing->Output(a) : std::max(0., b - a) / rate;
    }
    double SpeedAt(double t) const { return timing && speedKeys ? timing->Rate(t) : rate; }
    double Advance(double t, double seconds) const {
        return timing && speedKeys ? timing->Source(timing->Output(t) + seconds) : t + seconds * rate;
    }
    uint64_t Frames() const { return uint64_t(std::ceil(std::max(0., OutputDuration()) * fps - 1e-9)); }
    double At(uint64_t n) const { return std::clamp(Advance(a, double(n) / fps), a, b); }
    int64_t Stamp(uint64_t n) const { return int64_t(n * 10000000ull / uint64_t(fps)); }
};
enum Block : int { RunInfo, Keyboard, Mouse, Controller, States, Events, BlockCount };
inline constexpr const char *BlockNames[] = {"Run information", "Keyboard",        "Mouse",
                                             "Controller",      "Player activity", "Legacy events"};
struct Placement {
    float x = 0, y = 0, width = .3f, opacity = 1;
};
struct Layout {
    std::array<Placement, BlockCount> blocks{{{.015f, .018f, .97f, 1},
                                              {.30f, .81f, .28f, 1},
                                              {.615f, .77f, .105f, 1},
                                              {.025f, .73f, .19f, 1},
                                              {.755f, .77f, .23f, 1},
                                              {.79f, .77f, .19f, 1}}};
    void Validate() {
        for (size_t i = 0; i < blocks.size(); i++) {
            auto &p = blocks[i];
            for (float v : {p.x, p.y, p.width, p.opacity})
                if (!std::isfinite(v))
                    throw std::runtime_error("Invalid overlay placement");
            p.x = std::clamp(p.x, 0.f, 1.f);
            p.y = std::clamp(p.y, 0.f, 1.f);
            p.width = std::clamp(p.width, i == RunInfo ? .55f : .04f, 1.f);
            p.opacity = std::clamp(p.opacity, i == RunInfo ? .45f : 0.f, 1.f);
        }
    }
};
inline const wchar_t *Extension(int format) {
    return format == 1   ? L".gif"
           : format == 2 ? L".mkv"
           : format == 3 ? L".frames"
           : format == 4 ? L".avi"
           : format == 5 ? L".mov"
                         : L".mp4";
}
// Shared command generation for the shipped Windows encoder and native tests.
inline bool ExternalFormat(int format) { return format == 2 || format >= 4; }
inline double EncodedSeconds(const Options &o) { return double(o.Frames()) / o.fps; }
inline const char *X264Preset(int quality) {
    return quality == 0 ? "veryfast" : quality == 2 ? "slow" : "medium";
}
inline std::string StreamInputArguments(const Options &o) {
    return "-hide_banner -nostdin -loglevel error -y -filter_threads 1 -xerror "
           "-f rawvideo -pixel_format bgra "
           "-video_size " +
           std::to_string(o.width) + "x" + std::to_string(o.height) + " -framerate " + std::to_string(o.fps) +
           " -i pipe:0 -an ";
}
inline std::string FrameClockArguments(const Options &o) {
    auto fps = std::to_string(o.fps);
    return "-vf " +
           std::string(o.format == 5 || o.format == 6
                           ? "scale=in_range=full:out_range=tv:out_color_matrix=bt709,setparams=range="
                             "limited:color_primaries=bt709:color_trc=bt709:colorspace=bt709,"
                           : "") +
           "settb=1/" + fps + ",setpts=N -fps_mode cfr -r " + fps + " -enc_time_base 1:" + fps +
           " -frames:v " + std::to_string(o.Frames()) + " ";
}
inline std::string VideoCodecArguments(const Options &o) {
    auto common = FrameClockArguments(o) + "-threads 4 ";
    if (o.format == 4)
        return common + "-c:v utvideo -pix_fmt gbrap -pred left -f avi ";
    if (o.format == 5)
        return common +
               "-c:v prores_ks -profile:v 3 -pix_fmt yuv422p10le -color_range tv "
               "-colorspace bt709 -color_primaries bt709 -color_trc bt709 -vendor "
               "apl0 -video_track_timescale " +
               std::to_string(o.fps * 1000) + " -f mov ";
    if (o.format == 6)
        return common + "-c:v libx264 -preset " + X264Preset(o.quality) + " -crf " +
               std::to_string(o.quality == 0   ? 23
                              : o.quality == 2 ? 16
                                               : 20) +
               " -pix_fmt yuv420p -color_range tv -colorspace bt709 "
               "-color_primaries bt709 -color_trc bt709 -movflags +faststart "
               "-video_track_timescale " +
               std::to_string(o.fps * 1000) + " -f mp4 ";
    return common + "-c:v ffv1 -level 3 -coder 0 -context 0 -slices 12 -g 1 "
                    "-pix_fmt bgra -slicecrc 1 -f matroska ";
}
inline std::string LosslessArguments(const Options &o) {
    return StreamInputArguments(o) + VideoCodecArguments(o);
}
inline uint64_t SizeReserve(uint64_t cap) { return std::max(uint64_t(65536), cap / 50); }
inline uint64_t SizeBitrate(const Options &o) {
    if (!o.maxFileBytes)
        return 0;
    auto reserve = SizeReserve(o.maxFileBytes);
    double duration = EncodedSeconds(o);
    if (duration <= 0 || o.maxFileBytes <= reserve)
        throw std::runtime_error("File size limit is too small");
    double available=double(o.maxFileBytes-reserve)*8/duration-(o.AudioEnabled()?128000:0);
    if(available<24000)throw std::runtime_error("File size limit is too small for video and recorded audio");
    auto bits = uint64_t(std::floor(available));
    if (bits < 24000)
        throw std::runtime_error("File size limit is too small for this clip duration");
    return std::min<uint64_t>(bits, 1000000000ull);
}
inline uint64_t SmallerBitrate(uint64_t previous, uint64_t actual, uint64_t cap) {
    if (actual <= cap)
        return previous;
    double factor = std::min(.94, double(cap - SizeReserve(cap)) / double(actual) * .98);
    return uint64_t(std::floor(previous * factor));
}
inline std::string AudioDurationArguments(const Options& o){
    std::ostringstream value;value.imbue(std::locale::classic());value<<std::setprecision(12)<<EncodedSeconds(o);return " -t "+value.str()+" ";
}
inline std::string BudgetCodecArguments(const Options &o, uint64_t bitrate, int pass) {
    if (pass < 1 || pass > 2 || bitrate < 24000)
        throw std::runtime_error("Invalid size-limited encode plan");
    return FrameClockArguments(o) + "-c:v libx264 -preset " + X264Preset(o.quality) +
           " -threads 4 -pix_fmt yuv420p -color_range tv -colorspace bt709 "
           "-color_primaries bt709 -color_trc bt709 -b:v " +
           std::to_string(bitrate) + " -pass " + std::to_string(pass) +
           (pass==2&&o.AudioEnabled()?AudioDurationArguments(o):std::string()) +
           (pass == 1 ? " -f null "
                      : " -movflags +faststart -video_track_timescale " + std::to_string(o.fps * 1000) +
                            " -f mp4 ");
}
inline Options PreviewOptions(Options o, float availableWidth, bool fullSize) {
    if (fullSize)
        return o;
    int width = availableWidth > 960 ? 1280 : availableWidth > 640 ? 960 : 640;
    width = std::min(width, o.width);
    o.height = std::max(180, int(std::lround(double(width) * o.height / o.width)) & ~1);
    o.width = width;
    return o;
}
inline uint8_t Index(uint8_t r, uint8_t g, uint8_t b) {
    int hi = std::max({int(r), int(g), int(b)}), lo = std::min({int(r), int(g), int(b)});
    if (hi - lo < 24) {
        int luma = (54 * r + 183 * g + 19 * b + 128) >> 8;
        return uint8_t(192 + (luma * 63 + 127) / 255);
    }
    return uint8_t(((int(r) * 6 / 256) << 5) | ((g & 224) >> 3) | (b >> 6));
}
inline void PaletteColor(int k, uint8_t &r, uint8_t &g, uint8_t &b) {
    if (k >= 192)
        r = g = b = uint8_t((k - 192) * 255 / 63);
    else {
        r = uint8_t(((k >> 5) & 7) * 255 / 5);
        g = uint8_t(((k >> 2) & 7) * 255 / 7);
        b = uint8_t((k & 3) * 255 / 3);
    }
}
inline void Quantize(uint8_t *bgra, size_t pixels) {
    for (size_t i = 0; i < pixels; i++) {
        auto p = bgra + i * 4;
        auto k = Index(p[2], p[1], p[0]);
        PaletteColor(k, p[2], p[1], p[0]);
        p[3] = 255;
    }
}
} // namespace suite_clip
