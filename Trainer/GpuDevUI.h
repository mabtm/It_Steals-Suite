#pragma once

inline void RenderGpuDevControls(std::shared_ptr<const practice::MapAnalysis>, bool)
{
    static bool discovered = false, reuseKey = true;
    static int adapter = 0, scope = 0, profile = 1;
    static std::string error;
    if (!discovered) { g_gpuDev.RefreshAdapters(); discovered = true; }
    auto found = g_gpuDev.GetAdapters(); auto state = g_gpuDev.Get();
    bool busy = g_gpuDev.Busy();
    ImGui::TextColored(g_accentCol, "GPU DEV 4 / automatic complete-search benchmark");
    ImGui::TextWrapped("One start runs the entire sequence: exhaustive reference, CPU workers 1-6, GPU workers 1-6 at batches 512/1024/2048, and spawn-filter validation. Every scored route and final Top10 must match the reference.");
    ImGui::BeginDisabled(busy);
    const char *name = adapter < int(found->entries.size()) ? found->entries[adapter].name.c_str() : "Select graphics adapter";
    if (ImGui::BeginCombo("Graphics adapter", name))
    {
        for (size_t i = 0; i < found->entries.size(); i++)
        {
            const auto &gpu = found->entries[i];
            std::string label = gpu.name + (gpu.software ? " (software)" : "");
            if (ImGui::Selectable(label.c_str(), adapter == int(i))) adapter = int(i);
        }
        ImGui::EndCombo();
    }
    if (suite_ui::Button("Refresh adapters")) g_gpuDev.RefreshAdapters();
    const char *scopes[]{"Outside + Classic Bonus / automatic level loading", "Current map only"};
    ImGui::Combo("Test maps", &scope, scopes, 2);
    const char *profiles[]{"Quick validation / 2,048 seeds / one round", "Full comparison / 16,384 seeds / three rounds"};
    ImGui::Combo("Test size", &profile, profiles, 2);
    if (state->startedUtcMs) ImGui::Checkbox("Reuse previous suite's seed key", &reuseKey);
    ImGui::EndDisabled();
    bool physical = adapter < int(found->entries.size()) && !found->entries[adapter].software;
    int current = g_pShared ? MapLevelIndex(g_pShared->currentSceneIndex) : -1;
    bool supported = scope == 0 || current == 5 || current == 7;
    bool ready = g_pShared && g_pShared->rngPatchComplete && physical && supported && !g_seedSearch.Busy() && !HasPendingRequest();
    ImGui::BeginDisabled(busy || !ready);
    if (suite_ui::Button("Start automatic test suite"))
    {
        uint64_t key = 0; bool generated = reuseKey && state->startedUtcMs;
        if (generated) key = state->options.key; else generated = GenerateSearchKey(key, error);
        if (generated)
        {
            gpu_dev::SuiteOptions options;
            options.adapter = found->entries[adapter].index; options.key = key;
            options.recipe = g_plannerOptions.recipe;
            options.count = profile ? 16384 : 2048; options.rounds = profile ? 3 : 1;
            if (scope) options.levels = {current};
            try
            {
                g_speedrunMode = g_practiceMode = true; g_seededPractice = g_rawPracticeSpawn = false;
                g_gpuDev.Start(options, int(g_pShared->pid), g_session); error.clear();
            }
            catch (const std::exception &e) { error = e.what(); }
        }
    }
    ImGui::EndDisabled();
    if (busy) { ImGui::SameLine(); if (suite_ui::Button("Stop suite")) g_gpuDev.Stop(); }
    if (!found->error.empty()) ImGui::TextWrapped("%s", found->error.c_str());
    if (!error.empty()) ImGui::TextWrapped("%s", error.c_str());
    ImGui::TextWrapped("The suite loads the selected maps in Practice and captures their verified geometry automatically. The menu may be closed. Share the single Dev4-Summary.json file after completion; individually named reports are also saved after every test.");
    ImGui::Separator();
    ImGui::TextColored(g_accentCol, "%s / %d of %d tests", state->status.c_str(), state->completedTests, state->totalTests);
    ImGui::TextWrapped("%s", state->active.c_str());
    if (busy)
    {
        suite_ui::Busy("Automatic benchmark running");
        ImGui::ProgressBar(state->totalTests ? float(state->completedTests) / state->totalTests : 0, ImVec2(-1, 0));
        ImGui::TextDisabled("Current search: %llu seeds evaluated", static_cast<unsigned long long>(g_gpuDev.Checked()));
    }
    if (!state->error.empty()) ImGui::TextWrapped("%s", state->error.c_str());
    if (!state->runs.empty() && ImGui::BeginTable("Dev4Results", 6,
        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp, ImVec2(0, 280)))
    {
        for (const char *label : {"Mode", "Test", "Seeds/s", "Scored / pruned", "Audited", "Top10"}) ImGui::TableSetupColumn(label);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(int(state->runs.size()));
        while (clipper.Step()) for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; index++)
        {
            const auto &run = state->runs[size_t(index)];
            const auto &s = run.state;
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(gpu_dev::ModeName(s.request.scene));
            ImGui::TableNextColumn(); ImGui::Text("%s w%d / b%d / r%d", run.experiment.reference ? "Reference" : run.experiment.gpu ? "GPU" : "CPU",
                run.experiment.workers, run.experiment.gpu ? run.experiment.batch : 64, run.experiment.round);
            ImGui::TableNextColumn(); ImGui::Text("%.0f", s.elapsed > 0 ? s.evaluated / s.elapsed : 0);
            ImGui::TableNextColumn(); ImGui::Text("%llu / %llu", static_cast<unsigned long long>(s.scored), static_cast<unsigned long long>(s.pruned));
            ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(run.audited));
            ImGui::TableNextColumn(); ImGui::TextUnformatted(run.topMatch ? "PASS" : s.phase == practice::SearchPhase::Completed ? "FAIL" : gpu_dev::SearchStatus(s.phase));
        }
        ImGui::EndTable();
    }
    if (!state->directory.empty() && suite_ui::Button("Open results / Dev4-Summary.json"))
    {
        auto path = std::filesystem::path(state->directory);
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
    ImGui::TextDisabled("Prepared geometry is shared. Timed searches include generation, filtering, bounds, scoring, audits and Top10 ranking.");
}
