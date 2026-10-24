// Headless ImGui integration: execute the real application UI, including every
// tab, at small game sizes / DPI scales with live, restricted and offline data.
#include "imgui.h"
#include "imgui_internal.h"
#include "SharedProtocol.h"
#include "TrainerPreferences.h"
#include "MinimapGeometry.h"
#include "PracticeMapWorker.h"
#include "SeedSearch.h"
#include "seed_batch_fixture.h"
#include <string>
#include <cstring>
#include <cstdio>
#include <fstream>
#include <cassert>
#include <iostream>

bool g_autoStartGame = true;
bool g_captureOverlayEnabled = false;
bool g_overlayVisible = true, g_allOverlayEnabled = true, g_quit = false, g_resetLayout = false;
bool g_compactMode = false, g_showMinimapWindow = true, g_showPath = false, minimapLockAspect = true;
bool g_showPlayerPanel = true, g_showCrystalPanel = true, g_showMonsterPanel = true,
     g_showVisualsPanel = true;
bool g_brightnessOn = false, g_accurateOn = false, g_hitboxCheck = false, g_hidePlayerHitbox = false;
bool g_speedrunMode = false, g_practiceMode = false, g_seededPractice = false;
bool g_validityBadgeDrawn = false;
int g_frameLimitMode = 0, g_gameFrameLimit = 240, g_vSyncMode = 0;
int g_selectedSpeedrunLevel = 0, g_practiceSeed = 123, g_minimapWidth = 250, g_minimapHeight = 250,
    g_renderFps = 60;
float g_minimapOpacity = 1, g_dpiScale = 1;
OverlayLayout g_hudLayout{false}, g_speedrunLayout{true};
MinimapOptions g_mapOptions;
MapTrail g_trail;
bool g_reportsEnabled = false, g_rawPracticeSpawn = false, g_plannerTabActive = false;
uint32_t g_session = 1;
int g_plannerAutoLevel = -1;
PlannerPreferences g_plannerOptions;
practice::MapWorker g_mapWorker;
practice::SeedSearch g_seedSearch;
std::string g_plannerRandomError;
uint32_t g_sentSerial = 0;
ImFont *g_timerFont = nullptr;

double SnapshotAge()
{
    return .008;
}

ImVec4 g_mainBgCol(.048f, .063f, .074f, 1), g_textCol(.91f, .95f, .95f, 1), g_headerCol(.37f, .83f, .77f, 1),
    g_accentCol(1, .69f, .38f, 1);
GameData data{};
GameData *g_pShared = &data;
TrainerCommands g_commands;
std::string g_connectionStatus = "Waiting for ItStealsHelper";

struct
{
    std::string error, loadWarning;
} g_settings;

uint64_t g_nextConnectTick = 0;
bool pendingRequest = false;

bool HasPendingRequest()
{
    return pendingRequest;
}

bool lastInteractiveRequest = false;

void SetOverlayInteractive(bool value)
{
    lastInteractiveRequest = value;
}

void ApplyPreferences(TrainerPreferences) {}

void SavePreferencesNow(bool = false) {}

void CloseSharedMemory() {}

void InvalidateFontTexture() {}

std::string g_uiFontName = "Host QA font";

ImFont *LoadUiFont(float pixels, int = 0)
{
    ImFontConfig font;
    font.SizePixels = pixels;
    // The host font is for headless QA only. Windows uses Segoe UI if available.
    std::ifstream hostFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    if (hostFont)
        return ImGui::GetIO().Fonts->AddFontFromFileTTF("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                                                        pixels);
    return ImGui::GetIO().Fonts->AddFontDefault(&font);
}

std::string BuildDiagnostics()
{
    return "Trainer: 7.1.0 / protocol 16\nMod: v7.1.0\nStatus: Connected\nGame PID: 1234\nClient: 1280 x 720 at 100, 100\nFocus: game\nWindow events: active\nDPI scale: 1\nRenderer: D3D11 hardware\nInput surface: interactive menu\nFrame limit: 60 FPS / hidden: no rendering\nCommands: idle\nRNG hooks: 22 / 22\nWall boxes: 128 / 200";
}

#include "TrainerUI.h"
#include "MinimapUI.h"

DiagnosticInputs CaptureDiagnosticInputs()
{
    DiagnosticInputs in;
    in.data = g_pShared;
    return in;
}

template <class T> void Write(std::ofstream &output, const T &value)
{
    output.write(reinterpret_cast<const char *>(&value), sizeof(value));
}

void Dump(const std::string &filename)
{
    auto *draw = ImGui::GetDrawData();
    unsigned char *pixels;
    int width, height;
    ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
    std::ofstream output(filename, std::ios::binary);
    Write(output, uint32_t(draw->DisplaySize.x));
    Write(output, uint32_t(draw->DisplaySize.y));
    Write(output, uint32_t(width));
    Write(output, uint32_t(height));
    Write(output, uint32_t(draw->CmdListsCount));
    output.write(reinterpret_cast<const char *>(pixels), width * height);
    for (const ImDrawList *list : draw->CmdLists)
    {
        Write(output, uint32_t(list->VtxBuffer.Size));
        Write(output, uint32_t(list->IdxBuffer.Size));
        Write(output, uint32_t(list->CmdBuffer.Size));
        output.write(reinterpret_cast<const char *>(list->VtxBuffer.Data),
                     list->VtxBuffer.Size * sizeof(ImDrawVert));
        for (ImDrawIdx index : list->IdxBuffer)
            Write(output, uint32_t(index));
        for (const ImDrawCmd &command : list->CmdBuffer)
        {
            assert(!command.UserCallback);
            Write(output, command.ClipRect);
            Write(output, command.ElemCount);
            Write(output, command.IdxOffset);
            Write(output, command.VtxOffset);
        }
    }
}

uint64_t CheckHudRegions()
{
    const auto &dd = *ImGui::GetDrawData();
    auto regions = HudContentRegions(dd, int(dd.DisplaySize.x), int(dd.DisplaySize.y));
    assert(!regions.empty() && regions.size() <= 4);
    uint64_t bytes = 0;
    for (size_t i = 0; i < regions.size(); i++)
    {
        auto r = regions[i];
        assert(r.Width() > 0 && r.Height() > 0 && r.left >= 0 && r.top >= 0 && r.right <= dd.DisplaySize.x &&
               r.bottom <= dd.DisplaySize.y);
        bytes += r.Bytes();
        for (size_t j = 0; j < i; j++)
            assert(!HudIntersects(r, regions[j]));
    }
    for (const auto *list : dd.CmdLists)
        for (const auto &cmd : list->CmdBuffer)
        {
            if (!cmd.ElemCount)
                continue;
            float x0 = FLT_MAX, y0 = FLT_MAX, x1 = -FLT_MAX, y1 = -FLT_MAX;
            for (unsigned i = 0; i < cmd.ElemCount; i++)
            {
                auto p = list->VtxBuffer[cmd.VtxOffset + list->IdxBuffer[cmd.IdxOffset + i]].pos;
                x0 = std::min(x0, p.x);
                y0 = std::min(y0, p.y);
                x1 = std::max(x1, p.x);
                y1 = std::max(y1, p.y);
            }
            x0 = std::max({x0, cmd.ClipRect.x, 0.f});
            y0 = std::max({y0, cmd.ClipRect.y, 0.f});
            x1 = std::min({x1, cmd.ClipRect.z, dd.DisplaySize.x});
            y1 = std::min({y1, cmd.ClipRect.w, dd.DisplaySize.y});
            if (x1 <= x0 || y1 <= y0)
                continue;
            bool covered = false;
            for (auto r : regions)
                covered |= r.left <= x0 && r.top <= y0 && r.right >= x1 && r.bottom >= y1;
            assert(covered);
        }
    return bytes;
}

void Frame()
{
    ImGui::NewFrame();
    g_plannerTabActive = false;
    RenderMainWindow("Ghost", 123);
    ImGui::Render();
    CheckHudRegions();
    auto *window = ImGui::FindWindowByName("It Steals | Practice Suite");
    assert(window && window->Pos.x >= 0 && window->Pos.y >= 0);
    if (window->Pos.x + window->Size.x > ImGui::GetIO().DisplaySize.x + 1)
        std::cerr << "Window overflow: pos " << window->Pos.x << "," << window->Pos.y << " size "
                  << window->Size.x << "," << window->Size.y << " display " << ImGui::GetIO().DisplaySize.x
                  << "," << ImGui::GetIO().DisplaySize.y << " review " << g_reviewTabActive << " wide "
                  << g_reviewWide << " reset " << g_resetLayout << std::endl;
    assert(window->Pos.x + window->Size.x <= ImGui::GetIO().DisplaySize.x + 1);
    assert(window->Pos.y + window->Size.y <= ImGui::GetIO().DisplaySize.y + 1);
    assert(ImGui::GetDrawData()->TotalVtxCount > 0);
}

void ExperienceCases(const std::string &output)
{
    auto &io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1000, 720);
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    suite_ui::preferences = {};
    suite_ui::notice.clear();
    suite_ui::ConsumeTone();
    bool value = false, disabled = false;
    int changes = 0;
    ImVec2 toggle{}, copy{};
    ImGuiID toggleId = 0, copyId = 0;
    float buttonWidth = 0;
    auto controls = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(620, 400), ImGuiCond_Always);
        ImGui::Begin("Experience interaction QA");
        ImGui::BeginDisabled(disabled);
        if (suite_ui::Checkbox("Measured movement", &value))
            changes++;
        toggle = ImGui::GetItemRectMin();
        toggle.x += 12;
        toggle.y += 12;
        toggleId = ImGui::GetItemID();
        ImGui::EndDisabled();
        if (suite_ui::Button("Copy seed"))
            suite_ui::CopyText("-674714359");
        copyId = ImGui::GetItemID();
        copy = ImGui::GetItemRectMin();
        copy.x += 20;
        copy.y += 12;
        buttonWidth = ImGui::GetItemRectSize().x;
        suite_ui::Section("Movement details", ImGuiTreeNodeFlags_DefaultOpen, [&] {
            ImGui::TextUnformatted("Five measured movement classes");
            suite_ui::Checkbox("Measured coverage", &g_reportsEnabled);
        });
        suite_ui::RenderNotice();
        ImGui::End();
        ImGui::Render();
        CheckHudRegions();
    };
    io.AddMousePosEvent(-1000, -1000);
    for (int i = 0; i < 3; i++)
        controls();
    assert(!value && changes == 0 && suite_ui::ConsumeTone() == suite_ui::Tone::None);
    io.AddMousePosEvent(toggle.x, toggle.y);
    controls();
    assert(suite_ui::ConsumeTone() == suite_ui::Tone::None);
    io.AddMouseButtonEvent(0, true);
    controls();
    controls();
    io.AddMouseButtonEvent(0, false);
    controls();
    assert(value && changes == 1 && suite_ui::ConsumeTone() == suite_ui::Tone::On);
    disabled = true;
    io.AddMouseButtonEvent(0, true);
    controls();
    io.AddMouseButtonEvent(0, false);
    controls();
    assert(value && changes == 1 && suite_ui::ConsumeTone() == suite_ui::Tone::None);
    disabled = false;
    ImGui::ActivateItemByID(toggleId);
    controls();
    assert(!value && changes == 2 && suite_ui::ConsumeTone() == suite_ui::Tone::Off);
    float width = buttonWidth;
    io.AddMousePosEvent(copy.x, copy.y);
    controls();
    io.AddMouseButtonEvent(0, true);
    controls();
    io.AddMouseButtonEvent(0, false);
    controls();
    assert(suite_ui::notice == "Copied to clipboard" && !suite_ui::noticeError &&
           suite_ui::ConsumeTone() == suite_ui::Tone::Confirm);
    auto originalId = copyId;
    controls();
    assert(copyId == originalId && buttonWidth == width && suite_ui::RecentSource(copyId));
    ImGui::NewFrame();
    suite_ui::BeginFrame(true, true);
    suite_ui::Request(suite_ui::Tone::Confirm);
    assert(suite_ui::ConsumeTone() == suite_ui::Tone::None);
    ImGui::Render();
    ImGui::NewFrame();
    suite_ui::BeginFrame(false, false);
    suite_ui::Request(suite_ui::Tone::Complete);
    assert(suite_ui::ConsumeTone() == suite_ui::Tone::None);
    ImGui::Render();
    ImGui::NewFrame();
    suite_ui::BeginFrame(true, false);
    suite_ui::Request(suite_ui::Tone::Click);
    suite_ui::Request(suite_ui::Tone::Confirm);
    suite_ui::Request(suite_ui::Tone::Click);
    assert(suite_ui::ConsumeTone() == suite_ui::Tone::Confirm);
    ImGui::Render();
    ImGui::NewFrame();
    suite_ui::BeginFrame(true, false);
    suite_ui::Request(suite_ui::Tone::Confirm);
    ImGui::Render();
    io.DeltaTime = .25f;
    ImGui::NewFrame();
    suite_ui::BeginFrame(true, false);
    assert(suite_ui::ConsumeTone() == suite_ui::Tone::None);
    ImGui::Render();
    io.DeltaTime = 1.f / 60;
    // Open/close a production animated section repeatedly; disabled motion
    // must restore the full body immediately, without stale clip/alpha state.
    for (bool motion : {true, false})
    {
        suite_ui::preferences.motion = motion;
        for (int frame = 0; frame < 20; frame++)
        {
            ImGui::NewFrame();
            suite_ui::BeginFrame(true, false);
            ImGui::SetNextWindowSize(ImVec2(500, 420), ImGuiCond_Always);
            ImGui::Begin("Section motion QA");
            ImGui::SetNextItemOpen(frame < 10, ImGuiCond_Always);
            suite_ui::Section("Details", 0, [&] {
                for (int i = 0; i < 4; i++)
                    ImGui::Text("Class %d: observed", i);
            });
            assert(std::isfinite(ImGui::GetCursorPosY()));
            ImGui::End();
            ImGui::Render();
            CheckHudRegions();
        }
    }
    suite_ui::preferences = {};
    suite_ui::notice.clear();
    suite_ui::lifecycle = {};
    suite_ui::lifecycleTime = 1000;
    for (int frame = 0; frame < 26; frame++)
    {
        suite_ui::lifecycleTime = 1000 + frame * .1;
        ImGui::NewFrame();
        suite_ui::BeginFrame(false, false);
        suite_ui::RenderLifecycle(true, false);
        ImGui::Render();
        suite_ui::CommitLifecycleFrame(true, suite_ui::LifecycleNow());
        CheckHudRegions();
        if (!output.empty() && (frame == 1 || frame == 3 || frame == 5 || frame == 14 || frame == 23))
            Dump(output + "/experience-start-" + std::to_string(frame) + ".draw");
    }
    assert(suite_ui::lifecycle.BeginExit(1003, suite_ui::preferences, true, false));
    for (int frame = 0; frame < 4; frame++)
    {
        suite_ui::lifecycleTime = 1003 + frame * .06;
        ImGui::NewFrame();
        suite_ui::BeginFrame(false, false);
        suite_ui::RenderLifecycle(true, false);
        ImGui::Render();
        CheckHudRegions();
        if (!output.empty() && frame == 1)
            Dump(output + "/experience-exit.draw");
    }
    suite_ui::lifecycle = {};
    suite_ui::lifecycleTime = 2000;
    ImGui::NewFrame();
    suite_ui::BeginFrame(false, true);
    suite_ui::RenderLifecycle(true, true);
    ImGui::Render();
    assert(ImGui::GetDrawData()->TotalVtxCount == 0);
    suite_ui::lifecycleTime = -1;
    suite_ui::lifecycle = {};
    io.DisplaySize = ImVec2(1000, 740);
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    g_pShared = &data;
    g_allOverlayEnabled = g_overlayVisible = true;
    g_speedrunMode = g_practiceMode = g_compactMode = false;
    g_reviewTabActive = false;
    g_selectRunReview = false;
    g_resetLayout = true;
    g_runReviewOpen = false;
    Frame();
    g_resetLayout = false;
    for (int tab : {0, 1, 2, 3})
    {
        suite_ui::navigation.Select(suite_ui::Page(tab), ImGui::GetTime());
        for (int frame = 0; frame < 18; frame++)
            Frame();
        if (!output.empty())
            Dump(output + "/experience-menu-" + std::to_string(tab) + ".draw");
    }
    std::cout
        << "Experience interaction: mouse/keyboard toggles fire once; disabled controls/hover silent; copied acknowledgement keeps button ID/size; cue priority/mute; section reduced motion; start/exit no-input graphics, active-run suppression and real menus: PASS\n";
}

void FeedbackRevisionCases(const std::string &output)
{
    auto saved = data;
    auto &io = ImGui::GetIO();
    io.DisplaySize = {1120, 960};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    data.currentSceneIndex = 3;
    data.timerState = 0;
    data.totalRuns = 128;
    data.totalFinishedRuns = 32;
    data.totalResets = 96;
    data.totalDeaths = 14;
    data.personalBestSeconds = 81.094;
    data.averagePace = 95;
    g_speedrunMode = true;
    g_pShared = &data;
    for (int n = 0; n < 3; n++)
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({18, 18});
        ImGui::SetNextWindowSize({650, 920});
        ImGui::Begin("Speedrun page QA", nullptr, ImGuiWindowFlags_NoSavedSettings);
        RenderSpeedrunPage();
        ImGui::End();
        ImGui::Render();
    }
    if (!output.empty())
        Dump(output + "/feedback-speedrun-page.draw");
    auto layout = g_speedrunLayout;
    layout.scale = 1;
    layout.width = 360;
    auto before = suite_hud::bounds[1];
    for (int n = 0; n < 3; n++)
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({18, 18});
        ImGui::SetNextWindowSize({650, 700});
        ImGui::Begin("HUD preview QA", nullptr, ImGuiWindowFlags_NoSavedSettings);
        g_validityBadgeDrawn = false;
        RenderHudPreview(layout, true);
        assert(!g_validityBadgeDrawn && suite_hud::bounds[1].x == before.x &&
               suite_hud::bounds[1].y == before.y && suite_hud::bounds[1].w == before.w &&
               suite_hud::bounds[1].h == before.h);
        ImGui::End();
        ImGui::Render();
    }
    if (!output.empty())
        Dump(output + "/feedback-live-hud-preview.draw");
    data = saved;
    g_speedrunMode = false;
}

void AppearanceCases(const std::string &output)
{
    auto &io = ImGui::GetIO();
    auto saved = suite_ui::appearance;
    g_allOverlayEnabled = g_overlayVisible = true;
    g_compactMode = g_speedrunMode = g_practiceMode = false;
    g_reviewWide = g_runReviewOpen = false;
    g_pShared = &data;
    int pages = 0;
    auto findTabs = [](const char *first, int count) -> ImGuiTabBar * {
        for (int i = 0; i < GImGui->TabBars.GetMapSize(); i++)
        {
            auto *bar = GImGui->TabBars.TryGetMapData(i);
            if (bar && bar->CurrFrameVisible == ImGui::GetFrameCount() && bar->Tabs.Size == count &&
                std::strcmp(ImGui::TabBarGetTabName(bar, &bar->Tabs[0]), first) == 0)
                return bar;
        }
        return nullptr;
    };
    for (ImVec2 size : {ImVec2(480, 360), ImVec2(1000, 740), ImVec2(1920, 1080)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = size;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            g_resetLayout = true;
            Frame();
            g_resetLayout = false;
            suite_ui::navigation.Select(suite_ui::Page::Settings, ImGui::GetTime());
            Frame();
            Frame();
            ImGuiWindow *child = nullptr;
            for (auto *window : GImGui->Windows)
                if (std::strstr(window->Name, "/SettingsScroll_") &&
                    window->LastFrameActive == ImGui::GetFrameCount())
                    child = window;
            assert(child);
            auto *tabs = findTabs("Appearance", 4);
            assert(tabs);
            auto settingsID = tabs->ID;
            std::array<ImGuiID, 4> pageIDs{};
            for (int i = 0; i < 4; i++)
                pageIDs[i] = tabs->Tabs[i].ID;
            for (int page = 0; page < 4; page++)
            {
                GImGui->TabBars.GetByKey(settingsID)->NextSelectedTabId = pageIDs[page];
                for (int frame = 0; frame < 14; frame++)
                    Frame();
                pages++;
                assert(child->Pos.x >= 0 && child->Pos.y >= 0 && child->Pos.x + child->Size.x <= size.x + 1);
                if (!output.empty() && size.x == 1000 && dpi == 1)
                    Dump(output + "/appearance-page-" + std::to_string(page) + ".draw");
                if (page == 1)
                {
                    auto *profiles = findTabs("Compact HUD", 3);
                    assert(profiles && profiles->Tabs.Size == 3);
                    profiles->NextSelectedTabId = profiles->Tabs[2].ID;
                    for (int frame = 0; frame < 14; frame++)
                        Frame();
                    if (!output.empty() && size.x == 1000 && dpi == 1)
                        Dump(output + "/appearance-speedrun-editor.draw");
                }
            }
        }
    // Exercise font replacement only between frames, including the default
    // pointer and the three independently used font handles after every clear.
    io.DisplaySize = {1280, 720};
    for (int choice : {0, 8, 9, 2, 0})
    {
        suite_ui::appearance.menuFont = choice;
        suite_ui::appearance.hudFont = (choice + 1) % kSuiteFontCount;
        suite_ui::appearance.timerFont = 8;
        suite_ui::fontsDirty = true;
        ApplyTheme(1);
        io.Fonts->Build();
        io.Fonts->SetTexID(1);
        assert(!suite_ui::fontsDirty && io.Fonts->Fonts.Size == 8 && io.FontDefault == io.Fonts->Fonts[0] &&
               suite_ui::hudFont == io.Fonts->Fonts[1] && g_timerFont == io.Fonts->Fonts[2]);
        Frame();
    }
    size_t fontCount = io.Fonts->Fonts.Size;
    suite_ui::appearance.colors[AppearancePreferences::Button] = {.23f, .09f, .31f};
    suite_ui::appearance.colors[AppearancePreferences::Animation] = {.95f, .40f, .65f};
    suite_ui::appearance.colors[AppearancePreferences::Outline] = {.48f, .29f, .59f};
    suite_ui::appearance.colors[AppearancePreferences::HudBackground] = {.14f, .04f, .12f};
    suite_ui::appearance.colors[AppearancePreferences::HudText] = {.99f, .90f, .79f};
    suite_ui::appearance.colors[AppearancePreferences::HudAccent] = {.95f, .40f, .65f};
    suite_ui::ApplyPalette(g_mainBgCol, g_textCol, g_headerCol);
    assert(io.Fonts->Fonts.Size == int(fontCount) && !suite_ui::fontsDirty);
    assert(ImGui::GetStyle().Colors[ImGuiCol_Button].x == .23f &&
           ImGui::GetStyle().Colors[ImGuiCol_Border].z == .59f);
    data.timerState = data.runValidity = 1;
    data.seedKnown = data.seedSourcesVerified = data.rngPatchComplete = data.observedFromStart =
        data.speedrunModeActive = 1;
    data.runAssisted = data.practiceModeActive = 0;
    data.runWarnings = data.runUsedFeatures = data.trainerFeatures = 0;
    data.seedKind = 1;
    for (bool follows : {false, true})
        for (float opacity : {.25f, 1.f})
        {
            suite_ui::appearance.hudFollowsTheme = follows;
            OverlayLayout layout(true);
            layout.opacity = opacity;
            for (int frame = 0; frame < 3; frame++)
            {
                ImGui::NewFrame();
                g_validityBadgeDrawn = false;
                RenderOverlayPanel("Custom palette QA", data, layout, true, "Classic", .01);
                ImGui::Render();
                CheckHudRegions();
            }
            bool bg = false, valid = false;
            auto expected = suite_ui::HudBackground(opacity);
            assert(expected.x == (follows ? g_mainBgCol.x : .14f));
            for (auto v : ImGui::FindWindowByName("Custom palette QA")->DrawList->VtxBuffer)
                bg |= v.col == ImGui::ColorConvertFloat4ToU32(expected);
            for (auto v : ImGui::GetForegroundDrawList()->VtxBuffer)
                valid |= v.col == ImGui::ColorConvertFloat4ToU32({.30f, .94f, .40f, 1});
            assert(bg && valid && g_validityBadgeDrawn);
            if (!output.empty() && !follows && opacity == 1)
                Dump(output + "/appearance-custom-hud.draw");
        }
    suite_ui::appearance = saved;
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    if (!output.empty())
    {
        io.DisplaySize = {1060, 840};
        suite_ui::preferences.motion = false;
        g_allOverlayEnabled = g_overlayVisible = true;
        g_compactMode = g_speedrunMode = g_practiceMode = false;
        g_resetLayout = true;
        data.seedActive = 0;
        Frame();
        g_resetLayout = false;
        auto *root = ImGui::FindWindowByName("It Steals | Practice Suite");
        ImGui::SetWindowPos(root, {20, 20});
        ImGui::SetWindowSize(root, {590, 800});
        suite_ui::navigation.Select(suite_ui::Page::Settings, ImGui::GetTime());
        Frame();
        Frame();
        auto *settings = findTabs("Appearance", 4);
        assert(settings);
        settings->NextSelectedTabId = settings->Tabs[0].ID;
        Frame();
        data.currentSceneIndex = 3;
        data.hideAndSeekMode = data.timerMode = 0;
        data.wallCount = 5;
        float boxes[] = {0, 0, 40, 2, 0, 38, 40, 40, 0, 0, 2, 40, 38, 0, 40, 40, 18, 10, 20, 28};
        std::copy(boxes, boxes + 20, data.wallBoxes);
        data.crystalCount = 6;
        float gems[] = {6, 6, 12, 32, 32, 8, 30, 30, 8, 22, 26, 18};
        std::copy(gems, gems + 12, data.crystalPositions);
        g_mapOptions = {};
        g_mapOptions.follow = false;
        g_mapOptions.anchor = 3;
        g_showMinimapWindow = true;
        g_minimapWidth = g_minimapHeight = 300;
        g_minimapOpacity = 1;
        for (int frame = 0; frame < 3; frame++)
        {
            ImGui::NewFrame();
            suite_ui::BeginFrame(true, false);
            RenderMainWindow("Classic", 482915720);
            OverlayLayout live(true);
            live.anchor = 1;
            live.width = 350;
            live.opacity = .92f;
            RenderOverlayPanel("Showcase HUD", data, live, true, "Classic", .01);
            RenderMinimapWindow(12, 16, 30, 28, 24, 6, 5, 0, nullptr, 0, 40, 0, 40, true, true, false);
            ImGui::Render();
            CheckHudRegions();
        }
        Dump(output + "/appearance-showcase.draw");
        suite_ui::preferences.motion = true;
    }
    std::cout
        << "Appearance: " << pages
        << " actual settings subpages at three viewports/two DPIs; safe repeated font-atlas replacement; palette edits without font rebuilds; inherited/custom HUD colors at both opacity extremes; mandatory green status independent of palette: PASS\n";
}

void MilestoneCases(const std::string &output)
{
    auto &io = ImGui::GetIO();
    auto saved = data;
    io.DisplaySize = {1920, 1080};
    g_pShared = &data;
    g_allOverlayEnabled = true;
    g_hudLayout = OverlayLayout(false);
    g_speedrunLayout = OverlayLayout(true);
    g_speedrunMode = g_practiceMode = g_compactMode = false;
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    assert(io.FontDefault->FontSize == 18 && suite_ui::introBodyFont->FontSize == 20 &&
           suite_ui::introTitleFont->FontSize == 32 && suite_ui::hudFont->FontSize == 19 &&
           suite_ui::labelFont->FontSize == 16);
    assert(HudFieldAllowed(OverlayField::Rotation, true, true) &&
           HudFieldAllowed(OverlayField::Coordinates, true, true));
    for (int f = int(OverlayField::PlayerState); f < kOverlayFields; f++)
    {
        assert(!HudFieldAllowed(OverlayField(f), true, false));
        assert(!HudFieldAllowed(OverlayField(f), false, true));
        assert(HudFieldAllowed(OverlayField(f), false, false));
    }
    suite_ui::lifecycleTime = 3000;
    data.currentSceneIndex = 13;
    g_practiceMode = g_seededPractice = true;
    g_commands = {};
    assert(SetSpeedrunMode(true, true));
    assert(g_speedrunMode && !g_practiceMode && !g_seededPractice && g_selectedSpeedrunLevel == 7 &&
           g_commands.speedrunLoadLevelCmd == 1 && suite_ui::GameTonePending());
    assert(suite_ui::ConsumeGameTone(true) == suite_ui::Tone::On &&
           suite_ui::ConsumeGameTone(true) == suite_ui::Tone::None);
    assert(SetSpeedrunMode(false));
    assert(!g_speedrunMode && suite_ui::ConsumeGameTone(true) == suite_ui::Tone::Off);
    suite_ui::PostGameTone(suite_ui::Tone::On);
    suite_ui::lifecycleTime += .3;
    assert(suite_ui::ConsumeGameTone(true) == suite_ui::Tone::None);
    data.timerState = 1;
    data.reviewReady = 1;
    g_commands.runReviewRequest = 0;
    g_runReviewOpen = false;
    RequestRunReview();
    assert(g_commands.runReviewRequest == 0 && !g_runReviewOpen);
    practice::ReplayInfo row;
    row.id = 42;
    row.scene = 3;
    OpenSavedReplay(row);
    assert(!g_runReviewOpen);
    ImGui::NewFrame();
    ImGui::SetNextWindowSize({600, 300});
    ImGui::Begin("Active replay guard QA");
    RenderRunReview();
    assert(ImGui::GetCursorPosY() < 100);
    ImGui::End();
    ImGui::Render();
    data = {};
    data.currentSceneIndex = 3;
    data.sceneEpoch = 12;
    data.timerState = 2;
    data.runOutcome = 1;
    data.currentRunTimeSeconds = 45.123;
    data.previousBestSeconds = 46.5;
    data.lastRunPb = 1;
    data.runValidity = data.seedKind = data.seedKnown = data.seedSourcesVerified = data.rngPatchComplete =
        data.observedFromStart = data.externalScanStatus = data.reviewReady = 1;
    data.reviewRunId = 42;
    data.reviewScene = 3;
    data.reviewSpawnSeed = 482915720;
    std::strcpy(data.gameAssemblyHash, "ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8");
    data.speedrunModeActive = 1;
    g_speedrunMode = true;
    g_practiceMode = false;
    g_runReviewOpen = false;
    g_overlayVisible = false;
    g_speedrunLayout.anchor = 0;
    for (float dpi : {1.f, 1.5f})
        for (bool valid : {false, true})
        {
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            data.runValidity = valid ? 1 : 2;
            data.runWarnings = valid ? 0 : 8192;
            auto &p = suite_run::presentation;
            p.Reset();
            p.hasResult = p.ask = true;
            p.completed = data;
            p.completedSession = g_session;
            p.scan.elapsed = suite_run::ScanTimeline::Duration;
            p.scene = 3;
            p.epoch = 12;
            p.ended = 3001;
            suite_ui::lifecycleTime = 3002;
            for (int frame = 0; frame < 3; frame++)
            {
                ImGui::NewFrame();
                RenderTimerOverlay("Classic");
                RenderRunExperience();
                ImGui::Render();
                CheckHudRegions();
            }
            auto *result = ImGui::FindWindowByName("##RunResult");
            auto *toast = ImGui::FindWindowByName("##RunNotification");
            assert(result && result->LastFrameActive == ImGui::GetFrameCount() && result->ScrollMax.y < 1 &&
                   !(result->Flags & ImGuiWindowFlags_NoInputs));
            assert(toast && toast->LastFrameActive == ImGui::GetFrameCount() &&
                   (toast->Flags & ImGuiWindowFlags_NoInputs));
            suite_run::Rect a{result->Pos.x, result->Pos.y, result->Size.x, result->Size.y},
                b{toast->Pos.x, toast->Pos.y, toast->Size.x, toast->Size.y};
            assert(!a.Intersects(b));
            auto *hud = ImGui::FindWindowByName("Speedrun overlay");
            assert(!a.Intersects({hud->Pos.x, hud->Pos.y, hud->Size.x, hud->Size.y}));
            assert(!b.Intersects({hud->Pos.x, hud->Pos.y, hud->Size.x, hud->Size.y}));
            if (!output.empty() && dpi == 1)
                Dump(output + (valid ? "/milestone-finish-valid.draw" : "/milestone-finish-invalid.draw"));
        }
    suite_run::presentation.Reset();
    suite_ui::lifecycleTime = 3010;
    SetSpeedrunMode(false);
    for (int frame = 0; frame < 3; frame++)
    {
        ImGui::NewFrame();
        RenderRunExperience();
        ImGui::Render();
        CheckHudRegions();
    }
    if (!output.empty())
        Dump(output + "/milestone-mode-toast.draw");
    g_plannerOptions.level = 0;
    g_plannerOptions.filters[0].spawn = 1;
    auto spawnSource = StreamingSource();
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    for (int frame = 0; frame < 3; frame++)
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowSize({660, 330});
        ImGui::Begin("Spawn preview QA");
        RenderSpawnPreview(*spawnSource);
        ImGui::End();
        ImGui::Render();
        CheckHudRegions();
    }
    bool spawnGreen = false;
    for (auto v : ImGui::FindWindowByName("Spawn preview QA")->DrawList->VtxBuffer)
        spawnGreen |= (v.col & 0xffffffu) == (IM_COL32(90, 245, 145, 255) & 0xffffffu);
    assert(spawnGreen);
    if (!output.empty())
        Dump(output + "/milestone-spawn-preview.draw");
    auto previewFixture = SeedFixture();
    previewFixture.recipe = 1;
    previewFixture.epoch = 21;
    auto previewFrame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({30, 30});
        ImGui::SetNextWindowSize({780, 900});
        ImGui::Begin("Seed preview reset QA");
        RenderSeedMapTab(false);
        ImGui::End();
        ImGui::Render();
        CheckHudRegions();
    };
    for (int seed = 0; seed < 2; seed++)
    {
        previewFixture.seed++;
        g_seedSearch.HidePreview();
        g_mapWorker.Submit(EncodeFixture(previewFixture), g_session);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline)
        {
            auto map = g_mapWorker.Get(0);
            if (map && !map->working && map->data && map->data->seed == previewFixture.seed &&
                map->data->epoch == 21)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(g_mapWorker.Get(0) && g_mapWorker.Get(0)->data->seed == previewFixture.seed);
        g_selectSeedPreview = true;
        g_seedPreviewPlayback.suggested = true;
        for (int frame = 0; frame < 3; frame++)
            previewFrame();
        assert(!g_selectSeedPreview && g_seedPreviewPlayback.suggested && g_seedPreviewPlayback.Complete());
    }
    if (!output.empty())
        Dump(output + "/milestone-seed-preview.draw");
    data.currentSceneIndex = 13;
    data.gameTargetFps = 90;
    data.gameVSync = 1;
    data.runWarnings = 8192;
    data.rngPatchComplete = 0;
    data.timerState = 2;
    io.AddMousePosEvent(-1000, -1000);
    auto diagnosticFrame = [&] {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({30, 30});
        ImGui::SetNextWindowSize({850, 950});
        ImGui::Begin("Diagnostic scan QA");
        RenderDiagnosticScan();
        ImGui::End();
        ImGui::Render();
        CheckHudRegions();
    };
    diagnosticFrame();
    auto *diagnostic = ImGui::FindWindowByName("Diagnostic scan QA");
    assert(diagnostic);
    ImGui::ActivateItemByID(diagnostic->GetID("Run diagnostic scan"));
    diagnosticFrame();
    for (int frame = 0; frame < 3; frame++)
        diagnosticFrame();
    if (!output.empty())
        Dump(output + "/milestone-diagnostic-scan.draw");
    if (!output.empty())
    {
        g_speedrunMode = g_practiceMode = false;
        g_overlayVisible = false;
        g_hudLayout.visible[int(OverlayField::Coordinates)] = true;
        data.speedrunModeActive = 0;
        data.playerReady = 1;
        data.health = 2;
        data.canMove = 1;
        data.smoothedSpeed = 9.7f;
        data.playerRotY = 123.4f;
        data.playerX = 12.34f;
        data.playerY = 1.75f;
        data.playerZ = -42.65f;
        ApplyTheme(1);
        io.Fonts->Build();
        io.Fonts->SetTexID(1);
        for (int frame = 0; frame < 3; frame++)
        {
            ImGui::NewFrame();
            RenderOverlayPanel("Compact milestone QA", data, g_hudLayout, false, "Classic", .01);
            ImGui::Render();
            CheckHudRegions();
        }
        Dump(output + "/milestone-compact.draw");
        suite_ui::lifecycle = {};
        suite_ui::lifecycleTime = 3020;
        suite_ui::lifecycle.Consider(3020, true, suite_ui::preferences, false);
        suite_ui::lifecycle.Presented(3020);
        suite_ui::lifecycleTime = 3021;
        ImGui::NewFrame();
        suite_ui::RenderLifecycle(true, false);
        ImGui::Render();
        CheckHudRegions();
        Dump(output + "/milestone-intro.draw");
    }
    suite_run::presentation.Reset();
    suite_ui::lifecycleTime = -1;
    data = saved;
    std::cout
        << "7.1.0 real UI: native 1080p fonts, restricted HUD fields, single expiring mode cue, active-run replay guard, valid/invalid no-overlap result cards at 100/150 percent DPI, pulsing spawn preview and optimal-route reset on two new seeds: PASS\n";
}

void PersistentFeedbackCases(const std::string &output)
{
    auto &io = ImGui::GetIO();
    auto saved = data;
    g_pShared = &data;
    g_speedrunMode = g_allOverlayEnabled = true;
    g_practiceMode = g_compactMode = g_runReviewOpen = false;
    auto &p = suite_run::presentation;
    for (ImVec2 viewport : {ImVec2(1920, 1080), ImVec2(480, 360)})
        for (float dpi : {1.f, 1.5f})
            for (bool restoreMenu : {false, true})
            {
                io.DisplaySize = viewport;
                ApplyTheme(dpi);
                io.Fonts->Build();
                io.Fonts->SetTexID(1);
                g_overlayVisible = restoreMenu;
                data = {};
                data.currentSceneIndex = 3;
                data.sceneEpoch = 1;
                data.timerState = 1;
                data.currentRunTimeSeconds = 42.125;
                p.Reset();
                p.Observe(&data, 701, 10);
                data.timerState = 2;
                data.runOutcome = 1;
                data.runEndedUtc = 123;
                data.reviewReady = 1;
                data.reviewRunId = 701;
                data.reviewSpawnSeed = 12345;
                p.Observe(&data, 701, 20);
                // Classic's automatic ending transition must not erase the result.
                data.currentSceneIndex = 11;
                data.sceneEpoch = 2;
                data.currentRunTimeSeconds = 0;
                p.Observe(&data, 701, 20.1);
                suite_ui::lifecycleTime = 3620;
                p.Observe(&data, 701, 3620);
                assert(p.hasResult && !p.ask && p.completed.currentRunTimeSeconds == 42.125 &&
                       p.WantsInput(&data, false));
                auto render = [&] {
                    ImGui::NewFrame();
                    suite_ui::BeginFrame(true, false);
                    RenderTimerOverlay("Classic");
                    RenderMainWindow("Ending", 0);
                    RenderRunExperience();
                    ImGui::Render();
                    CheckHudRegions();
                };
                io.AddMousePosEvent(-1000, -1000);
                for (int frame = 0; frame < 3; frame++)
                    render();
                auto *result = ImGui::FindWindowByName("##RunResult");
                assert(result && result->LastFrameActive == ImGui::GetFrameCount() &&
                       !(result->Flags & ImGuiWindowFlags_NoInputs));
                auto *root = ImGui::FindWindowByName("It Steals | Practice Suite");
                assert(!root || root->LastFrameActive != ImGui::GetFrameCount());
                assert(result->Pos.x >= 0 && result->Pos.y >= 0 &&
                       result->Pos.x + result->Size.x <= viewport.x &&
                       result->Pos.y + result->Size.y <= viewport.y);
                if (!output.empty() && viewport.x == 1920 && dpi == 1 && !restoreMenu)
                    Dump(output + "/persistent-result-after-ending.draw");
                lastInteractiveRequest = !restoreMenu;
                if (viewport.x == 1920 && dpi == 1 && !restoreMenu)
                {
                    auto at = ImVec2(result->Pos.x + result->Size.x - 35 * dpi, result->Pos.y + 31 * dpi);
                    io.AddMousePosEvent(at.x, at.y);
                    render();
                    io.AddMouseButtonEvent(0, true);
                    render();
                    io.AddMouseButtonEvent(0, false);
                    render();
                }
                else
                {
                    ImGui::ActivateItemByID(result->GetID("X##CloseRunResult"));
                    render();
                }
                assert(!p.hasResult && lastInteractiveRequest == restoreMenu);
                p.Observe(&data, 701, 3621);
                assert(!p.hasResult);
                render();
                assert(result->LastFrameActive != ImGui::GetFrameCount());
            }
    io.DisplaySize = {1920, 1080};
    g_speedrunMode = false;
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    suite_ui::preferences = {};
    suite_ui::lifecycle = {};
    p.Reset();
    auto intro = [&](double now, bool visible, bool running, bool ready, bool presented) {
        suite_ui::lifecycleTime = now;
        ImGui::NewFrame();
        suite_ui::BeginFrame(false, running);
        suite_ui::RenderLifecycle(visible, running, ready);
        ImGui::Render();
        CheckHudRegions();
        suite_ui::CommitLifecycleFrame(presented, now);
    };
    assert(suite_ui::lifecycle.WantsStartup(0, suite_ui::preferences, false));
    intro(0, false, false, true, false);
    intro(1, true, false, false, false);
    intro(2, true, true, true, false);
    assert(!suite_ui::lifecycle.considered);
    intro(10, true, false, true, false);
    intro(100, true, false, true, false);
    assert(!suite_ui::lifecycle.presented && suite_ui::lifecycle.StartupAge(100) == 0 &&
           suite_ui::introFrameDrawn);
    intro(101, true, false, true, true);
    assert(suite_ui::lifecycle.presented && suite_ui::lifecycle.start == 101);
    intro(101.8, true, false, true, true);
    if (!output.empty())
        Dump(output + "/intro-first-visible-presentation.draw");
    intro(101.9, false, false, true, false);
    intro(1000, true, false, true, true);
    assert(std::abs(suite_ui::lifecycle.StartupAge(1000) - .9) < 1e-6);
    intro(1001.8, true, false, true, true);
    assert(!suite_ui::introFrameDrawn &&
           !suite_ui::lifecycle.WantsStartup(1001.8, suite_ui::preferences, false));
    suite_ui::preferences.motion = false;
    suite_ui::lifecycle = {};
    intro(1100, true, false, true, true);
    assert(suite_ui::introFrameDrawn);
    suite_ui::preferences.launchGraphic = false;
    intro(1101, true, false, true, true);
    assert(!suite_ui::introFrameDrawn);
    suite_ui::preferences = {};
    suite_ui::lifecycle = {};
    suite_ui::lifecycleTime = -1;
    data = saved;
    p.Reset();
    std::cout
        << "Persistent feedback: result survives automatic ending and one hour, real mouse X / keyboard close restores prior input, bounded scrollable dialog at 480/1920 and 100/150 percent DPI; intro waits for ready game and successful presentation, defers active run, pauses hidden, supports reduced motion and explicit off: PASS\n";
}

void FinishVerificationCases(const std::string &output)
{
    auto saved = data;
    auto &io = ImGui::GetIO();
    auto &p = suite_run::presentation;
    constexpr int64_t id = 638961530000000000ll;
    auto prepare = [&] {
        data = {};
        data.currentSceneIndex = data.reviewScene = 3;
        data.sceneEpoch = 12;
        data.timerState = 2;
        data.runOutcome = 1;
        data.currentRunTimeSeconds = 45.123;
        data.previousBestSeconds = 46.5;
        data.lastRunPb = 1;
        data.runValidity = data.seedKind = data.seedKnown = data.seedSourcesVerified = data.rngPatchComplete =
            data.observedFromStart = data.externalScanStatus = data.reviewReady = 1;
        data.reviewRunId = id;
        data.reviewSpawnSeed = data.runSeed = data.initialRngSeed = 482915720;
        data.reviewSamples = 2257;
        data.runStartedUtc = id;
        data.runEndedUtc = id + 451230000;
        data.speedrunModeActive = 1;
        data.pid = 1234;
        data.runSceneIndex = 3;
        std::strcpy(data.version, "v7.1.0");
        std::strcpy(data.validationDetail,
                    "Local checks passed; external memory changes are not fully detectable");
        std::strcpy(data.gameAssemblyHash,
                    "ead3f5f861ad7988ad5fdb6f188bb3f77639927dd496b73066860d6c853f0dd8");
        std::strcpy(data.gameVersion, "9");
        std::strcpy(data.unityVersion, "2021.2.19f1");
        g_pShared = &data;
        g_session = 703;
        g_speedrunMode = g_allOverlayEnabled = true;
        g_practiceMode = g_compactMode = g_runReviewOpen = g_overlayVisible = false;
        g_commands = {};
        pendingRequest = false;
        p.Reset();
        p.hasResult = p.ask = true;
        p.completed = data;
        p.completedSession = g_session;
        p.scene = 3;
        p.epoch = 12;
        p.ended = 100;
        suite_ui::lifecycleTime = 100;
        suite_ui::preferences.motion = true;
    };
    auto render = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        RenderRunExperience();
        ImGui::Render();
        CheckHudRegions();
    };
    io.DisplaySize = {1920, 1080};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    io.AddMousePosEvent(-1000, -1000);
    prepare();
    assert(FinishedReplayAvailable().source == FinishedReplaySource::Helper);
    render();
    for (double phase : {.25, .50,  .75,  1.,   1.25, 1.50, 1.75, 2.,   2.25, 2.50, 2.75,
                         3.,  3.25, 3.50, 3.75, 4.,   4.25, 4.50, 4.75, 5.,   5.40})
    {
        suite_ui::lifecycleTime = 100 + phase;
        render();
        render();
        assert(p.scan.elapsed >= std::min(phase, suite_run::ScanTimeline::Duration) - 1e-6);
        assert(p.completed.currentRunTimeSeconds == 45.123 && suite_run::FinalValid(p.completed));
        if (!output.empty())
            Dump(output + "/finish-scan-" + std::to_string(int(phase * 100)) + ".draw");
    }
    assert(p.scan.Done(true) && p.scan.Completed(true) == 7 && p.scan.notified);
    auto *result = ImGui::FindWindowByName("##RunResult");
    auto childOf = [](ImGuiWindow *parent) {
        for (auto *child : parent->DC.ChildWindows)
            if (child->ChildId == parent->GetID("##RunResultDetails"))
                return child;
        return static_cast<ImGuiWindow *>(nullptr);
    };
    auto *details = childOf(result);
    assert(details && details->ScrollMax.y < 1);
    bool white = false, green = false;
    for (auto v : details->DrawList->VtxBuffer)
    {
        white |= v.col == ImGui::ColorConvertFloat4ToU32(ImVec4(.95f, .97f, .98f, 1));
        green |= v.col == ImGui::ColorConvertFloat4ToU32(ImVec4(.34f, .94f, .61f, 1));
    }
    assert(white && green);
    // The native-size detail font is independent of the optional HUD scale.
    assert(suite_ui::resultDetailFont->FontSize == 16);
    assert(p.Inspecting(true) == 4 && p.scan.FieldsShown(true) == 42);
    auto inspectorOf = [](ImGuiWindow *parent) {
        for (auto *child : parent->DC.ChildWindows)
            if (std::strstr(child->Name, "##TechnicalInspector"))
                return child;
        return static_cast<ImGuiWindow *>(nullptr);
    };
    auto *table = GImGui->Tables.GetByKey(details->GetID("##ResultInspection"));
    assert(table);
    auto pin = ImVec2(table->Columns[0].MinX + 24, table->RowPosY1 + 2 * 64 + 20);
    io.AddMousePosEvent(pin.x, pin.y);
    render();
    io.AddMouseButtonEvent(0, true);
    render();
    io.AddMouseButtonEvent(0, false);
    render();
    assert(p.inspected == 2 && p.Inspecting(true) == 2);
    auto *inspector = inspectorOf(details);
    assert(inspector && inspector->ScrollMax.y < 1);
    ImGui::ActivateItemByID(inspector->GetID("Copy report"));
    render();
    std::string copied = ImGui::GetClipboardText();
    assert(copied.find("Evidence fields: 42") != std::string::npos &&
           copied.find(data.gameAssemblyHash) != std::string::npos &&
           copied.find(std::to_string(id)) != std::string::npos && p.hasResult && g_speedrunMode &&
           g_commands.runReviewRequest == 0);
    ImGui::ActivateItemByID(inspector->GetID("Follow scan"));
    render();
    assert(p.inspected == -1 && p.Inspecting(true) == 4);
    suite_ui::lifecycleTime = 4000;
    p.Observe(&data, g_session, 4000);
    assert(!p.ask && p.hasResult && FinishedReplayAvailable());
    render();
    ImGui::ActivateItemByID(result->GetID("Analyse Run"));
    render();
    assert(!p.hasResult && g_commands.runReviewRequest == 1 && g_commands.runReviewId == id &&
           g_reviewTarget == id && g_runReviewOpen && g_selectRunReview && g_reviewWide && g_speedrunMode &&
           !g_practiceMode && g_overlayVisible && lastInteractiveRequest);
    // Real mouse activation, before the scan finishes, with fixed footer
    // actions reachable at small sizes and high DPI.
    for (ImVec2 viewport : {ImVec2(1920, 1080), ImVec2(480, 360)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = viewport;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            prepare();
            suite_ui::lifecycleTime = 100.3;
            io.AddMousePosEvent(-1000, -1000);
            for (int frame = 0; frame < 3; frame++)
                render();
            result = ImGui::FindWindowByName("##RunResult");
            assert(result->ScrollMax.y < 1 && result->Pos.y + result->Size.y <= viewport.y &&
                   p.scan.Completed(true) == 0);
            if (!output.empty() && viewport.x == 480 && dpi == 1.5f)
                Dump(output + "/finish-small-150.draw");
            auto click = ImVec2(result->Pos.x + 100 * dpi, result->Pos.y + result->Size.y - 60 * dpi);
            io.AddMousePosEvent(click.x, click.y);
            render();
            io.AddMouseButtonEvent(0, true);
            render();
            io.AddMouseButtonEvent(0, false);
            render();
            assert(!p.hasResult && g_runReviewOpen && g_reviewTarget == id && g_speedrunMode &&
                   g_commands.runReviewId == id);
        }
    // The compact inspector opens from the fixed footer and keeps its report
    // action reachable at small sizes / DPI, without acknowledging the result.
    for (float dpi : {1.f, 1.5f})
    {
        io.DisplaySize = {480, 360};
        ApplyTheme(dpi);
        io.Fonts->Build();
        io.Fonts->SetTexID(1);
        prepare();
        suite_ui::lifecycleTime = 101;
        io.AddMousePosEvent(-1000, -1000);
        for (int frame = 0; frame < 3; frame++)
            render();
        result = ImGui::FindWindowByName("##RunResult");
        ImGui::ActivateItemByID(result->GetID("Technical scan"));
        render();
        render();
        assert(!GImGui->OpenPopupStack.empty());
        auto *popup = GImGui->OpenPopupStack.back().Window;
        assert(popup && popup->Size.x <= 480 && popup->Size.y <= 360);
        inspector = inspectorOf(popup);
        assert(inspector && inspector->ScrollMax.y < 1);
        ImGui::ActivateItemByID(inspector->GetID("Copy report"));
        render();
        assert(std::string(ImGui::GetClipboardText()).find(data.gameAssemblyHash) != std::string::npos &&
               p.hasResult && g_speedrunMode && g_commands.runReviewRequest == 0);
        if (!output.empty() && dpi == 1.5f)
        {
            Dump(output + "/technical-small-150.draw");
        }
        ImGui::ActivateItemByID(popup->GetID("Close details"));
        render();
        render();
        assert(GImGui->OpenPopupStack.empty() && p.hasResult);
    }
    io.DisplaySize = {1920, 1080};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    prepare();
    suite_ui::preferences.motion = false;
    suite_ui::lifecycleTime = 101;
    render();
    render();
    assert(p.scan.Completed(false) == 7 && p.scan.Done(false));
    data.runWarnings = 8192;
    data.runValidity = 2;
    p.completed = data;
    p.evidenceBuilt = false;
    render();
    bool red = false;
    for (const auto *list : ImGui::GetDrawData()->CmdLists)
        for (auto v : list->VtxBuffer)
            red |= v.col == ImGui::ColorConvertFloat4ToU32(ImVec4(1, .36f, .38f, 1));
    assert(red && !suite_run::FinalValid(p.completed));
    if (!output.empty())
        Dump(output + "/finish-invalid.draw");
    prepare();
    data.currentSceneIndex = 11;
    data.sceneEpoch = 13;
    p.Observe(&data, g_session, 101);
    assert(!p.CanAnalyze(&data, 101) && FinishedReplayAvailable().source == FinishedReplaySource::Helper &&
           AnalyseFinishedRun() && g_reviewTarget == id && g_reviewLevel == 0 && g_speedrunMode);
    prepare();
    pendingRequest = true;
    assert(!FinishedReplayAvailable() && !AnalyseFinishedRun());
    render();
    result = ImGui::FindWindowByName("##RunResult");
    ImGui::ActivateItemByID(result->GetID("Analyse Run"));
    render();
    assert(p.hasResult && !g_runReviewOpen && g_commands.runReviewRequest == 0);
    pendingRequest = false;
    data.timerState = 1;
    assert(!FinishedReplayAvailable() && !AnalyseFinishedRun());
    data.timerState = 2;
    data.reviewRunId = id + 1;
    assert(!FinishedReplayAvailable() && !AnalyseFinishedRun() &&
           g_commands.runReviewId == 0); // Never open a newer attempt.
    data.reviewRunId = id;
    data.reviewScene = 5;
    assert(!FinishedReplayAvailable() && !AnalyseFinishedRun());
    data.reviewScene = 3;
    g_session++;
    assert(!FinishedReplayAvailable());
    prepare();
    p.completed.reviewRunId = 0;
    assert(!FinishedReplayAvailable() && !AnalyseFinishedRun());
    suite_ui::lifecycleTime = 101;
    render();
    result = ImGui::FindWindowByName("##RunResult");
    ImGui::ActivateItemByID(result->GetID("Replays"));
    render();
    assert(!p.hasResult && !g_runReviewOpen && g_selectRunReview && g_overlayVisible && g_speedrunMode &&
           lastInteractiveRequest);
    // Persist real, decodable fixture files, then request the middle ID while
    // a different helper session advertises a newer attempt.
    auto folder = std::filesystem::path(output.empty() ? std::filesystem::temp_directory_path()
                                                       : std::filesystem::path(output)) /
                  "finish-review-702";
    std::filesystem::remove_all(folder);
    practice::g_replayArchive.SetDirectory(folder);
    for (int offset : {-1, 0, 1})
    {
        auto fixture = SeedFixture();
        auto review = std::make_shared<practice::RunReview>();
        review->id = id + offset;
        review->outcome = 1;
        review->seconds = 45.123;
        review->trace = {{0, {2, 1, 2}}, {45.123, {7, 1, 2}}};
        review->validationChecked = review->valid = review->observedFromStart = review->seedKnown = true;
        review->seedKind = 1;
        review->validityReason = "Synthetic UI QA";
        fixture.review = review;
        std::string error;
        auto decoded = practice::DecodeMap(EncodeFixture(fixture, 10), error);
        assert(decoded);
        practice::g_replayArchive.Save(decoded, "Finish QA " + std::to_string(offset), "QA", "");
    }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline && practice::g_replayArchive.Get()->runs.size() != 3)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(practice::g_replayArchive.Get()->runs.size() == 3);
    prepare();
    p.completed.reviewRetained = 1;
    data.reviewRunId = id + 1;
    g_session++;
    suite_ui::lifecycleTime = 4000;
    assert(FinishedReplayAvailable().source == FinishedReplaySource::Archive);
    render();
    result = ImGui::FindWindowByName("##RunResult");
    ImGui::ActivateItemByID(result->GetID("Analyse Run"));
    render();
    assert(!p.hasResult && g_runReviewOpen && g_reviewFromArchive && g_reviewTarget == id &&
           g_commands.runReviewRequest == 0 && g_speedrunMode);
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline &&
           (!practice::g_replayArchive.Loaded() || practice::g_replayArchive.Loaded()->review->id != id))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(practice::g_replayArchive.Loaded() && practice::g_replayArchive.Loaded()->review->id == id);
    prepare();
    g_pShared = nullptr;
    assert(FinishedReplayAvailable().source == FinishedReplaySource::Archive);
    p.completed.reviewSpawnSeed++;
    assert(!FinishedReplayAvailable() && !AnalyseFinishedRun());
    p.completed.reviewSpawnSeed--;
    p.completed.currentRunTimeSeconds++;
    assert(!FinishedReplayAvailable() && !AnalyseFinishedRun());
    // Copy uses the captured layout seed; no replay request or mode change.
    prepare();
    suite_ui::lifecycleTime = 101;
    for (int frame = 0; frame < 3; frame++)
        render();
    result = ImGui::FindWindowByName("##RunResult");
    details = childOf(result);
    ImGui::ActivateItemByID(details->GetID("Copy seed"));
    render();
    assert(std::string(ImGui::GetClipboardText()) == "482915720" && p.hasResult && g_speedrunMode &&
           g_commands.runReviewRequest == 0);
    p.Reset();
    suite_ui::preferences.motion = true;
    suite_ui::lifecycleTime = -1;
    pendingRequest = false;
    g_session = 1;
    g_runReviewOpen = false;
    data = saved;
    g_pShared = &data;
    std::cout
        << "Finish verification UI: ordered scan, actual evidence, native white labels / green-red tags, reduced motion, fixed footer with real mouse at 480/1920 and 100/150 DPI; permanent Analyse / seed copy / library actions; exact-ID replay across expiry, ending, disconnect and archive fallback; pending/active/scene/session/missing/mismatched-run guards: PASS\n";
}

#include "design_ui_cases.inl"
#include "update_ui_cases.inl"
#include "replay_input_ui_cases.inl"

void Workshop740Cases(const std::string &output)
{
    auto &io = ImGui::GetIO();
    auto originalHud = g_hudLayout;
    auto originalInput = suite_input::preferences;
    data = {};
    g_pShared = &data;
    data.playerReady = true;
    data.totalObjectives = 18;
    data.objectives = 10;
    data.gameFps = 144;
    data.seedKnown = true;
    data.seedKind = 1;
    data.runSeed = -674714359;
    data.inputSequence = 1;
    data.inputSeconds = 12.438;
    data.inputFrame = 428;
    data.inputTick = 172;
    data.inputState = 1 | 4 | 512;
    data.inputFlags = 257;
    data.inputKeys['W' / 8] |= 1u << ('W' % 8);
    data.inputKeys[162 / 8] |= 1u << (162 % 8);
    data.inputPadButtons[0] = 4096;
    data.inputPadAxes[0] = .3f;
    data.inputPadAxes[1] = .9f;
    data.inputPadAxes[3] = .8f;
    io.DisplaySize = {1920, 1080};
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    io.AddMousePosEvent(-1000, -1000);
    suite_hud::Open();
    auto frame = [&] {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        suite_hud::Render();
        ImGui::Render();
        CheckHudRegions();
    };
    for (int i = 0; i < 3; i++)
        frame();
    assert(suite_hud::active);
    auto bounds = suite_hud::bounds[0];
    ImVec2 at{bounds.x + 30, bounds.y + 80};
    io.AddMousePosEvent(at.x, at.y);
    frame();
    io.AddMouseButtonEvent(0, true);
    frame();
    io.AddMousePosEvent(at.x + 100, at.y + 70);
    frame();
    io.AddMouseButtonEvent(0, false);
    frame();
    assert(suite_hud::draft.compact.anchor == 4);
    assert(g_hudLayout.anchor == originalHud.anchor && g_hudLayout.x == originalHud.x);
    assert(!suite_hud::undo.empty());
    if (!output.empty())
        Dump(output + "/740-hud-editor.draw");
    auto *workspace = ImGui::FindWindowByName("HUD workspace");
    assert(workspace);
    ImGui::ActivateItemByID(workspace->GetID("Undo"));
    frame();
    assert(suite_hud::draft.compact.anchor == originalHud.anchor &&
           suite_hud::draft.compact.x == originalHud.x);
    io.AddKeyEvent(ImGuiKey_Escape, true);
    frame();
    io.AddKeyEvent(ImGuiKey_Escape, false);
    frame();
    assert(!suite_hud::active && g_hudLayout.x == originalHud.x);
    suite_hud::Open();
    suite_hud::draft.input.x = .22f;
    frame();
    ImGui::ActivateItemByID(workspace->GetID("Apply layout"));
    frame();
    assert(!suite_hud::active && suite_input::preferences.x == .22f);
    suite_input::preferences = originalInput;
    data.timerState = 1;
    suite_hud::Open();
    assert(!suite_hud::active);
    data.timerState = 0;
    suite_input::preferences.enabled = true;
    suite_input::preferences.fullKeyboard = true;
    for (int i = 0; i < 3; i++)
    {
        ImGui::NewFrame();
        suite_input::LiveHud();
        ImGui::Render();
        CheckHudRegions();
    }
    auto *panel = ImGui::FindWindowByName("Input HUD");
    assert(panel && (panel->Flags & ImGuiWindowFlags_NoInputs));
    assert(panel->Pos.x >= 0 && panel->Pos.y >= 0 && panel->Pos.x + panel->Size.x <= 1920);
    if (!output.empty())
        Dump(output + "/740-full-keyboard.draw");
    suite_input::preferences.fullKeyboard = false;
    for (int i = 0; i < 3; i++)
    {
        ImGui::NewFrame();
        suite_input::LiveHud();
        ImGui::Render();
    }
    if (!output.empty())
        Dump(output + "/740-live-input.draw");
    auto map = std::make_shared<practice::MapData>();
    map->scene = 3;
    map->seed = -674714359;
    map->minX = map->minZ = -20;
    map->maxX = map->maxZ = 20;
    map->spawn = 0;
    map->spawns = {{-15, 0, -15}};
    map->points = {{3, 0, 4}, {10, 0, 10}, {-7, 0, 12}};
    map->selected = {0, 1, 2};
    map->walls = {{-19, -19, -18, 19}, {18, -19, 19, 19}, {-18, 18, 18, 19}, {-18, -19, 18, -18},
                  {-10, -10, -9, 8},   {-9, 7, 7, 8},     {5, -10, 6, 0}};
    auto review = std::make_shared<practice::RunReview>();
    review->id = 17;
    review->seconds = 10;
    review->outcome = 1;
    review->validationChecked = review->valid = true;
    for (int i = 0; i <= 200; i++)
        review->trace.push_back(
            {i * .05,
             {float(i <= 100 ? -15 + i * .3 : 15), 0, float(i <= 100 ? -15 : -15 + (i - 100) * .3)},
             true});
    map->review = review;
    auto analysis = std::make_shared<practice::MapAnalysis>();
    analysis->data = map;
    auto motion = std::make_shared<practice::ReplayMotion>();
    motion->id = 17;
    motion->seconds = 10;
    motion->deviceThrough = 10;
    auto input = suite_input::Demo();
    input.state |= 512;
    motion->devices = {input};
    motion->events = {{0, 0, 0, 0, 0, 0, 0, 1}, {2, 60, 50, 0, 0, 0, 0, 1 | 4 | 512}};
    suite_clip::Content c;
    c.analysis = analysis;
    c.motion = motion;
    c.input = suite_input::preferences;
    c.font = suite_ui::hudFont;
    c.palette = suite_input::Colors(.92f);
    suite_clip::Options o;
    o.a = 1;
    o.b = 6;
    o.width = 1920;
    o.height = 1080;
    o.fps = 60;
    o.Validate(10);
    for (int i = 0; i < 3; i++)
    {
        ImGui::NewFrame();
        suite_clip::Compose(ImGui::GetBackgroundDrawList(), o, c, 3.25);
        ImGui::Render();
    }
    if (!output.empty())
        Dump(output + "/740-clip-composition.draw");
    suite_clip::Open(analysis, motion);
    assert(suite_clip::open);
    suite_clip::options.a = 2;
    suite_clip::options.b = 6;
    suite_clip::options.rate = .5;
    suite_clip::options.camera = 2;
    suite_clip::options.fullbright = true;
    auto fullMotion = std::make_shared<practice::ReplayMotion>(*motion);
    practice::ReplayViewFrame first, last;
    last.seconds = review->seconds;
    fullMotion->frames = {first, last};
    suite_clip::OpenFullRun(analysis, fullMotion);
    assert(suite_clip::options.a == 0 && suite_clip::options.b == analysis->data->review->seconds &&
           suite_clip::options.rate == 1 && suite_clip::options.camera == 0 && suite_clip::options.scene3d &&
           !suite_clip::options.speedKeys && !suite_clip::options.fullbright && !suite_clip::options.route &&
           suite_clip::options.width == 1920 && suite_clip::options.fps == 60);
    suite_clip::Open(analysis, motion);
    for (int i = 0; i < 3; i++)
    {
        ImGui::NewFrame();
        suite_ui::BeginFrame(true, false);
        suite_clip::Render();
        ImGui::Render();
    }
    if (!output.empty())
        Dump(output + "/740-export-dialog.draw");
    suite_clip::Close();
    assert(!suite_clip::open);
    for (auto viewport : {ImVec2(480, 360), ImVec2(1280, 720), ImVec2(1920, 1080)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = viewport;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            suite_hud::Open();
            for (int i = 0; i < 3; i++)
                frame();
            auto *editor = ImGui::FindWindowByName("HUD workspace");
            assert(editor && suite_hud::canvas.y < viewport.y - 80);
            suite_hud::Close(false);
            suite_clip::Open(analysis, motion);
            auto *dialog = ImGui::FindWindowByName(suite_clip::WindowTitle().c_str());
            if (dialog)
                dialog->SizeFull = {viewport.x - 32, viewport.y - 32};
            for (int i = 0; i < 3; i++)
            {
                ImGui::NewFrame();
                suite_ui::BeginFrame(true, false);
                suite_clip::Render();
                ImGui::Render();
                CheckHudRegions();
            }
            dialog = ImGui::FindWindowByName(suite_clip::WindowTitle().c_str());
            assert(dialog);
            ImGui::ActivateItemByID(dialog->GetID("Close"));
            ImGui::NewFrame();
            suite_clip::Render();
            ImGui::Render();
            assert(!suite_clip::open);
        }
    suite_input::preferences = originalInput;
    g_hudLayout = originalHud;
    suite_hud::active = false;
    std::cout
        << "7.4.0 real ImGui: draft-only HUD dragging, Escape rollback, active-run lock, passive full keyboard/controller HUD, common replay clip composition and export dialog: PASS\n";
}

#include "clip_ui_cases.inl"
#include "clip_studio_cases.inl"
#include "clip_timeline_ui_cases.inl"

#include "replay_experience_ui_cases.inl"
#include "replay_health_ui_cases.inl"
#include "physical_map_ui_cases.inl"
#include "inspector_ui_cases.inl"

void SeedRouteUICases(const std::string &output)
{
    auto &io = ImGui::GetIO();
    practice::SeedRoutePlayback preview;
    auto fixture = std::make_shared<practice::MapData>(SeedFixture());
    fixture->CalculateBounds();
    practice::MapAnalysis analysis;
    analysis.data = fixture;
    analysis.route.complete = true;
    analysis.route.line = {{2, 0, 2}, {12, 0, 2}, {12, 0, 12}, {22, 0, 12}};
    analysis.route.length = 30;
    for (float dpi : {1.f, 1.5f})
        for (ImVec2 size : {ImVec2(480, 700), ImVec2(1280, 900)})
        {
            io.DisplaySize = size;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            preview = {};
            suite_ui::preferences.motion = true;
            auto frame = [&] {
                ImGui::NewFrame();
                ImGui::SetNextWindowPos({10, 10});
                ImGui::SetNextWindowSize({size.x - 20, size.y - 20});
                ImGui::Begin("Seed route hotfix QA");
                RenderSeedRouteControls(preview);
                auto line = preview.suggested
                                ? practice::SuggestedSlice(analysis.route, preview.Fraction(), 1.)
                                : practice::PlaybackSlice{};
                auto pulse = preview.suggested && preview.playing && suite_ui::preferences.motion
                                 ? practice::SuggestedSlice(analysis.route, preview.Pulse(), .035)
                                 : practice::PlaybackSlice{};
                SeedCanvasPlayback canvas;
                canvas.suggested = &line;
                canvas.routePulse = &pulse;
                canvas.showOrder = false;
                RenderSeedCanvas(analysis, 300 * dpi, &canvas);
                ImGui::End();
                ImGui::Render();
                CheckHudRegions();
            };
            for (int i = 0; i < 3; i++)
            {
                frame();
            }
            auto *w = ImGui::FindWindowByName("Seed route hotfix QA");
            assert(w);
            ImGui::ActivateItemByID(w->GetID("Optimal route"));
            frame();
            assert(preview.suggested);
            assert(preview.Complete() && !preview.playing);
            ImGui::ActivateItemByID(w->GetID("Animate route"));
            frame();
            assert(preview.playing);
            for (int i = 0; i < 330; i++)
            {
                frame();
            }
            assert(preview.Complete());
            bool white = false, cyan = false;
            for (const auto *list : ImGui::GetDrawData()->CmdLists)
                for (const auto &v : list->VtxBuffer)
                {
                    white |= v.col == IM_COL32(255, 255, 255, 240);
                    cyan |= v.col == IM_COL32(106, 214, 196, 255);
                }
            assert(white && cyan);
            if (dpi == 1 && size.x == 1280)
                Dump(output + "/seed-route-full.draw");
            ImGui::ActivateItemByID(w->GetID("Animate route"));
            frame();
            assert(!preview.playing && preview.Complete());
            double stopped = preview.pulseSeconds;
            for (int i = 0; i < 3; i++)
                frame();
            assert(preview.pulseSeconds == stopped && preview.Complete());
            ImGui::ActivateItemByID(w->GetID("Animate route"));
            frame();
            assert(preview.playing && preview.Complete() && preview.pulseSeconds > stopped);
            suite_ui::preferences.motion = false;
            frame();
            assert(preview.Complete());
        }
    suite_ui::preferences.motion = true;
    std::cout
        << "Seed route actual ImGui: 480/1280 at 100/150 DPI; optimal toggle, immediate full route, completed prefix persists across repeated pulse cycles, restart/pause/resume, white pulse over cyan route and reduced motion: PASS\n";
}

void SeedRouteReliabilityUICases(const std::string &output)
{
    auto &io = ImGui::GetIO();
    auto originalOptions = g_plannerOptions;
    auto data = std::make_shared<practice::MapData>();
    data->pid = 10;
    data->scene = 13;
    data->epoch = 4;
    data->seed = 714;
    data->recipe = 2;
    data->spawn = 0;
    data->spawns = {{2, 0, 2}};
    data->minX = data->minZ = 0;
    data->maxX = 30;
    data->maxZ = 10;
    practice::MapAnalysis analysis;
    analysis.data = data;
    analysis.session = 7;
    analysis.route.complete = analysis.route.estimated = true;
    analysis.route.line = {{2, 0, 2}, {5, 0, 2}, {20, 0, 2}, {25, 0, 2}};
    analysis.route.estimatedLine = {0, 1, 0};
    BindSeedRoutePreview(analysis);
    g_seedPreviewPlayback.suggested = true;
    g_seedPreviewPlayback.ShowAll();
    g_seedPreviewPlayback.playing = false;
    auto refresh = analysis;
    refresh.data = std::make_shared<practice::MapData>(*data);
    refresh.working = true;
    refresh.route = {};
    BindSeedRoutePreview(refresh);
    assert(g_seedPreviewPlayback.suggested && g_seedPreviewPlayback.Complete() &&
           !g_seedPreviewPlayback.playing);
    auto done = analysis;
    done.data = std::make_shared<practice::MapData>(*data);
    BindSeedRoutePreview(done);
    assert(g_seedPreviewPlayback.suggested && g_seedPreviewPlayback.Complete());
    auto next = std::make_shared<practice::MapData>(*data);
    next->seed++;
    done.data = next;
    BindSeedRoutePreview(done);
    assert(g_seedPreviewPlayback.suggested && g_seedPreviewPlayback.Complete() &&
           !g_seedPreviewPlayback.playing);
    g_seedPreviewPlayback.suggested = true;
    done.session++;
    BindSeedRoutePreview(done);
    assert(g_seedPreviewPlayback.suggested && g_seedPreviewPlayback.Complete());
    for (auto viewport : {ImVec2(480, 700), ImVec2(1280, 900)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = viewport;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            io.AddMousePosEvent(-1000, -1000);
            g_plannerOptions.level = 7;
            ImVec2 lo, hi;
            auto frame = [&] {
                ImGui::NewFrame();
                ImGui::SetNextWindowPos({10, 10});
                ImGui::SetNextWindowSize({viewport.x - 20, viewport.y - 20});
                ImGui::Begin("Route visibility reliability QA");
                auto line = practice::SuggestedSlice(analysis.route, 1, 1);
                auto pulse = practice::SuggestedSlice(analysis.route, .5, .035);
                SeedCanvasPlayback canvas;
                canvas.suggested = &line;
                canvas.routePulse = &pulse;
                canvas.showOrder = false;
                RenderSeedCanvas(analysis, 280 * dpi, &canvas);
                lo = ImGui::GetItemRectMin();
                hi = ImGui::GetItemRectMax();
                ImGui::End();
                ImGui::Render();
                CheckHudRegions();
            };
            for (int i = 0; i < 3; i++)
            {
                frame();
            }
            float scale = std::min((hi.x - lo.x - 32) / 30, (hi.y - lo.y - 32) / 10),
                  left = (lo.x + hi.x) * .5f + (5 - 15) * scale,
                  right = (lo.x + hi.x) * .5f + (20 - 15) * scale;
            bool cyan = false, white = false;
            for (const auto *list : ImGui::GetDrawData()->CmdLists)
            {
                for (const auto &v : list->VtxBuffer)
                {
                    if (v.col == IM_COL32(106, 214, 196, 255))
                    {
                        cyan = true;
                        assert(v.pos.x <= left + 8 * dpi || v.pos.x >= right - 8 * dpi);
                    }
                    white |= v.col == IM_COL32(255, 255, 255, 240);
                }
            }
            assert(cyan && !white);
            if (viewport.x == 1280 && dpi == 1)
                Dump(output + "/714-known-route-gaps.draw");
        }
    g_plannerOptions = originalOptions;
    std::cout
        << "7.11.4 real ImGui: route survives same-layout working/completed refresh, new seed/session resets, partial route retains checked pieces without crossing unknown gaps or showing a pulse in them at 480/1280 and 100/150 DPI: PASS\n";
}

int main(int argc, char **argv)
{
    std::cout.setf(std::ios::unitbuf);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DeltaTime = 1.f / 60;
    if (argc > 2 && std::string(argv[2]) == "--seed-route-only")
    {
        SeedRouteUICases(argv[1]);
        SeedRouteReliabilityUICases(argv[1]);
        ImGui::DestroyContext();
        return 0;
    }
    if (argc > 2 && std::string(argv[2]) == "--inspector-only")
    {
        InspectorUICases(argv[1]);
        ImGui::DestroyContext();
        return 0;
    }
    data.pid = 1234;
    auto fixture = SeedFixture();
    g_mapWorker.Submit(EncodeFixture(fixture), g_session);
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < deadline)
    {
        auto result = g_mapWorker.Get(0);
        if (result && !result->working)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(g_mapWorker.Get(0) && g_mapWorker.Get(0)->route.complete);
    g_speedrunMode = g_practiceMode = g_plannerOptions.minimapRoute = true;
    data.currentSceneIndex = 3;
    data.sceneEpoch = 1;
    data.playerReady = 1;
    data.speedrunModeActive = data.practiceModeActive = 1;
    auto live = *g_mapWorker.Get(0);
    assert(LivePlannerRoute(live));
    live.live = false;
    assert(!LivePlannerRoute(live));
    live.live = true;
    live.session = 2;
    assert(!LivePlannerRoute(live));
    live.session = 1;
    data.pid = 999;
    assert(!LivePlannerRoute(live));
    data.pid = 1234;
    data.sceneEpoch = 2;
    assert(!LivePlannerRoute(live));
    data.sceneEpoch = 1;
    data.currentSceneIndex = 4;
    assert(!LivePlannerRoute(live));
    data.currentSceneIndex = 3;
    data.playerReady = 0;
    assert(!LivePlannerRoute(live));
    data.playerReady = 1;
    g_practiceMode = false;
    assert(!LivePlannerRoute(live));
    g_practiceMode = true;
    auto different = std::make_shared<practice::MapData>(*live.data);
    different->flags &= ~8;
    live.data = different;
    assert(!LivePlannerRoute(live));
    g_speedrunMode = g_practiceMode = g_plannerOptions.minimapRoute = false;
    data.freecamActive = 1;
    data.rngPatchComplete = 1;
    data.seedActive = 1;
    data.levelSeed = 123;
    strcpy(data.version, "v7.1.0");
    data.timerState = 1;
    data.currentRunTimeSeconds = 42.125;
    data.personalBestSeconds = 48.567;
    data.totalRuns = 42;
    data.totalFinishedRuns = 12;
    data.totalDeaths = 30;
    data.averagePace = 56.789;
    data.playerReady = 1;
    data.health = 2;
    data.currentSceneIndex = 3;
    data.sceneEpoch = 1;
    data.gameFps = 144;
    data.totalResets = 30;
    data.currentRunNumber = 42;
    data.sessionTimeSeconds = 3584;
    data.sessionAttempts = 8;
    data.sessionCompleted = 2;
    data.sessionResets = 6;
    data.runProgress = .55f;
    data.runSeed = 482915720;
    data.seedKnown = 1;
    data.seedKind = 1;
    data.ghostChasing = 1;
    data.ghostChaseCount = 2;
    data.ghostDefaultSpeed = 8;
    data.ghostDetectionRange = 13;
    for (int i = 0; i < 80; i++)
    {
        data.crystalPositions[i * 2] = float(i);
        data.crystalPositions[i * 2 + 1] = float(i * 2);
    }
    int cases = 0;
    for (ImVec2 size : {ImVec2(480, 360), ImVec2(1280, 720), ImVec2(1920, 1080)})
    {
        for (float scale : {1.f, 1.5f})
        {
            io.DisplaySize = size;
            ApplyTheme(scale);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            g_resetLayout = true;
            for (int state = 0; state < 4; state++)
            {
                g_pShared = state == 3 ? nullptr : &data;
                g_speedrunMode = state == 1 || state == 2;
                g_practiceMode = state == 2;
                data.speedrunModeActive = g_speedrunMode;
                data.practiceModeActive = g_practiceMode;
                Frame();
                g_resetLayout = false;
                assert(suite_ui::PageCount == 8);
                for (int tab = 0; tab < suite_ui::PageCount; tab++)
                {
                    suite_ui::navigation.Select(suite_ui::Page(tab), ImGui::GetTime());
                    Frame();
                    Frame();
                    cases++;
                    if (argc > 1 && size.x == 1280 && scale == 1 && state == 2 && tab == 1)
                        Dump(std::string(argv[1]) + "/practice-controls.draw");
                    if (argc > 1 && size.x == 1280 && scale == 1 && state == 0)
                        Dump(std::string(argv[1]) + "/tab-" + std::to_string(tab) + ".draw");
                }
            }
        }
    }
    int overlayCases = 0;
    for (ImVec2 size : {ImVec2(480, 360), ImVec2(1280, 720), ImVec2(1920, 1080)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = size;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            // Two product defaults, with user-editable fields and ordering. The
            // scenarios below stress customization; they are not selectable presets.
            for (int scenario = 0; scenario < 6; scenario++)
                for (int kind = 0; kind < 3; kind++)
                    for (bool speedrun : {false, true})
                    {
                        OverlayLayout layout(speedrun);
                        if (scenario == 1)
                        {
                            layout.visible.fill(false);
                            layout.visible[static_cast<int>(OverlayField::Timer)] = true;
                        }
                        if (scenario == 2)
                            layout.visible.fill(true);
                        if (scenario == 3)
                        {
                            layout.visible.fill(true);
                            std::reverse(layout.order.begin(), layout.order.end());
                        }
                        if (scenario == 4)
                            for (int field = 0; field < kOverlayFields; field++)
                                layout.visible[field] = (field % 3 == 0);
                        if (scenario == 5)
                            layout.visible.fill(false);
                        data.speedrunModeActive = 1;
                        data.seedKind = static_cast<byte>(kind);
                        data.seedKnown = kind != 0;
                        data.runAssisted = kind == 2;
                        data.trainerFeatures = kind == 2 ? 2 : 0;
                        const char *name = speedrun ? "Speedrun QA" : "Compact QA";
                        for (int frame = 0; frame < 3; frame++)
                        {
                            ImGui::NewFrame();
                            RenderOverlayPanel(name, data, layout, speedrun, "Classic", .008);
                            ImGui::Render();
                        }
                        auto *window = ImGui::FindWindowByName(name);
                        assert(window && (window->Flags & ImGuiWindowFlags_NoInputs));
                        assert(window->Pos.x >= 0 && window->Pos.y >= 0 &&
                               window->Pos.x + window->Size.x <= size.x + 1 &&
                               window->Pos.y + window->Size.y <= size.y + 1);
                        if (window->ScrollMax.y >= 1)
                            std::cerr << "HUD overflow " << size.x << "," << size.y << " DPI " << dpi
                                      << " scenario " << scenario << " speedrun " << speedrun << " kind "
                                      << kind << " height " << window->Size.y << " scroll "
                                      << window->ScrollMax.y << std::endl;
                        assert(window->ScrollMax.y < 1 && ImGui::GetDrawData()->TotalVtxCount > 0);
                        overlayCases++;
                        if (argc > 1 && size.x == 1280 && dpi == 1 && scenario == 0)
                            Dump(std::string(argv[1]) + "/" + (speedrun ? "speedrun" : "compact") +
                                 "-default-seed" + std::to_string(kind) + ".draw");
                    }
            // Even with all optional fields off, a seeded run must retain its warning.
            OverlayLayout layout(true);
            layout.visible.fill(false);
            data.seedKind = 2;
            data.seedKnown = 1;
            data.runAssisted = 1;
            for (int frame = 0; frame < 3; frame++)
            {
                ImGui::NewFrame();
                RenderOverlayPanel("Warnings QA", data, layout, true, "Classic", 0);
                ImGui::Render();
            }
            assert(ImGui::GetDrawData()->TotalVtxCount > 100);
            if (argc > 1 && size.x == 1280 && dpi == 1)
                Dump(std::string(argv[1]) + "/warnings-only.draw");
        }
    // Exercise actual render entry points, including stale mode telemetry,
    // saved Compact preferences, F2 hiding and Normal/Practice/Speedrun switches.
    int modeCases = 0;
    io.DisplaySize = ImVec2(1280, 720);
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    g_pShared = &data;
    g_hudLayout.enabled = g_speedrunLayout.enabled = true;
    auto active = [](const char *name) {
        auto *window = ImGui::FindWindowByName(name);
        return window && window->LastFrameActive == ImGui::GetFrameCount();
    };
    for (bool requestedRun : {false, true})
        for (bool requestedPractice : {false, true})
            for (bool compact : {false, true})
                for (bool shown : {false, true})
                    for (bool enabled : {false, true})
                        for (bool reportedRun : {false, true})
                            for (bool reportedPractice : {false, true})
                            {
                                g_speedrunMode = requestedRun;
                                g_practiceMode = requestedPractice;
                                g_compactMode = compact;
                                g_overlayVisible = shown;
                                g_allOverlayEnabled = enabled;
                                data.speedrunModeActive = reportedRun;
                                data.practiceModeActive = reportedPractice;
                                for (int frame = 0; frame < 3; frame++)
                                {
                                    ImGui::NewFrame();
                                    RenderTimerOverlay("Classic");
                                    RenderMainWindow("Classic", 123);
                                    ImGui::Render();
                                }
                                assert(active("Compact HUD") ==
                                       bool((requestedRun && requestedPractice && compact) ||
                                            (!requestedRun && enabled && shown && compact)));
                                assert(active("Speedrun overlay") ==
                                       bool(requestedRun && !(requestedPractice && compact)));
                                assert(!(active("Compact HUD") && active("Speedrun overlay")));
                                assert(active("It Steals | Practice Suite") ==
                                       bool(enabled && shown && !compact));
                                modeCases++;
                            }
    int mapCases = 0;
    // Render the real mandatory status, with metrics/F2/connection all off,
    // and inspect emitted vertex colors rather than relying on a mock label.
    io.DisplaySize = ImVec2(1280, 720);
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    auto hasColor = [](ImGuiWindow *window, ImU32 color) {
        for (const auto &v : window->DrawList->VtxBuffer)
            if (v.col == color)
                return true;
        return false;
    };
    auto foregroundColor = [](ImU32 color) {
        for (const auto &v : ImGui::GetForegroundDrawList()->VtxBuffer)
            if (v.col == color)
                return true;
        return false;
    };
    g_speedrunMode = true;
    g_practiceMode = false;
    g_allOverlayEnabled = false;
    g_overlayVisible = false;
    g_speedrunLayout.enabled = false;
    data.runValidity = 1;
    data.observedFromStart = data.seedKnown = data.seedSourcesVerified = data.rngPatchComplete = 1;
    data.seedKind = 1;
    data.practiceModeActive = data.runAssisted = 0;
    data.trainerFeatures = data.runUsedFeatures = data.runWarnings = 0;
    for (float opacity : {.25f, .86f})
    {
        g_speedrunLayout.opacity = opacity;
        for (int frame = 0; frame < 3; frame++)
        {
            g_validityBadgeDrawn = false;
            ImGui::NewFrame();
            RenderTimerOverlay("Classic");
            ImGui::Render();
        }
        auto *status = ImGui::FindWindowByName("Speedrun overlay");
        assert(status && g_validityBadgeDrawn &&
               foregroundColor(ImGui::ColorConvertFloat4ToU32(ImVec4(.30f, .94f, .40f, 1))));
        assert(hasColor(status, ImGui::ColorConvertFloat4ToU32(suite_ui::HudBackground(opacity))));
        if (argc > 1)
            Dump(std::string(argv[1]) + "/mandatory-valid-opacity-" + std::to_string(int(opacity * 100)) +
                 ".draw");
    }
    data.runValidity = 2;
    for (int frame = 0; frame < 3; frame++)
    {
        ImGui::NewFrame();
        RenderTimerOverlay("Classic");
        ImGui::Render();
    }
    assert(foregroundColor(ImGui::ColorConvertFloat4ToU32(ImVec4(1, .23f, .23f, 1))));
    g_pShared = nullptr;
    for (int frame = 0; frame < 3; frame++)
    {
        ImGui::NewFrame();
        RenderTimerOverlay("Offline");
        ImGui::Render();
    }
    assert(active("Speedrun overlay") &&
           foregroundColor(ImGui::ColorConvertFloat4ToU32(ImVec4(1, .23f, .23f, 1))));
    data.playerReady = 1;
    data.playerX = 12.5f;
    data.playerY = 1.75f;
    data.playerZ = -42;
    assert(OverlayValue(OverlayField::Coordinates, data, "Classic", 0) == "X 12.50  Y 1.75  Z -42.00");
    assert(OverlayValue(OverlayField::Coordinates, data, "Classic", 1) == "X --  Y --  Z --");
    g_hudLayout.enabled = g_speedrunLayout.enabled = true;
    g_speedrunMode = false;
    g_practiceMode = false;
    g_allOverlayEnabled = true;
    g_pShared = &data;
    data.mapBoundsValid = 1;
    data.playerReady = 1;
    data.health = 2;
    data.currentSceneIndex = 3;
    data.sceneEpoch = 1;
    data.mapMinX = data.mapMinZ = -50;
    data.mapMaxX = data.mapMaxZ = 50;
    data.crystalCount = 10;
    data.wallCount = 2;
    data.monsterActive = 1;
    float walls[] = {-20, -20, -15, 20, 15, -20, 20, 20};
    std::memcpy(data.wallBoxes, walls, sizeof(walls));
    for (ImVec2 size : {ImVec2(480, 360), ImVec2(1280, 720), ImVec2(1920, 1080)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = size;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            for (bool follow : {false, true})
                for (bool heading : {false, true})
                    for (float zoom : {.5f, 1.f, 4.f})
                        for (int filter = 0; filter < 4; filter++)
                        {
                            g_mapOptions.follow = follow;
                            g_mapOptions.playerUp = heading;
                            g_mapOptions.zoom = zoom;
                            g_mapOptions.walls = filter != 1;
                            g_mapOptions.crystals = filter != 2;
                            g_mapOptions.monster = filter != 3;
                            g_mapOptions.trail = true;
                            for (int frame = 0; frame < 3; frame++)
                            {
                                ImGui::NewFrame();
                                RenderMinimapWindow(10, 0, 0, 20, 10, 10, 2, 0, data.pathCorners, -50, 50,
                                                    -50, 50, true, true, false);
                                ImGui::Render();
                            }
                            auto *map = ImGui::FindWindowByName("Minimap");
                            assert(map && (map->Flags & ImGuiWindowFlags_NoInputs));
                            assert(map->Pos.x >= 0 && map->Pos.y >= 0 &&
                                   map->Pos.x + map->Size.x <= size.x + 1 &&
                                   map->Pos.y + map->Size.y <= size.y + 1);
                            assert(ImGui::GetDrawData()->TotalVtxCount > 0);
                            mapCases++;
                            if (argc > 1 && size.x == 1280 && dpi == 1 && zoom == 1 && filter == 0)
                                Dump(std::string(argv[1]) + "/map-" + (follow ? "follow" : "overview") + "-" +
                                     (heading ? "player" : "north") + ".draw");
                        }
        }
    auto timerMap = SeedFixture();
    timerMap.scene = 7;
    g_mapWorker.Submit(EncodeFixture(timerMap), g_session);
    auto timerDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < timerDeadline)
    {
        auto ready = g_mapWorker.Get(3);
        if (ready && !ready->working)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(g_mapWorker.Get(3) && g_mapWorker.Get(3)->outline && !g_mapWorker.Get(3)->outline->empty());
    data.currentSceneIndex = 7;
    data.timerMode = 1;
    g_mapOptions = {};
    g_mapOptions.follow = false;
    io.DisplaySize = ImVec2(1280, 720);
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    for (int count : {0, 3, 128})
    {
        data.timerMonsterCount = count;
        data.timerMonsterOverflow = count == 128 ? 4 : 0;
        for (int i = 0; i < count; i++)
        {
            data.timerMonsterPositions[i * 2] = 2 + i % 7 * 5;
            data.timerMonsterPositions[i * 2 + 1] = 2 + i % 6 * 5;
            data.timerMonsterStates[i] = i % 3;
        }
        for (int frame = 0; frame < 3; frame++)
        {
            ImGui::NewFrame();
            RenderMinimapWindow(2, 2, 0, 20, 10, 10, 2, 0, data.pathCorners, -1, 1, -1, 1, false, true,
                                false);
            ImGui::Render();
        }
        assert(ImGui::GetDrawData()->TotalVtxCount > 0);
        if (argc > 1)
            Dump(std::string(argv[1]) + "/map-timer-" + std::to_string(count) + ".draw");
    }
    data.timerMode = 0;
    data.currentSceneIndex = 3;
    data.timerMonsterCount = 0;
    data.seedKind = 2;
    data.seedKnown = 1;
    assert(std::string(SeedTypeLabel(data)) == "SET SEED");
    data.seedKnown = 0;
    assert(std::string(SeedTypeLabel(data)) == "SEED: UNKNOWN");
    // Menu-aborted runs keep a stopped timer and cannot look like completions.
    data.timerState = 2;
    data.runOutcome = 4;
    data.currentRunTimeSeconds = 42.125;
    data.speedrunModeActive = 1;
    data.practiceModeActive = 0;
    data.runWarnings = 1;
    data.seedKind = 1;
    data.seedKnown = 1;
    data.runSeed = 42;
    data.runAssisted = 0;
    data.trainerFeatures = 0;
    data.runUsedFeatures = 0;
    data.runIntegrity = 0;
    assert(std::string(RunStatusLabel(data)) == "ABORTED");
    assert(OverlayValue(OverlayField::Timer, data, "Classic", 3) == "00:42.125");
    io.DisplaySize = ImVec2(1280, 720);
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    OverlayLayout stoppedLayout(true);
    for (int frame = 0; frame < 3; frame++)
    {
        ImGui::NewFrame();
        RenderOverlayPanel("Speedrun QA", data, stoppedLayout, true, "Classic", 3);
        ImGui::Render();
    }
    if (argc > 1)
        Dump(std::string(argv[1]) + "/speedrun-aborted.draw");
    data.runOutcome = 1;
    assert(std::string(RunStatusLabel(data)) == "COMPLETED");
    data.runOutcome = 0;
    assert(std::string(RunStatusLabel(data)) == "STOPPED");
    data.timerState = 0;
    assert(std::string(RunStatusLabel(data)) == "READY");
    data.timerState = 1;
    assert(std::string(RunStatusLabel(data)) == "RUNNING");
    int plannerCases = 0;
    g_pShared = &data;
    g_speedrunMode = g_practiceMode = true;
    data.speedrunModeActive = data.practiceModeActive = 1;
    data.pid = 1234;
    data.playerReady = 1;
    data.sceneEpoch = 1;
    data.currentSceneIndex = 3;
    g_plannerOptions.level = 0;
    io.DisplaySize = ImVec2(1280, 720);
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    for (int state = 0; state < 6; state++)
    {
        auto variant = SeedFixture();
        variant.seed += state + 100;
        variant.sourceKind = state == 0 ? 2 : state == 1 ? 0 : 1;
        if (state == 1)
            variant.flags &= ~1;
        if (state == 2)
        {
            variant.flags |= 64;
            variant.message = "Prediction mismatch: captured locations remain available.";
        }
        if (state == 3)
        {
            variant.vertices.clear();
            variant.indices.clear();
        }
        if (state == 4)
        {
            variant.spawn = variant.liveSpawn = -1;
            variant.flags = 64;
            variant.stage = 3;
            variant.sourceKind = variant.liveRecipe = 0;
        }
        g_mapWorker.Submit(EncodeFixture(variant), g_session);
        auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < end)
        {
            auto result = g_mapWorker.Get(0);
            if (result && result->data && result->data->seed == variant.seed && !result->working)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(g_mapWorker.Get(0) && g_mapWorker.Get(0)->data->seed == variant.seed &&
               !g_mapWorker.Get(0)->working);
        if (state == 5)
            g_mapWorker.InvalidateLive();
        for (int frame = 0; frame < 3; frame++)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(740, 680), ImGuiCond_Always);
            ImGui::Begin("Planner states QA");
            ImGui::BeginChild("Planner QA scroll");
            RenderSeedMapTab(false);
            ImGui::EndChild();
            ImGui::End();
            ImGui::Render();
        }
        assert(ImGui::GetDrawData()->TotalVtxCount > 0);
        plannerCases++;
        if (argc > 1)
            Dump(std::string(argv[1]) + "/planner-state-" + std::to_string(state) + ".draw");
    }
    auto corridorUI = SeedFixture();
    corridorUI.scene = 7;
    corridorUI.vertices.clear();
    corridorUI.indices.clear();
    corridorUI.walls.clear();
    corridorUI.spawns = {{2, 1, 2}};
    corridorUI.points = {{8, 1, 2}, {18, 1, 2}};
    corridorUI.keep = 2;
    corridorUI.spawn = corridorUI.liveSpawn = 0;
    corridorUI.selected = corridorUI.liveSelected = {0, 1};
    corridorUI.samples.clear();
    corridorUI.requested = corridorUI.completed = 0;
    corridorUI.navigationStatus = 2;
    corridorUI.navigationNodes = {{2, 1, 2}, {8, 1, 2}, {18, 1, 2}};
    corridorUI.navigationEdges = {0, 1, 1, 2};
    corridorUI.navigationAnchors = {0, 1, 2};
    corridorUI.navigationQueries = corridorUI.navigationTotal = 3;
    corridorUI.navigationNote = "Synthetic Unity-corridor UI fixture.";
    for (int state : {2, 1})
    {
        auto model = corridorUI;
        if (state == 1)
        {
            model.navigationStatus = 1;
            model.flags |= 128;
            model.navigationQueries = 1;
            model.navigationNodes.clear();
            model.navigationEdges.clear();
            model.navigationAnchors.clear();
        }
        g_mapWorker.Submit(EncodeFixture(model), g_session);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline)
        {
            auto loaded = g_mapWorker.Get(3);
            if (loaded && !loaded->working && loaded->data->navigationStatus == state)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        assert(g_mapWorker.Get(3) && g_mapWorker.Get(3)->data->navigationStatus == state);
        g_plannerOptions.level = 3;
        data.currentSceneIndex = 7;
        for (int frame = 0; frame < 3; frame++)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(740, 680), ImGuiCond_Always);
            ImGui::Begin("Native navigation QA");
            RenderSeedMapTab(false);
            ImGui::End();
            ImGui::Render();
        }
        if (argc > 1)
            Dump(std::string(argv[1]) + "/planner-native-" + std::to_string(state) + ".draw");
    }
    g_plannerOptions.level = 0;
    data.currentSceneIndex = 3;
    std::cout
        << "Seed Map SET/UNKNOWN/mismatch/no-navmesh/missing-spawn/offline and live route identity gates: PASS ("
        << plannerCases << " states + 9 route checks)\n";
    g_seedSearch.Stop();
    g_seedSearch.HidePreview();
    g_commands = {};
    g_plannerOptions.recipe = 1;
    assert(RandomPlannerPreview() && g_commands.plannerRequest == 1 &&
           g_commands.plannerSeed == g_plannerOptions.seed && g_plannerRandomError.empty());
    auto batch = SeedFixture();
    batch.recipe = 1;
    batch.seed = g_plannerOptions.seed;
    batch.requestSerial = g_sentSerial + 1;
    batch.stage = 2;
    batch.requested = batch.completed = 0;
    auto choices = batch.samples;
    batch.samples.clear();
    std::string packetError;
    auto decoded = practice::DecodeMap(EncodeFixture(batch), packetError);
    assert(decoded);
    practice::NavGraph nav(*decoded);
    auto source = std::make_shared<practice::MapAnalysis>(practice::AnalyzeMap(decoded, nav));
    source->live = true;
    source->session = g_session;
    for (uint64_t count : {uint64_t{1000000}, practice::SeedSpace})
    {
        g_plannerOptions.searchCount = count;
        assert(BeginPlannerSearch(*source));
        assert(g_commands.plannerSearchCount == practice::SeedSpace &&
               g_seedSearch.Get()->request.count == practice::SeedSpace && g_seedSearch.Busy());
        g_seedSearch.Stop();
    }
    g_plannerOptions.searchCount = 20;
    assert(BeginPlannerSearch(*source));
    assert(g_commands.plannerRequest == 3 && g_commands.plannerSearchCount == practice::SeedSpace &&
           g_seedSearch.Get()->request.count == practice::SeedSpace &&
           g_seedSearch.Get()->request.serial == g_sentSerial + 1);
    // The real Start action above is continuous; use a finite engine fixture for the streaming-render cases.
    auto finiteRequest = g_seedSearch.Get()->request;
    finiteRequest.count = 20;
    g_seedSearch.Begin(finiteRequest);
    auto randomRequest = g_seedSearch.Get()->request;
    assert(g_commands.plannerSearchKey == randomRequest.key &&
           randomRequest.seed == practice::RandomSeedOrder(randomRequest.key).SeedAt(0));
    auto randomData = std::make_shared<practice::MapData>(*source->data);
    randomData->seed = randomRequest.seed;
    auto randomSource = std::make_shared<practice::MapAnalysis>(*source);
    randomSource->data = randomData;
    source = randomSource;
    g_seedSearch.Update(source);
    FeedStreaming(g_seedSearch, g_seedSearch.Get()->request, choices);
    auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (g_seedSearch.Busy() && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    assert(g_seedSearch.Get()->phase == practice::SearchPhase::Completed && !g_seedSearch.Get()->top.empty());
    auto selectedBest = g_seedSearch.Get();
    g_practiceSeed = 99;
    g_rawPracticeSpawn = false;
    g_commands.speedrunLoadLevelCmd = 0;
    assert(AdoptSearchResult(*selectedBest, 0));
    assert(g_practiceSeed == selectedBest->top[0].seed && g_plannerOptions.seed == g_practiceSeed &&
           g_rawPracticeSpawn && g_seededPractice && g_practiceMode && g_speedrunMode &&
           g_selectedSpeedrunLevel == 0);
    assert(g_commands.practiceSeed == g_practiceSeed && (g_commands.plannerOptions & 2u) &&
           g_commands.speedrunLoadLevelCmd == 0);
    assert(g_seedSearch.Get()->preview->data->seed == g_practiceSeed &&
           g_seedSearch.Get()->preview->route.length == selectedBest->top[0].route.length);
    assert(KnownPracticeSeedType(g_practiceSeed, 0) == 1 && KnownPracticeSeedType(g_practiceSeed, 6) == 0);
    SetPracticeSelection(-71234, 2, 6);
    assert(g_practiceSeed == -71234 && !g_rawPracticeSpawn && g_selectedSpeedrunLevel == 6 &&
           g_commands.practiceSeed == -71234 && !(g_commands.plannerOptions & 2u));
    g_plannerOptions.level = 0;
    TrainerPreferences transferred;
    transferred.seed = -71234;
    transferred.level = 6;
    transferred.rawSpawn = true;
    transferred.planner.portableGeneration = false;
    auto recovered = TrainerPreferences::Decode(transferred.Encode());
    assert(recovered.seed == -71234 && recovered.level == 6 && recovered.rawSpawn &&
           !recovered.planner.portableGeneration);
    int searchViews = 0;
    for (int state = 0; state < 3; state++)
    {
        if (state == 1)
            g_seedSearch.Begin(g_seedSearch.Get()->request);
        if (state == 2)
            g_seedSearch.Stop();
        for (int frame = 0; frame < 3; frame++)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(740, 680), ImGuiCond_Always);
            ImGui::Begin("Seed search QA");
            RenderSeedSearchControls(source, true, false);
            ImGui::End();
            ImGui::Render();
        }
        assert(ImGui::GetDrawData()->TotalVtxCount > 0);
        searchViews++;
        if (argc > 1)
            Dump(std::string(argv[1]) + "/search-state-" + std::to_string(state) + ".draw");
    }
    std::cout << "Actual Random Seed / Search request actions and result/progress/stop UI: PASS ("
              << searchViews << " states)\n";
    g_seedSearch.Stop();
    g_plannerOptions.level = 0;
    BindPlannerFilters(*source);
    auto &filter = g_plannerOptions.filters[0];
    filter.spawn = 1;
    g_plannerOptions.searchObjective = 1;
    assert(BeginPlannerSearch(*source));
    filter.spawn = -1;
    g_plannerOptions.searchObjective = 0;
    assert(g_seedSearch.Get()->request.criteria.spawn == 1 &&
           g_seedSearch.Get()->request.criteria.objective == practice::SearchObjective::LongestRoute);
    g_seedSearch.Stop();
    filter.spawn = 1;
    for (int frame = 0; frame < 3; frame++)
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(740, 680), ImGuiCond_Always);
        ImGui::Begin("Search customization QA");
        RenderSearchFilters(source, false);
        ImGui::End();
        ImGui::Render();
    }
    if (argc > 1)
        Dump(std::string(argv[1]) + "/search-filters.draw");
    for (bool autoLoad : {false, true})
        for (bool route : {false, true})
        {
            g_plannerOptions.automaticLoad = autoLoad;
            g_plannerOptions.calculateRoutes = route;
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(740, 680), ImGuiCond_Always);
            ImGui::Begin("Search modes QA");
            RenderSeedSearchControls(source, true, false);
            ImGui::End();
            ImGui::Render();
            if (argc > 1)
                Dump(std::string(argv[1]) + "/search-mode-" + std::to_string(autoLoad) + "-" +
                     std::to_string(route) + ".draw");
        }
    std::cout
        << "Spawn-only filters / frozen criteria / automatic and manual generation / optional route UI: PASS\n";
    auto missingUI = SeedFixture();
    missingUI.vertices.clear();
    missingUI.indices.clear();
    missingUI.walls.clear();
    auto estimatedSource = StreamingSource(missingUI);
    auto estimatedRequest = StreamingRequest(*estimatedSource, 20, 2, true);
    g_seedSearch.Begin(estimatedRequest);
    g_seedSearch.Update(estimatedSource);
    FeedStreaming(g_seedSearch, estimatedRequest, missingUI.samples);
    auto estimatedResult = AwaitStreaming(g_seedSearch);
    assert(estimatedResult->phase == practice::SearchPhase::Completed &&
           estimatedResult->top.front().route.estimated && estimatedResult->preview);
    for (int frame = 0; frame < 3; frame++)
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(740, 680), ImGuiCond_Always);
        ImGui::Begin("Estimated search QA");
        RenderSeedSearchControls(estimatedSource, true, false);
        ImGui::End();
        ImGui::Render();
    }
    if (argc > 1)
        Dump(std::string(argv[1]) + "/search-estimated.draw");
    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(740, 680), ImGuiCond_Always);
    ImGui::Begin("Estimated route QA");
    RenderSeedCanvas(*estimatedResult->preview);
    ImGui::TextWrapped("%s", estimatedResult->preview->route.message.c_str());
    ImGui::End();
    ImGui::Render();
    bool amber = false;
    for (const auto *list : ImGui::GetDrawData()->CmdLists)
        for (const auto &vertex : list->VtxBuffer)
            if (vertex.col == IM_COL32(249, 187, 95, 255))
                amber = true;
    assert(!amber);
    if (argc > 1)
        Dump(std::string(argv[1]) + "/planner-estimated.draw");
    std::cout << "Missing navigation search UI retains results and hides unverified path legs: PASS\n";
    auto recorded = SeedFixture();
    recorded.scene = 5;
    recorded.wallOutlines = {{{practice::Vec3{15, 0, 12}, practice::Vec3{18, 0, 15},
                               practice::Vec3{15, 0, 18}, practice::Vec3{12, 0, 15}},
                              false}};
    recorded.agentRadius = .35f;
    recorded.agentHeight = 1.8f;
    recorded.agentSlope = 45;
    recorded.agentStep = .3f;
    recorded.movementTick = .02f;
    recorded.movementSprintStep = .14f;
    recorded.agentFloor = 20; // First capture while airborne must not erase the floor.
    for (auto wall : recorded.walls)
        recorded.obstacles.push_back({{{{wall.minX, 0, wall.minZ},
                                        {wall.maxX, 0, wall.minZ},
                                        {wall.maxX, 0, wall.maxZ},
                                        {wall.minX, 0, wall.maxZ}}},
                                      false});
    auto captured = std::make_shared<practice::MapData>(recorded);
    practice::NavGraph reviewGraph(recorded);
    auto suggested = practice::AnalyzeMap(captured, reviewGraph).route;
    assert(suggested.complete && !suggested.estimated);
    auto review = std::make_shared<practice::RunReview>();
    review->id = 123456;
    review->outcome = 3;
    review->seconds = 12;
    for (int i = 0; i <= 300; i++)
        review->trace.push_back({i * .04, practice::SuggestedSlice(suggested, i / 300.).marker});
    review->collected = {0, 1};
    review->pickupSeconds = {5, 8};
    review->validationChecked = review->valid = review->observedFromStart = review->seedKnown = true;
    review->seedKind = 1;
    review->lightSources = 255;
    review->validityReason = "Synthetic local validation for UI QA";
    practice::ReplayMonster enemy;
    enemy.id = 1;
    enemy.kind = 2;
    for (int i = 0; i <= 120; i++)
        enemy.trace.push_back(
            {i * .1, practice::SuggestedSlice(suggested, std::min(1., i / 150.)).marker, true});
    enemy.paths = {{0, suggested.line}};
    review->monsters.push_back(enemy);
    review->flashes = {{3, review->trace[75].point, 1, 0}, {6.1, review->trace[152].point, 2, 0}};
    recorded.review = review;
    g_mapWorker.Submit(EncodeFixture(recorded, 10), g_session);
    auto reviewEnd = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (std::chrono::steady_clock::now() < reviewEnd)
    {
        auto r = g_mapWorker.Get(2);
        if (r && !r->working && r->data && r->data->review)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    assert(g_mapWorker.Get(2) && g_mapWorker.Get(2)->data->review && !g_mapWorker.Get(2)->live);
    data.reviewReady = 1;
    data.reviewRunId = review->id;
    data.reviewScene = 5;
    data.reviewSamples = 3;
    g_speedrunMode = true;
    g_practiceMode = false;
    data.timerState = 2;
    RequestRunReview();
    assert(g_speedrunMode && !g_practiceMode && g_commands.runReviewRequest == 1 &&
           g_commands.runReviewId == review->id && g_reviewLevel == 2 && g_runReviewOpen);
    // Ending a newly started run may update telemetry's "last run". Review
    // must keep the requested identity/scene rather than silently switching.
    data.reviewRunId = 654321;
    data.reviewScene = 3;
    int levelBefore = g_plannerOptions.level;
    io.DisplaySize = ImVec2(1280, 1080);
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    for (int frame = 0; frame < 3; frame++)
    {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(880, 1020), ImGuiCond_Always);
        ImGui::Begin("Recorded run QA");
        RenderRunReview();
        ImGui::End();
        ImGui::Render();
    }
    assert(g_plannerOptions.level == levelBefore && g_reviewTarget == 123456 && g_reviewLevel == 2);
    bool pink = false;
    for (const auto *list : ImGui::GetDrawData()->CmdLists)
        for (const auto &vertex : list->VtxBuffer)
            if (vertex.col == IM_COL32(233, 119, 220, 255))
                pink = true;
    assert(pink);
    if (argc > 1)
        Dump(std::string(argv[1]) + "/run-review.draw");
    // Independent toggles must affect actual map drawing, including both off.
    for (bool played : {false, true})
        for (bool optimal : {false, true})
        {
            auto p = played ? practice::PlayedSlice(*review, 4) : practice::PlaybackSlice{};
            auto q = optimal ? practice::SuggestedSlice(suggested, .33) : practice::PlaybackSlice{};
            SeedCanvasPlayback options{&p, &q, optimal};
            ImGui::NewFrame();
            ImGui::SetNextWindowSize(ImVec2(880, 720), ImGuiCond_Always);
            ImGui::Begin("Replay toggles QA");
            RenderSeedCanvas(*g_mapWorker.Get(2), 550, &options);
            ImGui::End();
            ImGui::Render();
            bool playerColor = false, suggestedColor = false;
            for (const auto *list : ImGui::GetDrawData()->CmdLists)
                for (const auto &vertex : list->VtxBuffer)
                {
                    playerColor |= vertex.col == IM_COL32(233, 119, 220, 255);
                    suggestedColor |= vertex.col == IM_COL32(106, 214, 196, 255);
                }
            assert(playerColor == played && suggestedColor == optimal);
        }
    // The new effects must obey the actual drawing toggles.
    for (bool enemies : {false, true})
        for (bool navigation : {false, true})
            for (bool flashes : {false, true})
            {
                auto poses = practice::MonstersAt(*review, 6.25, navigation);
                practice::PlaybackSlice none;
                SeedCanvasPlayback options{&none,        &none, false,  enemies ? &poses : nullptr,
                                           review.get(), 6.25,  flashes};
                ImGui::NewFrame();
                ImGui::SetNextWindowSize(ImVec2(1000, 720), ImGuiCond_Always);
                ImGui::Begin("Replay effects QA");
                RenderSeedCanvas(*g_mapWorker.Get(2), 550, &options);
                ImGui::End();
                ImGui::Render();
                bool monsterColor = false, dashColor = false, flashColor = false;
                for (const auto *list : ImGui::GetDrawData()->CmdLists)
                    for (const auto &v : list->VtxBuffer)
                    {
                        monsterColor |= v.col == IM_COL32(250, 104, 91, 255);
                        dashColor |= v.col == IM_COL32(248, 122, 100, 180);
                        flashColor |= (v.col & 0xffffffu) == 0xffffffu && (v.col >> 24) == 163;
                    }
                assert(monsterColor == enemies && dashColor == (enemies && navigation) &&
                       flashColor == flashes);
            }
    {
        auto missed = *review;
        missed.monsters.clear();
        practice::PlaybackSlice none;
        SeedCanvasPlayback options{&none, &none, false, nullptr, &missed, 6.25, true};
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(1000, 720), ImGuiCond_Always);
        ImGui::Begin("Missed camera flash QA");
        RenderSeedCanvas(*g_mapWorker.Get(2), 550, &options);
        ImGui::End();
        ImGui::Render();
        bool pulse = false, floor = false, exactWall = false;
        for (const auto *list : ImGui::GetDrawData()->CmdLists)
            for (const auto &v : list->VtxBuffer)
            {
                pulse |= (v.col & 0xffffffu) == 0xffffffu && (v.col >> 24) == 163;
                floor |= v.col == IM_COL32(27, 43, 54, 255);
                exactWall |= v.col == IM_COL32(92, 117, 139, 220);
            }
        assert(pulse && floor && exactWall);
        if (argc > 1)
            Dump(std::string(argv[1]) + "/replay-flash-miss.draw");
    }
    if (argc > 1)
    {
        auto folder = std::filesystem::path(argv[1]) / "replay-ui-library";
        std::filesystem::remove_all(folder);
        practice::g_replayArchive.SetDirectory(folder);
        practice::g_replayArchive.Save(g_mapWorker.Get(2)->data, "Hide & Seek - Training", "Routing",
                                       "Check the camera corner.");
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (std::chrono::steady_clock::now() < deadline && practice::g_replayArchive.Get()->runs.empty())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        assert(practice::g_replayArchive.Get()->runs.size() == 1);
        for (int frame = 0; frame < 3; frame++)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowSize(ImVec2(1100, 640), ImGuiCond_Always);
            ImGui::Begin("Replay library QA");
            ReplayList();
            ImGui::End();
            ImGui::Render();
        }
        Dump(std::string(argv[1]) + "/replay-library.draw");
        bool orange = false, green = false;
        auto abortColor = ImGui::ColorConvertFloat4ToU32(ReplayStatusColor(3, true)),
             validColor = ImGui::ColorConvertFloat4ToU32(ReplayStatusColor(1, true));
        for (const auto *list : ImGui::GetDrawData()->CmdLists)
            for (const auto &v : list->VtxBuffer)
            {
                orange |= v.col == abortColor;
                green |= v.col == validColor;
            }
        assert(orange && !green);
        for (int outcome : {2, 3, 4, 5})
            for (bool valid : {false, true})
                assert(ImGui::ColorConvertFloat4ToU32(ReplayStatusColor(outcome, valid)) == abortColor);
        for (ImVec2 viewport : {ImVec2(480, 360), ImVec2(1280, 720)})
        {
            io.DisplaySize = viewport;
            ApplyTheme(1);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            ImGui::NewFrame();
            ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_Always);
            ImGui::Begin("Replay edit QA");
            ImGui::OpenPopup("Replay details");
            ReplayEditor(g_mapWorker.Get(2)->data);
            auto popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
            assert(popup && popup->Size.x <= viewport.x);
            ImGui::End();
            ImGui::Render();
        }
    }
    std::cout
        << "Replay library/edit popup, mandatory saved validation, enemy/path/flash toggles and v8 drawing: PASS\n";
    g_reviewPlayback.seconds = 6.25;
    g_reviewPlayback.playing = false;
    // Full application tab selection expands the review, and switching back
    // restores the user's original window size. Cover small/high-DPI displays.
    g_allOverlayEnabled = g_overlayVisible = true;
    g_compactMode = false;
    for (ImVec2 viewport : {ImVec2(480, 360), ImVec2(1280, 720), ImVec2(1920, 1080)})
        for (float dpi : {1.f, 1.5f})
        {
            io.DisplaySize = viewport;
            ApplyTheme(dpi);
            io.Fonts->Build();
            io.Fonts->SetTexID(1);
            g_reviewWide = true;
            g_resetLayout = true;
            g_selectRunReview = true;
            Frame();
            g_resetLayout = false;
            Frame();
            Frame();
            auto *window = ImGui::FindWindowByName("It Steals | Practice Suite");
            assert(window && window->Size.x >= std::min(1000.f, viewport.x - 24));
            if (viewport.x >= 1280 && dpi == 1)
            {
                float top = FLT_MAX, bottom = -FLT_MAX;
                for (const auto *list : ImGui::GetDrawData()->CmdLists)
                    for (const auto &vertex : list->VtxBuffer)
                        if (vertex.col == IM_COL32(12, 19, 27, 255))
                        {
                            top = std::min(top, vertex.pos.y);
                            bottom = std::max(bottom, vertex.pos.y);
                        }
                if (bottom - top < (viewport.x == 1280 ? 350.f : 650.f))
                    std::cerr << "Replay map height " << bottom - top << " at " << viewport.x << ","
                              << viewport.y << "\n";
                assert(bottom - top >= (viewport.x == 1280 ? 350.f : 650.f));
                for (auto *pane : GImGui->Windows)
                    if (std::strstr(pane->Name, "/Workspace_") && pane->ParentWindow == window &&
                        pane->LastFrameActive == ImGui::GetFrameCount())
                    {
                        if (pane->ScrollMax.y >= 1)
                            std::cerr << "Unexpected replay scroll " << pane->Name << " = "
                                      << pane->ScrollMax.y << " / size " << pane->Size.y << " / content "
                                      << pane->ContentSize.y << " at " << viewport.x << "\n";
                        assert(pane->ScrollMax.y < 1);
                    }
            }
            if (argc > 1 && dpi == 1 && viewport.x >= 1280)
                Dump(std::string(argv[1]) + "/run-review-" + std::to_string(int(viewport.x)) + ".draw");
            suite_ui::navigation.Select(suite_ui::Page::Cheats, ImGui::GetTime());
            Frame();
            Frame();
            assert(!g_reviewTabActive);
        }
    std::cout
        << "Replay map: large responsive view, previous size restoration and all four independent route toggle combinations: PASS\n";
    std::cout
        << "Mandatory foreground VALID / UNVALID despite hidden metrics/F2/disconnection, HUD alpha/coordinates and frozen review identity: PASS\n";
    io.DisplaySize = ImVec2(1920, 1080);
    ApplyTheme(1);
    io.Fonts->Build();
    io.Fonts->SetTexID(1);
    g_pShared = &data;
    g_speedrunMode = g_practiceMode = g_compactMode = true;
    g_overlayVisible = false;
    g_allOverlayEnabled = true;
    g_showMinimapWindow = true;
    data.playerReady = 1;
    data.timerState = 1;
    data.currentSceneIndex = 3;
    g_minimapWidth = g_minimapHeight = 250;
    g_mapOptions.anchor = 0;
    g_speedrunLayout.anchor = 3;
    g_hudLayout.anchor = 3;
    uint64_t copied = 0;
    for (float opacity : {.25f, 1.f})
    {
        g_hudLayout.opacity = opacity;
        for (int frame = 0; frame < 3; frame++)
        {
            ImGui::NewFrame();
            g_validityBadgeDrawn = false;
            g_validityBadgeBounds = {};
            RenderMinimapWindow(12, 12, 0, 0, 0, 0, 0, 0, nullptr, 0, 100, 0, 100, true, false, false);
            RenderTimerOverlay("Classic");
            ImGui::Render();
            copied = CheckHudRegions();
            auto regions = HudContentRegions(*ImGui::GetDrawData(), 1920, 1080);
            assert(regions.size() == 2 && copied < 1920ull * 1080 * 4 / 4);
            bool badge = false;
            for (auto r : regions)
                badge |= r.left <= g_validityBadgeBounds.left && r.top <= g_validityBadgeBounds.top &&
                         r.right >= g_validityBadgeBounds.right && r.bottom >= g_validityBadgeBounds.bottom;
            assert(g_validityBadgeDrawn && badge);
        }
        if (argc > 1)
            Dump(std::string(argv[1]) + "/hud-regions-opacity-" + std::to_string(int(opacity * 100)) +
                 ".draw");
    }
    std::cout << "Occupied HUD transfer: " << copied << " vs " << 1920ull * 1080 * 4
              << " bytes/frame at 1920x1080 (two corners, same pixels and alpha)\n";
    for (int scene : kMapScenes)
    {
        data.currentSceneIndex = scene;
        bool obsoleteLabel = false;
        for (int frame = 0; frame < 3; frame++)
        {
            ImGui::NewFrame();
            RenderMinimapWindow(12, 12, 0, 0, 0, 0, 0, 0, nullptr, 0, 100, 0, 100, true, false, false);
            ImGui::Render();
            CheckHudRegions();
            obsoleteLabel = false;
            for (const auto *list : ImGui::GetDrawData()->CmdLists)
                for (auto v : list->VtxBuffer)
                    obsoleteLabel |= v.col == IM_COL32(255, 177, 76, 255);
        }
        assert(!obsoleteLabel);
        auto d = std::make_shared<practice::MapData>(fixture);
        d->scene = scene;
        d->CalculateBounds();
        practice::MapAnalysis map;
        map.data = d;
        map.route = live.route;
        map.outline = live.outline;
        for (int frame = 0; frame < 3; frame++)
        {
            ImGui::NewFrame();
            ImGui::SetNextWindowSize(ImVec2(1000, 600), ImGuiCond_Always);
            ImGui::Begin("Supported map geometry QA");
            RenderSeedCanvas(map, 450);
            ImGui::End();
            ImGui::Render();
            CheckHudRegions();
        }
        obsoleteLabel = false;
        for (const auto *list : ImGui::GetDrawData()->CmdLists)
            for (auto v : list->VtxBuffer)
                obsoleteLabel |= v.col == IM_COL32(255, 177, 76, 255);
        assert(!obsoleteLabel);
        if (argc > 1 && scene == 5)
            Dump(std::string(argv[1]) + "/hide-seek-supported.draw");
    }
    auto timingMap = std::make_shared<practice::MapAnalysis>(live);
    auto timingData = std::make_shared<practice::MapData>(*live.data);
    timingData->agentRadius = .35f;
    timingData->agentHeight = 1.8f;
    timingData->movementTick = .02f;
    timingData->movementSprintStep = .14f;
    timingMap->data = timingData;
    auto movement = std::make_shared<practice::MovementData>();
    movement->scene = 3;
    movement->hash = timingData->hash;
    movement->tick = .02f;
    movement->radius = .35f;
    movement->height = 1.8f;
    for (int k = 0; k < 5; k++)
    {
        movement->counts[k] = 100;
        movement->lengths[k] = 1;
        movement->speeds[k * 3] = 8.1f;
        movement->speeds[k * 3 + 1] = 9.f;
        movement->speeds[k * 3 + 2] = 9.6f;
    }
    practice::PublishMovement(movement);
    const auto *cached = &PreviewMovementTiming(timingMap);
    assert(cached->valid);
    double seconds = cached->seconds;
    assert(&PreviewMovementTiming(timingMap) == cached && cached->seconds == seconds);
    auto updated = std::make_shared<practice::MovementData>(*movement);
    updated->revision++;
    updated->speeds[1] = 9.5f;
    practice::PublishMovement(updated);
    assert(PreviewMovementTiming(timingMap).seconds < seconds);
    std::cout
        << "Clipped ImGui primitives fully covered by <=4 nonoverlapping HUD regions, mandatory badge, two corner panels/opacity, all supported scenes without obsolete labels, and live timing cache refresh: PASS\n";
    UpdateUICases(argc > 1 ? argv[1] : "");
    ExperienceCases(argc > 1 ? argv[1] : "");
    AppearanceCases(argc > 1 ? argv[1] : "");
    FeedbackRevisionCases(argc > 1 ? argv[1] : "");
    MilestoneCases(argc > 1 ? argv[1] : "");
    PersistentFeedbackCases(argc > 1 ? argv[1] : "");
    FinishVerificationCases(argc > 1 ? argv[1] : "");
    DesignInteractionCases();
    DesignGallery(argc > 1 ? argv[1] : "");
    ReplayInputCases(argc > 1 ? argv[1] : ".");
    if (argc > 1)
    {
        ReplayExperienceUICases(argv[1]);
        ReplayHealthUICases(argv[1]);
    }
    Workshop740Cases(argc > 1 ? argv[1] : "");
    Workshop750Cases(argc > 1 ? argv[1] : "");
    Workshop751Cases(argc > 1 ? argv[1] : "");
    Workshop752Cases(argc > 1 ? argv[1] : "");
    std::fprintf(
        stderr,
        "7.9 all UI groups completed: recording check, clip/camera/speed/layer editing, main GUI, HUD, minimap and finish screen.\n");
    PhysicalMapUICases(argc > 1 ? argv[1] : "");
    if (argc > 1)
    {
        InspectorUICases(argv[1]);
        SeedRouteReliabilityUICases(argv[1]);
    }
    if (argc > 1)
    {
        std::ofstream done(std::string(argv[1]) + "/ui-complete.txt");
        done
            << "All UI groups completed: recording check, exact gap seek/copy, 480/1920, 100/150 DPI, camera/speed/layers, HUD/minimap and finish screen.\n";
        assert(done);
    }
    ImGui::DestroyContext();
    std::cout << "Real ImGui UI: PASS (" << cases << " tab / viewport / DPI / connection cases)\n";
    std::cout << "Two HUD configurations / customization / mandatory warnings: PASS (" << overlayCases
              << " cases)\n";
    std::cout << "Aborted / completed / ready status and stopped overlay timer: PASS\n";
    std::cout << "Actual overlay mode exclusivity / stale telemetry / hiding: PASS (" << modeCases
              << " cases)\n";
    std::cout << "Actual minimap transforms / filters / zoom: PASS (" << mapCases << " cases)\n";
}
