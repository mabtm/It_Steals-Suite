#pragma once

inline void RenderGpuDevControls(std::shared_ptr<const practice::MapAnalysis> source, bool verified)
{
    static bool discovered = false;
    static int selected = 0, sizeChoice = 1;
    static std::string exportStatus;
    static gpu_dev::Options running;
    if (!discovered)
    {
        g_gpuDev.RefreshAdapters();
        discovered = true;
    }
    auto adapters = g_gpuDev.GetAdapters();
    bool busy = g_gpuDev.Busy();
    ImGui::TextColored(g_accentCol, "GPU DEV 1 / live scoring comparison");
    ImGui::TextWrapped("Compare the same captured layouts on CPU and GPU. GPU visit orders use the existing CPU pickup-contact scoring. Three paired passes alternate execution order.");
    ImGui::Spacing();
    ImGui::BeginDisabled(busy);
    const char *name = selected < int(adapters->entries.size()) ? adapters->entries[selected].name.c_str()
                                                               : "Select an adapter";
    if (ImGui::BeginCombo("Graphics adapter", name))
    {
        for (size_t i = 0; i < adapters->entries.size(); i++)
        {
            const auto &a = adapters->entries[i];
            std::string label = a.name + (a.software ? " (software renderer)" : "");
            if (ImGui::Selectable(label.c_str(), selected == int(i))) selected = int(i);
        }
        ImGui::EndCombo();
    }
    if (suite_ui::Button("Refresh adapters")) g_gpuDev.RefreshAdapters();
    const char *sizes[]{"32 layouts", "128 layouts", "512 layouts", "1024 layouts"};
    ImGui::Combo("Test size", &sizeChoice, sizes, 4);
    ImGui::EndDisabled();
    if (!adapters->error.empty()) ImGui::TextWrapped("%s", adapters->error.c_str());
    bool supported = source && source->data && source->data->keep >= 16 && source->data->keep <= 52;
    bool ready = verified && supported && !source->working && !(source->data->flags & 128) &&
                 selected < int(adapters->entries.size()) && !g_seedSearch.Busy() && !HasPendingRequest();
    ImGui::BeginDisabled(busy || !ready);
    if (suite_ui::Button("Compare live layouts"))
    {
        uint64_t key = 0;
        if (GenerateSearchKey(key, exportStatus))
        {
            constexpr int counts[]{32, 128, 512, 1024};
            running = {};
            running.adapter = adapters->entries[selected].index;
            running.seeds = counts[sizeChoice];
            running.key = key;
            running.recipe = g_plannerOptions.recipe;
            gpu_dev::Observations baseline;
            baseline.baselineGameFps = g_pShared ? g_pShared->gameFps : 0;
            baseline.baselineMenuCpuP95 = g_menuFrameCost.P95();
            g_gpuDev.Start(source, running, baseline);
            exportStatus.clear();
        }
    }
    ImGui::EndDisabled();
    if (busy && g_gpuDev.Status() != gpu_dev::Phase::Detecting)
    {
        ImGui::SameLine();
        if (suite_ui::Button("Stop test")) g_gpuDev.Stop();
    }
    if (!verified) ImGui::TextDisabled("Load a verified live map in Practice to begin.");
    else if (!supported) ImGui::TextDisabled("This prototype supports heuristic routing for 16-52 crystals.");
    else if (source->working || (source->data->flags & 128)) ImGui::TextDisabled("Waiting for navigation capture.");
    if (busy)
    {
        suite_ui::Busy(gpu_dev::PhaseName(g_gpuDev.Status()));
        if (g_gpuDev.Status() != gpu_dev::Phase::Detecting)
        {
            int total = running.seeds * running.pairs;
            ImGui::ProgressBar(total ? float(g_gpuDev.Checked()) / total : 0, ImVec2(-1, 0));
            ImGui::TextDisabled("%d / %d paired layout checks", g_gpuDev.Checked(), total);
        }
    }
    auto report = g_gpuDev.Get();
    if (report && !busy)
    {
        ImGui::Separator();
        ImGui::TextColored(report->phase == gpu_dev::Phase::Completed && !report->mismatches
                              ? ImVec4(.38f, .85f, .62f, 1) : g_accentCol,
                          "%s | %d checked | %d mismatches", gpu_dev::PhaseName(report->phase),
                          report->checked, report->mismatches);
        ImGui::TextWrapped("%s | %s", report->adapter.name.c_str(), report->model.c_str());
        if (ImGui::BeginTable("GpuDevPairs", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn("Pair");
            ImGui::TableSetupColumn("CPU score ms");
            ImGui::TableSetupColumn("GPU hybrid ms");
            ImGui::TableSetupColumn("CPU / GPU");
            ImGui::TableHeadersRow();
            for (const auto &p : report->timings)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%d", p.pair);
                ImGui::TableNextColumn(); ImGui::Text("%.2f", p.cpuMs);
                ImGui::TableNextColumn(); ImGui::Text("%.2f", p.gpuMs);
                ImGui::TableNextColumn(); ImGui::Text("%.2fx", p.gpuMs > 0 ? p.cpuMs / p.gpuMs : 0);
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("One CPU worker. Scoring timings exclude map preparation and comparison/export overhead.");
        if (!report->error.empty()) ImGui::TextWrapped("%s", report->error.c_str());
        if (!report->exportError.empty()) ImGui::TextWrapped("JSON export: %s", report->exportError.c_str());
        if (suite_ui::Button("Export JSON"))
        {
            try
            {
                auto path = g_gpuDev.directory / ("gpu-dev-export-" + std::to_string(gpu_dev::UtcMs()) + ".json");
                gpu_dev::Save(path, *report);
                exportStatus = "JSON exported. Open the report folder to share it.";
            }
            catch (const std::exception &e) { exportStatus = e.what(); }
        }
        ImGui::SameLine();
        if (suite_ui::Button("Open reports"))
        {
            std::error_code ec;
            std::filesystem::create_directories(g_gpuDev.directory, ec);
            if (!ec) ShellExecuteW(nullptr, L"open", g_gpuDev.directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            else exportStatus = ec.message();
        }
        if (!report->savedPath.empty()) ImGui::TextDisabled("JSON saved automatically after this test.");
    }
    if (!exportStatus.empty()) ImGui::TextWrapped("%s", exportStatus.c_str());
    ImGui::Spacing();
    Help("The normal seed search remains the CPU reference. This dev test runs a real GPU hybrid scorer on the current map, without changing your seed or moving the player. JSON includes exact orders, pickup contacts, timings and reproducible input matrices. Frame observations cover the whole test and are not controlled OBS/FPS benchmarks.");
}
