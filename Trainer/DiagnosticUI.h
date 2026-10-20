#pragma once
#include "DiagnosticScan.h"
DiagnosticInputs CaptureDiagnosticInputs();
inline void RenderDiagnosticScan(){
    static std::vector<DiagnosticFinding> findings;static std::string report;static double scanned=-1;
    if(suite_ui::Button("Run diagnostic scan")){findings=Diagnose(CaptureDiagnosticInputs());report=DiagnosticReport(findings)+"\nTechnical observations\n"+BuildDiagnostics();scanned=ImGui::GetTime();}
    Help("Checks current connection, input ownership, measured frame work, frame limits, RNG, map capture and write errors. Uses existing observations; no heavy game benchmark.");
    SameLineIfFits("Copy scan result");ImGui::BeginDisabled(report.empty());if(suite_ui::Button("Copy scan result"))suite_ui::CopyText(report.c_str());ImGui::EndDisabled();
    if(scanned<0)suite_ui::EmptyState("Diagnostics ready",suite_ui::Icon::Scan,"Ready for a system check","Scan current observations for connection, input, geometry and performance issues. Findings include an explanation and a suggested next step.",184);
    else{int issues=0;for(auto r:findings)issues+=r.severity>=2;ImGui::TextColored(issues?g_accentCol:g_headerCol,"%d checks / %d need attention",int(findings.size()),issues);ImGui::TextDisabled("Snapshot: %.0f s ago",ImGui::GetTime()-scanned);
        for(const auto& r:findings)if(r.severity){ImGui::PushID(r.code.c_str());if(suite_ui::CollapsingHeader((r.code+" / "+r.title).c_str(),r.severity>=2?ImGuiTreeNodeFlags_DefaultOpen:0)){if(!r.evidence.empty())ImGui::TextWrapped("%s",r.evidence.c_str());if(!r.solution.empty())ImGui::TextWrapped("%s",r.solution.c_str());}ImGui::PopID();}
        if(suite_ui::CollapsingHeader("Passed checks"))for(const auto& r:findings)if(!r.severity)ImGui::TextWrapped("%s / %s",r.code.c_str(),r.title.c_str());}
    ImGui::BeginDisabled(!g_pShared||g_pShared->timerState==1||HasPendingRequest());if(suite_ui::Button("Refresh map geometry [F5]"))g_commands.resetMapCmd=1;ImGui::EndDisabled();
    if(suite_ui::CollapsingHeader("Technical details")){if(suite_ui::Button("Copy technical report"))suite_ui::CopyText(BuildDiagnostics().c_str());RenderDiagnosticSections(BuildDiagnostics());}
}
