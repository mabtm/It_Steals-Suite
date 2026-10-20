#pragma once
#include "InspectorRuntime.h"
#include "InspectorVisuals.h"
#include <cctype>

namespace inspector_ui
{
inline char objectSearch[128]{}, fieldSearch[128]{};
inline int objectFilter = 0, lastObject = 0, tabRequest = -1;
inline uint32_t lastEpoch = 0, lastSession = 0;
inline int lastPid = 0;
inline bool objectsPopup = false;
inline std::vector<int> filtered;
inline std::shared_ptr<const inspector::Catalog> filteredCatalog;
inline std::string filterKey;
inline std::vector<inspector::Difference> differences;
inline std::shared_ptr<const inspector::Detail> compared, comparedBaseline;

inline bool Allowed()
{
    return g_pShared && SnapshotAge() < 1.2 && !(g_speedrunMode && !g_practiceMode) &&
           !(g_pShared->replay3dState >= 1 && g_pShared->replay3dState <= 3);
}

inline bool Fresh(const inspector::State &state)
{
    return state.detail && state.detail->active && inspector::Age(state) < 1.2 &&
           inspector::SameScene(state.identity, state.detail->header) && state.error.empty();
}

inline std::string Lower(std::string s)
{
    for (char &c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

inline bool expanded = false, browse = false, resetDetails = false, choicesOpen = false, resetScroll = false;
inline std::optional<inspector::Pointer> choicePointer;
inline std::shared_ptr<const inspector::Detail> groupedDetail;
inline std::vector<inspector::ValueGroup> fieldGroups;
inline double selectedAt = -10;
inline double pickerAt = -10, hoverAt = -10, cardAt = -10;
inline int hoverId = 0, cardFrame = -2;
inline uint32_t detailRevealGeneration = 0;
inline bool cardPopupWasOpen = false;
inline std::optional<inspector::Hover> confirmedBounds;

inline bool FullReady(const inspector::State &s)
{
    return Fresh(s) && s.detail && (s.detail->options & 16) && !inspector::pending;
}

inline bool EnsureOpen()
{
    if (!Allowed())
        return false;
    inspector::Bind(
        inspector::Identity(g_pShared->pid, g_session, g_pShared->currentSceneIndex, g_pShared->sceneEpoch));
    inspector::open = true;
    g_seedSearch.Stop();
    return true;
}

inline void ReturnToGame()
{
    Sound(suite_ui::Tone::InspectorClose);
    inspector::dockInteractive = false;
    g_compactMode = inspector::resumeCompact;
    g_overlayVisible = inspector::resumeCompact;
    inspector::dock = inspector::pinned;
    SetOverlayInteractive(false);
}

inline void Open()
{
    if (!EnsureOpen())
        return;
    Sound(suite_ui::Tone::InspectorOpen);
    inspector::picking = false;
    inspector::StopPointer();
    inspector::dockInteractive = false;
    resetScroll = true;
    suite_ui::navigation.Select(suite_ui::Page::Inspector, ImGui::GetTime());
    g_overlayVisible = true;
    g_allOverlayEnabled = true;
    g_compactMode = false;
    SetOverlayInteractive(true);
}

inline void StartPick(int action)
{
    if (inspector::pending || !EnsureOpen())
        return;
    pickerAt = ImGui::GetTime();
    hoverId = 0;
    confirmedBounds.reset();
    Sound(suite_ui::Tone::InspectorOpen);
    inspector::pickResumeCompact = inspector::resumeCompact;
    inspector::pickWasVisible = g_overlayVisible;
    inspector::pickWasCompact = g_compactMode;
    inspector::pickWasAll = g_allOverlayEnabled;
    if (!inspector::dockInteractive)
        inspector::resumeCompact = g_overlayVisible && g_compactMode;
    inspector::pickFromMenu = g_overlayVisible && !g_compactMode;
    inspector::pickFromDock = inspector::dockInteractive;
    inspector::options &= ~16u;
    inspector::pickAction = action;
    inspector::picking = true;
    inspector::dockInteractive = false;
    inspector::pickerArmFrame = ImGui::GetFrameCount() + 1;
    choicesOpen = false;
    choicePointer.reset();
    inspector::StopPointer();
    inspector::transportError.clear();
    g_overlayVisible = false;
    g_allOverlayEnabled = true;
    SetOverlayInteractive(true);
}

inline void EndPick()
{
    Sound(suite_ui::Tone::InspectorClose);
    inspector::picking = false;
    choicesOpen = false;
    choicePointer.reset();
    inspector::StopPointer();
    g_overlayVisible = inspector::pickWasVisible;
    g_compactMode = inspector::pickWasCompact;
    g_allOverlayEnabled = inspector::pickWasAll;
    inspector::resumeCompact = inspector::pickResumeCompact;
    inspector::dockInteractive = inspector::pickFromDock;
    SetOverlayInteractive((g_overlayVisible && !g_compactMode) || inspector::dockInteractive);
}

inline void FullbrightButton()
{
    ImGui::BeginDisabled(!Allowed());
    suite_ui::Checkbox("Fullbright", &g_brightnessOn);
    ImGui::EndDisabled();
    Help("Use the Suite's existing Fullbright lighting. Disabled in clean Speedrun and Replay.");
}

inline void SelectionChanged(const inspector::State &state)
{
    auto d = state.detail;
    if (d)
        inspector_fx::changes.Observe(*d, ImGui::GetTime());
    if (d && (d->selected != lastObject || d->header.epoch != lastEpoch || d->header.pid != lastPid ||
              d->header.session != lastSession))
    {
        lastObject = d->selected;
        lastEpoch = d->header.epoch;
        lastPid = d->header.pid;
        lastSession = d->header.session;
        inspector::baseline.reset();
        compared = nullptr;
        fieldSearch[0] = 0;
        expanded = false;
        resetDetails = true;
        resetScroll = true;
        inspector::options &= ~16u;
        selectedAt = ImGui::GetTime();
    }
}

inline const char *Value(const inspector::Detail &d, const char *source)
{
    for (const auto &v : d.values)
        if (v.source == source)
            return v.value.c_str();
    return "--";
}

inline void Basics(const inspector::State &state)
{
    const auto &d = *state.detail;
    float u = suite_ui::Unit();
    auto a = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x, height = 190 * u;
    ImGui::Dummy({width, height});
    Help(d.path.c_str());
    auto *draw = ImGui::GetWindowDrawList();
    ImVec2 b(a.x + width, a.y + height);
    Panel(draw, a, b, true);
    auto accent = inspector::pinned ? PinnedAccent() : Accent();
    ImVec4 clip(a.x + 14 * u, a.y + 8 * u, b.x - 14 * u, b.y - 8 * u);
    auto muted = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    suite_ui::DrawIcon(draw, suite_ui::Icon::Collider, {a.x + 14 * u, a.y + 15 * u}, 23 * u, Ink(accent));
    Text(draw, {a.x + 46 * u, a.y + 18 * u}, "SELECTED OBJECT", 14 * u, muted, clip);
    bool inactive = std::string_view(Value(d, "GameObject.activeInHierarchy")) == "false";
    const char *status = Fresh(state) ? (inactive ? "INACTIVE" : "LIVE") : "WAITING";
    float statusWidth = Fresh(state) && !inactive ? 65 * u : 103 * u;
    if (width > 280 * u)
        Badge(draw, {b.x - statusWidth - 12 * u, a.y + 12 * u}, status,
              Fresh(state) && !inactive ? accent : g_accentCol, statusWidth);
    Text(draw, {a.x + 14 * u, a.y + 49 * u}, d.name.c_str(), 23 * u, g_textCol, clip);
    char id[64];
    std::snprintf(id, sizeof(id), "#%d", d.selected);
    std::string type = std::string(Value(d, "Inspector.primaryType")) + "  /  " + id;
    Text(draw, {a.x + 14 * u, a.y + 80 * u}, type.c_str(), 16 * u, accent, clip);
    draw->AddLine({a.x + 14 * u, a.y + 108 * u}, {b.x - 14 * u, a.y + 108 * u},
                  Ink(suite_ui::Color(AppearancePreferences::Outline), .45f), u);
    auto field = [&](const char *label, const char *source, float y) {
        Text(draw, {a.x + 14 * u, a.y + y * u}, label, 15 * u, muted, clip);
        float pulse = 0;
        for (const auto &v : d.values)
            if (v.source == source)
            {
                pulse = ValuePulse(v);
                break;
            }
        Text(draw, {a.x + 104 * u, a.y + y * u}, Value(d, source), 17 * u,
             suite_ui::Mix(g_textCol, accent, pulse * .8f), clip);
    };
    field("POSITION", "Transform.position", 121);
    field("DISTANCE", "Inspector.viewDistance", 150);
    double age = ImGui::GetTime() - selectedAt;
    BorderImpulse(draw, {a.x + 1 * u, a.y + 1 * u}, {b.x - 1 * u, b.y - 1 * u}, age, accent);
    if (Motion() && age >= 0 && age < .35)
    {
        float progress = suite_ui::Ease(float(age / .35));
        float x = a.x + 14 * u + (width - 28 * u) * progress;
        draw->AddLine({x, a.y + 113 * u}, {x, b.y - 12 * u}, Ink(accent, (1 - progress) * .16f), u);
    }
}

inline const char *KindName(int kind)
{
    return kind == 1 ? "Collider" : kind == 3 ? "Mesh" : kind == 4 ? "Sprite" : "Bounds estimate";
}

inline void PreferTarget(int id)
{
    inspector::Prefer(id);
    if (choicesOpen)
        choicePointer = inspector::pointer;
}

inline void Picker()
{
    if (!inspector::picking)
        return;
    if (!Allowed())
    {
        inspector::Close();
        SetOverlayInteractive(g_overlayVisible && !g_compactMode);
        return;
    }
    auto &io = ImGui::GetIO();
    auto state = inspector::worker.Get();
    float u = suite_ui::Unit();
    ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
    ImGui::Begin("##Inspector world picker", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
    inspector::Pointer p;
    bool valid = false;
    if (choicesOpen && choicePointer)
    {
        p = *choicePointer;
        valid = true;
    }
    else if (inspector::pointerProvider)
        valid = inspector::pointerProvider(p);
    else if (auto n = inspector::NormalizePointer(io.MousePos.x, io.MousePos.y, 0, 0, int(io.DisplaySize.x),
                                                  int(io.DisplaySize.y)))
    {
        p = *n;
        valid = true;
    }
    p.active = valid;
    p.action = inspector::pickAction;
    if (valid && !choicesOpen && p.action == 3 && p.width == inspector::pointer.width &&
        p.height == inspector::pointer.height && std::abs(p.x - inspector::pointer.x) * p.width <= 4 &&
        std::abs(p.y - inspector::pointer.y) * p.height <= 4)
        p.preferred = inspector::pointer.preferred;
    if (!valid)
    {
        p = inspector::pointer;
        p.active = false;
        p.preferred = 0;
    }
    inspector::Point(p);
    auto *draw = ImGui::GetWindowDrawList();
    auto hover = state->hover;
    bool observation = hover && hover->active && inspector::HoverAge(*state) < .25 &&
                       inspector::SameScene(hover->header, inspector::identity) &&
                       (hover->serial == inspector::pointer.serial ||
                        (std::abs(hover->x - inspector::pointer.x) * io.DisplaySize.x < 3 &&
                         std::abs(hover->y - inspector::pointer.y) * io.DisplaySize.y < 3));
    bool current =
        observation && (!inspector::pointer.preferred || hover->object == inspector::pointer.preferred);
    bool hit = current && hover->object;
    auto color = ImGui::GetColorU32(hit ? g_headerCol : g_textCol);
    auto mouse = io.MousePos;
    if (valid)
    {
        draw->AddCircle(mouse, 8 * u, color, 24, 1.5f * u);
        draw->AddLine({mouse.x - 15 * u, mouse.y}, {mouse.x - 10 * u, mouse.y}, color, u);
        draw->AddLine({mouse.x + 10 * u, mouse.y}, {mouse.x + 15 * u, mouse.y}, color, u);
    }
    if (hit && hover->outline)
    {
        if (hoverId != hover->object)
        {
            hoverId = hover->object;
            hoverAt = ImGui::GetTime();
        }
        auto c = g_headerCol;
        ImVec2 a(hover->left * io.DisplaySize.x, hover->top * io.DisplaySize.y),
            b(hover->right * io.DisplaySize.x, hover->bottom * io.DisplaySize.y);
        float reveal = Motion() ? suite_ui::Ease(float((ImGui::GetTime() - hoverAt) / .16)) : 1;
        Brackets(draw, a, b, c, reveal);
        BorderImpulse(draw, a, b, ImGui::GetTime() - hoverAt, c);
        std::string label = hover->name + "  /  " + KindName(hover->kind);
        float y = std::clamp(a.y - 26 * u, 4.f, io.DisplaySize.y - 25 * u);
        float x = std::clamp(a.x, 4.f,
                             std::max(4.f, io.DisplaySize.x - ImGui::CalcTextSize(label.c_str()).x - 16 * u));
        draw->AddText({x + 1, y + 1}, IM_COL32(0, 0, 0, 235), label.c_str());
        draw->AddText({x, y}, color, label.c_str());
    }
    bool canChoose =
        inspector::pickAction == 3 && observation && !inspector::clickPointer && !inspector::pending;
    if (canChoose && ImGui::IsKeyPressed(ImGuiKey_Tab))
    {
        choicesOpen = !choicesOpen;
        if (choicesOpen)
            choicePointer = inspector::pointer;
        else
            choicePointer.reset();
        inspector::nativeClick.reset();
    }
    if (canChoose && !choicesOpen && io.MouseWheel != 0 && hover->targets.size() > 1)
        PreferTarget(inspector::CycleTarget(*hover, io.MouseWheel > 0 ? -1 : 1));
    float width = std::min(680 * u, io.DisplaySize.x - 24);
    ImVec2 a(std::max(12.f, (io.DisplaySize.x - width) * .5f), 18 * u), b(a.x + width, a.y + 120 * u);
    Panel(draw, a, b, true);
    char title[128];
    if (inspector::pickAction == 3)
        std::snprintf(title, sizeof(title), "PICKER / %zu targets%s", observation ? hover->targets.size() : 0,
                      observation && hover->scanning ? " / preparing scene" : "");
    else
        std::snprintf(title, sizeof(title), "PICKER / route %s",
                      inspector::pickAction == 7 ? "start" : "end");
    ImVec4 clip(a.x + 14 * u, a.y, a.x + width - 12 * u, b.y);
    suite_ui::DrawIcon(draw, suite_ui::Icon::Scan, {a.x + 14 * u, a.y + 10 * u}, 22 * u, color);
    Text(draw, {a.x + 46 * u, a.y + 12 * u}, title, 18 * u, g_headerCol, clip);
    BorderImpulse(draw, a, b, ImGui::GetTime() - pickerAt, g_headerCol);
    const char *status = inspector::clickPointer ? "Confirming clicked target..."
                         : hit                   ? hover->name.c_str()
                         : observation ? (hover->object && !current ? "Switching highlighted target..."
                                                                    : hover->status.c_str())
                                       : "Move over an object";
    suite_ui::TextAt(draw, {a.x + 14 * u, a.y + 39 * u}, status, g_textCol, 17 * u, clip);
    suite_ui::TextAt(draw, {a.x + 14 * u, a.y + 70 * u},
                     inspector::pickAction == 3 ? "Wheel: change target   |   Tab: target list"
                                                : "Click: choose surface",
                     g_textCol, 14 * u, clip);
    suite_ui::TextAt(draw, {a.x + 14 * u, a.y + 94 * u}, "Click / Enter: select   |   Esc / RMB / F7: cancel",
                     g_textCol, 14 * u, clip);
    if (!inspector::transportError.empty())
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), {a.x + 10 * u, b.y + 8 * u},
                      IM_COL32(255, 163, 105, 255), inspector::transportError.c_str(), nullptr,
                      width - 20 * u);
    ImVec2 listA(12 * u, std::min(b.y + 12 * u, io.DisplaySize.y - 90 * u)), listB;
    bool confirm = false;
    if (choicesOpen)
    {
        float listWidth = std::min(390 * u, io.DisplaySize.x - 24),
              listHeight = std::max(60.f, std::min(396 * u, io.DisplaySize.y - listA.y - 12 * u));
        listB = {listA.x + listWidth, listA.y + listHeight};
        ImGui::SetCursorScreenPos(listA);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, g_mainBgCol);
        ImGui::BeginChild("Picker targets", {listWidth, listHeight}, ImGuiChildFlags_Borders);
        ImGui::TextColored(g_headerCol, "TARGET LAYERS");
        ImGui::TextDisabled("Frozen point / choose an object");
        ImGui::BeginDisabled(!canChoose);
        if (suite_ui::Button("Select highlighted"))
            confirm = true;
        SameLineIfFits("Close list");
        if (suite_ui::Button("Close list"))
        {
            choicesOpen = false;
            choicePointer.reset();
            inspector::nativeClick.reset();
        }
        ImGui::EndDisabled();
        if (observation && hover->partial)
            ImGui::TextDisabled("Showing a bounded subset of targets");
        if (observation)
            for (const auto &target : hover->targets)
            {
                ImGui::PushID(target.id);
                std::string label = target.name + "##Target";
                ImGui::BeginDisabled(!canChoose);
                bool chosen = target.id == hover->object;
                if (ImGui::Selectable(label.c_str(), chosen, ImGuiSelectableFlags_AllowDoubleClick))
                {
                    PreferTarget(target.id);
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        confirm = true;
                }
                Help(
                    (target.type + " / #" + std::to_string(target.id) + "\n" + KindName(target.kind) +
                     "\nBounds are approximate. Mesh/sprite geometry does not test transparent texture pixels. Ray layers may include hidden objects.")
                        .c_str());
                ImGui::TextDisabled("%s  /  %s%.2f u", KindName(target.kind), target.kind == 2 ? "~" : "",
                                    target.distance);
                ImGui::EndDisabled();
                ImGui::PopID();
            }
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    if (cancel)
        EndPick();
    else if (ImGui::GetFrameCount() > inspector::pickerArmFrame && !inspector::pending &&
             !inspector::clickPointer)
    {
        if (confirm || (canChoose && hit && ImGui::IsKeyPressed(ImGuiKey_Enter)))
        {
            inspector::QueueClick(inspector::pointer);
            inspector::nativeClick.reset();
        }
        else
        {
            auto inControls = [&](const inspector::Pointer &point) {
                float x = point.x * io.DisplaySize.x, y = (1 - point.y) * io.DisplaySize.y;
                bool banner = x >= a.x && x <= b.x && y >= a.y && y <= b.y;
                bool list = listB.x > listA.x && x >= listA.x && x <= listB.x && y >= listA.y && y <= listB.y;
                return banner || list;
            };
            if (inspector::nativeClick)
            {
                auto clicked = *inspector::nativeClick;
                inspector::nativeClick.reset();
                if (!inControls(clicked) && !choicesOpen)
                    inspector::QueueClick(clicked);
            }
            else if (!inspector::pointerProvider && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                auto at = io.MouseClickedPos[0];
                if (auto n = inspector::NormalizePointer(at.x, at.y, 0, 0, int(io.DisplaySize.x),
                                                         int(io.DisplaySize.y)))
                    if (!inControls(*n) && !choicesOpen)
                        inspector::QueueClick(*n);
            }
        }
    }
    ImGui::End();
}

inline void Objects(const inspector::State &state, bool popup = false)
{
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Object search", "Find an object...", objectSearch, sizeof(objectSearch));
    const char *filters[] = {"All objects", "Colliders", "Renderers", "Inactive at refresh"};
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##Object filter", &objectFilter, filters, 4);
    Help(
        "The catalog is sampled on refresh. The selected object's properties are live. Inactive and collider-free objects can be selected here.");
    auto catalog = state.catalog;
    if (!catalog)
    {
        suite_ui::Busy(state.detail && state.detail->scanning ? "Reading scene objects"
                                                              : "Waiting for the game");
        return;
    }
    std::string key = Lower(objectSearch) + std::to_string(objectFilter);
    if (catalog != filteredCatalog || key != filterKey)
    {
        filteredCatalog = catalog;
        filterKey = key;
        filtered.clear();
        auto text = Lower(objectSearch);
        for (size_t i = 0; i < catalog->objects.size(); i++)
        {
            const auto &row = catalog->objects[i];
            if (objectFilter == 1 && !(row.flags & 4))
                continue;
            if (objectFilter == 2 && !(row.flags & 8))
                continue;
            if (objectFilter == 3 && (row.flags & 2))
                continue;
            if (!text.empty() && Lower(row.name).find(text) == std::string::npos)
                continue;
            filtered.push_back(int(i));
        }
    }
    ImGui::TextDisabled("%zu / %zu objects%s", filtered.size(), catalog->objects.size(),
                        catalog->truncated ? " / partial" : "");
    ImGui::BeginChild("Object rows", {0, 0}, ImGuiChildFlags_None);
    ImGuiListClipper clip;
    clip.Begin(int(filtered.size()));
    while (clip.Step())
        for (int line = clip.DisplayStart; line < clip.DisplayEnd; line++)
        {
            const auto &row = catalog->objects[size_t(filtered[size_t(line)])];
            ImGui::PushID(row.id);
            bool selected = state.detail && state.detail->selected == row.id;
            ImGui::BeginDisabled(inspector::pending.has_value() || (state.detail && state.detail->scanning));
            if (!(row.flags & 2))
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (ImGui::Selectable(row.name.c_str(), selected))
            {
                inspector::Request(2, row.id);
                if (popup)
                    ImGui::CloseCurrentPopup();
            }
            if (!(row.flags & 2))
                ImGui::PopStyleColor();
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                ImGui::Text("ID %d | parent %d | layer %d", row.id, row.parent, row.layer);
                ImGui::Text("%s%s", row.flags & 4 ? "Collider  " : "", row.flags & 8 ? "Renderer" : "");
                ImGui::EndTooltip();
            }
            ImGui::PopID();
        }
    ImGui::EndChild();
}

inline void Related(const inspector::State &state)
{
    const auto &d = *state.detail;
    const inspector::Object *object = nullptr;
    if (state.catalog)
        for (const auto &row : state.catalog->objects)
            if (row.id == d.selected)
            {
                object = &row;
                break;
            }
    ImGui::BeginDisabled(inspector::pending.has_value() || !Fresh(state));
    if (object && object->parent)
    {
        if (suite_ui::Button("Select parent"))
            inspector::Request(2, object->parent);
        SameLineIfFits("Related objects");
    }
    ImGui::SetNextItemWidth(std::max(80.f, ImGui::GetContentRegionAvail().x));
    if (ImGui::BeginCombo("##Related objects", "Children / overlapping hits"))
    {
        if (state.catalog)
            for (const auto &row : state.catalog->objects)
                if (row.parent == d.selected)
                {
                    ImGui::PushID(row.id);
                    if (ImGui::Selectable(row.name.c_str()))
                        inspector::Request(2, row.id);
                    ImGui::PopID();
                }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
}

inline void TargetStack(const inspector::State &state)
{
    if (!state.detail || state.detail->hits.size() < 2)
        return;
    const auto &d = *state.detail;
    auto selected =
        std::find_if(d.hits.begin(), d.hits.end(), [&](const auto &t) { return t.id == d.selected; });
    char caption[96];
    std::snprintf(caption, sizeof(caption), "Target %zu / %zu",
                  selected == d.hits.end() ? 0 : size_t(selected - d.hits.begin()) + 1, d.hits.size());
    ImGui::SetNextItemWidth(std::min(360 * suite_ui::Unit(), ImGui::GetContentRegionAvail().x));
    ImGui::BeginDisabled(inspector::pending.has_value() || !Fresh(state));
    if (ImGui::BeginCombo("##Selected target layers", caption))
    {
        for (const auto &target : d.hits)
        {
            ImGui::PushID(target.id);
            if (ImGui::Selectable(target.name.c_str(), target.id == d.selected))
                inspector::Request(2, target.id);
            Help((std::string(KindName(target.kind)) + " / #" + std::to_string(target.id)).c_str());
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    Help(
        "Targets from the last picker ray. Switch directly between a wall and its graffiti, including objects without colliders.");
}

inline void Properties(const inspector::State &state)
{
    Related(state);
    const auto &d = *state.detail;
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##Field search", "Find a property or field...", fieldSearch,
                             sizeof(fieldSearch));
    if (d.valuesDropped)
        ImGui::TextColored(g_accentCol, "%d fields omitted by the packet budget", d.valuesDropped);
    if (groupedDetail != state.detail)
    {
        groupedDetail = state.detail;
        fieldGroups = inspector::GroupValues(d.values);
    }
    std::string filter = Lower(fieldSearch);
    for (const auto &group : fieldGroups)
    {
        bool any = false;
        for (size_t index : group.values)
        {
            const auto &v = d.values[index];
            if (filter.empty() ||
                Lower(v.group + " " + v.name + " " + v.source).find(filter) != std::string::npos)
            {
                any = true;
                break;
            }
        }
        if (!any)
            continue;
        ImGui::PushID(group.name.c_str());
        if (!filter.empty())
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        bool opened =
            suite_ui::CollapsingHeader(group.name.c_str(), group.name == "Object" || group.name == "Transform"
                                                               ? ImGuiTreeNodeFlags_DefaultOpen
                                                               : ImGuiTreeNodeFlags_None);
        if (opened &&
            ImGui::BeginTable("##Fields", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthStretch, .38f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, .62f);
            ImGui::TableSetupColumn("Watch", ImGuiTableColumnFlags_WidthFixed, 30 * suite_ui::Unit());
            for (size_t index : group.values)
            {
                const auto &v = d.values[index];
                if (!filter.empty() &&
                    Lower(v.group + " " + v.name + " " + v.source).find(filter) == std::string::npos)
                    continue;
                ImGui::PushID(v.component);
                ImGui::PushID(v.source.c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", v.name.c_str());
                Help(v.source.c_str());
                ImGui::TableNextColumn();
                PropertyValue(v);
                Help((v.source + ((v.flags & 2) ? "\nPrivate game field; read without invoking a getter."
                                                : "\nObserved value; read only."))
                         .c_str());
                ImGui::TableNextColumn();
                ImGui::BeginDisabled(inspector::pending.has_value() || !Fresh(state));
                if (suite_ui::SmallButton(v.flags & 4 ? "-" : "+"))
                {
                    if (inspector::Request(4, 0, v.key))
                        Sound(suite_ui::Tone::InspectorPin);
                }
                Help(
                    v.flags & 4
                        ? "Remove this watch"
                        : "Watch once per game frame. Up to eight fields; observations do not identify the calling script.");
                ImGui::EndDisabled();
                ImGui::PopID();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::PopID();
    }
}

inline void Footprints(const inspector::Detail &d)
{
    float u = suite_ui::Unit(), width = ImGui::GetContentRegionAvail().x,
          height = std::clamp(ImGui::GetContentRegionAvail().y * .45f, 130 * u, 300 * u);
    auto a = ImGui::GetCursorScreenPos();
    ImGui::Dummy({width, height});
    auto *draw = ImGui::GetWindowDrawList();
    suite_ui::Surface(draw, a, {a.x + width, a.y + height});
    float x0 = 1e30f, z0 = 1e30f, x1 = -1e30f, z1 = -1e30f;
    for (const auto &g : d.geometry)
        for (const auto &part : g.parts)
            for (auto p : part.points)
            {
                x0 = std::min(x0, p.x);
                z0 = std::min(z0, p.z);
                x1 = std::max(x1, p.x);
                z1 = std::max(z1, p.z);
            }
    if (x0 > x1)
    {
        draw->AddText({a.x + 15 * u, a.y + 18 * u}, ImGui::GetColorU32(g_textCol),
                      "No captured footprint for this object");
        return;
    }
    float scale =
        std::min((width - 42 * u) / std::max(.5f, x1 - x0), (height - 42 * u) / std::max(.5f, z1 - z0));
    auto project = [&](practice::Vec3 p) {
        return ImVec2(a.x + width * .5f + (p.x - (x0 + x1) * .5f) * scale,
                      a.y + height * .5f - (p.z - (z0 + z1) * .5f) * scale);
    };
    draw->PushClipRect(a, {a.x + width, a.y + height}, true);
    for (const auto &g : d.geometry)
        for (const auto &p : g.parts)
        {
            std::array<ImVec2, 64> points;
            for (size_t i = 0; i < p.points.size(); i++)
                points[i] = project(p.points[i]);
            auto color = IM_COL32(212, 130, 255, 245);
            if (p.points.size() >= 3)
                draw->AddConvexPolyFilled(points.data(), int(p.points.size()), IM_COL32(139, 83, 169, 45));
            draw->AddPolyline(points.data(), int(p.points.size()), color, ImDrawFlags_Closed, 1.7f * u);
        }
    draw->PopClipRect();
}

inline void Geometry(const inspector::State &state)
{
    const auto &d = *state.detail;
    bool actual = inspector::options & 1, projection = inspector::options & 2;
    ImGui::BeginDisabled(inspector::pending.has_value());
    bool changed = suite_ui::Checkbox("Actual collider wire", &actual);
    SameLineIfFits("Captured footprint");
    changed |= suite_ui::Checkbox("Captured footprint", &projection);
    if (changed)
    {
        inspector::options = (inspector::options & ~3u) | (actual ? 1u : 0u) | (projection ? 2u : 0u);
        inspector::Request(0);
    }
    ImGui::EndDisabled();
    Help(
        "In the game: cyan is live enabled collider geometry; orange is inactive or disabled. Purple is the footprint from the last Suite capture, drawn at its captured floor height. Primitive wire is live. Readable non-convex mesh wire is an edge-budgeted snapshot at selection; reselect to refresh topology. Cooked convex hulls are not exposed. No approximate collision box is substituted.");
    Footprints(d);
    if (d.geometry.empty())
        ImGui::TextDisabled("No collider on this GameObject. Select a collider child in the object list.");
    for (const auto &g : d.geometry)
    {
        ImGui::PushID(g.id);
        ImGui::SeparatorText(g.type.c_str());
        ImGui::TextWrapped("%s", g.reason.c_str());
        ImGui::Text("Live: %s | %s | %s", g.flags & 1 ? "enabled" : "disabled",
                    g.flags & 2 ? "active" : "inactive", g.flags & 4 ? "trigger" : "solid");
        if (g.frame >= 0)
        {
            ImGui::TextDisabled("Captured frame %d | %.3f s | %zu parts%s", g.frame, g.seconds,
                                g.parts.size(), g.partial ? " / partial" : "");
            ImGui::Text("Height band %.3f - %.3f | radius %.3f", g.low, g.high, g.radius);
            ImGui::TextDisabled("Grounded at capture: %s%s", g.grounded ? "yes" : "no",
                                g.inactiveAccepted ? " | accepted by culling catalog" : "");
        }
        ImGui::PopID();
    }
}

inline void Watch(const inspector::State &state)
{
    const auto &d = *state.detail;
    int count = 0;
    for (const auto &v : d.values)
        if (v.flags & 4)
            count++;
    ImGui::TextColored(g_headerCol, "%d / 8 watches", count);
    Help(
        "Fields are sampled once per game frame while the Inspector is active and focused. Times identify the observation, not the original instruction or every intermediate physics change. Display publications are limited to 5 Hz.");
    SameLineIfFits("Clear watches");
    ImGui::BeginDisabled(inspector::pending.has_value() || !Fresh(state) || !count);
    if (suite_ui::Button("Clear watches"))
        inspector::Request(6);
    ImGui::EndDisabled();
    for (const auto &v : d.values)
        if (v.flags & 4)
        {
            ImGui::TextWrapped("%s: %s", v.name.c_str(), v.value.c_str());
            Help((v.group + " / " + v.source).c_str());
        }
    if (!count)
        ImGui::TextDisabled("Use + beside a property to watch it.");
    ImGui::SeparatorText("Observed changes");
    if (d.changesDropped)
        ImGui::TextColored(g_accentCol, "%d older observations omitted", d.changesDropped);
    std::unordered_map<uint64_t, std::string> names;
    for (const auto &v : d.values)
        names[v.key] = v.name;
    for (auto it = d.changes.rbegin(); it != d.changes.rend(); ++it)
    {
        auto found = names.find(it->key);
        const std::string name = found == names.end() ? std::to_string(it->key) : found->second;
        ImGui::TextColored(g_headerCol, "Frame %d  |  %.3f s", it->frame, it->seconds);
        ImGui::TextWrapped("%s: %s  ->  %s", name.c_str(), it->before.c_str(), it->after.c_str());
    }
}

inline void Comparison(const inspector::State &state)
{
    const auto &d = *state.detail;
    ImGui::BeginDisabled(!FullReady(state));
    if (suite_ui::Button(inspector::baseline ? "Replace baseline" : "Capture baseline"))
    {
        inspector::baseline = state.detail;
        compared = nullptr;
        suite_ui::Request(suite_ui::Tone::Confirm);
    }
    ImGui::EndDisabled();
    if (!FullReady(state))
    {
        suite_ui::Busy("Reading requested properties");
        return;
    }
    if (!inspector::baseline)
    {
        ImGui::TextWrapped(
            "Capture this object's properties, perform a game action, then return to compare.");
        return;
    }
    ImGui::TextDisabled("Baseline frame %d -> live frame %d", inspector::baseline->header.frame,
                        d.header.frame);
    if (compared != state.detail || comparedBaseline != inspector::baseline)
    {
        differences = inspector::Compare(*inspector::baseline, d);
        compared = state.detail;
        comparedBaseline = inspector::baseline;
    }
    if (differences.empty())
        ImGui::TextDisabled("No observed property changes.");
    for (const auto &change : differences)
    {
        ImGui::SeparatorText(change.name.c_str());
        ImGui::TextWrapped("Before: %s", change.before.c_str());
        ImGui::TextColored(g_headerCol, "Now:");
        ImGui::SameLine();
        ImGui::TextWrapped("%s", change.after.c_str());
        Help(change.source.c_str());
    }
}

inline void RouteProbe(const inspector::State &state)
{
    inspector::geometryFrame = ImGui::GetFrameCount();
    const auto &d = *state.detail;
    ImGui::BeginDisabled(inspector::pending.has_value() || !Fresh(state));
    if (suite_ui::Button("Pick start"))
        StartPick(7);
    SameLineIfFits("Pick end");
    if (suite_ui::Button("Pick end"))
        StartPick(8);
    ImGui::EndDisabled();
    Help(
        "Click actual solid surfaces. Endpoints are projected onto the common standing floor with player clearance; original and projected positions are shown separately.");
    auto map = g_mapWorker.Get(MapLevelIndex(d.header.scene));
    bool matching = map && map->data && map->session == d.header.session && map->data->pid == d.header.pid &&
                    uint32_t(map->data->epoch) == d.header.epoch;
    ImGui::BeginDisabled(!d.hasA || !d.hasB || !matching || state.probing || !Fresh(state));
    SameLineIfFits("Check route");
    if (suite_ui::Button("Check route"))
        inspector::worker.ProbeMap(map->data, d.a, d.b);
    ImGui::EndDisabled();
    if (d.hasA)
        ImGui::Text("Start %.3f, %.3f, %.3f", d.a.x, d.a.y, d.a.z);
    if (d.hasB)
        ImGui::Text("End   %.3f, %.3f, %.3f", d.b.x, d.b.y, d.b.z);
    if (!matching)
        ImGui::TextDisabled("Waiting for matching live map geometry.");
    if (state.probing)
        suite_ui::Busy("Checking common floor and clearance");
    if (state.probe)
    {
        const auto &p = *state.probe;
        ImGui::TextWrapped("%s", p.message.c_str());
        ImGui::TextDisabled("Floor projection: A %s / B %s | connected: %s", p.projectedA ? "yes" : "no",
                            p.projectedB ? "yes" : "no", p.sameComponent ? "yes" : "no");
        if (p.projectedA)
            ImGui::Text("Projected A %.3f, %.3f, %.3f", p.start.x, p.start.y, p.start.z);
        if (p.projectedB)
            ImGui::Text("Projected B %.3f, %.3f, %.3f", p.end.x, p.end.y, p.end.z);
        if (std::isfinite(p.path.length) && matching)
        {
            ImGui::TextColored(g_headerCol, "%.3f units | %zu path vertices", p.path.length,
                               p.path.points.size());
            practice::MapAnalysis view = *map;
            view.route = {};
            view.route.line = p.path.points;
            view.route.length = p.path.length;
            view.route.complete = true;
            view.route.estimated = !p.physical;
            view.route.estimatedLine.assign(p.path.points.size() > 0 ? p.path.points.size() - 1 : 0,
                                            p.physical ? 0 : 1);
            RenderSeedCanvas(view, std::max(140.f, ImGui::GetContentRegionAvail().y - 10));
        }
    }
}

inline void FocusButton(const inspector::State &state)
{
    ImGui::BeginDisabled(!Fresh(state) || !state.detail || !state.detail->selected ||
                         inspector::pending.has_value());
    if (Action("Focus in Freecam", suite_ui::Icon::Camera))
    {
        if (inspector::Request(5))
            inspector::focusSerial = inspector::pending->serial;
    }
    ImGui::EndDisabled();
    Help(
        "Frame the selected object using the existing spectator camera. The player body stays in place. After acknowledgement, return to the game: RMB look, WASD/QE move, F3 menu. Closing the Inspector restores a Freecam it created.");
}

inline void SaveButton(const inspector::State &state)
{
    ImGui::BeginDisabled(!FullReady(state) || !state.detail || !state.detail->selected);
    if (suite_ui::Button("Save diagnostic snapshot"))
    {
        try
        {
            inspector::ReportContext context;
            context.version = suite_brand::Version;
            context.seedKnown = g_pShared->seedKnown;
            context.seed = g_pShared->runSeed;
            context.seedKind = g_pShared->seedKind;
            context.game = g_pShared->gameVersion;
            context.unity = g_pShared->unityVersion;
            context.assemblyHash = g_pShared->gameAssemblyHash;
            context.probe = state.probe.get();
            auto body =
                inspector::Report(*state.detail, inspector::baseline.get(), BuildDiagnostics(), context);
            auto folder = inspector::reportFolder.empty()
                              ? std::filesystem::temp_directory_path() / "ItStealsPracticeSuite" / "Inspector"
                              : inspector::reportFolder;
            auto saved = inspector::SaveReport(folder, body, state.detail->header.utc).u8string();
            inspector::lastReport.assign(saved.begin(), saved.end());
            suite_ui::Notify("Inspector snapshot saved");
            suite_ui::Request(suite_ui::Tone::Confirm);
        }
        catch (const std::exception &e)
        {
            inspector::transportError = e.what();
        }
    }
    ImGui::EndDisabled();
    Help(
        "Save local JSON with observed properties, collider capture decisions, watch changes, optional baseline differences, seed, camera pose and diagnostics. No upload. This is a debugging record, not a complete save state or trusted validation report.");
}

inline void PickingMode()
{
    float u = suite_ui::Unit();
    int mode = (inspector::options & 4) ? ((inspector::options & 32) ? 1 : 0) : 2;
    const char *modes[] = {"Visuals + colliders", "Visuals only", "Colliders only"};
    ImGui::SetNextItemWidth(std::min(300 * u, ImGui::GetContentRegionAvail().x));
    ImGui::BeginDisabled(inspector::pending.has_value());
    if (ImGui::Combo("##Picking source", &mode, modes, 3))
    {
        inspector::options = (inspector::options & ~36u) | (mode == 0 ? 4u : mode == 1 ? 36u : 0u);
        inspector::Request(0);
    }
    bool triggers = inspector::options & 8;
    if (mode != 1 && suite_ui::Checkbox("Include trigger colliders", &triggers))
    {
        inspector::options = (inspector::options & ~8u) | (triggers ? 8u : 0u);
        inspector::Request(0);
    }
    ImGui::EndDisabled();
    Help(
        "Visuals include renderers without colliders. Readable mesh and simple sprite triangles are tested; other geometry uses clearly-labelled approximate bounds. Textured alpha is not tested. Wheel cycles targets; Tab opens the ray layer list. A short sliced scene scan prepares visual targets.");
}

inline void Status(const inspector::State &state)
{
    if (!state.detail)
        return;
    const auto &d = *state.detail;
    ImGui::TextDisabled("%s | %.1f ms | frame %d", Fresh(state) ? "LIVE" : "WAITING", d.detailMs,
                        d.header.frame);
    Help(
        "Selected properties publish at up to 5 Hz. Watched fields are sampled once per game frame while focused. Scan work is sliced over frames; this displays the last detail-build cost, not total game overhead.");
    if (d.scanning)
        ImGui::TextDisabled("Refreshing: %d objects", d.scanned);
}

inline void Errors(const inspector::State &state)
{
    if (!state.error.empty())
        ImGui::TextWrapped("%s", state.error.c_str());
    if (!inspector::transportError.empty())
        ImGui::TextWrapped("%s", inspector::transportError.c_str());
}

inline void DetailContent(const inspector::State &state)
{
    inspector::detailsFrame = ImGui::GetFrameCount();
    inspector::options |= 16u;
    if (!(state.detail->options & 16))
    {
        suite_ui::Busy("Reading engine properties");
    }
    else if (ImGui::BeginTabBar("Inspector tabs", ImGuiTabBarFlags_FittingPolicyScroll))
    {
        const char *tabs[] = {"Properties", "Geometry", "Watch", "Compare", "Route probe"};
        for (int i = 0; i < 5; i++)
        {
            ImGuiTabItemFlags flags = tabRequest == i ? ImGuiTabItemFlags_SetSelected : 0;
            if (suite_ui::BeginTabItem(tabs[i], nullptr, flags))
            {
                if (tabRequest == i)
                    tabRequest = -1;
                switch (i)
                {
                case 0:
                    Properties(state);
                    break;
                case 1:
                    Geometry(state);
                    break;
                case 2:
                    Watch(state);
                    break;
                case 3:
                    Comparison(state);
                    break;
                case 4:
                    RouteProbe(state);
                    break;
                }
                suite_ui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator();
    SaveButton(state);
    if (!inspector::lastReport.empty())
    {
        SameLineIfFits("Copy report path");
        if (suite_ui::Button("Copy report path"))
            ImGui::SetClipboardText(inspector::lastReport.c_str());
    }
    Status(state);
    PickingMode();
}

inline void RenderTab()
{
    if (!Allowed())
    {
        suite_ui::EmptyState("Inspector lock", suite_ui::Icon::Lock, "Inspector unavailable",
                             "Use training or practice mode outside the 3D Replay.");
        return;
    }
    if (!EnsureOpen())
        return;
    auto state = inspector::worker.Get();
    SelectionChanged(*state);
    if (resetScroll)
    {
        ImGui::SetScrollY(0);
        resetScroll = false;
    }
    float u = suite_ui::Unit();
    ImGui::BeginDisabled(inspector::pending.has_value());
    if (Action("Picker  [F7]", suite_ui::Icon::Scan, true))
        StartPick(3);
    ImGui::EndDisabled();
    Help(
        "Hide the menu and select in the game. Hover highlights the hit; click confirms that same object. Esc / RMB / F7 cancel.");
    SameLineIfFits("Fullbright");
    FullbrightButton();
    InlineAction(browse ? "Hide object list" : "Object list");
    if (Action(browse ? "Hide object list" : "Object list", suite_ui::Icon::Map, browse))
    {
        browse = !browse;
        if (browse && !state->catalog)
            inspector::Request(1);
    }
    Help(
        "Browse inactive objects or objects without a collider. The visual picker prepares a sliced catalog. Refresh explicitly to include newly created scene objects.");
    InlineAction("Style");
    StyleControls();
    if (state->detail && state->detail->selected)
    {
        SameLineIfFits("In-game card");
        if (suite_ui::Button("In-game card"))
        {
            inspector::resumeCompact = false;
            inspector::dock = inspector::dockInteractive = true;
            g_overlayVisible = false;
            SetOverlayInteractive(true);
        }
    }
    Errors(*state);
    if (browse)
    {
        ImGui::BeginDisabled(inspector::pending.has_value());
        if (suite_ui::Button("Refresh objects"))
            inspector::Request(1);
        ImGui::EndDisabled();
        ImGui::BeginChild("Inspector catalog", {0, 220 * u}, ImGuiChildFlags_Borders);
        Objects(*state);
        ImGui::EndChild();
    }
    if (!state->detail || !state->detail->selected)
    {
        suite_ui::EmptyState(
            "Inspector selection", suite_ui::Icon::Scan, "Point. Pick. Inspect.",
            "Press F7 to select directly in the game. A compact object card keeps the view clear.", 178);
        PickingMode();
        return;
    }
    Basics(*state);
    TargetStack(*state);
    ImGui::BeginDisabled(!Fresh(*state) || inspector::pending.has_value());
    if (suite_ui::Button("Copy object ID"))
        ImGui::SetClipboardText(std::to_string(state->detail->selected).c_str());
    ImGui::EndDisabled();
    Help(state->detail->path.c_str());
    SameLineIfFits("Focus in Freecam");
    FocusButton(*state);
    if (resetDetails)
    {
        ImGui::SetNextItemOpen(expanded, ImGuiCond_Always);
        resetDetails = false;
        ++detailRevealGeneration;
    }
    bool newExpanded =
        suite_ui::CollapsingHeader("Technical details", expanded ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    // The ImGui tree owns user toggles. SelectionChanged resets it via SetNextItemOpen below on the next
    // object.
    expanded = newExpanded;
    auto revealId = ImHashData(&detailRevealGeneration, sizeof(detailRevealGeneration),
                               ImGui::GetID("##Inspector details drawer"));
    float reveal =
        Motion() ? suite_ui::Animate(revealId, expanded ? 1.f : 0.f, expanded, .18f) : (expanded ? 1.f : 0.f);
    if (expanded || reveal > .001f)
    {
        auto *window = ImGui::GetCurrentWindow();
        auto begin = ImGui::GetCursorPos(), screen = ImGui::GetCursorScreenPos(),
             maximum = window->DC.CursorMaxPos;
        auto *storage = ImGui::GetStateStorage();
        float previousHeight = storage->GetFloat(revealId, 0);
        bool clipped = reveal < .999f;
        if (clipped)
            ImGui::PushClipRect(screen, {window->ClipRect.Max.x, screen.y + previousHeight * reveal}, true);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * (.7f + .3f * reveal));
        ImGui::SetCursorPosX(begin.x + (1 - reveal) * 12 * u);
        ImGui::BeginDisabled(!expanded);
        ImGui::BeginGroup();
        DetailContent(*state);
        ImGui::EndGroup();
        ImGui::EndDisabled();
        float height = std::max(0.f, ImGui::GetCursorPosY() - begin.y);
        storage->SetFloat(revealId, height);
        ImGui::PopStyleVar();
        if (clipped)
        {
            ImGui::PopClipRect();
            ImGui::SetCursorPosY(begin.y + height * reveal);
            window->DC.CursorMaxPos.y = std::max(maximum.y, screen.y + height * reveal);
        }
        ImGui::SetCursorPosX(begin.x);
    }
    if (Action("End inspection", suite_ui::Icon::Lock))
    {
        Sound(suite_ui::Tone::InspectorClose);
        inspector::Close();
        expanded = false;
        resetDetails = true;
        suite_ui::navigation.Select(suite_ui::Page::Cheats, ImGui::GetTime());
    }
    Help("Stop inspection, watches and collider drawing. Restore a Freecam created by the Inspector.");
}

inline void Card()
{
    if (!inspector::dock || (g_overlayVisible && !g_compactMode) ||
        (!inspector::dockInteractive && !inspector::pinned) || !g_allOverlayEnabled)
        return;
    auto state = inspector::worker.Get();
    SelectionChanged(*state);
    if (!state->detail || !state->detail->selected)
        return;
    auto display = ImGui::GetIO().DisplaySize;
    float u = suite_ui::Unit(), width = std::min(418 * u, display.x - 24),
          height = std::min((inspector::dockInteractive ? 530 : 320) * u, display.y - 24);
    if (cardFrame != ImGui::GetFrameCount() - 1)
    {
        cardAt = ImGui::GetTime();
        cardPopupWasOpen = false;
    }
    cardFrame = ImGui::GetFrameCount();
    ImGui::SetNextWindowPos({std::max(12.f, display.x - width - 20 * u), 20 * u}, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({width, height}, ImGuiCond_Always);
    ImGui::SetNextWindowPos({std::clamp(ImGui::FindWindowByName("Inspector / object")
                                            ? ImGui::FindWindowByName("Inspector / object")->Pos.x
                                            : display.x - width - 20 * u,
                                        0.f, std::max(0.f, display.x - width)),
                             std::clamp(ImGui::FindWindowByName("Inspector / object")
                                            ? ImGui::FindWindowByName("Inspector / object")->Pos.y
                                            : 20 * u,
                                        0.f, std::max(0.f, display.y - height))},
                            ImGuiCond_Always);
    bool visible = true;
    auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoSavedSettings;
    if (!inspector::dockInteractive)
        flags |= ImGuiWindowFlags_NoInputs;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {16 * u, 14 * u});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(
        ImGuiStyleVar_Alpha,
        ImGui::GetStyle().Alpha *
            (Motion() ? .8f + .2f * suite_ui::Ease(float((ImGui::GetTime() - cardAt) / .16)) : 1));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, {0, 0, 0, 0});
    if (ImGui::Begin("Inspector / object", nullptr, flags))
    {
        auto p = ImGui::GetWindowPos();
        auto *draw = ImGui::GetWindowDrawList();
        Panel(draw, p, {p.x + width, p.y + height}, true);
        BorderImpulse(draw, {p.x + u, p.y + u}, {p.x + width - u, p.y + height - u},
                      ImGui::GetTime() - cardAt, Accent());
        auto a = ImGui::GetCursorScreenPos();
        float headerWidth = ImGui::GetContentRegionAvail().x - 34 * u;
        ImGui::InvisibleButton("##Inspector card drag", {headerWidth, 26 * u});
        if (inspector::dockInteractive && ImGui::IsItemActivated() &&
            ImGui::IsMouseDown(ImGuiMouseButton_Left))
            ImGui::StartMouseMovingWindow(ImGui::GetCurrentWindow());
        suite_ui::DrawIcon(draw, suite_ui::Icon::Scan, {a.x, a.y + u}, 21 * u,
                           Ink(inspector::pinned ? PinnedAccent() : Accent()));
        Text(draw, {a.x + 31 * u, a.y + 3 * u}, "LIVE INSPECTOR", 16 * u, g_textCol,
             {a.x, a.y, a.x + headerWidth, a.y + 26 * u});
        if (inspector::dockInteractive)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton("X##Close inspector"))
            {
                Sound(suite_ui::Tone::InspectorClose);
                visible = false;
            }
        }
        ImGui::Spacing();
        Basics(*state);
        if (inspector::dockInteractive)
        {
            TargetStack(*state);
            FullbrightButton();
            SameLineIfFits("Pin card");
            if (suite_ui::Checkbox("Pin card", &inspector::pinned))
                Sound(inspector::pinned ? suite_ui::Tone::InspectorPin : suite_ui::Tone::InspectorClose);
            Help("Keep this card visible while playing. F3 opens the menu; F7 selects another object.");
            ImGui::BeginDisabled(inspector::pending.has_value());
            if (Action("Picker  [F7]", suite_ui::Icon::Scan, true))
                StartPick(3);
            ImGui::EndDisabled();
            InlineAction("Copy ID");
            if (Action("Copy ID", suite_ui::Icon::Collider))
                ImGui::SetClipboardText(std::to_string(state->detail->selected).c_str());
            FocusButton(*state);
            InlineAction("Details");
            if (Action("Details", suite_ui::Icon::Settings))
            {
                Open();
                expanded = true;
                resetDetails = true;
                tabRequest = 0;
            }
            if (Action("Return to game  [F3]", suite_ui::Icon::Path))
            {
                ReturnToGame();
            }
            InlineAction("Style");
            StyleControls();
            Errors(*state);
            bool popup =
                ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !popup && !cardPopupWasOpen)
                ReturnToGame();
            cardPopupWasOpen = popup;
        }
        else
            ImGui::TextDisabled("PINNED  |  F7 picker");
        ImGui::Spacing();
        ImGui::TextDisabled("%s", suite_brand::Credit);
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
    if (!visible)
    {
        ReturnToGame();
        inspector::Close();
        SetOverlayInteractive(false);
    }
}

inline void RenderOverlay()
{
    if (!inspector::open)
        return;
    if (!Allowed())
    {
        inspector::Close();
        SetOverlayInteractive(g_overlayVisible && !g_compactMode);
        return;
    }
    if (inspector::picking)
        Picker();
    else
    {
        if (confirmedBounds && Motion() && ImGui::GetTime() - selectedAt < .48)
        {
            auto size = ImGui::GetIO().DisplaySize;
            const auto &h = *confirmedBounds;
            if (Fresh(*inspector::worker.Get()) && h.object == lastObject &&
                inspector::SameScene(h.header, inspector::identity))
            {
                ImVec2 a(h.left * size.x, h.top * size.y), b(h.right * size.x, h.bottom * size.y);
                float age = float(ImGui::GetTime() - selectedAt);
                Brackets(ImGui::GetBackgroundDrawList(), a, b, PinnedAccent(), suite_ui::Ease(age / .16f),
                         1 - age / .48f);
                BorderImpulse(ImGui::GetBackgroundDrawList(), a, b, age, g_textCol);
            }
        }
        Card();
    }
}

inline void FinishFrame()
{
    if (!inspector::open)
        return;
    if (!Allowed())
    {
        inspector::Close();
        SetOverlayInteractive(g_overlayVisible && !g_compactMode);
        return;
    }
    auto state = inspector::worker.Get();
    if (inspector::detailsFrame != ImGui::GetFrameCount())
        inspector::options &= ~16u;
    if (inspector::picking && inspector::clickPointer && !inspector::commitSerial && !inspector::pending)
    {
        auto h = state->hover;
        const auto click = *inspector::clickPointer;
        if (h && h->serial == click.serial && h->active &&
            inspector::SameScene(h->header, inspector::identity) && inspector::HoverAge(*state) < .25)
        {
            if (h->object)
            {
                if (inspector::Request(inspector::pickAction, h->object, h->serial, click.x, click.y))
                    inspector::commitSerial = inspector::pending->serial;
            }
            else
            {
                inspector::transportError = h->status;
                inspector::clickPointer.reset();
            }
        }
        else if (std::chrono::duration<double>(inspector::Clock::now() - inspector::pointerAt).count() > 1)
        {
            inspector::transportError = "No fresh target from the game. Move the pointer and try again.";
            inspector::clickPointer.reset();
        }
    }
    if (inspector::commitSerial && !inspector::pending && Fresh(*state) &&
        state->detail->ack == inspector::commitSerial)
    {
        auto d = state->detail;
        bool success = d->ackSuccess;
        inspector::commitSerial = 0;
        inspector::clickPointer.reset();
        if (success)
        {
            int action = inspector::pickAction;
            if (action == 3 && state->hover && state->hover->outline && state->hover->object == d->selected &&
                inspector::SameScene(state->hover->header, inspector::identity))
                confirmedBounds = *state->hover;
            inspector::picking = false;
            inspector::StopPointer();
            expanded = false;
            inspector::options &= ~16u;
            SelectionChanged(*state);
            selectedAt = ImGui::GetTime();
            if (action == 3)
            {
                inspector::dock = inspector::dockInteractive = true;
                g_overlayVisible = false;
                SetOverlayInteractive(true);
                Sound(suite_ui::Tone::InspectorSelect);
            }
            else
            {
                g_overlayVisible = true;
                inspector::dockInteractive = false;
                SetOverlayInteractive(true);
            }
        }
        else
            inspector::transportError = d->status;
    }
    if (inspector::focusSerial && !inspector::pending && Fresh(*state))
    {
        auto d = state->detail;
        bool focused = d->ack == inspector::focusSerial && d->ackAction == 5 && d->ackSuccess;
        inspector::focusSerial = 0;
        if (focused)
            ReturnToGame();
    }
    if (!inspector::pending && inspector::lastSubmittedOptions != inspector::options)
        inspector::Request(0);
}
} // namespace inspector_ui
