#pragma once
#include "ClipModel.h"

namespace suite_clip
{
inline constexpr const char *ExportPresetNames[] = {
    "Share",         "Best Quality",        "Archive",  "Editing Master", "Size Limit",
    "Quick Preview", "Transparent Overlay", "GIF Loop", "Windows MP4",    "Custom"};
inline constexpr const char *ExportPresetDescriptions[] = {
    "MP4 / x264, balanced quality and broad playback support.",
    "AVI / UtVideo: exact RGB and alpha, fast lossless decoding. 30 / 60 / 120 "
    "FPS.",
    "MKV / FFV1: exact RGB and alpha for archiving. Playback needs a capable "
    "decoder.",
    "MOV / ProRes HQ: high quality 10-bit 4:2:2 editing intermediate; not "
    "lossless.",
    "MP4 / x264 two-pass compression. Verified maximum file size; needs "
    "temporary lossless storage.",
    "720p / 30 FPS MP4, fast compression for quick checks.",
    "1080p RGBA PNG frames with transparent overlays and source timing.",
    "640 x 360 / 20 FPS, small animated palette preview.",
    "MP4 / Windows Media Foundation; hardware acceleration depends on the "
    "installed encoder.",
    "Your current format, quality, size and frame rate."};

inline void ApplyExportPreset(Options &o, int preset)
{
    if (preset < 0 || preset > 9)
        throw std::runtime_error("Unknown export preset");
    o.preset = preset;
    if (preset == 9)
        return;
    o.maxFileBytes = 0;
    o.transparent = false;
    o.width = 1920;
    o.height = 1080;
    o.fps = 60;
    o.quality = 1;
    switch (preset)
    {
    case 0:
        o.format = 6;
        break;
    case 1:
        o.format = 4;
        break;
    case 2:
        o.format = 2;
        break;
    case 3:
        o.format = 5;
        break;
    case 4:
        o.format = 6;
        o.quality = 2;
        o.maxFileBytes = 25000000;
        break;
    case 5:
        o.format = 6;
        o.width = 1280;
        o.height = 720;
        o.fps = 30;
        o.quality = 0;
        break;
    case 6:
        o.format = 3;
        o.transparent = true;
        break;
    case 7:
        o.format = 1;
        o.width = 640;
        o.height = 360;
        o.fps = 20;
        break;
    case 8:
        o.format = 0;
        break;
    }
}
} // namespace suite_clip
