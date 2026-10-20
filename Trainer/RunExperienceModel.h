#pragma once
#include "SharedProtocol.h"
#include "SuiteBranding.h"
#include <array>
#include <cmath>
#include <vector>
#include <optional>
#include <string>
#include <cstdio>
#include <cstring>
#include <sstream>

namespace suite_run
{
struct Rect
{
    float x = 0, y = 0, w = 0, h = 0;

    bool Intersects(const Rect &b) const
    {
        return x < b.x + b.w + 10 && x + w + 10 > b.x && y < b.y + b.h + 10 && y + h + 10 > b.y;
    }
};

inline std::optional<Rect> Place(float width, float height, float w, float h,
                                 const std::vector<Rect> &occupied, bool center = false)
{
    if (w > width - 24 || h > height - 24)
        return {};
    std::vector<Rect> options;
    if (center)
        options.push_back({(width - w) / 2, (height - h) / 2, w, h});
    options.push_back({width - w - 12, height - h - 12, w, h});
    for (const auto &b : occupied)
    {
        options.push_back({width - w - 12, b.y - h - 12, w, h});
        options.push_back({b.x - w - 12, height - h - 12, w, h});
    }
    for (float y = height - h - 12; y >= 12; y -= h + 12)
        for (float x = width - w - 12; x >= 12; x -= w + 12)
            options.push_back({x, y, w, h});
    for (auto r : options)
    {
        if (r.x < 12 || r.y < 12 || r.x + r.w > width - 12 || r.y + r.h > height - 12)
            continue;
        bool free = true;
        for (auto b : occupied)
            free &= !r.Intersects(b);
        if (free)
            return r;
    }
    return {};
}
enum class CheckState
{
    Pass,
    Fail,
    Unknown
};

struct Check
{
    const char *label;
    CheckState state;
};

inline std::array<Check, 7> Checks(const GameData &d)
{
    auto pass = [](bool ok) { return ok ? CheckState::Pass : CheckState::Fail; };
    uint32_t w = uint32_t(d.runWarnings);
    return {
        {{"Trainer features", pass(!d.runUsedFeatures && !d.runAssisted && !d.practiceModeActive)},
         {"Seed / RNG", pass(d.seedKnown && d.seedKind == 1 && d.seedSourcesVerified && d.rngPatchComplete)},
         {"Run observation", pass(d.observedFromStart && !(w & (1u | 2u | 4u | 8u | 32u)))},
         {"Timing / player state", pass(!(w & (16u | 512u | 1024u)))},
         {"Build / hooks", (w & (256u | 2048u | ~16383u)) ? CheckState::Fail
                           : !d.gameAssemblyHash[0] || std::strncmp(d.gameAssemblyHash, "UNKNOWN",
                                                                    sizeof(d.gameAssemblyHash)) == 0
                               ? CheckState::Unknown
                               : CheckState::Pass},
         {"Local integrity", w & 128u                    ? CheckState::Fail
                             : d.externalScanStatus == 1 ? CheckState::Pass
                                                         : CheckState::Unknown},
         {"HUD / commands", pass(!(w & (4096u | 8192u)))}}};
}

inline bool FinalValid(const GameData &d)
{
    if (d.runValidity != 1)
        return false;
    for (auto c : Checks(d))
        if (c.state != CheckState::Pass)
            return false;
    return true;
}

inline bool PersonalBest(const GameData &d)
{
    return d.timerState == 2 && d.runOutcome == 1 && d.lastRunPb && std::isfinite(d.currentRunTimeSeconds) &&
           d.currentRunTimeSeconds > 0 && std::isfinite(d.previousBestSeconds) &&
           d.previousBestSeconds >= 0 &&
           (d.previousBestSeconds == 0 || d.currentRunTimeSeconds < d.previousBestSeconds) && FinalValid(d);
}

using EvidenceRows = std::array<std::array<std::string, 3>, 7>;

template <size_t N> inline std::string CapturedText(const char (&text)[N])
{
    return {text, std::find(text, text + N, '\0')};
}

inline std::string Hex(uint32_t mask)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", unsigned(mask));
    return text;
}

inline EvidenceRows Evidence(const GameData &d)
{
    uint32_t w = uint32_t(d.runWarnings);
    char seconds[32];
    std::snprintf(seconds, sizeof(seconds), "%.3f s", d.currentRunTimeSeconds);
    auto hash = CapturedText(d.gameAssemblyHash);
    if (hash.empty())
        hash = "UNKNOWN";
    if (hash.size() > 24)
        hash = hash.substr(0, 10) + ".." + hash.substr(hash.size() - 8);
    auto game = CapturedText(d.gameVersion), unity = CapturedText(d.unityVersion),
         tool = CapturedText(d.externalTool);
    return {{{{"Used aids  " + Hex(uint32_t(d.runUsedFeatures)),
               std::string("Practice ") + (d.practiceModeActive ? "ON" : "OFF") + " / assisted " +
                   (d.runAssisted ? "YES" : "NO"),
               "Integrity flags  " + Hex(w)}},
             {{std::string(d.seedKind == 1   ? "RANDOM"
                           : d.seedKind == 2 ? "SET"
                                             : "UNKNOWN") +
                   " / " + (d.seedKnown ? std::to_string(d.runSeed) : "seed unknown"),
               std::string("Sources ") + (d.seedSourcesVerified ? "verified" : "unverified") + " / hooks " +
                   (d.rngPatchComplete ? "verified" : "unverified"),
               "Observed reseeds  " + std::to_string(d.rngReseedCount)}},
             {{std::string("Start observed  ") + (d.observedFromStart ? "YES" : "NO"),
               "Observation flags  " + Hex(w & (1u | 2u | 4u | 8u | 32u)),
               "Recorded samples  " + std::to_string(d.reviewSamples)}},
             {{std::string("Stopped clock  ") + seconds, "Timing flags  " + Hex(w & (16u | 512u)),
               "Player flags  " + Hex(w & 1024u)}},
             {{"SHA256  " + hash,
               "Game " + (game.empty() ? "--" : game) + " / Unity " + (unity.empty() ? "--" : unity),
               "Build / hook flags  " + Hex(w & (256u | 2048u | ~16383u))}},
             {{std::string("Recorded scan  ") + (w & 128u                    ? "flagged during run"
                                                 : d.externalScanStatus == 1 ? "complete"
                                                 : d.externalScanStatus == 2 ? "flagged"
                                                                             : "unavailable"),
               tool.empty() ? "No tool name reported" : "Reported tool  " + tool,
               "External flag  " + Hex(w & 128u)}},
             {{std::string("HUD observation  ") + (w & 4096u ? "FAILED" : "no failure flagged"),
               std::string("Forbidden commands  ") + (w & 8192u ? "DETECTED" : "none flagged"),
               "HUD / command flags  " + Hex(w & (4096u | 8192u))}}}};
}

inline constexpr int FieldsPerCheck = 6, TechnicalFieldCount = 7 * FieldsPerCheck;
using TechnicalRows = std::array<std::array<std::string, FieldsPerCheck>, 7>;

inline TechnicalRows TechnicalEvidence(const GameData &d, uint32_t session)
{
    TechnicalRows rows{};
    auto basic = Evidence(d);
    for (int i = 0; i < 7; i++)
        for (int j = 0; j < 3; j++)
            rows[i][j] = basic[i][j];
    auto flag = [](bool value) { return value ? "YES" : "NO"; };
    rows[0][3] = "Captured feature mask  " + Hex(uint32_t(d.trainerFeatures));
    rows[0][4] =
        std::string("Captured freecam ") + flag(d.freecamActive) + " / hitboxes " + flag(d.hitboxEnabled);
    rows[0][5] = std::string("Captured fullbright ") + flag(d.brightnessEnabled) + " / SET active " +
                 flag(d.seedActive);
    rows[1][3] = "Initial RNG seed  " + std::to_string(d.initialRngSeed);
    rows[1][4] = std::string("Sources verified ") + flag(d.seedSourcesVerified) + " / hooks complete " +
                 flag(d.rngPatchComplete);
    rows[1][5] =
        std::string("Layout seed  ") + (d.reviewReady ? std::to_string(d.reviewSpawnSeed) : "UNAVAILABLE");
    rows[2][3] = "Start UTC ticks  " + std::to_string(d.runStartedUtc);
    rows[2][4] = "End UTC ticks  " + std::to_string(d.runEndedUtc);
    rows[2][5] = "Run / replay scene  " + std::to_string(d.runSceneIndex) + " / " +
                 std::to_string(d.reviewScene) + "; epoch " + std::to_string(d.sceneEpoch);
    char wall[64];
    bool haveWall = d.runStartedUtc > 0 && d.runEndedUtc >= d.runStartedUtc;
    std::snprintf(wall, sizeof(wall), "%.6f s",
                  haveWall ? double(d.runEndedUtc - d.runStartedUtc) / 10000000. : 0);
    rows[3][3] = std::string("UTC span  ") + (haveWall ? wall : "UNAVAILABLE");
    rows[3][4] = "Recorded outcome  " + std::to_string(d.runOutcome) + " / timer state " +
                 std::to_string(d.timerState);
    rows[3][5] =
        "Replay samples  " + std::to_string(d.reviewSamples) + " / ID " + std::to_string(d.reviewRunId);
    auto hash = CapturedText(d.gameAssemblyHash);
    if (hash.empty())
        hash = "UNKNOWN";
    rows[4][3] = "File SHA256 [00-31]  " + hash.substr(0, 32);
    rows[4][4] = "File SHA256 [32-63]  " + (hash.size() > 32 ? hash.substr(32, 32) : "UNAVAILABLE");
    auto helper = CapturedText(d.version);
    rows[4][5] =
        "Helper  " + (helper.empty() ? "UNAVAILABLE" : helper) + " / trainer " + suite_brand::Version;
    rows[5][3] = "Monitor status code  " + std::to_string(d.externalScanStatus);
    auto detail = CapturedText(d.validationDetail);
    rows[5][4] = "Recorded verdict  " + (detail.empty() ? "UNAVAILABLE" : detail);
    rows[5][5] = "Helper validity code  " + std::to_string(d.runValidity);
    rows[6][3] = "Captured PID  " + std::to_string(d.pid) + " / session " + std::to_string(session);
    rows[6][4] = std::string("Captured input owner ") + flag(d.menuInputOwned) + " / cursor lock " +
                 std::to_string(d.cursorLock);
    rows[6][5] = "IPC v" + std::to_string(kProtocolVersion) + " / snapshot " +
                 std::to_string(sizeof(GameData)) + " B / command " +
                 std::to_string(sizeof(TrainerCommands)) + " B";
    return rows;
}

inline std::string TechnicalReport(const GameData &d, uint32_t session)
{
    std::ostringstream out;
    out << "PracticeSuite " << suite_brand::Version << " / " << suite_brand::Author
        << "\nRun verification / captured evidence\nResult: " << (FinalValid(d) ? "VALID" : "INVALID")
        << "\nReplay ID: " << d.reviewRunId << "\n";
    out << "Evidence fields: " << TechnicalFieldCount
        << "\nGame file SHA256: " << CapturedText(d.gameAssemblyHash) << "\n";
    auto rows = TechnicalEvidence(d, session);
    auto checks = Checks(d);
    for (int i = 0; i < 7; i++)
    {
        out << "\n["
            << (checks[i].state == CheckState::Pass      ? "VALID"
                : checks[i].state == CheckState::Unknown ? "INVALID / MISSING EVIDENCE"
                                                         : "INVALID")
            << "] " << checks[i].label << "\n";
        for (const auto &field : rows[i])
            out << "  " << field << "\n";
    }
    out << "\nCaptured snapshot values are labelled as such. UTC span is wall-clock time, not the run timer or a validity test. File SHA256 identifies the file on disk, not runtime memory. This presentation does not perform a new process/memory scan or attest against every external modification.\n";
    return out.str();
}

// Presentation of frozen observations, not a fresh process/memory scan.
struct ScanTimeline
{
    static constexpr double Lead = .18, Step = .66, Settle = .24, Duration = Lead + 7 * Step + Settle;
    double elapsed = 0, last = -1;
    bool visible = false, notified = false;

    void Tick(double now, bool shown)
    {
        if (visible && last >= 0 && now > last)
            elapsed = std::min(Duration, elapsed + (now - last));
        last = now;
        visible = shown;
    }

    int Completed(bool motion) const
    {
        return motion ? std::clamp(int((elapsed - Lead) / Step), 0, 7) : 7;
    }

    bool Done(bool motion) const
    {
        return !motion || elapsed >= Duration;
    }

    int Active(bool motion) const
    {
        return motion && elapsed >= Lead && Completed(true) < 7 ? Completed(true) : -1;
    }

    float Fraction(bool motion) const
    {
        return motion ? float(std::clamp((elapsed - Lead) / (7 * Step), 0., 1.)) : 1;
    }

    float Phase(int row) const
    {
        return float(std::clamp((elapsed - Lead - row * Step) / Step, 0., 1.));
    }

    int FieldsShown(bool motion) const
    {
        return motion ? std::clamp(int(std::max(0., elapsed - Lead) / Step * FieldsPerCheck), 0,
                                   TechnicalFieldCount)
                      : TechnicalFieldCount;
    }

    bool TakeCompletion(bool motion)
    {
        if (!Done(motion) || notified)
            return false;
        notified = true;
        return true;
    }
};

inline bool ReplayIdentityUsable(const GameData &d)
{
    return d.reviewRunId > 0 &&
           (d.runStartedUtc <= 0 || (d.reviewRunId >= d.runStartedUtc && d.reviewRunId <= d.runEndedUtc));
}

struct Presentation
{
    GameData completed{};
    bool initialized = false, hasResult = false, ask = false;
    long long seen = 0;
    double ended = -100, toastAt = -100, awardAt = -1;
    int scene = -1, epoch = 0;
    uint32_t session = 0;
    std::string toast;
    bool toastOn = false;
    uint32_t completedSession = 0;
    ScanTimeline scan;
    EvidenceRows evidence;
    TechnicalRows technical;
    bool evidenceBuilt = false;
    int inspected = -1;

    const EvidenceRows &RecordedEvidence()
    {
        if (!evidenceBuilt)
        {
            evidence = Evidence(completed);
            technical = TechnicalEvidence(completed, completedSession);
            evidenceBuilt = true;
        }
        return evidence;
    }

    const TechnicalRows &Technical()
    {
        RecordedEvidence();
        return technical;
    }

    int Inspecting(bool motion) const
    {
        if (inspected >= 0 && inspected < 7)
            return inspected;
        int active = scan.Active(motion);
        if (active >= 0)
            return active;
        if (!scan.Done(motion))
            return 0;
        auto checks = Checks(completed);
        for (int i = 0; i < 7; i++)
            if (checks[i].state != CheckState::Pass)
                return i;
        return 4;
    }

    void Reset()
    {
        *this = Presentation{};
    }

    void Mode(bool enabled, double now)
    {
        toast = enabled ? "Speedrun mode enabled" : "Speedrun mode disabled";
        toastOn = enabled;
        toastAt = now;
    }

    void Observe(const GameData *d, uint32_t currentSession, double now)
    {
        if (!d)
        {
            ask = false;
            return;
        }
        if (session != currentSession)
        {
            initialized = false;
            seen = 0;
            session = currentSession;
            ask = false;
        }
        if (!initialized)
        {
            initialized = true;
            seen = d->runEndedUtc;
        }
        if (d->timerState == 1 || (hasResult && (scene != d->currentSceneIndex || epoch != d->sceneEpoch)))
            ask = false;
        if (d->timerState == 2 && d->runOutcome == 1 && d->runEndedUtc > seen)
        {
            seen = d->runEndedUtc;
            completed = *d;
            completedSession = currentSession;
            scan = {};
            awardAt = -1;
            inspected = -1;
            evidenceBuilt = false;
            scene = d->currentSceneIndex;
            epoch = d->sceneEpoch;
            ended = now;
            hasResult = ask = true;
        }
        if (hasResult && currentSession == completedSession && d->runEndedUtc == completed.runEndedUtc &&
            d->reviewRunId == completed.reviewRunId && d->reviewReady && !completed.reviewReady)
        {
            completed.reviewReady = d->reviewReady;
            completed.reviewScene = d->reviewScene;
            completed.reviewSpawnSeed = d->reviewSpawnSeed;
            completed.reviewSamples = d->reviewSamples;
            completed.reviewRetained = d->reviewRetained;
            evidenceBuilt = false;
        }
        if (now - ended >= 15 || now < ended)
            ask = false;
    }

    bool WantsInput(const GameData *d, bool reviewOpen) const
    {
        return hasResult && !reviewOpen && (!d || d->timerState != 1);
    }

    bool CanAnalyze(const GameData *d, double now) const
    {
        return ask && hasResult && ReplayIdentityUsable(completed) && now - ended >= 0 && now - ended < 15 &&
               d && d->timerState != 1 && d->currentSceneIndex == scene && d->sceneEpoch == epoch &&
               d->reviewReady && d->reviewRunId == completed.reviewRunId;
    }

    void Acknowledge()
    {
        ask = hasResult = false;
        scan = {};
        inspected = -1;
        awardAt = -1;
    }
};

inline Presentation presentation;
} // namespace suite_run
