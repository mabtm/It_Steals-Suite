#pragma once
#include "SuiteUpdater.h"
#include "SuiteDesign.h"
#include <sstream>

namespace suite_update_ui
{
inline int selected = 0;
inline std::vector<suite_update::Release> history;
inline std::shared_ptr<const suite_update::Snapshot> historySource;
inline size_t bundledCount = 0;
inline char token[suite_update::TokenLimit + 1]{};
inline bool showAccess = false;
inline std::string requestedVersion;
inline double historyOpenedAt = -1;

inline bool Open()
{
    auto &p = suite_update::service.presentation;
    return p.changelog || p.prompt;
}

inline void Close()
{
    auto &service = suite_update::service;
    if (service.presentation.changelog)
        service.MarkRead();
    service.presentation.prompt = false;
}

inline void ShowHistory(std::string version = suite_brand::Version)
{
    requestedVersion = std::move(version);
    selected = 0;
    suite_update::service.presentation.changelog = true;
    suite_update::service.presentation.prompt = false;
    suite_update::service.presentation.appeared = suite_ui::LifecycleNow();
}

inline void RefreshHistory(std::shared_ptr<const suite_update::Snapshot> view)
{
    const auto &snapshot = *view;
    if (historySource == view && bundledCount == suite_update::service.bundled.size())
        return;
    historySource = std::move(view);
    bundledCount = suite_update::service.bundled.size();
    history = suite_update::service.bundled;
    // GitHub pages include artwork and downloads. Show their changelog section
    // here, retaining embedded history when the trainer is offline.
    for (const auto &release : snapshot.releases)
    {
        auto display = release;
        auto sections = suite_update::Changelog(release.notes);
        auto changes = std::find_if(sections.begin(), sections.end(),
                                    [&](const auto &r) { return r.version == release.version; });
        if (changes != sections.end())
            display.notes = changes->notes;
        auto existing = std::find_if(history.begin(), history.end(),
                                     [&](const auto &r) { return r.version == release.version; });
        if (existing == history.end())
            history.push_back(std::move(display));
        else if (!display.notes.empty())
            *existing = std::move(display);
    }
    std::sort(history.begin(), history.end(), [](const auto &a, const auto &b) {
        return suite_update::Version::Parse(a.version) > suite_update::Version::Parse(b.version);
    });
    selected = std::clamp(selected, 0, std::max(0, int(history.size()) - 1));
}

inline void Notes(const std::string &body)
{
    std::istringstream text(body);
    std::string line;
    bool blank = false;
    while (std::getline(text, line))
    {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
        {
            if (!blank)
                ImGui::Spacing();
            blank = true;
            continue;
        }
        blank = false;
        if (line.starts_with("#"))
        {
            auto begin = line.find_first_not_of("# ");
            ImGui::PushStyleColor(ImGuiCol_Text, suite_ui::heading);
            ImGui::TextWrapped("%s", begin == std::string::npos ? "" : line.c_str() + begin);
            ImGui::PopStyleColor();
        }
        else if (line.starts_with("- ") || line.starts_with("* "))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, suite_ui::heading);
            ImGui::Bullet();
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::TextWrapped("%s", line.c_str() + 2);
        }
        else
            ImGui::TextWrapped("%s", line.c_str());
    }
}

inline void Versions(const suite_update::Snapshot &snapshot)
{
    float u = suite_ui::Unit();
    bool newer = suite_update::Newer(snapshot.latest, suite_brand::Version);
    if (ImGui::BeginTable("##ReleaseVersions", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();
        ImGui::TextDisabled("CURRENT");
        if (suite_ui::introTitleFont)
            ImGui::PushFont(suite_ui::introTitleFont);
        ImGui::Text("v%s", suite_brand::Version);
        if (suite_ui::introTitleFont)
            ImGui::PopFont();
        ImGui::TableNextColumn();
        ImGui::TextDisabled(newer ? "NEW RELEASE" : "STABLE CHANNEL");
        if (suite_ui::introTitleFont)
            ImGui::PushFont(suite_ui::introTitleFont);
        ImGui::TextColored(suite_ui::heading, "%s",
                           newer ? ("v" + snapshot.latest.version).c_str()
                           : snapshot.phase == suite_update::Phase::Current ? "Up to date"
                                                                            : "Not checked");
        if (suite_ui::introTitleFont)
            ImGui::PopFont();
        ImGui::EndTable();
    }
    ImGui::Dummy({0, 4 * u});
}

inline void Steps(const suite_update::Snapshot &snapshot)
{
    float u = suite_ui::Unit();
    auto at = ImGui::GetCursorScreenPos();
    float w = std::max(100.f, ImGui::GetContentRegionAvail().x);
    int step = snapshot.phase == suite_update::Phase::Installing ? 2
               : snapshot.phase == suite_update::Phase::Ready    ? 1
                                                                 : 0;
    auto *draw = ImGui::GetWindowDrawList();
    constexpr const char *names[] = {"DOWNLOAD", "VERIFY", "RESTART"};
    for (int i = 0; i < 3; i++)
    {
        float x = at.x + 8 * u + i * w / 3;
        auto ink = i <= step ? suite_ui::heading : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
        draw->AddCircleFilled({x, at.y + 8 * u}, 3.5f * u, ImGui::GetColorU32(ink));
        if (i < 2)
            draw->AddLine({x + 7 * u, at.y + 8 * u}, {x + w / 3 - 8 * u, at.y + 8 * u},
                          ImGui::GetColorU32(ImGuiCol_Border), u);
        draw->AddText(nullptr, 12 * u, {x - 4 * u, at.y + 20 * u}, ImGui::GetColorU32(ink), names[i]);
    }
    ImGui::Dummy({w, 43 * u});
}

inline void StartInstall()
{
    auto &service = suite_update::service;
    if (service.DownloadLatest())
    {
        service.presentation.installRequested = true;
        service.presentation.prompt = false;
    }
}

inline void Controls(const suite_update::Snapshot &snapshot, bool blocked)
{
    auto &service = suite_update::service;
    bool newer = suite_update::Newer(snapshot.latest, suite_brand::Version);
    bool busy = suite_update::Busy(snapshot.phase);
    ImGui::PushTextWrapPos();
    if (snapshot.phase == suite_update::Phase::Error)
        ImGui::PushStyleColor(ImGuiCol_Text, {1.f, .59f, .44f, 1});
    ImGui::TextWrapped("%s", snapshot.message.c_str());
    if (snapshot.phase == suite_update::Phase::Error)
        ImGui::PopStyleColor();
    ImGui::PopTextWrapPos();
    if (snapshot.phase == suite_update::Phase::Downloading || snapshot.phase == suite_update::Phase::Ready ||
        snapshot.phase == suite_update::Phase::Installing)
    {
        Steps(snapshot);
        float progress = snapshot.latest.size
                             ? std::min(1.f, float(service.received.load()) / float(snapshot.latest.size))
                             : 0;
        ImGui::ProgressBar(progress, {-1, 6 * suite_ui::Unit()}, "");
        ImGui::TextDisabled("%.1f / %.1f MB", service.received.load() / 1048576.,
                            snapshot.latest.size / 1048576.);
    }
    if (snapshot.phase == suite_update::Phase::Checking)
        suite_ui::Busy("Contacting GitHub...");
    ImGui::BeginDisabled(busy || snapshot.phase == suite_update::Phase::Ready);
    if (suite_ui::Button("Check for updates"))
        service.Check();
    ImGui::EndDisabled();
    if (newer)
    {
        ImGui::SameLine();
        ImGui::BeginDisabled(blocked || busy || !snapshot.latest.Installable());
        if (suite_ui::Button(snapshot.phase == suite_update::Phase::Ready ? "Restart & install"
                                                                          : "Install update"))
        {
            if (snapshot.phase == suite_update::Phase::Ready)
                service.presentation.installRequested = true;
            else
                StartInstall();
        }
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(
                blocked
                    ? "Finish the current run or close the Clip Studio before installing."
                    : "Downloads and verifies the complete Suite, closes the game and trainer, installs, then restarts both. Windows will ask for permission.");
    }
    if (snapshot.phase == suite_update::Phase::Downloading)
    {
        ImGui::SameLine();
        if (suite_ui::Button("Cancel"))
        {
            service.Cancel();
            service.presentation.installRequested = false;
        }
    }
    if (blocked && (newer || snapshot.phase == suite_update::Phase::Ready))
        ImGui::TextWrapped("Installation waits until the run or export has finished.");
}

inline void Page(const suite_update::Snapshot &snapshot, bool blocked)
{
    auto &service = suite_update::service;
    ImGui::BeginChild("##UpdatesScroll", {0, 0});
    suite_ui::Section("PracticeSuite / stable releases", ImGuiTreeNodeFlags_DefaultOpen, [&] {
        Versions(snapshot);
        Controls(snapshot, blocked);
        ImGui::Spacing();
        if (suite_ui::Button("Version history"))
            ShowHistory();
        ImGui::SameLine();
        if (suite_ui::Checkbox("Check automatically", &service.autoCheck))
        {
            service.SaveState();
            if (service.autoCheck)
                service.Check();
        }
    });
    ImGui::Spacing();
    suite_ui::Section("Release access", ImGuiTreeNodeFlags_DefaultOpen, [&] {
        ImGui::TextColored(suite_ui::heading, "%s",
                           snapshot.credential ? "GitHub credential saved on this PC"
                                               : "Public releases need no sign-in");
        ImGui::TextWrapped(
            "Private builds require repository access. Use a fine-grained GitHub token for this repository with Contents: read only.");
        if (suite_ui::Button(showAccess ? "Hide connection settings" : "Connect private releases"))
        {
            showAccess = !showAccess;
            if (!showAccess)
                std::fill(std::begin(token), std::end(token), 0);
        }
        if (showAccess)
        {
            ImGui::BeginDisabled(suite_update::Busy(snapshot.phase));
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##ReleaseToken", "GitHub token", token, sizeof(token),
                                     ImGuiInputTextFlags_Password);
            ImGui::BeginDisabled(token[0] == 0);
            if (suite_ui::Button("Save & check"))
            {
                if (service.StoreCredential(token))
                {
#ifdef _WIN32
                    SecureZeroMemory(token, sizeof(token));
#else
                    std::fill(std::begin(token), std::end(token), 0);
#endif
                    showAccess = false;
                    service.Check();
                }
            }
            ImGui::EndDisabled();
            if (snapshot.credential)
            {
                ImGui::SameLine();
                if (suite_ui::Button("Disconnect"))
                    service.StoreCredential({});
            }
            ImGui::EndDisabled();
            ImGui::TextDisabled(
                "Stored in Windows Credential Manager. Never included in reports or update files.");
        }
    });
    if (!snapshot.lastResult.empty())
    {
        ImGui::Spacing();
        suite_ui::Section("Previous installation", 0,
                          [&] { ImGui::TextWrapped("%s", snapshot.lastResult.c_str()); });
    }
    if (!service.stateError.empty())
        ImGui::TextWrapped("%s", service.stateError.c_str());
    ImGui::EndChild();
}

inline void Overlays(std::shared_ptr<const suite_update::Snapshot> view, bool blocked)
{
    const auto &snapshot = *view;
    auto &service = suite_update::service;
    auto &p = service.presentation;
    bool working = snapshot.phase == suite_update::Phase::Downloading ||
                   snapshot.phase == suite_update::Phase::Installing;
    if ((!Open() && !working) || blocked)
        return;
    if (Open() && ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        Close();
        return;
    }
    float u = suite_ui::Unit();
    auto display = ImGui::GetIO().DisplaySize;
    float fade = suite_ui::preferences.motion
                     ? suite_ui::Ease(float((suite_ui::LifecycleNow() - p.appeared) / .24))
                     : 1;
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * fade);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14 * u);
    if (p.changelog)
    {
        RefreshHistory(view);
        if (historyOpenedAt != p.appeared)
        {
            historyOpenedAt = p.appeared;
            auto version = requestedVersion.empty() ? std::string(suite_brand::Version) : requestedVersion;
            auto found = std::find_if(history.begin(), history.end(),
                                      [&](const auto &r) { return r.version == version; });
            selected = found == history.end() ? 0 : int(found - history.begin());
            requestedVersion.clear();
        }
        // A lightweight modal input shield disappears immediately when a run
        // begins; it does not leave a persistent ImGui popup stack behind.
        ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
        ImGui::SetNextWindowSize(display, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(.55f * fade);
        ImGui::Begin("##ChangelogShield", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav |
                         ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::End();
        ImVec2 size(std::min(740 * u, std::max(260.f, display.x - 24)),
                    std::min(700 * u, std::max(220.f, display.y - 32)));
        ImGui::SetNextWindowPos(
            {(display.x - size.x) * .5f, (display.y - size.y) * .5f + (1 - fade) * 12 * u}, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::Begin("##SuiteChangelog", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings);
        auto at = ImGui::GetCursorScreenPos();
        auto *draw = ImGui::GetWindowDrawList();
        suite_ui::Motif(draw, at, {at.x + size.x, at.y + 66 * u});
        if (suite_ui::introTitleFont)
            ImGui::PushFont(suite_ui::introTitleFont);
        ImGui::TextColored(suite_ui::heading, "What's new");
        if (suite_ui::introTitleFont)
            ImGui::PopFont();
        ImGui::SameLine(std::max(150 * u, ImGui::GetWindowContentRegionMax().x - 44 * u));
        if (suite_ui::Button("X##Changelog"))
            Close();
        ImGui::TextDisabled("PRACTICESUITE  /  v%s  /  mab", suite_brand::Version);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-1);
        const char *preview =
            history.empty() ? "Version history unavailable" : history[selected].version.c_str();
        if (ImGui::BeginCombo("##ChangelogVersion", preview))
        {
            for (int i = 0; i < int(history.size()); i++)
            {
                auto label = "v" + history[i].version +
                             (history[i].version == suite_brand::Version ? "  /  installed" : "");
                if (ImGui::Selectable(label.c_str(), selected == i))
                    selected = i;
                if (selected == i)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        float footer = ImGui::GetFrameHeightWithSpacing() + 10 * u;
        ImGui::BeginChild("##ChangelogBody", {0, std::max(40.f, ImGui::GetContentRegionAvail().y - footer)});
        if (!history.empty())
        {
            if (!history[selected].date.empty())
                ImGui::TextDisabled("RELEASED  %s", history[selected].date.c_str());
            Notes(history[selected].notes);
        }
        else
            ImGui::TextWrapped("Open Updates and check GitHub to load release notes.");
        ImGui::EndChild();
        if (suite_ui::Button("Continue", {-1, 0}))
            Close();
        ImGui::End();
    }
    else if (p.prompt)
    {
        float w = std::min(470 * u, std::max(260.f, display.x - 24));
        ImGui::SetNextWindowPos({std::max(12.f, display.x - w - 24 * u),
                                 std::max(12.f, display.y - 265 * u) + (1 - fade) * 14 * u},
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize({w, 0}, ImGuiCond_Always);
        ImGui::SetNextWindowSizeConstraints({w, 0}, {w, std::max(220.f, display.y - 24)});
        ImGui::Begin("##SuiteUpdateAvailable", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextColored(suite_ui::heading, "AN UPDATE IS AVAILABLE");
        ImGui::Text("Current  v%s   /   New  v%s", suite_brand::Version, snapshot.latest.version.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("The complete Suite is ready for an upgrade.");
        ImGui::BeginDisabled(!snapshot.latest.Installable());
        if (suite_ui::Button("Install"))
            StartInstall();
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (suite_ui::Button("Later"))
            p.prompt = false;
        ImGui::SameLine();
        if (suite_ui::Button("What's new"))
            ShowHistory(snapshot.latest.version);
        ImGui::TextDisabled("Install closes and restarts the game and trainer.");
        ImGui::End();
    }
    else if (working)
    {
        float w = std::min(470 * u, std::max(260.f, display.x - 24));
        ImGui::SetNextWindowPos({std::max(12.f, display.x - w - 24 * u), std::max(12.f, display.y - 220 * u)},
                                ImGuiCond_Always);
        ImGui::SetNextWindowSize({w, 0}, ImGuiCond_Always);
        ImGui::Begin("##SuiteUpdateProgress", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextColored(suite_ui::heading, "UPDATING PRACTICESUITE / v%s",
                           snapshot.latest.version.c_str());
        ImGui::TextWrapped("%s", snapshot.message.c_str());
        Steps(snapshot);
        if (snapshot.phase == suite_update::Phase::Downloading)
        {
            float progress =
                snapshot.latest.size ? float(service.received.load()) / float(snapshot.latest.size) : 0;
            ImGui::ProgressBar(std::min(1.f, progress), {-1, 6 * u}, "");
        }
        ImGui::End();
    }
    ImGui::PopStyleVar(2);
}
} // namespace suite_update_ui
