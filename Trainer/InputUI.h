#pragma once
#include "InputModules.h"
#include "ReplayArchive.h"
#include "ReplayMotion.h"
#include <bit>
#include <optional>

namespace suite_input
{
inline std::optional<Frame> ReplayFrame(const practice::ReplayMotion &m, double seconds)
{
    if (!m.devices.empty())
    {
        if (seconds > m.deviceThrough + .25)
            return {};
        auto p = practice::ReplayAt(m.devices, seconds);
        return p ? std::optional<Frame>(*p) : std::nullopt;
    }
    const auto *e = practice::ReplayInputAt(m, seconds);
    if (!e)
        return {};
    Frame f;
    f.seconds = e->seconds;
    f.frame = e->frame;
    f.tick = e->tick;
    f.state = e->state;
    f.knownStates = m.inputFormat >= 3 ? 2047 : 511;
    f.legacyKeys = true;
    if (auto v = practice::ReplayViewAt(m, seconds))
    {
        f.moveX = v->horizontal;
        f.moveY = v->vertical;
        f.lookX = v->mouseX;
        f.lookY = v->mouseY;
        f.joyX = v->joystickX;
        f.joyY = v->joystickY;
    }
    static const unsigned vk[] = {'W', 'A', 'S', 'D', 160, 161, 'R'};
    for (int i = 0; i < 7; i++)
        if (e->keys & (1u << i))
            f.Key(vk[i]);
    return f;
}

inline std::vector<Change> ReplayChanges(const practice::ReplayMotion &m, double seconds)
{
    std::vector<Change> out;
    auto at = std::upper_bound(m.events.begin(), m.events.end(), seconds,
                               [](double s, const auto &e) { return s < e.seconds; });
    int examined = 0;
    while (at != m.events.begin() && out.size() < 8 && examined++ < 4096)
    {
        auto p = --at;
        if (seconds - p->seconds > 2.5)
            break;
        uint32_t before = p == m.events.begin() ? p->state : std::prev(p)->state;
        uint32_t changed = before ^ p->state;
        if (changed & VisibleChanges)
            out.push_back({p->seconds, p->frame, p->tick, changed, p->state});
    }
    std::reverse(out.begin(), out.end());
    return out;
}

inline Glow ReplayGlow(const practice::ReplayMotion &motion, double seconds)
{
    Glow result{};
    auto end = std::upper_bound(motion.devices.begin(), motion.devices.end(), seconds,
                                [](double t, const auto &f) { return t < f.seconds; });
    int checked = 0;
    for (auto it = end; it != motion.devices.begin() && checked++ < 4096;)
    {
        --it;
        if (seconds - it->seconds > .16)
            break;
        if (it == motion.devices.begin())
            continue;
        auto prior = std::prev(it);
        float fade = float(std::clamp(1 - (seconds - it->seconds) / .16, 0., 1.));
        for (size_t group = 0; group < 4; group++)
        {
            uint64_t released = prior->keys[group] & ~it->keys[group];
            while (released)
            {
                unsigned bit = std::countr_zero(released), key = unsigned(group * 64 + bit);
                result[key] = std::max(result[key], fade);
                released &= released - 1;
            }
        }
        for (unsigned key : {16u, 17u, 18u})
            if (Held(&*prior, key) && !Held(&*it, key))
                result[key] = std::max(result[key], fade);
    }
    return result;
}

inline Palette Colors(float opacity)
{
    Palette c;
    c.background = ImGui::ColorConvertFloat4ToU32(suite_ui::HudBackground(opacity));
    c.ink = ImGui::ColorConvertFloat4ToU32(suite_ui::HudInk());
    c.accent = ImGui::ColorConvertFloat4ToU32(suite_ui::HudAccent());
    c.muted = ImGui::ColorConvertFloat4ToU32(suite_ui::HudMuted());
    c.edge = ImGui::ColorConvertFloat4ToU32(suite_ui::Color(AppearancePreferences::Outline, .8f));
    return c;
}

inline Frame Demo()
{
    Frame f;
    f.flags = 257;
    f.state = 1 | 4 | 64 | 128;
    f.Key('W');
    f.Key('D');
    f.Key(160);
    f.pads[0].ly = 20000;
    f.pads[0].rt = 180;
    return f;
}

inline void Customization(Preferences &p, int module = -1)
{
    if (module < 0)
    {
        suite_ui::Checkbox("Keyboard", &p.keyboard);
        ImGui::SameLine();
        suite_ui::Checkbox("Mouse", &p.mouse);
        ImGui::SameLine();
        suite_ui::Checkbox("Controller", &p.controller);
        suite_ui::Checkbox("Player activity", &p.states);
        if (p.states)
        {
            ImGui::SameLine();
            suite_ui::Checkbox("Recent changes", &p.events);
        }
    }
    if (module == 3 && p.states)
        suite_ui::Checkbox("Recent changes", &p.events);
    if ((module < 0 || module == 0) && p.keyboard && ImGui::TreeNode("Visible keys"))
    {
        suite_ui::Checkbox("Full keyboard", &p.fullKeyboard);
        suite_ui::Checkbox("Choose individual keys", &p.customKeys);
        if (p.customKeys)
        {
            if (suite_ui::Button("Movement preset"))
            {
                for (auto &mask : p.keyMask)
                    mask = 0;
                for (unsigned key : {87u, 65u, 83u, 68u, 16u, 17u, 82u, 32u, 18u})
                    p.ShowKey(key, true);
            }
            ImGui::SameLine();
            if (suite_ui::Button("All keys"))
                for (auto &mask : p.keyMask)
                    mask = 0xffffffffu;
            static char search[40] = "";
            ImGui::InputText("Filter keys", search, sizeof(search));
            ImGui::BeginChild("Key choices", {0, 180 * suite_ui::Unit()}, true);
            int col = 0;
            for (unsigned k = 1; k < 256; k++)
            {
                auto name = KeyName(k);
                if (*search && name.find(search) == std::string::npos)
                    continue;
                ImGui::PushID(int(k));
                bool on = p.ShowKey(k);
                if (suite_ui::Checkbox(name.c_str(), &on))
                    p.ShowKey(k, on);
                ImGui::PopID();
                if (++col % 3)
                    SameLineIfFits("VK 255");
            }
            ImGui::EndChild();
        }
        ImGui::TreePop();
    }
    if ((module < 0 || module == 1) && p.mouse && ImGui::TreeNode("Mouse appearance"))
    {
        Help(
            "Movement shows the game's observed Mouse X / Y axes. Up is up; circular scaling preserves diagonal direction without hard clipping. These are game input values, not raw physical mouse counts.");
        ImGui::SliderFloat("Mouse size", &p.mouseScale, .5f, 2.5f, "%.2fx");
        for (int i = 0; i < 7; i++)
        {
            ImGui::PushID(i);
            bool on = p.mouseMask & (1 << i);
            std::string name = "Mouse " + std::to_string(i + 1);
            if (suite_ui::Checkbox(name.c_str(), &on))
            {
                if (on)
                    p.mouseMask |= 1 << i;
                else
                    p.mouseMask &= ~(1 << i);
            }
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
    if ((module < 0 || module == 3) && p.states && ImGui::TreeNode("Visible states"))
    {
        const char *names[] = {"Sprint", "Crouch", "Radar", "Light", "Clock"};
        for (int i = 0; i < 5; i++)
        {
            bool on = p.stateMask & (1 << i);
            if (suite_ui::Checkbox(names[i], &on))
            {
                if (on)
                    p.stateMask |= 1 << i;
                else
                    p.stateMask &= ~(1 << i);
            }
        }
        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Style", module >= 0 ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None))
    {
        suite_ui::Checkbox("Panel background", &p.background);
        suite_ui::Checkbox("Panel outline", &p.border);
        suite_ui::Checkbox("Panel shadow", &p.shadow);
        suite_ui::Checkbox("Key outlines", &p.keyOutline);
        suite_ui::Checkbox("Keyboard heading", &p.heading);
        suite_ui::Checkbox("Subtle animations", &p.animations);
        ImGui::SliderFloat("Panel opacity", &p.opacity, 0, 1, "%.2f");
        ImGui::SliderFloat("Content opacity", &p.contentOpacity, 0, 1, "%.2f");
        ImGui::SliderFloat("Idle key fill", &p.keyFill, 0, 1, "%.2f");
        ImGui::TreePop();
    }
    if ((module < 0 || module == 3) && p.states && p.events)
    {
        const char *clocks[] = {"Run / session time", "Game frame", "Physics tick"};
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##Event timestamp", &p.clock, clocks, 3);
    }
    if ((module < 0 || module == 2) && p.controller && ImGui::TreeNode("Controller settings"))
    {
        const char *symbols[] = {"Xbox labels", "PlayStation labels"};
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##Controller symbols", &p.symbols, symbols, 2);
        const char *devices[] = {"Automatic",    "Controller 1", "Controller 2",
                                 "Controller 3", "Controller 4", "Controller 5",
                                 "Controller 6", "Controller 7", "Controller 8"};
        int selected = p.controllerIndex + 1;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##Controller device", &selected, devices, 9))
            p.controllerIndex = selected - 1;
        Help("XInput buttons, triggers and sticks are recorded directly. Generic "
             "game joysticks retain neutral B0-B19 "
             "labels.");
        ImGui::TreePop();
    }
    p.Validate();
}

inline void Settings(Preferences &p = preferences)
{
    suite_ui::Checkbox("Live input HUD", &p.enabled);
    Help("Focus-scoped observations. Keys and accepted game states are separate. "
         "Live settings are independent of clip "
         "projects.");
    Customization(p);
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("Input scale", &p.scale, .5f, 2.5f, "%.2fx");
    p.Validate();
    float width = ImGui::GetContentRegionAvail().x;
    auto size = Size(p);
    float height = width * size.y / size.x;
    auto a = ImGui::GetCursorScreenPos();
    auto b = ImVec2(a.x + width, a.y + height);
    auto f = Demo();
    Draw(ImGui::GetWindowDrawList(), a, b, &f, p, {}, 0,
         suite_ui::hudFont ? suite_ui::hudFont : ImGui::GetFont(), Colors(1));
    ImGui::Dummy({width, height});
    Help("Synthetic layout preview. Edit HUD on screen sets position and overall "
         "scale.");
}

inline Glow liveGlow{};
inline std::array<double, 263> lastHeld{};
inline double glowNow = -1;
inline int64_t glowSource = 0;

inline void LiveHud()
{
    if (!preferences.enabled || !g_allOverlayEnabled)
        return;
    Frame f;
    const Frame *value = nullptr;
    std::vector<Change> changes;
    double now = 0;
    if (g_pShared && g_pShared->replay3dState >= 2 && g_pShared->replay3dState <= 3)
    {
        auto m = practice::g_replayArchive.Motion();
        if (m && m->id == g_pShared->replay3dRunId)
        {
            now = g_pShared->replay3dSeconds;
            auto sample = ReplayFrame(*m, now);
            if (sample)
            {
                f = *sample;
                value = &f;
            }
            changes = ReplayChanges(*m, now);
        }
    }
    else if (g_pShared && SnapshotAge() < .5 && g_pShared->inputSequence)
    {
        f = Live(*g_pShared);
        value = &f;
        now = f.seconds + std::min(.5, SnapshotAge());
        changes = Changes(*g_pShared);
    }
    int64_t source = g_pShared && g_pShared->replay3dState >= 2 && g_pShared->replay3dState <= 3
                         ? g_pShared->replay3dRunId
                         : 0;
    if (source != glowSource || now < glowNow || glowNow < 0)
    {
        lastHeld.fill(-100);
        glowSource = source;
    }
    glowNow = now;
    for (unsigned k = 1; k < 256; k++)
    {
        if (Held(value, k))
            lastHeld[k] = now;
        liveGlow[k] = float(std::clamp(1 - (now - lastHeld[k]) / .16, 0., 1.));
    }
    auto view = ImGui::GetIO().DisplaySize, base = Size(preferences);
    auto place = [&](ImVec2 area, ImVec2 &at, ImVec2 &size) {
        float scale = std::min(preferences.scale * g_dpiScale,
                               std::min((area.x - 24) / base.x, (area.y - 24) / base.y));
        scale = std::max(.05f, scale);
        size = {base.x * scale, base.y * scale};
        at = {12 + (area.x - 24 - size.x) * preferences.x, 12 + (area.y - 24 - size.y) * preferences.y};
    };
    ImVec2 size, at;
    place(view, at, size);
    if (g_pShared && g_pShared->replay3dState >= 1 && g_pShared->replay3dState <= 3)
    {
        auto *dock = ImGui::FindWindowByName("##Replay3D status");
        if (dock && dock->LastFrameActive == ImGui::GetFrameCount() && at.x < dock->Pos.x + dock->Size.x &&
            at.x + size.x > dock->Pos.x && at.y + size.y > dock->Pos.y)
        {
            place({view.x, std::max(64.f, dock->Pos.y - 8)}, at, size);
        }
    }
    ImGui::SetNextWindowPos(at, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::Begin("Input HUD", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                     ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav);
    DrawModules(ImGui::GetWindowDrawList(), at, {at.x + size.x, at.y + size.y}, value, preferences, changes,
                now, suite_ui::hudFont ? suite_ui::hudFont : ImGui::GetFont(), Colors(1), &liveGlow);
    ImGui::End();
    ImGui::PopStyleVar();
}
} // namespace suite_input
