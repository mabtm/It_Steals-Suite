#pragma once
#include "ReplayHealthUI.h"
#include "SuiteExperience.h"
#include "SuiteDesign.h"
#include "ReplayTimelineUI.h"
#include "RunReviewState.h"
#include "ReplayArchive.h"
#include "ReplayLibraryModel.h"
#include "ReplayInputUI.h"
#include "ClipUI.h"
#include "RunExperienceModel.h"
#include "ReplaySplitModel.h"
#include "ReplayReferenceUI.h"
inline bool g_reviewFromArchive = false;
inline int64_t g_reviewLoadedId = 0;
inline practice::ReplayLibraryView g_replayLibraryView;
inline practice::ReplayLibraryFilter g_replayLibraryFilter;
inline void RestoreReplayFilters()
{
    const auto &p = suite_ui::replayPreferences;
    auto &f = g_replayLibraryFilter;
    f.level = p.libraryLevel; f.category = p.libraryCategory; f.status = p.libraryStatus;
    f.sort = practice::ReplaySort(p.librarySort); f.ascending = p.libraryAscending;
    f.favorites = p.libraryFavorites;
}
inline void RememberReplayFilters()
{
    auto &p = suite_ui::replayPreferences;
    const auto &f = g_replayLibraryFilter;
    p.libraryLevel = f.level; p.libraryCategory = f.category; p.libraryStatus = f.status;
    p.librarySort = int(f.sort); p.libraryAscending = f.ascending; p.libraryFavorites = f.favorites;
}

inline ImVec4 ReplayStatusColor(int outcome, bool valid)
{
    return practice::ReplayAborted(outcome) ? ImVec4(1, .65f, .22f, 1)
           : valid                          ? ImVec4(.4f, .9f, .55f, 1)
                                            : ImVec4(1, .38f, .32f, 1);
}

inline void ReplayStatusHelp(int outcome, bool valid, const std::string &reason)
{
    if (practice::ReplayAborted(outcome))
        Help((std::string("Aborted attempt: ") + OutcomeLabel(byte(outcome)) +
              "\nLocal integrity: " + (valid ? "VALID" : "UNVALID") + "\n" + reason)
                 .c_str());
    else
        Help(reason.c_str());
}

inline bool RequestRunReview(int64_t id, int scene)
{
    if (!g_pShared || RunInProgress(*g_pShared) || !g_pShared->reviewReady || HasPendingRequest() ||
        id <= 0 || g_pShared->reviewRunId != id || g_pShared->reviewScene != scene ||
        MapLevelIndex(scene) < 0)
        return false;
    StopReplay3D();
    g_commands.runReviewRequest = 1;
    g_commands.runReviewId = id;
    g_overlayVisible = true;
    g_compactMode = false;
    SetOverlayInteractive(true);
    g_seedSearch.Stop();
    g_seedSearch.HidePreview();
    suite_run::presentation.Acknowledge();
    g_selectRunReview = true;
    g_reviewWide = true;
    g_reviewFromArchive = false;
    g_runReviewOpen = true;
    g_reviewLevel = MapLevelIndex(scene);
    g_reviewTarget = id;
    practice::g_replayArchive.Refresh();
    return true;
}

inline void RequestRunReview()
{
    if (g_pShared)
        RequestRunReview(g_pShared->reviewRunId, g_pShared->reviewScene);
}

inline auto &g_reviewPlayback = suite_replay::playback;

inline void OpenSavedReplay(const practice::ReplayInfo &row)
{
    if (HasPendingRequest() || (g_pShared && RunInProgress(*g_pShared)))
        return;
    StopReplay3D();
    if (g_runReviewOpen && !g_reviewFromArchive)
        g_commands.runReviewRequest = 2;
    g_overlayVisible = true;
    g_compactMode = false;
    SetOverlayInteractive(true);
    g_seedSearch.Stop();
    g_seedSearch.HidePreview();
    suite_run::presentation.Acknowledge();
    g_reviewFromArchive = g_runReviewOpen = g_selectRunReview = g_reviewWide = true;
    g_reviewLevel = MapLevelIndex(row.scene);
    g_reviewTarget = row.id;
    g_reviewLoadedId = 0;
    practice::g_replayArchive.Load(row.id);
}
enum class FinishedReplaySource
{
    Missing,
    Helper,
    Archive
};

struct FinishedReplayAccess
{
    FinishedReplaySource source = FinishedReplaySource::Missing;
    const char *detail = "This run has no replay recording.";

    explicit operator bool() const
    {
        return source != FinishedReplaySource::Missing;
    }
};

inline bool FinishedReplayMatches(const practice::ReplayInfo &row, const GameData &d)
{
    return row.id == d.reviewRunId && row.scene == d.reviewScene && row.outcome == d.runOutcome &&
           std::abs(row.seconds - d.currentRunTimeSeconds) < .001 &&
           (!d.reviewReady || row.seed == d.reviewSpawnSeed);
}

inline FinishedReplayAccess FinishedReplayAvailable()
{
    const auto &p = suite_run::presentation;
    const auto &d = p.completed;
    if (!p.hasResult)
        return {};
    if (g_pShared && RunInProgress(*g_pShared))
        return {FinishedReplaySource::Missing, "Replay is unavailable during an active run."};
    if (HasPendingRequest())
        return {FinishedReplaySource::Missing, "Waiting for the previous command to finish."};
    if (!suite_run::ReplayIdentityUsable(d) || MapLevelIndex(d.reviewScene) < 0)
        return {FinishedReplaySource::Missing, "No matching recording was captured for this run."};
    if (g_pShared && g_session == p.completedSession && g_pShared->reviewReady &&
        g_pShared->reviewRunId == d.reviewRunId && g_pShared->reviewScene == d.reviewScene)
        return {FinishedReplaySource::Helper,
                "Open this run's recorded route and suggested route in Replay."};
    auto state = practice::g_replayArchive.Get();
    auto row = std::lower_bound(state->runs.begin(), state->runs.end(), d.reviewRunId,
                                [](const auto &item, int64_t id) { return item.id > id; });
    if (row != state->runs.end() && FinishedReplayMatches(*row, d))
        return {FinishedReplaySource::Archive, "Open this run's retained recording in Replay."};
    // Refresh only on a new result, or while waiting for a retained recording.
    // Directory IO remains on the existing archive worker.
    static int64_t checked = 0;
    static double next = 0;
    double now = suite_ui::LifecycleNow();
    if (checked != d.reviewRunId || (d.reviewRetained && (now >= next || now < next - 5)))
    {
        checked = d.reviewRunId;
        next = now + 5;
        practice::g_replayArchive.Refresh();
    }
    return {
        FinishedReplaySource::Missing,
        d.reviewRetained
            ? "The retained recording is not available yet. Open Replays to inspect the archive."
            : "This temporary attempt is no longer available. Adjust the recording rule in Replay for future runs."};
}

inline bool AnalyseFinishedRun()
{
    auto access = FinishedReplayAvailable();
    if (!access)
        return false;
    // Copy before acknowledging the result; never substitute the latest run.
    auto id = suite_run::presentation.completed.reviewRunId;
    auto scene = suite_run::presentation.completed.reviewScene;
    if (access.source == FinishedReplaySource::Helper)
        return RequestRunReview(id, scene);
    practice::ReplayInfo row;
    row.id = id;
    row.scene = scene;
    OpenSavedReplay(row);
    return g_runReviewOpen && g_reviewTarget == id;
}

inline void OpenReplayLibrary()
{
    if (HasPendingRequest() || (g_pShared && RunInProgress(*g_pShared)))
        return;
    StopReplay3D();
    if (g_runReviewOpen && !g_reviewFromArchive)
        g_commands.runReviewRequest = 2;
    g_runReviewOpen = false;
    g_reviewFromArchive = false;
    g_reviewTarget = 0;
    suite_run::presentation.Acknowledge();
    g_overlayVisible = true;
    g_compactMode = false;
    SetOverlayInteractive(true);
    g_seedSearch.Stop();
    g_seedSearch.HidePreview();
    g_selectRunReview = g_reviewWide = true;
    practice::g_replayArchive.Refresh();
}

inline void ReplayRows(const std::shared_ptr<const practice::ReplayArchiveState> &state,
                       const std::vector<size_t> &shown, float height)
{
    using namespace suite_ui;
    const float u = Unit(), rowHeight = 75 * u;
    ImGui::BeginChild("Replay rows", {0, height});
    ImGuiListClipper clipper;
    clipper.Begin(int(shown.size()), rowHeight);
    while (clipper.Step())
        for (int visible = clipper.DisplayStart; visible < clipper.DisplayEnd; visible++)
        {
            size_t index = shown[visible];
            const auto &row = state->runs[index];
            auto &text = g_replayLibraryView.text[index];
            if (text.date.empty()) text.date = practice::ReplayRecordedAt(row.id);
            ImGui::PushID(text.id.c_str());
            auto at = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            const bool narrow = width < 540 * u;
            ImGui::BeginDisabled(HasPendingRequest());
            bool open = ImGui::InvisibleButton("Open replay", {std::max(30.f, width - 38 * u), rowHeight - 7 * u}, ImGuiButtonFlags_EnableNav);
            auto id = ImGui::GetItemID();
            float hover = Animate(id ^ 0x7632u, ImGui::IsItemHovered() || ImGui::IsItemFocused() ? 1.f : 0.f, true, .13f);
            const bool selected = g_runReviewOpen && g_reviewTarget == row.id;
            auto *draw = ImGui::GetWindowDrawList();
            draw->AddRectFilled(at, {at.x + width, at.y + rowHeight - 7 * u},
                ImGui::GetColorU32(Blend(background, heading, selected ? .15f : .025f + hover * .075f)), 4 * u);
            if (hover > .01f || selected)
                draw->AddRectFilled(at, {at.x + 2 * u, at.y + rowHeight - 7 * u}, ImGui::GetColorU32(Alpha(heading, selected ? 1 : hover)), u);
            float timeWidth = narrow ? 106 * u : 145 * u, right = at.x + width - 50 * u;
            DrawIcon(draw, row.protectedRecording ? Icon::Trophy : Icon::Replay,
                     {at.x + 10 * u, at.y + 18 * u}, 24 * u,
                     ImGui::GetColorU32(row.protectedRecording ? ImVec4(1, .79f, .41f, 1) : Alpha(heading, .6f + hover * .4f)));
            ImVec4 titleClip(at.x + 44 * u, at.y, right - timeWidth - 12 * u, at.y + rowHeight - 7 * u);
            TextAt(draw, {at.x + 44 * u, at.y + 9 * u}, row.name.c_str(), ink, 16 * u, titleClip);
            int mode = MapLevelIndex(row.scene);
            std::string detail = std::string(mode < 0 ? "Unknown" : kMapLevelNames[mode]) + " / " + practice::RecordedCategoryName(row.runCategory);
            TextAt(draw, {at.x + 44 * u, at.y + 31 * u}, detail.c_str(), Muted(), 13 * u, titleClip);
            std::string seed = "Seed " + std::to_string(row.runCategory == 1 ? row.selectedSeed : row.seed);
            if (!narrow && !text.date.empty()) seed += "  /  " + text.date;
            TextAt(draw, {at.x + 44 * u, at.y + 49 * u}, seed.c_str(), Muted(), 12 * u, titleClip);
            auto time = FormatRunTime(row.seconds);
            auto *face = g_timerFont ? g_timerFont : ImGui::GetFont();
            float fontSize = narrow ? 18 * u : 23 * u;
            float extent = face->CalcTextSizeA(fontSize, FLT_MAX, 0, time.c_str()).x;
            fontSize *= std::min(1.f, timeWidth / std::max(1.f, extent));
            extent = face->CalcTextSizeA(fontSize, FLT_MAX, 0, time.c_str()).x;
            draw->AddText(face, fontSize, {right - extent, at.y + 11 * u}, ImGui::GetColorU32(ink), time.c_str());
            auto status = practice::ReplayStatusName(row.outcome, row.valid);
            TextAt(draw, {right - ImGui::CalcTextSize(status).x, at.y + 40 * u}, status,
                   ReplayStatusColor(row.outcome, row.valid), ImGui::GetFontSize(), {right - timeWidth, at.y, right, at.y + rowHeight});
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s%s%s", row.reason.c_str(), row.notes.empty() ? "" : "\n", row.notes.c_str());
            ImGui::EndDisabled();
            if (open) { OpenSavedReplay(row); Request(Tone::Click); ImGui::CloseCurrentPopup(); }
            ImGui::SetCursorScreenPos({at.x + width - 32 * u, at.y + 20 * u});
            if (SmallButton(row.favorite ? "*" : "+")) practice::g_replayArchive.Favorite(row.id, !row.favorite);
            Help(row.favorite ? "Remove from favorites" : "Add to favorites");
            ImGui::SetCursorScreenPos({at.x, at.y + rowHeight});
            ImGui::PopID();
        }
    ImGui::EndChild();
}

#include "FullGameReplayUI.h"

inline void ReplayList()
{
    struct Remember { ~Remember() { RememberReplayFilters(); } } remember;
    static char search[160]{};
    auto &filter = g_replayLibraryFilter;
    float width = ImGui::GetContentRegionAvail().x;
    bool compact = width < 650 * g_dpiScale;
    float buttons =
        ImGui::CalcTextSize(compact ? "RefreshFilters *" : "Refresh").x +
        (ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x) * (compact ? 2 : 1);
    ImGui::SetNextItemWidth(std::min(340.f * g_dpiScale, std::max(80.f * g_dpiScale, width - buttons)));
    if (ImGui::InputTextWithHint("##Replay filter", "Name, category, notes or seed", search, sizeof(search)))
        filter.query = search;
    SameLineIfFits("Refresh");
    if (suite_ui::Button("Refresh"))
        practice::g_replayArchive.Refresh();
    auto drawFilters = [&] {
        float filterWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SetNextItemWidth(
            std::min(210.f * g_dpiScale, (filterWidth - ImGui::GetStyle().ItemSpacing.x) * .53f));
        if (ImGui::BeginCombo("##Replay mode", filter.level < 0 ? "All modes" : kMapLevelNames[filter.level]))
        {
            for (int level = -1; level < kMapLevelCount; level++)
                if (ImGui::Selectable(level < 0 ? "All modes" : kMapLevelNames[level], filter.level == level))
                    filter.level = level;
            ImGui::EndCombo();
        }
        Help("Filter recordings by game mode.");
        ImGui::SameLine();
        const char *statuses[] = {"All statuses", "Finished", "Finished / valid", "Finished / unvalid",
                                  "Aborted"};
        ImGui::SetNextItemWidth(std::min(180.f * g_dpiScale, ImGui::GetContentRegionAvail().x));
        ImGui::Combo("##Replay status", &filter.status, statuses, 5);
        Help("Filter by run outcome and observed local validity.");
        SameLineIfFits("Run category");
        const char *categories[] = {"All run categories", "Random Seed", "Set-Seed", "Practice"};
        int category = filter.category + 1;
        ImGui::SetNextItemWidth(std::min(200.f * g_dpiScale, ImGui::GetContentRegionAvail().x));
        if (ImGui::Combo("##Replay run category", &category, categories, 4)) filter.category = category - 1;
        Help("The category recorded when the run started. Editing a replay's label does not change its speedrun category.");
        SameLineIfFits("Favorites");
        suite_ui::Checkbox("Favorites", &filter.favorites);
        Help("Show only recordings marked as favorites.");
        SameLineIfFits("Clear filters");
        if (suite_ui::Button("Clear filters"))
        {
            search[0] = 0;
            filter.query.clear();
            filter.level = -1;
            filter.status = 0;
            filter.category = -1;
            filter.favorites = false;
        }
    };
    if (compact)
    {
        SameLineIfFits("Filters *");
        bool active = filter.level >= 0 || filter.category >= 0 || filter.status || filter.favorites;
        if (suite_ui::Button(active ? "Filters *###ReplayFilters" : "Filters###ReplayFilters"))
            ImGui::OpenPopup("Replay library filters");
        Help("Mode, status and favorites filters. The asterisk indicates active filters.");
        ImGui::SetNextWindowSize({std::min(400.f * g_dpiScale, ImGui::GetIO().DisplaySize.x - 32), 0},
                                 ImGuiCond_Appearing);
        if (ImGui::BeginPopup("Replay library filters"))
        {
            drawFilters();
            ImGui::EndPopup();
        }
    }
    else
        drawFilters();
    int &style = suite_ui::replayPreferences.libraryView;
    if (suite_ui::Button(style == 0 ? "List *###ReplayListView" : "List###ReplayListView")) style = 0;
    ImGui::SameLine();
    if (suite_ui::Button(style == 1 ? "Table *###ReplayTableView" : "Table###ReplayTableView")) style = 1;
    SameLineIfFits("Full-Game");
    if(suite_ui::Button(style==2?"Full-Game *###FullGameReplayView":"Full-Game###FullGameReplayView"))style=2;
    SameLineIfFits("Sort");
    const char *sorts[] = {"Recorded", "Name", "Game mode", "Run time", "Seed", "Status", "Run category"};
    int sort = int(filter.sort);
    ImGui::SetNextItemWidth(std::min(170.f * g_dpiScale, ImGui::GetContentRegionAvail().x));
    if (ImGui::Combo("##ReplaySort", &sort, sorts, 7)) filter.sort = practice::ReplaySort(sort);
    SameLineIfFits("Ascending");
    suite_ui::Checkbox("Ascending", &filter.ascending);
    auto state = practice::g_replayArchive.Get();
    if(style==2){suite_full::ReplayGroups(state,filter);return;}
    if (!state->error.empty())
    {
        ImGui::TextColored(g_accentCol, "Replay archive warning");
        Help(state->error.c_str());
    }
    if (state->runs.empty())
    {
        if (state->busy)
        {
            suite_ui::LoadingState("Archive loading", "Reading replay library");
            return;
        }
        suite_ui::EmptyState(
            "Replay empty", suite_ui::Icon::Replay, "Your next run starts here",
            "Record a Speedrun or Practice attempt. Runs meeting the recording rule are kept automatically; open one to add a name, category and notes.",
            204);
        return;
    }
    auto &view = g_replayLibraryView;
    view.Update(state, filter);
    const auto &shown = view.shown;
    ImGui::TextDisabled("%zu / %zu recordings", shown.size(), state->runs.size());
    Help("Sort by date, time, mode or category. Switch to Table for detailed columns.");
    if (shown.empty())
    {
        suite_ui::EmptyState("Replay filtered", suite_ui::Icon::Replay, "No matching recordings",
                             "Change the search, mode or status, or clear the filters.");
        return;
    }
    float height = std::max(1.f, ImGui::GetContentRegionAvail().y - 4 * g_dpiScale);
    if (style == 0)
    {
        ReplayRows(state, shown, height);
        return;
    }
    if (ImGui::BeginTable("Replay archive", 8,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX |
                              ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Sortable | ImGuiTableFlags_NoSavedSettings,
                          ImVec2(0, height), std::max(1150.f * g_dpiScale, ImGui::GetContentRegionAvail().x)))
    {
        const auto preferred = [&](practice::ReplaySort value) -> ImGuiTableColumnFlags {
            return filter.sort == value ? ImGuiTableColumnFlags_DefaultSort |
                (filter.ascending ? ImGuiTableColumnFlags_PreferSortAscending : ImGuiTableColumnFlags_PreferSortDescending) : 0;
        };
        ImGui::TableSetupColumn("Fav", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoSort,
                                36 * g_dpiScale);
        ImGui::TableSetupColumn("Name / label", ImGuiTableColumnFlags_WidthStretch | preferred(practice::ReplaySort::Name), 0, 2);
        ImGui::TableSetupColumn("Mode", ImGuiTableColumnFlags_WidthFixed | preferred(practice::ReplaySort::Mode), 155 * g_dpiScale, 3);
        ImGui::TableSetupColumn("Run category", ImGuiTableColumnFlags_WidthFixed | preferred(practice::ReplaySort::Category), 125 * g_dpiScale, 7);
        ImGui::TableSetupColumn("Time",
                                ImGuiTableColumnFlags_WidthFixed | preferred(practice::ReplaySort::Time),
                                95 * g_dpiScale, 4);
        ImGui::TableSetupColumn("Recorded",
                                ImGuiTableColumnFlags_WidthFixed | preferred(practice::ReplaySort::Date),
                                155 * g_dpiScale, 1);
        ImGui::TableSetupColumn("Seed", ImGuiTableColumnFlags_WidthFixed | preferred(practice::ReplaySort::Seed), 100 * g_dpiScale, 5);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed | preferred(practice::ReplaySort::Status), 85 * g_dpiScale, 6);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        if (auto sort = ImGui::TableGetSortSpecs(); sort && sort->SpecsDirty && sort->SpecsCount)
        {
            filter.sort = practice::ReplaySort(sort->Specs[0].ColumnUserID - 1);
            filter.ascending = sort->Specs[0].SortDirection == ImGuiSortDirection_Ascending;
            view.Update(state, filter);
            sort->SpecsDirty = false;
        }
        ImGuiListClipper clipper;
        clipper.Begin(int(shown.size()));
        while (clipper.Step())
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
            {
                size_t index = shown[i];
                const auto &row = state->runs[index];
                auto &text = view.text[index];
                if (text.date.empty())
                    text.date = practice::ReplayRecordedAt(row.id);
                ImGui::PushID(text.id.c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                bool favoriteClicked = suite_ui::SmallButton(row.favorite ? "*" : "+");
                if (favoriteClicked)
                    practice::g_replayArchive.Favorite(row.id, !row.favorite);
                Help(row.favorite ? "Remove from favorites" : "Add to favorites");
                ImGui::TableNextColumn();
                ImGui::BeginDisabled(HasPendingRequest());
                if (ImGui::Selectable(text.label.c_str(), g_runReviewOpen && g_reviewTarget == row.id,
                                      ImGuiSelectableFlags_SpanAllColumns) &&
                    !favoriteClicked)
                {
                    OpenSavedReplay(row);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                if (ImGui::BeginPopupContextItem("Replay actions"))
                {
                    ImGui::BeginDisabled(!row.hasSplits);
                    if (ImGui::MenuItem("Use as split reference")) practice::g_replayArchive.SetReference(row.id);
                    ImGui::EndDisabled();
                    ImGui::EndPopup();
                }
                Help((std::string(OutcomeLabel(byte(row.outcome))) +
                      (row.saved ? " | saved replay" : " | recent recording") +
                      (row.protectedRecording ? " | protected PB / Top 5 recording" : "") +
                      (row.notes.empty() ? "" : "\n" + row.notes))
                         .c_str());
                ImGui::TableNextColumn();
                int level = MapLevelIndex(row.scene);
                ImGui::TextUnformatted(level >= 0 ? kMapLevelNames[level] : "Unknown");
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(practice::RecordedCategoryName(row.runCategory));
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(FormatRunTime(row.seconds).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(text.date.empty() ? "Unknown" : text.date.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%d", row.runCategory == 1 ? row.selectedSeed : row.seed);
                ImGui::TableNextColumn();
                ImGui::TextColored(ReplayStatusColor(row.outcome, row.valid), "%s",
                                   practice::ReplayStatusName(row.outcome, row.valid));
                ReplayStatusHelp(row.outcome, row.valid, row.reason);
                ImGui::PopID();
            }
        ImGui::EndTable();
    }
}

inline void ReplayEditor(std::shared_ptr<const practice::MapData> data)
{
    static int64_t bound = 0;
    static char name[161]{}, category[129]{}, notes[8193]{};
    static bool pending = false;
    if (!data || !data->review)
        return;
    int64_t id = data->review->id;
    auto state = practice::g_replayArchive.Get();
    auto row = std::lower_bound(state->runs.begin(), state->runs.end(), id,
                                [](const auto &item, int64_t target) { return item.id > target; });
    bool retained = row != state->runs.end() && row->id == id;
    if (bound != id)
    {
        bound = id;
        pending = false;
        std::string title = retained ? row->name : practice::ReplayName(*data);
        std::string cat = retained ? row->category : "", detail = retained ? row->notes : "";
        snprintf(name, sizeof(name), "%s", title.c_str());
        snprintf(category, sizeof(category), "%s", cat.c_str());
        snprintf(notes, sizeof(notes), "%s", detail.c_str());
    }
    bool mayEdit = retained || g_reviewFromArchive ||
                   (g_pShared && g_pShared->reviewRunId == id && g_pShared->reviewRetained);
    if (!mayEdit)
    {
        ImGui::TextDisabled("Temporary attempt / discarded on restart");
        return;
    }
    if (suite_ui::Button("Rename / notes"))
        ImGui::OpenPopup("Replay details");
    Help(
        "Rename, categorize and annotate this retained recording. Recorded data cannot be deleted or replaced in this interface.");
    ImGui::SetNextWindowSize(ImVec2(std::min(480.f * g_dpiScale, ImGui::GetIO().DisplaySize.x - 40), 0),
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopup("Replay details"))
    {
        ImGui::SetNextItemWidth(-80 * g_dpiScale);
        ImGui::InputText("Name", name, sizeof(name));
        ImGui::SetNextItemWidth(-80 * g_dpiScale);
        ImGui::InputText("Label", category, sizeof(category));
        Help("An optional library label. The recorded Random Seed / Set-Seed / Practice category cannot be edited.");
        ImGui::InputTextMultiline("Notes", notes, sizeof(notes),
                                  ImVec2(std::max(100.f, ImGui::GetContentRegionAvail().x - 80 * g_dpiScale),
                                         std::min(140.f * g_dpiScale, ImGui::GetIO().DisplaySize.y * .25f)));
        ImGui::BeginDisabled(name[0] == 0);
        if (suite_ui::Button("Save replay"))
        {
            practice::g_replayArchive.Save(data, name, category, notes);
            pending = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    if (pending)
    {
        bool found =
            retained && row->saved && row->name == name && row->category == category && row->notes == notes;
        if (found)
        {
            ImGui::TextDisabled("Replay saved");
            pending = false;
        }
        else if (!state->error.empty())
        {
            ImGui::TextColored(g_accentCol, "Save failed");
            Help(state->error.c_str());
            pending = false;
        }
        else
            ImGui::TextDisabled("Saving replay...");
    }
}

inline void ReplaySplits(std::shared_ptr<const practice::MapData> source, bool)
{
    const auto &data=*source;
    suite_splits::ReferencePicker(source,"Replay reference");
    if (!suite_ui::CollapsingHeader("Crystal split details")) return;
    auto reference=practice::g_replayArchive.ResolvedReference(data);
    if(reference && data.review->outcome==1 && reference->review->outcome==1)
        ImGui::Text("Finish delta: %+.3f s",data.review->seconds-reference->review->seconds);
    static std::shared_ptr<const practice::MapData> cachedSource,cachedReference;
    static std::vector<practice::PickupSplit> splits;
    if(cachedSource!=source || cachedReference!=reference)
    {cachedSource=source;cachedReference=reference;splits=practice::PickupSplits(data,reference.get());}
    if (splits.empty()) { ImGui::TextDisabled("No observed crystal pickups in this recording."); return; }
    if (ImGui::BeginTable("Pickup splits", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
    {
        for (auto c : {"Crystal", "Elapsed", "Segment", "Delta"}) ImGui::TableSetupColumn(c);
        ImGui::TableHeadersRow();
        for (const auto &s : splits)
        {
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("#%d", s.crystal + 1);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(FormatRunTime(s.at).c_str());
            ImGui::TableNextColumn(); ImGui::TextUnformatted(FormatRunTime(s.segment).c_str());
            ImGui::TableNextColumn();
            if (s.comparable) ImGui::TextColored(s.delta <= 0 ? ImVec4(.4f, .9f, .65f, 1) : g_accentCol, "%+.3f", s.delta);
            else ImGui::TextDisabled("--");
        }
        ImGui::EndTable();
    }
}

inline void RecordingCpu(int64_t run)
{
    auto archive = practice::g_replayArchive.Get();
    auto found = archive->byId.find(run);
    if (found == archive->byId.end() || !archive->runs[found->second].hasCaptureCpu ||
        !suite_ui::CollapsingHeader("Recording CPU profile")) return;
    const auto &row = archive->runs[found->second];
    if (ImGui::BeginTable("Capture stages", 4, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_RowBg))
    {
        for (auto label : {"Stage", "Mean ms", "p95 ms", "Max ms"}) ImGui::TableSetupColumn(label);
        ImGui::TableHeadersRow();
        const char *stages[] = {"Colliders", "Audio sync", "Cameras / UI", "Effects", "Rig poses", "View frame", "Total"};
        for (int i = 0; i < 7; i++)
        {
            const auto &c = row.captureCpu[i];
            ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(stages[i]);
            for (auto value : {c.mean, c.p95, c.maximum}) { ImGui::TableNextColumn(); ImGui::Text("%.3f", value); }
        }
        ImGui::EndTable();
    }
    Help("CPU observation time per capture call. Mean and maximum cover this run; p95 uses the last 128 calls per stage. GPU work and game rendering are not included. Every PC uses the same recording policy.");
}

inline void RenderReplayPolicy()
{
    auto &rule = suite_ui::replayPreferences;
    if (suite_ui::CollapsingHeader("Recording rule"))
    {
        const char *rules[] = {"Longer than a time threshold", "At least a number of crystals"};
        ImGui::Combo("Keep replay when", &rule.mode, rules, 2);
        if (rule.mode == 0)
            ImGui::SliderFloat("Minimum duration", &rule.seconds, 0, 600, "%.0f s");
        else
            ImGui::SliderInt("Crystals collected", &rule.crystals, 1, 64);
        rule.Validate();
        ImGui::TextWrapped(
            "The rule is fixed at run start. Valid PB / Top 5 finishes bypass it and receive save priority. Other short attempts stay temporary until restart. Storage warnings are shown explicitly; recordings have no delete action.");
    }
}

inline void RenderRunReview()
{
    g_reviewTabActive = true;
    if (g_pShared && RunInProgress(*g_pShared))
    {
        ImGui::TextDisabled("Replay is unavailable during an active run.");
        return;
    }
    static double nextRefresh = 0;
    if (ImGui::GetTime() >= nextRefresh)
    {
        nextRefresh = ImGui::GetTime() + 5;
        practice::g_replayArchive.Refresh();
    }
    ImGui::BeginDisabled(!g_pShared || !g_pShared->reviewReady || HasPendingRequest());
    if (suite_ui::Button("Previous attempt"))
        RequestRunReview();
    Help("Open the last attempt. Speedrun mode stays enabled; replay is blocked while a run is active.");
    ImGui::EndDisabled();
    SameLineIfFits("Replay list");
    if (suite_ui::Button("Replay list"))
    {
        practice::g_replayArchive.Refresh();
        ImGui::OpenPopup("Choose replay");
    }
    SameLineIfFits("Import package");
    RenderReplayImport();
    ImGui::SetNextWindowSize(ImVec2(std::min(980.f * g_dpiScale, ImGui::GetIO().DisplaySize.x - 40),
                                    std::min(540.f * g_dpiScale, ImGui::GetIO().DisplaySize.y - 50)),
                             ImGuiCond_Appearing);
    if (ImGui::BeginPopup("Choose replay"))
    {
        ReplayList();
        ImGui::EndPopup();
    }
    SameLineIfFits(g_reviewWide ? "Restore window" : "Expand map");
    if (suite_ui::Button(g_reviewWide ? "Restore window" : "Expand map"))
        g_reviewWide = !g_reviewWide;
    if (g_runReviewOpen)
    {
        SameLineIfFits("Close");
        ImGui::BeginDisabled(HasPendingRequest());
        if (suite_ui::Button("Close"))
        {
            StopReplay3D();
            if (!g_reviewFromArchive)
                g_commands.runReviewRequest = 2;
            g_runReviewOpen = false;
        }
        ImGui::EndDisabled();
    }
    if (g_reviewFromArchive && g_runReviewOpen)
    {
        auto data = practice::g_replayArchive.Loaded();
        if (data && data->review && data->review->id == g_reviewTarget && g_reviewLoadedId != g_reviewTarget)
        {
            g_reviewLoadedId = g_reviewTarget;
            g_replayMapWorker.Submit(data->bytes);
        }
    }
    if (g_pShared && RunInProgress(*g_pShared))
    {
        ImGui::TextDisabled("Replay is unavailable during an active run.");
        return;
    }
    auto result = g_reviewFromArchive ? g_replayMapWorker.Get(g_reviewLevel) : g_mapWorker.Get(g_reviewLevel);
    if (!g_runReviewOpen)
    {
        RenderReplayPolicy();
        ImGui::Separator();
        ReplayList();
        return;
    }
    if (!result || !result->data || !result->data->review || result->data->review->id != g_reviewTarget)
    {
        suite_ui::LoadingState("Replay loading", "Loading recorded run");
        auto state = practice::g_replayArchive.Get();
        if (!state->error.empty())
        {
            ImGui::TextColored(g_accentCol, "Replay could not be opened");
            Help(state->error.c_str());
        }
        return;
    }
    const auto &d = *result->data;
    const auto &review = *d.review;
    suite_replay::Bind(result->data);
    auto &replay = g_reviewPlayback;
    if (replay.run != review.id)
        replay.collectedOnly = !review.collected.empty() && review.collected.size() < d.selected.size();
    replay.Bind(review.id);
    const auto &route = replay.collectedOnly ? result->collectedRoute : result->route;
    const auto &learned = PreviewMovementTiming(result, replay.collectedOnly);
    auto gameTravel = practice::EstimateGameTravel(route.length, d.movementTick, d.movementSprintStep,
                                                   route.complete && !route.estimated);
    auto travel = gameTravel.valid
                      ? gameTravel
                      : practice::EstimateTravel(route.length, route.complete && !route.estimated);
    double suggestedDuration = learned.valid      ? learned.seconds
                               : gameTravel.valid ? gameTravel.fast
                               : travel.valid     ? practice::TravelDuration(route.length, replay.travelSpeed)
                                                  : 0;
    double duration = replay.Duration(review.seconds, suggestedDuration, travel.valid);
    bool in3D = Replay3DActive(review.id);
    if (in3D)
    {
        duration = review.seconds;
        replay.seconds = std::clamp(double(g_pShared->replay3dSeconds), 0., review.seconds);
        replay.playing = g_pShared->replay3dState == 2;
    }
    else
    {
        bool wasPlaying = replay.playing;
        replay.Advance(ImGui::GetIO().DeltaTime, duration);
        double looped = suite_replay::LoopTime(replay.seconds, wasPlaying);
        if (looped != replay.seconds)
        {
            replay.seconds = looped;
            replay.playing = true;
        }
    }
    double beforeTime = replay.seconds;
    bool beforePlaying = replay.playing;
    float beforeSpeed = replay.speed;
    bool retained = g_reviewFromArchive ||
                    (g_pShared && g_pShared->reviewRunId == review.id && g_pShared->reviewRetained);
    for (const auto &row : practice::g_replayArchive.Get()->runs)
        retained |= row.id == review.id;
    auto motion = ReplayMotionFor(result->data, retained);
    bool loopChanged = false;
    ImGui::Separator();
    bool columns = ImGui::GetContentRegionAvail().x >= 740 * g_dpiScale;
    bool table = columns && ImGui::BeginTable("Review layout", 2, ImGuiTableFlags_SizingFixedFit);
    if (table)
    {
        ImGui::TableSetupColumn("Playback", ImGuiTableColumnFlags_WidthFixed, 290 * g_dpiScale);
        ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
    }
    if (table)
    {
        float sidebar = std::max(160.f * g_dpiScale, ImGui::GetCurrentWindow()->InnerClipRect.Max.y -
                                                         ImGui::GetCursorScreenPos().y - 16 * g_dpiScale);
        ImGui::BeginChild("Replay controls", {0, sidebar});
    }
    ImGui::TextColored(g_headerCol, "%s", kMapLevelNames[MapLevelIndex(d.scene)]);
    SameLineIfFits(practice::ReplayStatusName(review.outcome, review.valid));
    ImGui::TextColored(ReplayStatusColor(review.outcome, review.valid), "%s",
                       practice::ReplayStatusName(review.outcome, review.valid));
    ReplayStatusHelp(review.outcome, review.valid, review.validityReason);
    if (suite_ui::introTitleFont)
        ImGui::PushFont(suite_ui::introTitleFont);
    ImGui::TextUnformatted(FormatRunTime(review.seconds).c_str());
    if (suite_ui::introTitleFont)
        ImGui::PopFont();
    ImGui::Text("Seed  %d", d.seed);
    Help("Captured layout seed.");
    ReplaySplits(result->data, retained);
    RecordingCpu(review.id);
    ImGui::TextDisabled("%s / %zu of %zu crystals", OutcomeLabel(byte(review.outcome)),
                        review.collected.size(), d.selected.size());
    ReplayEditor(result->data);
    RenderReplayTransfer(result->data, retained);
    suite_clip::Button(result, motion);
    RenderReplay3D(result->data, replay, retained && motion && !motion->frames.empty());
    if (suite_ui::CollapsingHeader("Markers / loop"))
    {
        RenderReplayMarkers(replay);
        loopChanged = RenderReplayLoop(replay);
    }
    if (motion)
    {
        if (RenderRecordingCheck(*motion, replay))
            Replay3DControl(review, replay, suite_replay::camera == 2, true);
        RenderReplayInputs(*motion, replay);
        if (RenderReplayExperience(*motion, replay))
            Replay3DControl(review, replay, suite_replay::camera == 2, true);
    }
    else if (review.motion)
    {
        auto state = practice::g_replayArchive.Get();
        ImGui::TextDisabled("%s", !retained ? "Inputs were not retained for this short attempt"
                                  : state->motionError.empty() ? "Input recording loading"
                                                               : "Input recording unavailable");
        if (retained && !state->motionError.empty())
        {
            Help(state->motionError.c_str());
            if (suite_ui::Button("Retry input load"))
                practice::g_replayArchive.LoadMotion(result->data);
        }
    }
    else
    {
        ImGui::TextDisabled("Legacy replay / no recorded inputs");
        Help(
            "Positions and existing events remain available. Camera and keyboard history cannot be reconstructed for older recordings.");
    }
    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.91f, .47f, .86f, 1));
    suite_ui::Checkbox("Player", &replay.played);
    ImGui::PopStyleColor();
    Help("Pink follows the exact recorded timestamps. Sampling gaps are not connected.");
    SameLineIfFits("Suggested");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(.42f, .84f, .77f, 1));
    suite_ui::Checkbox("Suggested", &replay.suggested);
    ImGui::PopStyleColor();
    Help(
        "Teal follows the shortest route found by the navigation model, using continuously learned physics movement when available.");
    suite_ui::Checkbox("Monster", &replay.monsters);
    Help(
        "Show recorded enemy positions on the same timeline. Old replays may contain no enemy observations.");
    SameLineIfFits("Options");
    if (suite_ui::Button("Options"))
        ImGui::OpenPopup("Review options");
    if (ImGui::BeginPopup("Review options"))
    {
        suite_ui::Checkbox("Collected crystals only", &replay.collectedOnly);
        Help("Compare the same crystals collected before death/reset, instead of the entire level.");
        suite_ui::Checkbox("Monster navigation", &replay.monsterPaths);
        Help(
            "Thin animated dashes show the monster's planned path at that moment. A planned path can change before it is walked.");
        suite_ui::Checkbox("Light flashes", &replay.flashes);
        Help(
            "White pulses show flashlight activation, camera flashes and confirmed light contacts at their recorded time. Flashes are also recorded when they miss. The visual radius is symbolic; older recordings cannot recover missing events.");
        ImGui::SliderFloat("Playback speed", &replay.speed, .1f, 8.f, "%.2fx",
                           ImGuiSliderFlags_Logarithmic | ImGuiSliderFlags_AlwaysClamp);
        if (!learned.valid && !gameTravel.valid)
        {
            if (ImGui::SliderFloat("Fallback speed (u/s)", &replay.travelSpeed, 8.1f, 10.f, "%.1f",
                                   ImGuiSliderFlags_AlwaysClamp))
                replay.travelSpeed = practice::RouteSpeed(replay.travelSpeed);
            Help("Used only when no measured profile or captured game movement model is available.");
        }
        ImGui::EndPopup();
    }
    if (result->working)
        suite_ui::Busy("Calculating route...");
    else if (!route.complete || route.estimated)
    {
        ImGui::TextColored(g_accentCol, route.complete ? "Suggested route has missing connections"
                                                       : "Suggested route unavailable");
        Help(route.message.c_str());
    }
    ImGui::Separator();
    double distance = replay.collectedOnly ? result->playedToLastPickup : result->playedDistance;
    if (route.complete && !route.estimated)
    {
        ImGui::Text("Distance: %.1f / %.1f u", distance, route.length);
        Help("Recorded player distance / suggested route distance, in world units.");
    }
    else
        ImGui::Text("Player: %.1f units", distance);
    if (learned.valid)
    {
        ImGui::Text("Estimated: %.2f s", learned.seconds);
        ImGui::TextDisabled("Learned movement: %.1f - %.1f s", learned.fast, learned.slow);
        Help(
            "Uses physics movement learned continuously during speedruns in this mode and build. Straight movement, bends, sharp turns, slopes and starts are measured separately. This is a movement estimate: enemy encounters and unobserved situations are not predicted.");
    }
    else if (travel.valid)
    {
        ImGui::TextDisabled("%s: %.1f - %.1f s", gameTravel.valid ? "Game movement" : "Fallback", travel.fast,
                            travel.slow);
        Help(
            gameTravel.valid
                ? "Verified sprint displacement per captured physics tick; diagonal inputs remain unnormalized. This movement range excludes fatigue, aiming and enemy delays."
                : "No measured profile or game model. Travel distance / the observed 8.1-10 u/s range, excluding delays.");
    }
    if (suite_ui::CollapsingHeader("Details"))
    {
        const auto &profile = result->timingProfile;
        ImGui::Text("Recording: %.2f s", review.seconds);
        ImGui::Text("Reconstructed: %.2f s", profile.reconstructed);
        ImGui::Text("Observed: %.0f%%", profile.coverage * 100);
        ImGui::Text("Waiting: %.2f s", profile.stopped);
        if (learned.valid)
            ImGui::Text("Measured movement coverage: %.0f%%", learned.coverage * 100);
        ImGui::Text("Monsters: %zu | light events: %zu", review.monsters.size(), review.flashes.size());
        ImGui::TextDisabled("Player signals: %s", review.lightSources & 37 ? "observed" : "not recorded");
        ImGui::TextDisabled("Monster signals: %s", review.lightSources & 218 ? "observed" : "not recorded");
        if (review.lightRejected)
            ImGui::Text("Rejected light events: %d", review.lightRejected);
        Help(
            "Shows whether actual player/monster observation callbacks reached this recording. Older replays contain no observer diagnostics. Missing events cannot be recreated from positions alone.");
        if (review.reduced || review.monstersReduced)
        {
            ImGui::TextColored(g_accentCol, "Recording contains sample gaps.");
            Help(
                "Reduced or missing samples limit movement, timing or monster detail. The run's final clock is preserved.");
        }
        ImGui::TextDisabled("Navigation information");
        Help(route.message.c_str());
    }
    if (table)
    {
        ImGui::EndChild();
        ImGui::TableNextColumn();
    }
    auto played = replay.played ? practice::PlayedSlice(review, replay.seconds) : practice::PlaybackSlice{};
    double suggestedFraction = learned.valid           ? learned.Fraction(replay.seconds)
                               : suggestedDuration > 0 ? replay.seconds / suggestedDuration
                                                       : 0;
    auto suggested =
        replay.suggested ? practice::SuggestedSlice(route, suggestedFraction) : practice::PlaybackSlice{};
    auto monsters = replay.monsters ? practice::MonstersAt(review, replay.seconds, replay.monsterPaths)
                                    : std::vector<practice::MonsterPlayback>{};
    SeedCanvasPlayback canvas{
        &played,        &suggested, replay.suggested && route.complete && !route.estimated,
        &monsters,      &review,    replay.seconds,
        replay.flashes, &route};
    int level = g_plannerOptions.level;
    g_plannerOptions.level = MapLevelIndex(d.scene);
    const auto &style = ImGui::GetStyle();
    float bottom = ImGui::GetCurrentWindow()->InnerClipRect.Max.y - 2 * style.CellPadding.y;
    float height = std::max(100.f * g_dpiScale, bottom - ImGui::GetCursorScreenPos().y -
                                                    ImGui::GetFrameHeightWithSpacing() * 2 -
                                                    ImGui::GetStyle().ItemSpacing.y - 68 * suite_ui::Unit());
    RenderSeedCanvas(*result, height, &canvas);
    g_plannerOptions.level = level;
    RenderReplayTimeline(review, replay, duration, suite_replay::marks.get(), motion.get());
    if (table)
        ImGui::EndTable();
    if (in3D && (loopChanged || beforeTime != replay.seconds || beforePlaying != replay.playing ||
                 beforeSpeed != replay.speed))
        Replay3DControl(review, replay, g_replayObserver,
                        beforeTime != replay.seconds || beforePlaying != replay.playing);
}
