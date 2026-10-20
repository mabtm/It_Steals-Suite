#pragma once
#include "SuiteExperience.h"
#include "SharedProtocol.h"
#include <sstream>
#include <locale>
#include <ctime>

inline std::string FeatureNames(uint32_t mask)
{
    const char *names[] = {"Practice", "Fixed seed", "Hitboxes", "Fullbright",    "Free camera",
                           "Teleport", "Minimap",    "Path",     "Spawn planner", "Live inspector"};
    std::string result;
    for (int i = 0; i < 10; i++)
        if (mask & (1u << i))
        {
            if (!result.empty())
                result += ", ";
            result += names[i];
        }
    if (mask & ~1023u)
    {
        if (!result.empty())
            result += ", ";
        result += "Unknown feature flags";
    }
    return result.empty() ? "OFF" : result;
}

inline std::string WarningNames(uint32_t mask)
{
    const char *names[] = {"Observation gap",       "Player state missing",   "Seed coverage lost",
                           "Process stopped",       "Invalid timing",         "Run interrupted",
                           "Event log truncated",   "Memory editor detected", "Integrity checks unavailable",
                           "Time scale changed",    "Player state changed",   "Game hooks changed",
                           "Mandatory HUD missing", "Forbidden command"};
    std::string result;
    for (int i = 0; i < 14; i++)
        if (mask & (1u << i))
        {
            if (!result.empty())
                result += ", ";
            result += names[i];
        }
    if (mask & ~16383u)
    {
        if (!result.empty())
            result += ", ";
        result += "Unknown warning flags";
    }
    return result.empty() ? "None observed" : result;
}

inline std::string VerificationSummary(const GameData &data)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "It Steals PracticeSuite - local verification summary\n"
        << "This is editable local evidence, not leaderboard approval.\n"
        << "Run: " << (data.currentRunNumber ? std::to_string(data.currentRunNumber) : "UNKNOWN")
        << "\nRun level: " << data.runSceneIndex << " (current scene " << data.currentSceneIndex
        << ")\nOutcome: " << OutcomeLabel(data.runOutcome)
        << "\nObserved duration: " << FormatRunTime(data.currentRunTimeSeconds)
        << "\nSeed type: " << SeedTypeLabel(data) << "\n"
        << (data.seedKind == 2 ? "Practice base seed: " : "Initial RNG seed (not a world replay seed): ")
        << (data.seedKnown ? std::to_string(data.runSeed) : "UNKNOWN")
        << "\nReseeds observed: " << data.rngReseedCount
        << "\nCurrent trainer features: " << FeatureNames(static_cast<uint32_t>(data.trainerFeatures))
        << "\nTrainer aids used during run: " << FeatureNames(static_cast<uint32_t>(data.runUsedFeatures))
        << "\nWarnings: " << WarningNames(static_cast<uint32_t>(data.runWarnings))
        << "\nLocal validation: " << (data.runValidity == 1 ? "VALID" : "UNVALID")
        << "\nReason: " << data.validationDetail << "\nSuite: " << data.version
        << "\nGame reported version: " << (data.gameVersion[0] ? data.gameVersion : "UNKNOWN")
        << "\nUnity: " << (data.unityVersion[0] ? data.unityVersion : "UNKNOWN")
        << "\nGame assembly FILE SHA256: " << (data.gameAssemblyHash[0] ? data.gameAssemblyHash : "UNKNOWN")
        << "\nStart UTC ticks: " << data.runStartedUtc << "\nEnd UTC ticks: " << data.runEndedUtc
        << "\nStart observed: " << (data.observedFromStart ? "YES" : "UNKNOWN")
        << "\nReport events: " << data.reportEventCount;
    return out.str();
}

inline void RenderVerification(const GameData &data)
{
    if (!suite_ui::CollapsingHeader("Verification summary", ImGuiTreeNodeFlags_DefaultOpen))
        return;
    bool valid = data.runValidity == 1 && SnapshotAge() < .5;
    ImGui::TextColored(valid ? ImVec4(.30f, .94f, .40f, 1) : ImVec4(1, .23f, .23f, 1), "Local validation: %s",
                       valid ? "VALID" : "UNVALID");
    ImGui::TextWrapped("%s", data.validationDetail);
    if (data.externalScanStatus == 2)
        ImGui::TextColored(ImVec4(1, .43f, .36f, 1), "Memory editor detected: %s", data.externalTool);
    ImGui::TextWrapped("Current features: %s",
                       FeatureNames(static_cast<uint32_t>(data.trainerFeatures)).c_str());
    ImGui::TextWrapped("Used during run: %s",
                       FeatureNames(static_cast<uint32_t>(data.runUsedFeatures)).c_str());
    ImGui::TextWrapped("Warnings: %s", WarningNames(static_cast<uint32_t>(data.runWarnings)).c_str());
    ImGui::TextWrapped("Game reported version: %s | Suite: %s",
                       data.gameVersion[0] ? data.gameVersion : "UNKNOWN", data.version);
    if (suite_ui::Button("Copy verification summary"))
        suite_ui::CopyText(VerificationSummary(data).c_str());
    suite_ui::Checkbox("Save verification reports for new runs", &g_reportsEnabled);
    Help(
        "Optional JSON reports in UserData/ItStealsPracticeSuite/Reports. Events are recorded only when enabled at run start. Reports are editable local observations, not anti-cheat proof.");
    if (data.reportSaveStatus == 2)
        ImGui::TextWrapped("Report not saved: %s", data.reportError);
    if (data.reportSaveStatus == 1)
        ImGui::TextDisabled("Writing report...");
    ImGui::TextWrapped(
        "A reported version can be generic. The JSON report includes the game assembly file SHA256, start/end times, seed observations and feature changes. External process changes are not fully detectable.");
}
