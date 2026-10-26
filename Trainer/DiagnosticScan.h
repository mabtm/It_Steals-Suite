#pragma once
#include "SharedProtocol.h"
#include "SuiteBranding.h"
#include "ReplayHealth.h"
#include <vector>
#include <string>
#include <sstream>

struct DiagnosticInputs
{
    const GameData *data = nullptr;
    double age = 0, menuP95 = 0, hudP95 = 0;
    bool software = false, menuRequested = false, mapRequested = false, mapReady = false, sounds = false;
    int audioStatus = 0;
    std::string settingsError, replayError;
    bool routeKnown = false, routeWorking = false, routeComplete = false, routeEstimated = false,
         searchActive = false;
    int omittedGeometry = 0, floorTriangles = 0, searchResults = 0;
    uint64_t checkedSeeds = 0;
    double searchRate = 0;
    std::string routeDetail, exportError;
    int64_t recordingRun = 0;
    std::shared_ptr<const practice::RecordingCheck> recording;
};

struct DiagnosticFinding
{
    int severity = 0;
    std::string code, title, evidence, solution;
};

inline std::vector<DiagnosticFinding> Diagnose(const DiagnosticInputs &in)
{
    std::vector<DiagnosticFinding> rows;
    auto add = [&](int severity, const char *code, const char *title, std::string evidence = {},
                   std::string solution = {}) {
        rows.push_back({severity, code, title, std::move(evidence), std::move(solution)});
    };
    if (!in.data)
    {
        add(3, "CON-001", "The game mod is not connected", {},
            "Start It Steals and check that MelonLoader loads ItStealsHelper.dll from the Mods folder.");
        return rows;
    }
    const auto &d = *in.data;
    std::string helper(d.version);
    if (!helper.empty() && helper.front() == 'v')
        helper.erase(0, 1);
    if (!helper.empty() && helper != suite_brand::ModVersion)
        add(2, "CON-003", "The installed game mod does not match this release",
            "Expected mod " + std::string(suite_brand::ModVersion) + " / installed " + helper,
            "Run Setup from this release to install the matching trainer and ItStealsHelper.dll.");
    add(in.age > 1 ? 2 : 0, "CON-002", in.age > 1 ? "Telemetry is stale" : "Telemetry is current",
        std::to_string(int(in.age * 1000)) + " ms",
        in.age > 1 ? "Wait for level loading to finish, then reconnect if the age keeps increasing." : "");
    if (in.menuRequested && (d.menuInputOwned != 1 || d.cursorLock == 1))
        add(2, "INP-001", "Menu cursor ownership is incomplete",
            "Owned: " + std::to_string(d.menuInputOwned) + " / lock: " + std::to_string(d.cursorLock),
            "Return to the game with the menu shortcut, then reopen the menu. Reconnect if the problem persists.");
    else
        add(0, "INP-001", "Input ownership is consistent");
    add(in.software ? 2 : 0, "REN-001",
        in.software ? "Software renderer is active" : "Hardware renderer is active", {},
        in.software ? "Check GPU driver and graphics availability. WARP can consume substantially more CPU."
                    : "");
    bool cpu = in.menuP95 > 8 || in.hudP95 > 4 || d.helperWorkP95 > 4;
    add(cpu ? 2 : 0, "PER-001",
        cpu ? "Measured frame work is elevated" : "Measured frame work is within the scan thresholds",
        "Menu p95 " + std::to_string(in.menuP95) + " ms / HUD " + std::to_string(in.hudP95) +
            " ms / helper " + std::to_string(d.helperWorkP95) + " ms",
        cpu ? "Stop seed search or reduce workers; disable unneeded collider/visual displays; use a 60 FPS menu limit."
            : "");
    if (d.gameTargetFps > 0)
        add(1, "PER-002", "The game has an FPS limit", "Target: " + std::to_string(d.gameTargetFps) + " FPS",
            "Settings > System > Game frame limit. The overlay frame limit is independent.");
    if (d.gameVSync > 0)
        add(1, "PER-003", "Game VSync is enabled",
            "VSync count: " + std::to_string(d.gameVSync) + " / display " + std::to_string(d.gameRefreshHz) +
                " Hz",
            "Settings > System > Game VSync changes the temporary override. VSync can limit presentation to display intervals.");
    add(d.rngPatchComplete ? 0 : 2, "RNG-001",
        d.rngPatchComplete ? "RNG hooks are complete" : "RNG hooks are incomplete", {},
        d.rngPatchComplete
            ? ""
            : "Confirm the supported game build and matching helper; reload the level to refresh the capture.");
    if (in.mapRequested && d.playerReady && !in.mapReady)
        add(2, "MAP-002", "Current map capture is missing", {},
            "Load the selected map once. Use Refresh map geometry here after a stable level load.");
    if (in.routeKnown)
    {
        if (in.routeWorking)
            add(1, "MAP-003", "Route preparation is in progress", in.routeDetail,
                "Wait for preparation to finish; this scan does not restart it.");
        else if (!in.routeComplete || in.routeEstimated)
            add(2, "MAP-003", "Route capture has unchecked connections", in.routeDetail,
                "Refresh map geometry after the player is standing on the floor. Include this report if the same connections remain missing.");
        else
            add(0, "MAP-003", "The displayed route has checked connections", in.routeDetail);
        if (in.omittedGeometry)
            add(2, "MAP-004", "Some physical geometry was omitted",
                std::to_string(in.omittedGeometry) + " parts / " + std::to_string(in.floorTriangles) +
                    " captured floor triangles",
                "Refresh geometry after loading completes. Include the map, seed, and this report if omissions persist.");
    }
    if (in.searchActive)
        add(in.checkedSeeds >= 10 && !in.searchResults ? 2 : 1, "MAP-005", "Seed search observations",
            std::to_string(in.checkedSeeds) + " checked / " + std::to_string(in.searchResults) +
                " results / " + std::to_string(int(in.searchRate)) + " seeds/s",
            "Check the spawn filter and current capture. An active filter can legitimately leave the results empty.");
    if (!in.settingsError.empty() || d.statsSaveStatus == 2 || d.reportSaveStatus == 2)
        add(3, "IO-001", "A write did not complete",
            in.settingsError + " " + d.statsError + " " + d.reportError,
            "Check free disk space and access to the settings / UserData directories. Existing files are preserved.");
    else
        add(0, "IO-001", "No storage failure is reported");
    if (!in.replayError.empty())
        add(2, "REP-001", "A replay could not be read", in.replayError,
            "Keep the original recording; inspect the named file or available backup, then refresh the replay list.");
    if ((d.timerState == 1 || d.timerState == 2) &&
        ((!d.practiceModeActive && d.runUsedFeatures) || (d.runWarnings & ~64u)))
        add(2, "VAL-001", "The run contains a recorded integrity failure", d.validationDetail,
            "Open the completed replay / validation summary. A failed check is retained for that run.");
    if (in.recording)
        for (const auto &channel : in.recording->channels)
        {
            bool problem = channel.status == practice::RecordingStatus::Partial ||
                           channel.status == practice::RecordingStatus::Missing;
            int severity = problem ? 2 : channel.status == practice::RecordingStatus::Unknown ? 1 : 0;
            rows.push_back(
                {severity, "REP-002",
                 std::string(channel.name) + " / " + practice::RecordingStatusName(channel.status),
                 "Loaded replay " + std::to_string(in.recordingRun) + ": " + channel.detail,
                 problem
                     ? "Open Recording check for the affected interval and source details. Existing recordings keep their original capture gaps."
                     : std::string()});
        }
    if (!in.exportError.empty())
        add(2, "EXP-001", "The clip pipeline reported an error", in.exportError,
            "Check the Clip Studio status and available disk space. Include the export JSON and format when reporting timing or playback issues.");
    if (in.sounds && in.audioStatus == 2)
        add(2, "AUD-001", "UI audio device is unavailable", {},
            "Check the selected Windows output device; disabling UI sounds keeps the interface silent.");
    return rows;
}

inline std::string DiagnosticReport(const std::vector<DiagnosticFinding> &rows)
{
    std::ostringstream out;
    out << "PracticeSuite " << suite_brand::Version << " / " << suite_brand::Author
        << " - diagnostic scan\nSnapshot of observed state; not a stress test.\n";
    for (int severity = 3; severity >= 0; severity--)
        for (const auto &r : rows)
        {
            if (r.severity != severity)
                continue;
            out << '[' << r.code << "] "
                << (r.severity == 3   ? "ERROR"
                    : r.severity == 2 ? "WARN"
                    : r.severity == 1 ? "INFO"
                                      : "PASS")
                << " - " << r.title << '\n';
            if (!r.evidence.empty())
                out << "Observed: " << r.evidence << '\n';
            if (!r.solution.empty())
                out << "Next: " << r.solution << '\n';
        }
    return out.str();
}
