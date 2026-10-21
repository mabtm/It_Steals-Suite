#include "RunExperienceModel.h"
#include "DiagnosticScan.h"
#include "TrainerPreferences.h"
#include "TelemetryPresentation.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <random>

static GameData CleanFinish()
{
    GameData d{};
    d.runValidity = d.seedKind = d.seedKnown = d.seedSourcesVerified = d.rngPatchComplete =
        d.observedFromStart = d.externalScanStatus = 1;
    std::strcpy(d.gameAssemblyHash, "ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8");
    d.timerState = 2;
    d.runOutcome = 1;
    d.currentSceneIndex = 3;
    d.sceneEpoch = 7;
    d.reviewReady = 1;
    d.reviewRunId = 42;
    d.reviewScene = 3;
    d.runEndedUtc = 100;
    d.currentRunTimeSeconds = 25;
    return d;
}

int main()
{
    using suite_run::ScanTimeline;
    ScanTimeline scan;
    scan.Tick(10, false);
    scan.Tick(1000, false);
    assert(scan.elapsed == 0 && scan.Completed(true) == 0 && scan.Active(true) == -1);
    scan.Tick(1000, true);
    scan.Tick(1000 + ScanTimeline::Lead + .01, true);
    assert(scan.Active(true) == 0 && scan.Completed(true) == 0 && scan.Fraction(true) > 0 &&
           !scan.TakeCompletion(true));
    for (int row = 1; row < 7; row++)
    {
        scan.Tick(1000 + ScanTimeline::Lead + row * ScanTimeline::Step + .01, true);
        assert(scan.Completed(true) == row && scan.Active(true) == row && scan.Phase(row) > 0);
    }
    auto partial = scan.elapsed;
    scan.Tick(scan.last, false);
    scan.Tick(5000, false);
    scan.Tick(5000, true);
    assert(scan.elapsed == partial); // Hidden time is not animation time.
    scan.Tick(5001, true);
    assert(scan.Done(true) && scan.Completed(true) == 7 && scan.Active(true) == -1 &&
           scan.Fraction(true) == 1 && scan.TakeCompletion(true) && !scan.TakeCompletion(true));
    scan.Tick(5001, true);
    scan.Tick(5001, true);
    assert(scan.Done(true) && scan.Completed(true) == 7 &&
           scan.notified); // A duplicate render timestamp cannot unfinish the scan.
    scan = {};
    assert(scan.Done(false) && scan.Completed(false) == 7 && scan.Active(false) == -1 &&
           scan.Fraction(false) == 1 && scan.TakeCompletion(false) && !scan.TakeCompletion(false));
    scan.Tick(10, true);
    scan.Tick(9, true);
    assert(scan.elapsed == 0);
    scan.Tick(9.1, true);
    assert(scan.elapsed > 0 && scan.elapsed < .11);
    auto d = CleanFinish();
    assert(suite_run::FinalValid(d));
    auto best = d;
    assert(!suite_run::PersonalBest(best));
    best.lastRunPb = 1;
    assert(suite_run::PersonalBest(best));
    best.previousBestSeconds = 26;
    assert(suite_run::PersonalBest(best));
    best.previousBestSeconds = 24;
    assert(!suite_run::PersonalBest(best));
    best.previousBestSeconds = NAN;
    assert(!suite_run::PersonalBest(best));
    best.previousBestSeconds = 26;
    best.runWarnings = 8192;
    assert(!suite_run::PersonalBest(best));
    best.runWarnings = 0;
    best.runOutcome = 3;
    assert(!suite_run::PersonalBest(best));
    best.runOutcome = 1;
    best.timerState = 1;
    assert(!suite_run::PersonalBest(best));
    d.pid = 7351;
    d.initialRngSeed = -123;
    d.runSceneIndex = d.reviewScene;
    d.runStartedUtc = 621355968000000000ll;
    d.runEndedUtc = d.runStartedUtc + 250000000;
    std::strcpy(d.version, "v7.3.5");
    auto technical = suite_run::TechnicalEvidence(d, 77);
    assert(technical.size() * technical[0].size() == 42 &&
           technical[1][3].find("-123") != std::string::npos &&
           technical[6][3].find("7351 / session 77") != std::string::npos &&
           technical[3][3].find("25.000000 s") != std::string::npos);
    auto technicalReport = suite_run::TechnicalReport(d, 77);
    assert(technicalReport.find(std::string(suite_brand::Version) + " / mab") != std::string::npos &&
           technicalReport.find("Evidence fields: 42") != std::string::npos &&
           technicalReport.find(d.gameAssemblyHash) != std::string::npos &&
           technicalReport.find("[VALID] Build / hooks") != std::string::npos &&
           technicalReport.find("not runtime memory") != std::string::npos);
    auto missing = d;
    missing.externalScanStatus = 0;
    missing.gameAssemblyHash[0] = 0;
    technicalReport = suite_run::TechnicalReport(missing, 77);
    assert(technicalReport.find("[INVALID / MISSING EVIDENCE] Build / hooks") != std::string::npos &&
           technicalReport.find("UNAVAILABLE") != std::string::npos);
    auto bounded = d;
    std::memset(bounded.version, 'v', sizeof(bounded.version));
    std::memset(bounded.validationDetail, 'x', sizeof(bounded.validationDetail));
    std::memset(bounded.gameAssemblyHash, 'h', sizeof(bounded.gameAssemblyHash));
    technicalReport = suite_run::TechnicalReport(bounded, 77);
    assert(technicalReport.size() < 10000 &&
           technicalReport.find(std::string(sizeof(bounded.validationDetail), 'x')) != std::string::npos);
    ScanTimeline entries;
    int previous = 0;
    entries.Tick(0, true);
    for (int i = 0; i < 1000; i++)
    {
        entries.Tick(i * .01, true);
        assert(entries.FieldsShown(true) >= previous && entries.FieldsShown(true) <= 42);
        previous = entries.FieldsShown(true);
    }
    assert(previous == 42 && entries.FieldsShown(false) == 42);
    suite_run::Presentation inspection;
    inspection.hasResult = true;
    inspection.completed = d;
    inspection.completedSession = 77;
    inspection.scan.elapsed = ScanTimeline::Duration;
    assert(inspection.Inspecting(true) == 4 && inspection.Technical()[6][3].find("77") != std::string::npos);
    inspection.inspected = 2;
    assert(inspection.Inspecting(true) == 2);
    inspection.inspected = -1;
    inspection.completed.runWarnings = 8192;
    inspection.evidenceBuilt = false;
    assert(inspection.Inspecting(true) == 6 &&
           inspection.Technical()[6][1].find("DETECTED") != std::string::npos &&
           !suite_run::FinalValid(inspection.completed));
    inspection.Acknowledge();
    assert(inspection.inspected == -1);
    d = CleanFinish();
    auto evidence = suite_run::Evidence(d);
    assert(evidence[0][0].find("0x00000000") != std::string::npos &&
           evidence[3][0].find("25.000 s") != std::string::npos &&
           evidence[4][0].find("ead3f5f861") != std::string::npos);
    auto flagged = d;
    flagged.runUsedFeatures = 16;
    flagged.runWarnings = 8192 | 128;
    flagged.externalScanStatus = 2;
    std::strcpy(flagged.externalTool, "observed-tool.exe");
    evidence = suite_run::Evidence(flagged);
    assert(evidence[0][0].find("0x00000010") != std::string::npos &&
           evidence[5][1].find("observed-tool.exe") != std::string::npos &&
           evidence[6][1].find("DETECTED") != std::string::npos);
    std::memset(flagged.gameAssemblyHash, 'x', sizeof(flagged.gameAssemblyHash));
    std::memset(flagged.gameVersion, 'y', sizeof(flagged.gameVersion));
    std::memset(flagged.unityVersion, 'z', sizeof(flagged.unityVersion));
    std::memset(flagged.externalTool, 't', sizeof(flagged.externalTool));
    evidence = suite_run::Evidence(flagged);
    assert(evidence[4][0].size() < 40 &&
           evidence[5][1].size() == sizeof(flagged.externalTool) + 15); // Bounded wire strings.
    auto identity = d;
    identity.runStartedUtc = 50;
    identity.reviewRunId = 49;
    assert(!suite_run::ReplayIdentityUsable(identity));
    identity.reviewRunId = 51;
    assert(suite_run::ReplayIdentityUsable(identity));
    identity.reviewRunId = 101;
    assert(!suite_run::ReplayIdentityUsable(identity));
    for (unsigned bit = 0; bit < 32; bit++)
    {
        auto changed = d;
        changed.runWarnings = int(uint32_t{1} << bit);
        if (bit == 6)
        {
            assert(suite_run::FinalValid(changed));
            continue;
        }
        assert(!suite_run::FinalValid(changed));
        bool fail = false;
        for (auto c : suite_run::Checks(changed))
            fail |= c.state == suite_run::CheckState::Fail;
        assert(fail);
    }
    auto changed = d;
    changed.externalScanStatus = 0;
    assert(!suite_run::FinalValid(changed));
    changed = d;
    std::strcpy(changed.gameAssemblyHash, "UNKNOWN");
    assert(!suite_run::FinalValid(changed));
    changed = d;
    changed.runUsedFeatures = 16;
    assert(!suite_run::FinalValid(changed));
    changed = d;
    changed.seedKind = 2;
    assert(!suite_run::FinalValid(changed));
    suite_run::Presentation p;
    p.Observe(&d, 1, 0);
    assert(!p.hasResult); // Do not reannounce an old finish on attach.
    d.timerState = 1;
    p.Observe(&d, 1, 1);
    d.timerState = 2;
    d.runEndedUtc = 101;
    p.Observe(&d, 1, 2);
    assert(p.hasResult && p.CanAnalyze(&d, 2));
    d.currentRunTimeSeconds = 100;
    p.Observe(&d, 1, 3);
    assert(p.completed.currentRunTimeSeconds == 25); // Frozen final time.
    assert(p.CanAnalyze(&d, 16.999) && !p.CanAnalyze(&d, 17));
    p.Observe(&d, 1, 17);
    assert(p.hasResult && !p.ask);
    p.Observe(&d, 1, 3602);
    assert(p.hasResult && p.completed.currentRunTimeSeconds == 25 && p.WantsInput(&d, false));
    d.runEndedUtc++;
    p.Observe(&d, 1, 20);
    assert(p.ask);
    d.reviewRunId++;
    assert(!p.CanAnalyze(&d, 20));
    d.reviewRunId--;
    d.sceneEpoch++;
    p.Observe(&d, 1, 20.1);
    assert(p.hasResult && !p.ask);
    d.runEndedUtc++;
    p.Observe(&d, 1, 21);
    assert(p.ask);
    d.currentSceneIndex = 11;
    p.Observe(&d, 1, 21.1);
    assert(p.hasResult && !p.ask && p.completed.currentSceneIndex == 3);
    d.runEndedUtc++;
    p.Observe(&d, 1, 22);
    d.timerState = 1;
    p.Observe(&d, 1, 22.1);
    assert(p.hasResult && !p.ask && !p.CanAnalyze(&d, 22.1) && !p.WantsInput(&d, false));
    d.timerState = 2;
    d.runEndedUtc++;
    p.Observe(&d, 1, 23);
    p.Observe(&d, 2, 23.1);
    assert(p.hasResult);
    d.runEndedUtc++;
    p.Observe(&d, 2, 24);
    p.Observe(nullptr, 2, 24.1);
    assert(p.hasResult && p.WantsInput(nullptr, false));
    assert(!p.WantsInput(&d, true));
    p.Acknowledge();
    p.Observe(&d, 2, 24.2);
    assert(!p.hasResult && !p.ask && !p.WantsInput(&d, false));
    p.Mode(false, 25);
    assert(p.toast == "Speedrun mode disabled" && !p.toastOn && p.toastAt == 25);
    p.Reset();
    d = CleanFinish();
    d.timerState = 1;
    d.runEndedUtc = 0;
    p.Observe(&d, 3, 0);
    d.timerState = 2;
    d.runStartedUtc = 40;
    d.runEndedUtc = 100;
    d.reviewReady = 0;
    p.Observe(&d, 3, 1);
    auto before = p.RecordedEvidence();
    d.reviewReady = 1;
    d.reviewSamples = 511;
    d.reviewSpawnSeed = 123;
    d.reviewRetained = 1;
    d.currentRunTimeSeconds = 99;
    d.runWarnings = 8192;
    p.Observe(&d, 4, 1.1);
    assert(!p.completed.reviewReady); // Another session cannot rewrite the result.
    p.Observe(&d, 3, 1.2);
    assert(p.completed.reviewReady && p.completed.reviewSamples == 511 &&
           p.completed.reviewSpawnSeed == 123 && p.completed.reviewRetained &&
           p.completed.currentRunTimeSeconds == 25 && p.completed.runWarnings == 0 &&
           suite_run::FinalValid(p.completed));
    assert(p.RecordedEvidence()[2][2].find("511") != std::string::npos && p.RecordedEvidence() != before);
    assert(!p.CanAnalyze(&d, 1.2)); // Session transition expires the shortcut, not the result.
    std::mt19937 rng(700);
    for (int i = 0; i < 10000; i++)
    {
        float width = 480 + rng() % 2081, height = 360 + rng() % 1081, w = 180 + rng() % 441,
              h = 70 + rng() % 451;
        std::vector<suite_run::Rect> occupied;
        for (int j = 0; j < 4; j++)
            occupied.push_back({float(rng() % unsigned(width)), float(rng() % unsigned(height)),
                                float(120 + rng() % 300), float(100 + rng() % 300)});
        auto r = suite_run::Place(width, height, w, h, occupied, i % 2 == 0);
        if (r)
        {
            assert(r->x >= 12 && r->y >= 12 && r->x + r->w <= width - 12 && r->y + r->h <= height - 12);
            for (auto b : occupied)
                assert(!r->Intersects(b));
        }
    }
    assert(!suite_run::Place(480, 360, 620, 520, {}));
    DiagnosticInputs in;
    auto rows = Diagnose(in);
    assert(rows.size() == 1 && rows[0].code == "CON-001" && rows[0].severity == 3);
    d = CleanFinish();
    in.data = &d;
    rows = Diagnose(in);
    for (auto r : rows)
        assert(r.severity < 2);
    in.age = 2;
    in.menuP95 = 9;
    in.software = in.menuRequested = in.mapRequested = in.sounds = true;
    in.audioStatus = 2;
    in.settingsError = "Disk full";
    in.replayError = "run-42.isrun checksum mismatch";
    d.currentSceneIndex = 13;
    d.playerReady = 1;
    d.menuInputOwned = 0;
    d.gameTargetFps = 90;
    d.gameVSync = 1;
    d.rngPatchComplete = 0;
    d.runWarnings = 8192;
    rows = Diagnose(in);
    for (auto code : {"CON-002", "INP-001", "REN-001", "PER-001", "PER-002", "PER-003", "RNG-001",
                      "MAP-002", "IO-001", "REP-001", "VAL-001", "AUD-001"})
    {
        bool found = false;
        for (auto r : rows)
            if (r.code == code)
            {
                found = true;
                assert(r.severity > 0 && !r.solution.empty());
            }
        assert(found);
    }
    auto report = DiagnosticReport(rows);
    assert(report.find(std::string(suite_brand::Version) + " / mab") != std::string::npos &&
           report.find("run-42.isrun") != std::string::npos && report.find("Disk full") != std::string::npos);
    TrainerPreferences preferences;
    preferences.replay.mode = 1;
    preferences.replay.crystals = 7;
    preferences.replay.seconds = 32;
    auto copy = TrainerPreferences::Decode(preferences.Encode());
    assert(copy.Encode() == preferences.Encode());
    auto legacy = TrainerPreferences::Decode(
        "version=14\nhud.width=240\nspeedrun.width=300\nplanner.calculateRoutes=0\nplanner.minimapRoute=1\nseed=-123\nhud.order0=25\nhud.order1=4\n");
    assert(legacy.hud.width == 320 && legacy.speedrun.width == 360 && legacy.seed == -123 &&
           legacy.planner.calculateRoutes && !legacy.planner.minimapRoute && legacy.hud.order[0] == 25 &&
           legacy.hud.order[1] == 4);
    assert(legacy.replay.mode == 0 && legacy.replay.seconds == 10);
    auto bad = TrainerPreferences::Decode(
        "version=15\nreplay.mode=2\nreplay.seconds=-100\nreplay.crystals=1e30\nhud.scale=0.1\n");
    assert(bad.replay.mode == 1 && bad.replay.seconds == 0 && bad.replay.crystals == 1000 &&
           bad.hud.scale == .85f);
    GameData states{};
    states.playerReady = 1;
    states.health = 2;
    states.canMove = 1;
    states.isSprinting = 1;
    assert(std::string(PlayerStateLabel(states, 0)) == "Sprinting");
    assert(std::string(PlayerStateLabel(states, .5)) == "--");
    states.hideAndSeekMode = states.hideSeek_hiding = 1;
    assert(MonsterStateLabel(states, 0) == "Hiding");
    states.hideSeek_foundPlayer = 1;
    assert(MonsterStateLabel(states, 0) == "Found player");
    states.hideAndSeekMode = 0;
    states.timerMode = 1;
    states.timerMonsterCount = 2;
    states.timerMonsterPositions[0] = 10;
    states.timerMonsterPositions[2] = 1;
    states.timerMonsterStates[0] = 1;
    states.timerMonsterStates[1] = 2;
    assert(MonsterStateLabel(states, 0) == "Angry");
    d.previousBestSeconds = INFINITY;
    d.reviewRetained = 9;
    SanitizeSnapshot(d);
    assert(d.previousBestSeconds == 0 && d.reviewRetained == 0);
    std::cout
        << "7.4.0: frozen results, all sticky validation flags, 15-second prompt / scene / session / active-run guards, 10,000 collision-free notification placements, diagnostic evidence, v15 preferences and actual state labels: PASS\n";
}
