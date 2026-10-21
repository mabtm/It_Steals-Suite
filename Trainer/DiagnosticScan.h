#pragma once
#include "SharedProtocol.h"
#include "SuiteBranding.h"
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
    add(in.age > 1 ? 2 : 0, "CON-002", in.age > 1 ? "Telemetry is stale" : "Telemetry is current",
        std::to_string(int(in.age * 1000)) + " ms",
        in.age > 1 ? "Wait for level loading to finish, then reconnect if the age keeps increasing." : "");
    if (in.menuRequested && (d.menuInputOwned != 1 || d.cursorLock == 1))
        add(2, "INP-001", "Menu cursor ownership is incomplete",
            "Owned: " + std::to_string(d.menuInputOwned) + " / lock: " + std::to_string(d.cursorLock),
            "Return to the game with F3, then reopen the menu. Reconnect if the problem persists.");
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
    if (!in.settingsError.empty() || d.statsSaveStatus == 2 || d.reportSaveStatus == 2)
        add(3, "IO-001", "A write did not complete",
            in.settingsError + " " + d.statsError + " " + d.reportError,
            "Check free disk space and access to the settings / UserData directories. Existing files are preserved.");
    else
        add(0, "IO-001", "No storage failure is reported");
    if (!in.replayError.empty())
        add(2, "REP-001", "A replay could not be read", in.replayError,
            "Keep the original recording; inspect the named file or available backup, then refresh the replay list.");
    if ((d.timerState == 1 || d.timerState == 2) && (d.runUsedFeatures || (d.runWarnings & ~64u)))
        add(2, "VAL-001", "The run contains a recorded integrity failure", d.validationDetail,
            "Open the completed replay / validation summary. A failed check is retained for that run.");
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
    for (const auto &r : rows)
        out << '[' << r.code << "] "
            << (r.severity == 3   ? "ERROR"
                : r.severity == 2 ? "WARN"
                : r.severity == 1 ? "INFO"
                                  : "PASS")
            << " - " << r.title << '\n'
            << r.evidence << '\n'
            << r.solution << '\n';
    return out.str();
}
