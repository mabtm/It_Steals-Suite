#pragma once
#include "DiagnosticScan.h"
#include "NavigationCapture.h"
DiagnosticInputs CaptureDiagnosticInputs();

inline void RenderNavigationCapture()
{
    namespace capture = suite_navigation_capture;
    auto result = capture::service.Poll();
    auto search = g_seedSearch.Get();
    auto selected = g_mapWorker.Get(g_plannerOptions.level);
    bool searching = search->Busy();
    auto source = capture::Source(searching, search->source ? search->source->data : nullptr,
                                  selected ? selected->data : nullptr);
    ImGui::BeginDisabled(result.phase == capture::Phase::Pending || !source || source->bytes.empty());
    if (suite_ui::Button("Save navigation capture"))
    {
        capture::Metadata metadata;
        metadata.trainerVersion = suite_brand::Version;
        metadata.modVersion = g_pShared ? g_pShared->version : "disconnected";
        metadata.source = searching ? "Exact active search input" : "Selected map packet";
        metadata.phase = searching ? "Active" : "Inactive";
        if (searching)
        {
            metadata.phase = search->phase == practice::SearchPhase::Preparing ? "Preparing"
                             : search->phase == practice::SearchPhase::Generating ? "Generating"
                                                                                 : "Searching";
            metadata.step = search->preparationStep;
            metadata.prepared = search->prepared;
            metadata.total = search->prepareTotal;
        }
        capture::service.Begin(std::move(source), std::move(metadata));
        result = capture::service.View();
    }
    ImGui::EndDisabled();
    Help("Save the exact geometry and navigation packet used by an active seed search, plus its progress and version details. The original .ismap packet and a short text report are written locally in the background. Share both files when reporting a stuck search.");
    if (result.phase == capture::Phase::Pending)
        suite_ui::Busy("Saving navigation capture...");
    else if (result.phase == capture::Phase::Saved)
    {
        ImGui::TextWrapped("Navigation capture saved.");
        auto path = result.folder.u8string();
        std::string utf8(reinterpret_cast<const char *>(path.data()), path.size());
        ImGui::TextWrapped("%s", utf8.c_str());
        if (suite_ui::SmallButton("Copy navigation capture path"))
            suite_ui::CopyText(utf8.c_str());
    }
    else if (result.phase == capture::Phase::Failed)
        ImGui::TextWrapped("Navigation capture failed: %s", result.error.c_str());
}

inline void RenderDiagnosticScan()
{
    static std::vector<DiagnosticFinding> findings;
    static std::string report;
    static double scanned = -1;
    if (suite_ui::Button("Run diagnostic scan"))
    {
        findings = Diagnose(CaptureDiagnosticInputs());
        report = DiagnosticReport(findings) + "\nTechnical observations\n" + BuildDiagnostics();
        scanned = ImGui::GetTime();
    }
    Help(
        "Checks current connection, input ownership, measured frame work, frame limits, RNG, map capture and write errors. Uses existing observations; no heavy game benchmark.");
    SameLineIfFits("Copy scan result");
    ImGui::BeginDisabled(report.empty());
    if (suite_ui::Button("Copy scan result"))
        suite_ui::CopyText(report.c_str());
    ImGui::EndDisabled();
    if (scanned < 0)
        suite_ui::EmptyState(
            "Diagnostics ready", suite_ui::Icon::Scan, "Ready for a system check",
            "Scan current observations for connection, input, geometry and performance issues. Findings include an explanation and a suggested next step.",
            184);
    else
    {
        int errors = 0, warnings = 0, passed = 0;
        for (const auto &finding : findings)
        {
            errors += finding.severity == 3;
            warnings += finding.severity == 2;
            passed += finding.severity == 0;
        }
        float gap = ImGui::GetStyle().ItemSpacing.x;
        float width = std::max(60.f, (ImGui::GetContentRegionAvail().x - 2 * gap) / 3);
        auto count = [&](const char *id, const char *label, int value, const char *note) {
            auto text = std::to_string(value);
            suite_ui::Metric(id, label, text.c_str(), note, width);
        };
        count("Diagnostic errors", "ERRORS", errors, "Action needed");
        ImGui::SameLine();
        count("Diagnostic warnings", "WARNINGS", warnings, "Check these");
        ImGui::SameLine();
        count("Diagnostic passed", "PASSED", passed, "Checked observations");
        ImGui::TextDisabled("Scanned %.0f s ago / %d checks", ImGui::GetTime() - scanned,
                            int(findings.size()));
        auto detail = [&](const DiagnosticFinding &finding) {
            ImGui::PushID(&finding);
            if (suite_ui::CollapsingHeader((finding.title + "##" + finding.code).c_str(),
                                           finding.severity >= 2 ? ImGuiTreeNodeFlags_DefaultOpen : 0))
            {
                ImGui::TextDisabled("%s", finding.code.c_str());
                if (!finding.evidence.empty())
                    ImGui::TextWrapped("%s", finding.evidence.c_str());
                if (!finding.solution.empty())
                {
                    ImGui::TextColored(g_headerCol, "Next step");
                    ImGui::TextWrapped("%s", finding.solution.c_str());
                }
            }
            ImGui::PopID();
        };
        if (!errors && !warnings)
            ImGui::TextWrapped("No issues found in the checked observations.");
        for (int severity : {3, 2})
            for (const auto &finding : findings)
                if (finding.severity == severity)
                    detail(finding);
        if (suite_ui::CollapsingHeader("Information and passed checks"))
            for (const auto &finding : findings)
                if (finding.severity < 2)
                    detail(finding);
    }
    ImGui::BeginDisabled(!g_pShared || g_pShared->timerState == 1 || HasPendingRequest());
    if (suite_ui::Button(suite_keys::Label("Refresh map geometry", suite_keys::RefreshMap).c_str()))
        g_commands.resetMapCmd = 1;
    ImGui::EndDisabled();
    if (suite_ui::CollapsingHeader("Technical details"))
    {
        if (suite_ui::Button("Copy technical report"))
            suite_ui::CopyText(BuildDiagnostics().c_str());
        RenderDiagnosticSections(BuildDiagnostics());
    }
    ImGui::Separator();
    RenderNavigationCapture();
}
