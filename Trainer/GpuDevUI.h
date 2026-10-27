#pragma once

inline void RenderGpuDevControls(std::shared_ptr<const practice::MapAnalysis> source, bool verified)
{
    static bool discovered = false, reuseKey = true, gpuTiming = true;
    static int selected = 0, profile = 1;
    static std::string exportStatus;
    static gpu_dev::Options running;
    if (!discovered) { g_gpuDev.RefreshAdapters(); discovered = true; }
    auto adapters = g_gpuDev.GetAdapters();
    auto report = g_gpuDev.Get();
    bool busy = g_gpuDev.Busy();
    ImGui::TextColored(g_accentCol, "GPU DEV 2 / batch scaling and endurance");
    ImGui::TextWrapped("Compare identical frozen layouts with the CPU reference. Every route, length and pickup contact is checked. Batch order rotates between rounds; CPU/GPU execution order alternates.");
    ImGui::Spacing();
    ImGui::BeginDisabled(busy);
    const char *name = selected < int(adapters->entries.size()) ? adapters->entries[selected].name.c_str() : "Select an adapter";
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
    const char *profiles[]{"Quick check / 1,024 layouts / 4 rounds / batch 16",
                           "Batch sweep / 4,096 layouts / 6 rounds / 16, 64, 128, 256",
                           "Endurance / 8,192 layouts / 12 rounds / 16, 64, 128, 256"};
    ImGui::Combo("Test profile", &profile, profiles, 3);
    auto request = gpu_dev::Preset(profile);
    ImGui::Checkbox("GPU timing queries", &gpuTiming);
    request.gpuTiming = gpuTiming;
    ImGui::TextDisabled("Disable queries for a throughput control run with the same seed key.");
    ImGui::TextDisabled("%s layouts per round | %d strict comparisons | %d s idle baseline",
                        profile == 2 ? "8,192" : profile == 1 ? "4,096" : "1,024",
                        gpu_dev::TotalChecks(request), request.baselineSeconds);
    if (report) ImGui::Checkbox("Reuse previous test's seed key", &reuseKey);
    ImGui::EndDisabled();
    if (!adapters->error.empty()) ImGui::TextWrapped("%s", adapters->error.c_str());
    bool supported = source && source->data && source->data->keep >= 16 && source->data->keep <= 52;
    bool adapterReady = selected < int(adapters->entries.size()) && !adapters->entries[selected].software;
    bool ready = verified && supported && !source->working && !(source->data->flags & 128) &&
                 adapterReady && !g_seedSearch.Busy() && !HasPendingRequest();
    ImGui::BeginDisabled(busy || !ready);
    if (suite_ui::Button("Start comparison"))
    {
        uint64_t key = 0;
        bool generated = reuseKey && report;
        if (generated) key = report->options.key;
        else generated = GenerateSearchKey(key, exportStatus);
        if (generated)
        {
            running = request;
            running.adapter = adapters->entries[selected].index;
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
    else if (!adapterReady) ImGui::TextDisabled("Select a physical graphics adapter.");
    else if (source->working || (source->data->flags & 128)) ImGui::TextDisabled("Waiting for navigation capture.");
    ImGui::TextWrapped("Start with Batch sweep, then Endurance. Keep the same scene loaded. The menu may be closed during the test. JSON checkpoints are saved after each paired pass, and again on completion, stop or error.");
    if (busy)
    {
        suite_ui::Busy(gpu_dev::PhaseName(g_gpuDev.Status()));
        if (g_gpuDev.Status() != gpu_dev::Phase::Detecting)
        {
            int total = 2 * gpu_dev::TotalChecks(running);
            ImGui::ProgressBar(total ? float(g_gpuDev.Checked()) / total : 0, ImVec2(-1, 0));
            ImGui::TextDisabled("%d / %d CPU/GPU layout scores | round %d / %d | batch %d",
                                g_gpuDev.Checked(), total, g_gpuDev.Pair(), running.pairs, g_gpuDev.Batch());
        }
    }
    if (report)
    {
        ImGui::Separator();
        ImGui::TextColored(report->phase == gpu_dev::Phase::Completed && !report->mismatches
                              ? ImVec4(.38f, .85f, .62f, 1) : g_accentCol,
                          "%s | %d checked | %d mismatches", gpu_dev::PhaseName(report->phase),
                          report->checked, report->mismatches);
        ImGui::TextWrapped("%s | %s", report->adapter.name.c_str(), report->model.c_str());
        if (ImGui::BeginTable("GpuDevSummary", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            for (const char *column : {"Batch", "Passes", "CPU layouts/s", "Hybrid layouts/s", "CPU / hybrid"})
                ImGui::TableSetupColumn(column);
            ImGui::TableHeadersRow();
            for (int batch : gpu_dev::Batches(report->options))
            {
                double cpu = 0, gpu = 0; int count = 0, passes = 0;
                for (const auto &p : report->timings)
                    if (p.batch == batch && p.complete) { cpu += p.cpuMs; gpu += p.gpuMs; count += p.checked; passes++; }
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%d", batch);
                ImGui::TableNextColumn(); ImGui::Text("%d", passes);
                ImGui::TableNextColumn(); ImGui::Text("%.0f", cpu > 0 ? count * 1000. / cpu : 0);
                ImGui::TableNextColumn(); ImGui::Text("%.0f", gpu > 0 ? count * 1000. / gpu : 0);
                ImGui::TableNextColumn(); ImGui::Text("%.2fx", gpu > 0 ? cpu / gpu : 0);
            }
            ImGui::EndTable();
        }
        ImGui::TextDisabled("One CPU worker. Scoring only; ratios above 1.00x favor the hybrid. Partial passes are excluded.");
        if (ImGui::CollapsingHeader("Per-pass timings") &&
            ImGui::BeginTable("GpuDevPairs", 7, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp, ImVec2(0, 230)))
        {
            for (const char *column : {"Round / batch", "CPU ms", "Hybrid ms", "GPU kernel ms", "Readback host ms", "Timestamp batches", "Errors"})
                ImGui::TableSetupColumn(column);
            ImGui::TableHeadersRow();
            for (const auto &p : report->timings)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%d / %d%s", p.pair, p.batch, p.complete ? "" : " (partial)");
                ImGui::TableNextColumn(); ImGui::Text("%.2f", p.cpuMs);
                ImGui::TableNextColumn(); ImGui::Text("%.2f", p.gpuMs);
                ImGui::TableNextColumn();
                if (p.timestampBatches && !p.timestampInvalid) ImGui::Text("%.2f", p.gpuKernelMs);
                else ImGui::TextDisabled(p.timestampBatches ? "partial; see JSON" : "unavailable");
                ImGui::TableNextColumn(); ImGui::Text("%.2f", p.hostReadbackMs);
                ImGui::TableNextColumn(); ImGui::Text("%d valid / %d invalid", p.timestampBatches, p.timestampInvalid);
                ImGui::TableNextColumn(); ImGui::Text("%d", p.mismatches);
            }
            ImGui::EndTable();
        }
        if (ImGui::CollapsingHeader("Frame observations by load phase") &&
            ImGui::BeginTable("GpuDevFrames", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
        {
            for (const char *column : {"Phase / batch", "Frames", "Interval p95 ms", "Interval p99 ms", "Game FPS snapshots"})
                ImGui::TableSetupColumn(column);
            ImGui::TableHeadersRow();
            for (const auto &p : report->observations.phases)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%s / %d", gpu_dev::PhaseName(p.phase), p.batch);
                ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(p.intervals.count));
                ImGui::TableNextColumn(); ImGui::Text("%.1f", p.intervals.P95());
                ImGui::TableNextColumn(); ImGui::Text("%.1f", p.intervals.Quantile(.99));
                ImGui::TableNextColumn(); ImGui::Text("%.1f", p.gameSamples ? p.gameFpsSum / p.gameSamples : 0);
            }
            ImGui::EndTable();
        }
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
        if (!report->savedPath.empty()) ImGui::TextDisabled("JSON checkpoint saved automatically. Successful examples are sampled; all layouts are checked.");
    }
    if (!exportStatus.empty()) ImGui::TextWrapped("%s", exportStatus.c_str());
    ImGui::Spacing();
    Help("The normal seed search stays on CPU. Device preparation, warmup, idle baseline, comparisons and JSON writes are excluded from scoring timings. Hybrid time includes packing, transfers, GPU execution, readback, CPU pickup scoring and timing collection. GPU timestamps follow the device timeline and can include game contention. Frame histograms cover the whole run; game FPS are shared snapshots, not a controlled OBS benchmark.");
}
