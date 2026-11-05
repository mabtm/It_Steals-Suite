#pragma once
#include "ReplayMotion.h"
#include "RunVerification.h"
#include <sstream>
#include <iomanip>
#include <atomic>

namespace practice
{
inline constexpr const char *ReplayFidelityScope =
    "New recordings include player arms, extra cameras, game UI and particle snapshots at up to 60 Hz. "
    "Poses, sprites, lights and animation metadata have separate tracks. Assets come from the installed game. "
    "Older replays cannot recover missing channels. Unobserved effects and material properties may still differ; "
    "complete describes captured tracks, not pixel-identical video.";

enum class RecordingStatus
{
    Complete,
    Partial,
    Missing,
    Unknown,
    NotNeeded
};

struct RecordingChannel
{
    const char *name = "";
    RecordingStatus status = RecordingStatus::Unknown;
    std::string detail;
    size_t samples = 0;
    double through = 0;
    std::vector<CoverageGap> gaps;
};

struct RecordingCheck
{
    std::array<RecordingChannel, 6> channels;
    int complete = 0, partial = 0, missing = 0, unknown = 0;
};

// A new file check is separate from the observations stored during gameplay.
// It cannot retrospectively scan that game's process or certify replay origin.
struct ReplayRevalidation
{
    uint64_t request = 0;
    int64_t run = 0, checkedUtc = 0;
    bool filesOk = false, reportIncluded = false, reportChecked = false, originalChecked = false,
         originalValid = false;
    uint32_t originalAids = 0, originalWarnings = 0;
    std::string originalReason;
    std::string error, mapSha256, inputSha256, audioSha256, reportSha256;
    std::shared_ptr<const RecordingCheck> coverage;
    std::shared_ptr<const suite_run::Verification> verification;
    std::string verificationSha256;
};

inline const char *RecordingStatusName(RecordingStatus status)
{
    switch (status)
    {
    case RecordingStatus::Complete:
        return "Complete";
    case RecordingStatus::Partial:
        return "Partial";
    case RecordingStatus::Missing:
        return "Missing";
    case RecordingStatus::NotNeeded:
        return "Not needed";
    default:
        return "Unknown";
    }
}

inline std::string RevalidationReport(const ReplayRevalidation &r)
{
    std::ostringstream out;
    out << "Elite-Suite replay recheck\nRun: " << r.run << " / checked UTC (Unix): " << r.checkedUtc
        << "\nFiles: " << (r.filesOk ? "CHECKED" : "FAILED") << "\n"
        << r.error << "\nRun SHA-256: " << r.mapSha256 << "\nInput SHA-256: " << r.inputSha256
        << "\nAudio SHA-256: " << r.audioSha256 << "\nOriginal report SHA-256: " << r.reportSha256
        << "\nOriginal report: "
        << (r.reportChecked    ? "identity and stored observations checked"
            : r.reportIncluded ? "historical schema"
                               : "unavailable")
        << "\nFresh checks cover stored files and their timelines. The recorded run verdict and warnings stay unchanged."
        << "\nLocal, unsigned evidence; editable software and files cannot prove authenticity or leaderboard eligibility.\n";
    out << "\nStored run verdict: "
        << (!r.originalChecked ? "unavailable"
            : r.originalValid  ? "VALID (local)"
                               : "UNVALID")
        << "\nStored aids: " << r.originalAids << " / stored warnings: " << r.originalWarnings << "\n"
        << r.originalReason << "\n";
    if (r.coverage)
        for (const auto &channel : r.coverage->channels)
        {
            out << channel.name << ": " << RecordingStatusName(channel.status) << " / " << channel.samples
                << " samples\n"
                << channel.detail << "\n";
            for (const auto &gap : channel.gaps)
                out << "  Gap: " << gap.begin << " - " << gap.end << " s\n";
        }
    return out.str();
}

inline void RecordingIssue(RecordingChannel &c, std::string reason, bool missing = false)
{
    if (missing)
        c.status = RecordingStatus::Missing;
    else if (c.status != RecordingStatus::Missing)
        c.status = RecordingStatus::Partial;
    if (!c.detail.empty())
        c.detail += " ";
    c.detail += std::move(reason);
}

inline RecordingChannel CoverageChannel(const ReplayMotion &motion, const char *name, size_t channel,
                                        size_t samples, bool optional = false)
{
    RecordingChannel out;
    out.name = name;
    out.samples = samples;
    if (!motion.coverage.recorded)
    {
        out.detail =
            "Older recording without observation heartbeats. Sparse or unchanged values do not prove continuous capture.";
        return out;
    }
    const auto &span = motion.coverage.spans[channel];
    out.through = span.through;
    out.gaps = span.gaps;
    if (optional && span.started && span.sources == 0)
    {
        out.status = RecordingStatus::NotNeeded;
        out.detail = "No sources of this type were bound during this recording.";
        return out;
    }
    if (!span.started || span.observations == 0)
    {
        out.status = RecordingStatus::Missing;
        out.detail = "No successful observations were recorded for this channel.";
        return out;
    }
    out.status = RecordingStatus::Complete;
    out.detail = std::to_string(span.observations) + " observation checks / " + std::to_string(span.sources) +
                 " sources.";
    if (span.errors)
        RecordingIssue(out, std::to_string(span.errors) + " observation errors.");
    if (span.gapCount)
        RecordingIssue(out, std::to_string(span.gapCount) + " observation gaps (showing at most 16).");
    if (span.through + CoverageLimits[channel] + .000001 < motion.seconds && span.gapCount == 0)
    {
        RecordingIssue(out, "Observation ended before the run.");
        out.gaps.push_back({span.through, motion.seconds});
    }
    return out;
}

template <class T>
inline void RecordingTimes(RecordingChannel &c, const std::vector<T> &samples, double duration, double limit,
                           double knownStart = 0)
{
    if (samples.empty())
    {
        RecordingIssue(c, "No recorded samples.", true);
        return;
    }
    double prior = 0;
    int gaps = 0;
    for (const auto &sample : samples)
    {
        if (sample.seconds - prior > limit + .000001 && sample.seconds > knownStart)
        {
            gaps++;
            if (c.gaps.size() < 32)
                c.gaps.push_back({prior, sample.seconds});
        }
        prior = sample.seconds;
    }
    if (duration - prior > limit + .000001)
    {
        gaps++;
        if (c.gaps.size() < 32)
            c.gaps.push_back({prior, duration});
    }
    c.through = std::max(c.through, prior);
    if (gaps)
        RecordingIssue(c, std::to_string(gaps) + " gaps in the stored sample timeline.");
}

inline std::shared_ptr<const RecordingCheck> BuildRecordingCheck(const ReplayMotion &m)
{
    auto check = std::make_shared<RecordingCheck>();
    auto &rows = check->channels;
    rows[0] = CoverageChannel(m, "Movement", 0, m.ticks.size());
    double stationaryStart = 0;
    if (m.ticks.size() >= 2)
    {
        const auto &a = m.ticks[0], &b = m.ticks[1];
        if (a.seconds == 0 && a.frame == 0 && b.frame == 0 && a.tick == 0 && b.tick == 0 &&
            a.state == b.state && a.horizontal == b.horizontal && a.vertical == b.vertical &&
            a.position.x == b.position.x && a.position.y == b.position.y && a.position.z == b.position.z &&
            a.rotation.x == b.rotation.x && a.rotation.y == b.rotation.y && a.rotation.z == b.rotation.z &&
            a.rotation.w == b.rotation.w)
            stationaryStart = b.seconds;
    }
    RecordingTimes(rows[0], m.ticks, m.seconds, .12, stationaryStart);
    rows[1] = CoverageChannel(m, "Camera", 1, m.frames.size());
    RecordingTimes(rows[1], m.frames, m.seconds, .25);
    rows[2] = CoverageChannel(m, "Inputs", 2, m.devices.size());
    if (m.devices.empty())
        RecordingIssue(rows[2], "No device history is available.", true);
    else if (m.deviceThrough + .25 < m.seconds)
        RecordingIssue(rows[2], "Device observation ended before the run.");
    if (m.deviceDropped)
        RecordingIssue(rows[2], std::to_string(m.deviceDropped) + " omitted device observations.");
    rows[3] = CoverageChannel(m, "Colliders", 3, size_t(m.colliders.count));
    if (!m.colliders.recorded || m.colliders.tracks.empty())
        RecordingIssue(rows[3], "No recorded collider shapes are available.", true);
    else if (m.colliders.flags || m.colliders.dropped)
        RecordingIssue(rows[3], "Some collider shapes were unsupported, unavailable or omitted.");
    rows[4] = CoverageChannel(m, "Animation / visuals", 4, size_t(m.visualSamples), true);
    if (!m.visualsRecorded || !m.visualSamples)
        RecordingIssue(rows[4], "No visual samples were recorded.", true);
    if (m.visualDropped)
        RecordingIssue(rows[4], std::to_string(m.visualDropped) + " omitted visual observations.");
    if (m.coverage.recorded)
    {
        auto meta = CoverageChannel(m, "Animation metadata", 5, size_t(m.experience.animations.size()), true);
        if (meta.status == RecordingStatus::Partial || meta.status == RecordingStatus::Missing)
        {
            RecordingIssue(rows[4], "Animator/shape metadata: " + meta.detail);
            for (const auto &gap : meta.gaps)
                if (rows[4].gaps.size() < 32)
                    rows[4].gaps.push_back(gap);
        }
        for (const auto &track : m.experience.animations)
            if (track.frames.empty())
            {
                RecordingIssue(rows[4], "An animation track has no observations.");
                break;
            }
    }
    if (!m.sceneData.recorded)
        rows[4].detail += " This recording has no game-UI, extra-camera or particle channel.";
    else
    {
        rows[4].samples += size_t(m.sceneData.samples);
        rows[4].detail += " Scene: " + std::to_string(m.sceneData.cameras) + " cameras / " +
                          std::to_string(m.sceneData.ui) + " UI tracks / " +
                          std::to_string(m.sceneData.particles) + " particle tracks.";
        if (m.sceneData.dropped)
            RecordingIssue(rows[4], std::to_string(m.sceneData.dropped) + " omitted scene observations.");
        if (m.sceneData.through + .05 < m.seconds)
        {
            RecordingIssue(rows[4], "Scene observation ended before the run.");
            rows[4].gaps.push_back({m.sceneData.through, m.seconds});
        }
    }
    auto &sound = rows[5];
    sound.name = "Audio";
    const auto &audio = m.experience.audio;
    sound.samples = size_t(std::max<int64_t>(0, audio.frames));
    sound.through = audio.Duration();
    if (!audio.recorded)
    {
        sound.status = RecordingStatus::Missing;
        sound.detail = "No recorded game audio is available.";
    }
    else if (!m.audioError.empty())
    {
        sound.status = RecordingStatus::Missing;
        sound.detail = m.audioError;
    }
    else if (m.audioFile.empty())
    {
        sound.status = RecordingStatus::Unknown;
        sound.detail = "Audio companion has not been verified.";
    }
    else
    {
        sound.status = m.coverage.recorded ? RecordingStatus::Complete : RecordingStatus::Unknown;
        sound.detail = "Audio identity and full checksum verified. ";
        bool fullAudio = sound.through + 1.0 / std::max(1, audio.rate) + .000001 >= m.seconds;
        sound.detail += !m.coverage.recorded ? "Historical recording without the current coverage checks."
                        : !audio.flags && !audio.gaps && fullAudio ? "No reported gaps or missing routes."
                                                                   : "Capture issues were reported.";
    }
    if (audio.recorded && (audio.flags || audio.gaps))
    {
        if (sound.status != RecordingStatus::Missing)
            sound.status = RecordingStatus::Partial;
        if (((audio.flags & 1) || audio.gaps) && !audio.diagnostics)
            sound.detail +=
                " Missing PCM blocks or queue overrun; this older audio header has no precise cause or timestamps.";
        if (audio.diagnostics)
        {
            if (audio.missingFrames)
                sound.detail += " Missing PCM: " + std::to_string(audio.missingFrames) + " sample frames.";
            if (audio.listenerOverruns)
                sound.detail += " Listener queue overruns: " + std::to_string(audio.listenerOverruns) + ".";
            if (audio.bypassOverruns)
                sound.detail += " Bypass queue overruns: " + std::to_string(audio.bypassOverruns) + ".";
            for (const auto &gap : audio.gapDetails)
            {
                std::ostringstream line;
                line << std::fixed << std::setprecision(6) << "\nPCM "
                     << (gap.reason == 1   ? "start boundary"
                         : gap.reason == 3 ? "finish boundary"
                                           : "timeline gap")
                     << ": " << double(gap.first) / audio.rate << " - "
                     << double(gap.first + gap.frames) / audio.rate << " s.";
                sound.detail += line.str();
                sound.gaps.push_back(
                    {double(gap.first) / audio.rate, double(gap.first + gap.frames) / audio.rate});
            }
        }
        if (audio.flags & 2)
            sound.detail += " Audio file limit reached.";
        if (audio.flags & 4)
            sound.detail += " An audible route could not be observed reliably.";
        if (audio.flags & 8)
            sound.detail += " Unsupported channels or invalid samples.";
        if (audio.flags & 16)
            sound.detail += " Audio storage error.";
    }
    if (!m.audioNotes.empty())
        sound.detail += "\n" + m.audioNotes;
    if (audio.recorded && sound.through + 1.0 / std::max(1, audio.rate) + .000001 < m.seconds &&
        sound.status != RecordingStatus::Missing)
    {
        RecordingIssue(sound, "Audio ends before the run.");
        sound.gaps.push_back({sound.through, m.seconds});
    }
    if (m.coverage.recorded)
        rows[4].detail +=
            " Adaptive poses up to 120 Hz; position tolerance 0.00075 u / angle tolerance 0.15 degrees at sampled poses.";
    for (auto &row : rows)
    {
        std::sort(row.gaps.begin(), row.gaps.end(), [](auto a, auto b) { return a.begin < b.begin; });
        std::vector<CoverageGap> merged;
        for (auto gap : row.gaps)
        {
            if (!merged.empty() && gap.begin <= merged.back().end + .000001)
                merged.back().end = std::max(merged.back().end, gap.end);
            else if (merged.size() < 32)
                merged.push_back(gap);
        }
        row.gaps = std::move(merged);
    }
    for (const auto &c : rows)
        switch (c.status)
        {
        case RecordingStatus::Complete:
            check->complete++;
            break;
        case RecordingStatus::Partial:
            check->partial++;
            break;
        case RecordingStatus::Missing:
            check->missing++;
            break;
        case RecordingStatus::Unknown:
            check->unknown++;
            break;
        default:
            break;
        }
    return check;
}

inline std::shared_ptr<const RecordingCheck> RecordingCheckFor(const ReplayMotion &m)
{
    auto check = std::atomic_load(&m.recordingCheck);
    if (check)
        return check;
    auto created = BuildRecordingCheck(m);
    std::shared_ptr<const RecordingCheck> empty;
    if (std::atomic_compare_exchange_strong(&m.recordingCheck, &empty, created))
        return created;
    return empty;
}

inline std::string RecordingCheckReport(const ReplayMotion &m, const RecordingCheck &check)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(6) << "Elite-Suite recording check\nRun: " << m.id
        << " / scene: " << m.scene << " / seed: " << m.seed << " / seconds: " << m.seconds
        << " / input format: " << m.inputFormat
        << "\nRecording coverage at observed resolution; separate from run validity and origin verification.\n"
        << "Replay fidelity: " << ReplayFidelityScope << "\n";
    for (const auto &c : check.channels)
    {
        out << c.name << ": " << RecordingStatusName(c.status) << " / samples: " << c.samples
            << " / observed through: " << c.through << "\n"
            << c.detail << "\n";
        for (const auto &gap : c.gaps)
            out << "  Gap: " << gap.begin << " - " << gap.end << " s\n";
    }
    return out.str();
}
} // namespace practice
