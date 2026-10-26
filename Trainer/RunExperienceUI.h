#pragma once
#include "SuiteDesign.h"
#include "RunExperienceModel.h"

inline std::vector<suite_run::Rect> OccupiedPanels()
{
    std::vector<suite_run::Rect> out;
    for (const char *name : {"Minimap", "Compact HUD", "Speedrun overlay", "It Steals | Practice Suite"})
    {
        auto *w = ImGui::FindWindowByName(name);
        if (w && w->LastFrameActive == ImGui::GetFrameCount() && !w->Hidden)
            out.push_back({w->Pos.x, w->Pos.y, w->Size.x, w->Size.y});
    }
    return out;
}

// Native-size text with a bounded single-line layout. Full evidence remains
// available in each row's tooltip, including at small window sizes.
inline void ResultText(ImFont *font, ImVec2 at, float width, ImVec4 color, const std::string &text)
{
    if (width <= 0)
        return;
    if (!font)
        font = ImGui::GetFont();
    float pixels = font->FontSize * ImGui::GetIO().FontGlobalScale;
    std::string shown = text;
    if (font->CalcTextSizeA(pixels, FLT_MAX, 0, text.c_str()).x > width)
    {
        const char *end = nullptr;
        float dots = font->CalcTextSizeA(pixels, FLT_MAX, 0, "..").x;
        font->CalcTextSizeA(pixels, std::max(0.f, width - dots), 0, text.c_str(), nullptr, &end);
        shown = std::string(text.c_str(), end) + "..";
    }
    auto clip = ImVec4(at.x, at.y, at.x + width, at.y + pixels + 2);
    ImGui::GetWindowDrawList()->AddText(font, pixels, at, ImGui::GetColorU32(color), shown.c_str(), nullptr,
                                        0, &clip);
}

inline bool RenderResultCheck(int index, suite_run::Check check, const std::array<std::string, 3> &evidence,
                              const suite_run::ScanTimeline &scan, bool motion, bool selected = false)
{
    float u = g_dpiScale, width = ImGui::GetContentRegionAvail().x;
    bool tiny = width < 360 * u, wide = width >= 660 * u;
    float height = (tiny ? 86 : 58) * u;
    auto at = ImGui::GetCursorScreenPos();
    ImGui::PushID(index);
    bool clicked = ImGui::InvisibleButton("##CapturedCheck", {width, height}, ImGuiButtonFlags_EnableNav);
    bool complete = index < scan.Completed(motion), active = index == scan.Active(motion);
    float phase = scan.Phase(index);
    auto green = ImVec4(.34f, .94f, .61f, 1), red = ImVec4(1, .36f, .38f, 1),
         white = ImVec4(.95f, .97f, .98f, 1), muted = ImVec4(.57f, .65f, .70f, 1);
    ImVec4 state = complete ? (check.state == suite_run::CheckState::Pass ? green : red)
                   : active ? suite_ui::heading
                            : muted;
    auto *draw = ImGui::GetWindowDrawList();
    auto bg = suite_ui::Blend(suite_ui::background, suite_ui::Color(AppearancePreferences::Button),
                              active ? .48f : .24f);
    draw->AddRectFilled(at, {at.x + width, at.y + height}, ImGui::GetColorU32(bg), 7 * u);
    draw->AddRect(at, {at.x + width, at.y + height},
                  ImGui::GetColorU32(suite_ui::Alpha(
                      active || selected ? state : suite_ui::Color(AppearancePreferences::Outline),
                      active || selected ? .7f : .35f)),
                  7 * u);
    draw->AddLine({at.x + 1, at.y + 10 * u}, {at.x + 1, at.y + height - 10 * u},
                  ImGui::GetColorU32(suite_ui::Alpha(state, complete || active ? .9f : .25f)), 2 * u);
    if (active)
    {
        float x = at.x + width * phase;
        draw->AddRectFilledMultiColor(
            {at.x + 1, at.y + 1}, {x, at.y + height - 1}, ImGui::GetColorU32(suite_ui::Alpha(state, 0)),
            ImGui::GetColorU32(suite_ui::Alpha(state, .09f)),
            ImGui::GetColorU32(suite_ui::Alpha(state, .09f)), ImGui::GetColorU32(suite_ui::Alpha(state, 0)));
    }
    const char *tag = complete ? (check.state == suite_run::CheckState::Pass      ? "[VALID]"
                                  : check.state == suite_run::CheckState::Unknown ? "[MISSING]"
                                                                                  : "[INVALID]")
                      : active ? "[SCANNING]"
                               : "[QUEUED]";
    ResultText(nullptr, {at.x + 14 * u, at.y + 11 * u}, 122 * u, state, tag);
    float labelX = at.x + (tiny ? 14 : 146) * u, labelY = at.y + (tiny ? 34 : 10) * u;
    ResultText(nullptr, {labelX, labelY}, wide ? 210 * u : width - (tiny ? 28 : 160) * u, white, check.label);
    int detail = active ? std::min(2, int(phase * 3)) : 0;
    float detailX = wide ? at.x + 370 * u : labelX,
          detailY = wide ? at.y + 8 * u : at.y + (tiny ? 59 : 34) * u,
          detailWidth = at.x + width - detailX - 12 * u;
    float reveal = active && motion ? suite_ui::Ease(std::fmod(phase * 3, 1.f) / .23f) : 1;
    ResultText(suite_ui::resultDetailFont, {detailX + (active && motion ? (1 - reveal) * 5 * u : 0), detailY},
               detailWidth, suite_ui::Alpha(active ? suite_ui::heading : muted, .6f + .4f * reveal),
               complete ? evidence[0]
               : active ? evidence[detail]
                        : "Awaiting verification");
    if (wide)
        ResultText(suite_ui::resultDetailFont, {detailX, at.y + 32 * u}, detailWidth, muted,
                   complete ? evidence[1]
                   : active ? evidence[(detail + 1) % 3]
                            : "Captured run evidence");
    std::string help =
        std::string(check.label) + "\n" + evidence[0] + "\n" + evidence[1] + "\n" + evidence[2];
    if (check.state == suite_run::CheckState::Unknown)
        help += "\nRequired evidence is missing. This check cannot validate the run.";
    help += "\nClick to inspect this check's captured technical evidence.";
    Help(help.c_str());
    ImGui::PopID();
    return clicked;
}

inline void RenderTechnicalScan(suite_run::Presentation &p, bool motion, float height)
{
    float u = g_dpiScale;
    int selected = p.Inspecting(motion);
    const auto &fields = p.Technical();
    auto checks = p.ResultChecks();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {12 * u, 10 * u});
    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        suite_ui::Blend(suite_ui::background, suite_ui::Color(AppearancePreferences::Button), .18f));
    ImGui::BeginChild("##TechnicalInspector", {0, height}, true,
                      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {6 * u, 4 * u});
    ImGui::TextColored(suite_ui::heading, "TECHNICAL SCAN");
    Help(
        "Captured run observations. Auto-follow tracks the animated check; click a check to pin it. This panel performs no new process or memory scan.");
    if (suite_ui::resultDetailFont)
        ImGui::PushFont(suite_ui::resultDetailFont);
    ImGui::Text("%02d / 07  %s", selected + 1, checks[selected].label);
    if (height >= 320 * u)
        ImGui::TextDisabled("%02d / %d evidence entries", p.scan.FieldsShown(motion),
                            suite_run::TechnicalFieldCount);
    ImGui::ProgressBar(float(p.scan.FieldsShown(motion)) / suite_run::TechnicalFieldCount, {-1, 3 * u}, "");
    float body = std::max(24.f, ImGui::GetContentRegionAvail().y - 52 * u);
    ImGui::BeginChild("##TechnicalFields", {0, body}, false, ImGuiWindowFlags_NoSavedSettings);
    for (int fieldId = 0; fieldId < suite_run::FieldsPerCheck; fieldId++)
    {
        ImGui::PushID(fieldId);
        const auto &field = fields[selected][fieldId];
        size_t split = field.find("  ");
        std::string label = split == std::string::npos ? "Observed value" : field.substr(0, split),
                    value = split == std::string::npos ? field : field.substr(split + 2);
        auto at = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        ImGui::InvisibleButton(label.c_str(), {width, 34 * u});
        ResultText(suite_ui::resultDetailFont, at, width, ImVec4(.57f, .65f, .70f, 1), label);
        ResultText(suite_ui::resultDetailFont, {at.x, at.y + 17 * u}, width, ImVec4(.95f, .97f, .98f, 1),
                   value);
        Help(field.c_str());
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::TextDisabled("EVIDENCE STREAM");
    int count = p.scan.FieldsShown(motion);
    if (!count)
        ImGui::TextDisabled("Awaiting presentation...");
    for (int index = std::max(0, count - 2); index < count; index++)
    {
        auto at = ImGui::GetCursorScreenPos();
        float width = ImGui::GetContentRegionAvail().x;
        const auto &field = fields[index / suite_run::FieldsPerCheck][index % suite_run::FieldsPerCheck];
        ImGui::InvisibleButton(("##Stream" + std::to_string(index)).c_str(), {width, 18 * u});
        ResultText(suite_ui::resultDetailFont, at, width,
                   index == count - 1 ? suite_ui::heading : ImVec4(.57f, .65f, .70f, 1),
                   std::to_string(index + 1) + " > " + field);
        Help(field.c_str());
    }
    ImGui::EndChild();
    if (suite_ui::resultDetailFont)
        ImGui::PopFont();
    if (suite_ui::Button("Copy report"))
        suite_ui::CopyText(p.Report().c_str());
    Help(
        "Copy all 42 captured fields, complete file SHA256, run identity and check results. Snapshot-only values and limitations are labelled in the report.");
    SameLineIfFits("Follow scan");
    ImGui::BeginDisabled(p.inspected < 0);
    if (suite_ui::Button("Follow scan"))
        p.inspected = -1;
    ImGui::EndDisabled();
    Help(
        "Resume automatic selection. At completion, inspect the first failed check, or the build if all checks passed.");
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

inline void RenderRunExperience(suite_run::Presentation &p, double now, bool exported = false)
{
    bool resultVisible = p.replay ? p.hasResult : p.WantsInput(g_pShared, g_runReviewOpen);
    if (!resultVisible && !p.CanAnalyze(g_pShared, now) && now - p.toastAt >= 3)
        return;
    auto size = ImGui::GetIO().DisplaySize;
    float u = g_dpiScale;
    auto occupied = OccupiedPanels();
    auto card = [&](const char *id, suite_run::Rect r, float fade, bool interactive, auto body) {
        ImGui::SetNextWindowPos({r.x, r.y}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({r.w, r.h}, ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {18 * u, 15 * u});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, suite_ui::Alpha(suite_ui::background, .98f));
        ImGui::Begin(id, nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                         (interactive ? 0 : ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoFocusOnAppearing));
        body();
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
    };
    if (resultVisible)
    {
        if (!exported)
            p.scan.Tick(now, true);
        bool motion = exported || suite_ui::preferences.motion, done = p.scan.Done(motion);
        if (!exported && p.scan.TakeCompletion(motion))
            suite_ui::Request(p.Pb() ? suite_ui::Tone::PersonalBest : suite_ui::Tone::Complete);
        if (done && p.Pb() && p.awardAt < 0)
            p.awardAt = now;
        bool splitInspection = exported || (size.x >= 1620 * u && size.y >= 900 * u);
        float w = std::min((splitInspection ? 1120 : 780) * u, size.x - 24),
              h = std::min(864 * u, size.y - 24);
        auto r = suite_run::Place(size.x, size.y, w, h, occupied, true);
        if (!r)
            r = suite_run::Rect{(size.x - w) / 2, (size.y - h) / 2, w, h};
        int action = 0;
        bool openTechnical = false;
        float age = float(std::max(0., now - p.ended)), fade = motion ? suite_ui::Ease(age / .18f) : 1;
        const auto &d = p.completed;
        bool valid = p.Valid();
        auto access = p.replay ? FinishedReplayAccess{} : FinishedReplayAvailable();
        card("##RunResult", *r, fade, !exported, [&] {
            auto white = ImVec4(.95f, .97f, .98f, 1), muted = ImVec4(.57f, .65f, .70f, 1),
                 verdict = valid ? ImVec4(.34f, .94f, .61f, 1) : ImVec4(1, .36f, .38f, 1);
            auto *draw = ImGui::GetWindowDrawList();
            auto pos = ImGui::GetWindowPos();
            draw->AddRectFilledMultiColor(
                {pos.x + 12 * u, pos.y + 1}, {pos.x + w - 12 * u, pos.y + 3 * u},
                ImGui::GetColorU32(suite_ui::Alpha(suite_ui::heading, .1f)),
                ImGui::GetColorU32(suite_ui::Alpha(done ? verdict : suite_ui::heading, .9f)),
                ImGui::GetColorU32(suite_ui::Alpha(done ? verdict : suite_ui::heading, .9f)),
                ImGui::GetColorU32(suite_ui::Alpha(suite_ui::heading, .1f)));
            auto header = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x, closeX = header.x + width - 34 * u;
            ImGui::SetCursorScreenPos({closeX, header.y});
            if (suite_ui::Button("X##CloseRunResult", {34 * u, 32 * u}))
                action = 1;
            Help(
                "Close this result and return to the previous view. The result stays open until you close it or open Replay.");
            ResultText(nullptr, header, closeX - header.x - 12 * u, white, "RUN FINISHED");
            std::string subtitle = "RUN VERIFICATION  /  " + std::string(suite_brand::Credit);
            auto *detailFont = suite_ui::resultDetailFont ? suite_ui::resultDetailFont : ImGui::GetFont();
            if (detailFont->CalcTextSizeA(detailFont->FontSize, FLT_MAX, 0, subtitle.c_str()).x >
                closeX - header.x - 12 * u)
                subtitle = suite_brand::Credit;
            ResultText(detailFont, {header.x, header.y + 25 * u}, closeX - header.x - 12 * u, muted,
                       subtitle);
            ImGui::SetCursorScreenPos({header.x, header.y + 54 * u});
            // Keep close and replay actions outside scrolling content, even
            // at 480x360 / 150% DPI.
            float footerHeight = 80 * u,
                  bodyHeight = std::max(24.f, ImGui::GetContentRegionAvail().y - footerHeight);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {8 * u, 6 * u});
            ImGui::BeginChild("##RunResultDetails", {0, bodyHeight}, false, ImGuiWindowFlags_NoSavedSettings);
            draw = ImGui::GetWindowDrawList();
            float content = ImGui::GetContentRegionAvail().x;
            auto hero = ImGui::GetCursorScreenPos();
            if (suite_ui::resultTimeFont)
                ImGui::PushFont(suite_ui::resultTimeFont);
            auto time = FormatRunTime(d.currentRunTimeSeconds);
            float oldScale = ImGui::GetCurrentWindow()->FontWindowScale,
                  textWidth = ImGui::CalcTextSize(time.c_str()).x;
            if (textWidth > content)
                ImGui::SetWindowFontScale(oldScale * content / std::max(1.f, textWidth));
            ImGui::TextColored(white, "%s", time.c_str());
            ImGui::SetWindowFontScale(oldScale);
            if (suite_ui::resultTimeFont)
                ImGui::PopFont();
            int level = MapLevelIndex(d.reviewScene);
            if (level < 0)
                level = MapLevelIndex(d.currentSceneIndex);
            ImGui::TextColored(muted, "%s", level >= 0 ? kMapLevelNames[level] : "Recorded run");
            if (content > 460 * u)
                suite_ui::Logo(draw, {hero.x + content - 83 * u, hero.y + 3 * u}, 76 * u,
                               done ? 1 : .2f + .8f * p.scan.Fraction(motion), .65f);
            if (done && p.Pb())
            {
                auto at = ImGui::GetCursorScreenPos();
                float width = ImGui::GetContentRegionAvail().x;
                ImGui::Dummy({width, 40 * u});
                auto gold = ImVec4(1.f, .8f, .38f, 1),
                     edge = suite_ui::Color(AppearancePreferences::Animation);
                float celebrate = motion ? 1 - suite_ui::Ease(float((now - p.awardAt) / 1.2)) : 0;
                draw->AddRectFilled(at, {at.x + width, at.y + 37 * u},
                                    ImGui::GetColorU32(suite_ui::Blend(suite_ui::background, gold, .055f)),
                                    7 * u);
                draw->AddRect(at, {at.x + width, at.y + 37 * u},
                              ImGui::GetColorU32(suite_ui::Alpha(edge, .3f + .5f * celebrate)), 7 * u, 0,
                              u * (1 + celebrate));
                suite_ui::DrawIcon(draw, suite_ui::Icon::Trophy, {at.x + 9 * u, at.y + 5 * u}, 26 * u,
                                   ImGui::GetColorU32(gold));
                char text[100];
                if (!p.previousBestKnown)
                    snprintf(text, sizeof(text), "LOCAL PB (recorded)");
                else if (d.previousBestSeconds > 0)
                    snprintf(text, sizeof(text), "NEW LOCAL PB   /   %+.3f s",
                             d.currentRunTimeSeconds - d.previousBestSeconds);
                else
                    snprintf(text, sizeof(text), "FIRST LOCAL PB");
                suite_ui::TextAt(draw, {at.x + 44 * u, at.y + 10 * u}, text, white, 16 * u,
                                 {at.x, at.y, at.x + width - 8 * u, at.y + 37 * u});
            }
            else if (d.previousBestSeconds > 0)
            {
                double delta = d.currentRunTimeSeconds - d.previousBestSeconds;
                ImGui::TextColored(muted, "%+.3f s  /  previous local PB", delta);
            }
            else
                ImGui::TextColored(muted, "Recorded finish time");
            std::string seed = d.reviewReady ? std::to_string(d.reviewSpawnSeed) : "UNKNOWN";
            ImGui::TextColored(white, "Layout seed  %s", seed.c_str());
            SameLineIfFits("Copy seed");
            ImGui::BeginDisabled(!d.reviewReady);
            if (suite_ui::SmallButton("Copy seed"))
                suite_ui::CopyText(seed.c_str());
            ImGui::EndDisabled();
            Help("Copy the captured layout seed for this exact run.");
            ImGui::Spacing();
            auto bar = ImGui::GetCursorScreenPos();
            content = ImGui::GetContentRegionAvail().x;
            auto checks = p.ResultChecks();
            ImGui::TextColored(done ? verdict : suite_ui::heading, "%s",
                               done ? (valid ? "[VALID]" : "[INVALID]") : "[SCANNING]");
            ImGui::SameLine();
            ImGui::TextColored(white, "%s",
                               done ? (valid ? "RUN VERIFIED" : "REVIEW REQUIRED") : "Captured run checks");
            Help(
                "This animation presents observations captured during the run. It does not start a new process or memory scan. Missing required evidence cannot validate a run. Local validation is not proof against every external modification.");
            float segment = (content - 6 * 5 * u) / 7;
            float y = bar.y + 29 * u;
            for (int i = 0; i < 7; i++)
            {
                float x = bar.x + i * (segment + 5 * u);
                auto col = i < p.scan.Completed(motion)
                               ? (checks[i].state == suite_run::CheckState::Pass ? ImVec4(.34f, .94f, .61f, 1)
                                                                                 : ImVec4(1, .36f, .38f, 1))
                               : suite_ui::heading;
                draw->AddRectFilled(
                    {x, y}, {x + segment, y + 3 * u},
                    ImGui::GetColorU32(suite_ui::Alpha(col, i < p.scan.Completed(motion) ? 1 : .15f)), 2 * u);
                if (i == p.scan.Active(motion))
                    draw->AddRectFilled({x, y}, {x + segment * p.scan.Phase(i), y + 3 * u},
                                        ImGui::GetColorU32(col), 2 * u);
            }
            ImGui::SetCursorScreenPos({bar.x, y + 11 * u});
            if (suite_ui::resultDetailFont)
                ImGui::PushFont(suite_ui::resultDetailFont);
            ImGui::TextColored(muted, "%d / 7 checks  /  %s", p.scan.Completed(motion),
                               done ? "evidence presented" : "reading captured evidence");
            if (suite_ui::resultDetailFont)
                ImGui::PopFont();
            bool table = splitInspection &&
                         ImGui::BeginTable("##ResultInspection", 2,
                                           ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings);
            if (table)
            {
                ImGui::TableSetupColumn("Checks", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Technical", ImGuiTableColumnFlags_WidthFixed, 430 * u);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
            }
            const auto &evidence = p.RecordedEvidence();
            for (int i = 0; i < 7; i++)
                if (RenderResultCheck(i, checks[i], evidence[i], p.scan, motion, p.inspected == i))
                {
                    p.inspected = i;
                    suite_ui::Request(suite_ui::Tone::Tab);
                    if (!splitInspection)
                        openTechnical = true;
                }
            if (table)
            {
                ImGui::TableNextColumn();
                RenderTechnicalScan(p, motion, 7 * 64 * u - 6 * u);
                ImGui::EndTable();
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();
            ImGui::Separator();
            float buttonWidth = ImGui::GetContentRegionAvail().x, gap = 10 * u,
                  secondary = std::min(126 * u, buttonWidth * .34f);
            ImGui::BeginDisabled(!access);
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  suite_ui::Blend(suite_ui::background, suite_ui::heading, .38f));
            ImGui::PushStyleColor(ImGuiCol_Text, white);
            if (suite_ui::Button("Analyse Run", {std::max(1.f, buttonWidth - secondary - gap), 40 * u}))
                action = 2;
            ImGui::PopStyleColor(2);
            ImGui::EndDisabled();
            Help(access.detail);
            ImGui::SameLine(0, gap);
            ImGui::BeginDisabled(HasPendingRequest());
            if (suite_ui::Button("Replays", {secondary, 40 * u}))
                action = 3;
            ImGui::EndDisabled();
            Help("Open the replay library. Speedrun mode stays enabled.");
            auto caption = ImGui::GetCursorScreenPos();
            float captionWidth = buttonWidth;
            if (!splitInspection)
            {
                if (suite_ui::resultDetailFont)
                    ImGui::PushFont(suite_ui::resultDetailFont);
                float width = ImGui::CalcTextSize("Technical scan").x + 2 * ImGui::GetStyle().FramePadding.x;
                ImGui::SetCursorScreenPos({caption.x + buttonWidth - width, caption.y});
                if (suite_ui::SmallButton("Technical scan"))
                    openTechnical = true;
                Help(
                    "Inspect the captured technical evidence and copy all 42 fields. The result and speedrun mode stay unchanged.");
                captionWidth -= width + 10 * u;
                if (suite_ui::resultDetailFont)
                    ImGui::PopFont();
            }
            ResultText(suite_ui::resultDetailFont, caption, captionWidth,
                       p.replay && !p.filesOk ? verdict : muted,
                       p.replay ? p.fileStatus
                       : access ? "Player route + suggested route  /  Replay ready"
                                : access.detail);
            if (openTechnical)
                ImGui::OpenPopup("Technical evidence");
            ImGui::SetNextWindowSize({std::min(520 * u, size.x - 36), std::min(580 * u, size.y - 36)},
                                     ImGuiCond_Appearing);
            if (ImGui::BeginPopup("Technical evidence"))
            {
                if (suite_ui::SmallButton("Close details"))
                    ImGui::CloseCurrentPopup();
                RenderTechnicalScan(p, motion, std::max(48.f, ImGui::GetContentRegionAvail().y));
                ImGui::EndPopup();
            }
        });
        occupied.push_back(*r);
        if (action == 1)
        {
            p.Acknowledge();
            SetOverlayInteractive(g_allOverlayEnabled && g_overlayVisible && !g_compactMode);
        }
        else if (action == 2)
            AnalyseFinishedRun();
        else if (action == 3)
        {
            if (p.replay)
                p.Acknowledge();
            OpenReplayLibrary();
        }
    }
    if (p.replay || exported)
        return;
    bool analyze = p.CanAnalyze(g_pShared, now);
    bool toast = now - p.toastAt >= 0 && now - p.toastAt < 3;
    if (!analyze && !toast)
        return;
    auto r = suite_run::Place(size.x, size.y, std::min(370 * u, size.x - 24), analyze ? 112 * u : 84 * u,
                              occupied);
    if (!r)
        return;
    double age = now - (analyze ? p.ended : p.toastAt), left = analyze ? 15 - age : 3 - age;
    float fade = suite_ui::preferences.motion
                     ? suite_ui::Ease(float(age / .18)) * suite_ui::Ease(float(left / .3))
                     : 1;
    card("##RunNotification", *r, fade, false, [&] {
        ImGui::TextColored(suite_ui::heading, "%s",
                           analyze ? suite_keys::Label("Analyse Run?", suite_keys::Analyze).c_str()
                                   : p.toast.c_str());
        if (analyze)
        {
            ImGui::Text("Replay / %.0f s", std::ceil(left));
            ImGui::ProgressBar(float(left / 15), {-1, 4 * u}, "");
        }
        else
            ImGui::Text("%s to %s", suite_keys::Hint(suite_keys::Speedrun).c_str(),
                        p.toastOn ? "disable" : "enable");
    });
}

// Both replay buttons and the video tail use the existing run-result renderer.
inline void RenderRunExperience()
{
    const double now = suite_ui::LifecycleNow();
    if (g_pShared && g_pShared->timerState == 1)
    {
        suite_run::replayPresentation.Acknowledge();
        suite_run::replayRequest = 0;
    }
    auto checked = practice::g_replayArchive.Get()->revalidation;
    if (suite_run::replayRequest && checked && checked->request == suite_run::replayRequest)
    {
        suite_run::replayRequest = 0;
        if (checked->verification)
        {
            checked->verification->Present(suite_run::replayPresentation, now, checked->filesOk,
                                           checked->filesOk ? "Stored run checks / replay files checked"
                                                            : "Replay file check failed: " + checked->error);
            SetOverlayInteractive(true);
        }
    }
    if (suite_run::replayPresentation.hasResult && (!g_pShared || g_pShared->timerState != 1))
        RenderRunExperience(suite_run::replayPresentation, now);
    else
        RenderRunExperience(suite_run::presentation, now);
}

namespace suite_clip
{
inline void RenderRecordedVerification(const Content &content, double elapsed)
{
    if (elapsed < 0 || !content.revalidation || !content.revalidation->verification)
        return;
    const auto &checked = *content.revalidation;
    static suite_run::Presentation result;
    static const practice::ReplayRevalidation *previous = nullptr;
    if (previous != &checked || elapsed == 0)
    {
        checked.verification->Present(result, 0, checked.filesOk,
                                      checked.filesOk ? "Stored run checks / replay files checked"
                                                      : "Replay file check failed: " + checked.error);
        previous = &checked;
    }
    result.scan.elapsed = std::clamp(elapsed, 0., suite_run::ScanTimeline::Duration);
    result.scan.last = elapsed;
    result.scan.visible = true;
    result.awardAt = suite_run::ScanTimeline::Duration;
    result.inspected = -1;
    auto size = ImGui::GetIO().DisplaySize;
    ImGui::GetBackgroundDrawList()->AddRectFilled(
        {0, 0}, size, IM_COL32(5, 10, 15, int(220 * suite_ui::Ease(float(elapsed / .3)))));

    struct ScaleRestore
    {
        float dpi = g_dpiScale, fonts = ImGui::GetIO().FontGlobalScale;
        ImGuiStyle style = ImGui::GetStyle();

        ~ScaleRestore()
        {
            g_dpiScale = dpi;
            ImGui::GetIO().FontGlobalScale = fonts;
            ImGui::GetStyle() = style;
        }
    } restore;

    g_dpiScale = std::min({g_dpiScale, (size.y - 24) / 864.f, (size.x - 24) / 1120.f});
    float scale = g_dpiScale / restore.dpi;
    ImGui::GetIO().FontGlobalScale *= scale;
    ImGui::GetStyle().ScaleAllSizes(scale);
    RenderRunExperience(result, elapsed, true);
}
} // namespace suite_clip

namespace suite_clip
{
inline const bool verificationRegistered = [] {
    verificationRenderer = RenderRecordedVerification;
    return true;
}();
} // namespace suite_clip
