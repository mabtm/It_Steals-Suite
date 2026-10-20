#pragma once
#include "ReplayArchive.h"
#include "SharedProtocol.h"
#include "RoutePlayback.h"

namespace suite_replay
{
inline constexpr uint32_t Seek = 1024; // Command-only: visual/rate changes keep the live clock.
inline bool controls = false, requestMarker = false;
inline double markerAt = -1;
inline practice::ReviewPlayback playback;
inline int camera = 0; // Recorded / following / free
inline bool player = false, monster = false, crystal = false, level = false, trail = true, xray = true;
inline bool hideCeiling = false, fadeWalls = true, simplified = false, fullbright = false;
inline float tail = 3, audioVolume = 1;
inline bool audio = true, perception = false;
inline std::shared_ptr<const practice::MapData> source;
inline std::shared_ptr<const practice::ReplayAnnotations> marks;
inline uint64_t annotationRevision = 0;
inline bool pendingAnnotations = false;

inline bool ControlsRequested(const GameData *data)
{
    return controls && data && data->timerState != 1 && data->replay3dState >= 2 && data->replay3dState <= 3;
}

inline void Bind(std::shared_ptr<const practice::MapData> data)
{
    if (!data || !data->review)
        return;
    if (!source || source->review->id != data->review->id)
    {
        marks.reset();
        annotationRevision = 0;
        pendingAnnotations = false;
        practice::g_replayArchive.LoadAnnotations(data);
    }
    source = std::move(data);
    auto state = practice::g_replayArchive.Get();
    auto a = practice::g_replayArchive.Annotations();
    if (a && a->Matches(*source) && state->annotationRevision != annotationRevision)
    {
        if (!pendingAnnotations || a == marks)
        {
            marks = a;
            pendingAnnotations = false;
            annotationRevision = state->annotationRevision;
        }
    }
}

inline void Save(std::shared_ptr<const practice::ReplayAnnotations> value)
{
    if (!source || !value || !value->Matches(*source))
        return;
    marks = std::move(value);
    pendingAnnotations = true;
    practice::g_replayArchive.SaveAnnotations(source, marks);
}

inline uint32_t Options(bool playing, bool free)
{
    return (playing ? 1u : 0u) |
           (free          ? 2u
            : camera == 1 ? 4u
                          : 0u) |
           (player ? 8u : 0u) | (monster ? 16u : 0u) | (crystal ? 32u : 0u) | (level ? 64u : 0u) |
           (trail ? 128u : 0u) | (marks && marks->loop && marks->LoopValid() ? 256u : 0u) |
           (xray ? 512u : 0u) | (hideCeiling ? 2048u : 0u) | (fadeWalls ? 4096u : 0u) |
           (simplified ? 16384u : 0u) | (fullbright ? 32768u : 0u) | (audio ? 65536u : 0u) |
           (perception ? 131072u : 0u) | (uint32_t(std::clamp(int(audioVolume * 100 + .5f), 0, 100)) << 24);
}

inline void Apply(TrainerCommands &commands, bool playing, bool free)
{
    commands.replay3dOptions = Options(playing, free);
    commands.replay3dFollowDistance = 4.3f;
    commands.replay3dFollowHeight = 0;
    commands.replay3dTrailSeconds = tail;
    commands.replay3dLoopA = marks && marks->LoopValid() ? marks->a : 0;
    commands.replay3dLoopB = marks && marks->LoopValid() ? marks->b : 0;
}

inline double LoopTime(double seconds, bool playing)
{
    return playing && marks && marks->loop && marks->LoopValid() && seconds >= marks->b
               ? marks->a + std::fmod(seconds - marks->b, marks->b - marks->a)
               : seconds;
}
} // namespace suite_replay
