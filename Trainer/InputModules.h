#pragma once
#include "InputModel.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <bit>
#include <cstdio>

namespace suite_input
{
struct Palette
{
    ImU32 background = IM_COL32(12, 19, 24, 230), ink = IM_COL32(232, 244, 244, 255),
          accent = IM_COL32(95, 216, 194, 255), muted = IM_COL32(140, 164, 172, 255),
          edge = IM_COL32(57, 82, 90, 255);
};

inline ImU32 Mix(ImU32 a, ImU32 b, float t)
{
    ImU32 c = 0;
    for (int i = 0; i < 3; i++)
        c |= uint32_t(((a >> (i * 8)) & 255) * (1 - t) + ((b >> (i * 8)) & 255) * t) << (i * 8);
    return c | 0xf0000000u;
}

inline ImU32 PressedInk(ImU32 c)
{
    return .2126f * (c & 255) + .7152f * ((c >> 8) & 255) + .0722f * ((c >> 16) & 255) > 135
               ? IM_COL32(8, 20, 26, 255)
               : IM_COL32(244, 250, 250, 255);
}

inline ImU32 Fade(ImU32 c, float a)
{
    return (c & 0x00ffffffu) | (uint32_t(std::clamp(a, 0.f, 1.f) * float(c >> 24)) << 24);
}

inline void Label(ImDrawList *d, ImFont *font, float size, ImVec2 p, ImU32 color, const char *text)
{
    d->AddText(font, size, p, color, text);
}

inline std::string MakeKeyName(unsigned key)
{
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        return std::string(1, char(key));
    if (key >= 112 && key <= 135)
        return "F" + std::to_string(key - 111);
    if (key >= 96 && key <= 105)
        return "Num " + std::to_string(key - 96);
    switch (key)
    {
    case 8:
        return "Back";
    case 9:
        return "Tab";
    case 13:
        return "Enter";
    case 16:
        return "Shift";
    case 17:
        return "Ctrl";
    case 18:
        return "Alt";
    case 20:
        return "Caps";
    case 27:
        return "Esc";
    case 32:
        return "Space";
    case 33:
        return "PgUp";
    case 34:
        return "PgDn";
    case 35:
        return "End";
    case 36:
        return "Home";
    case 37:
        return "Left";
    case 38:
        return "Up";
    case 39:
        return "Right";
    case 40:
        return "Down";
    case 45:
        return "Ins";
    case 46:
        return "Del";
    case 91:
    case 92:
        return "Win";
    case 160:
        return "L Shift";
    case 161:
        return "R Shift";
    case 162:
        return "L Ctrl";
    case 163:
        return "R Ctrl";
    case 164:
        return "L Alt";
    case 165:
        return "R Alt";
    default:
        return "VK " + std::to_string(key);
    }
}

inline const std::string &KeyName(unsigned key)
{
    static const auto names = []() {
        std::array<std::string, 256> values;
        for (unsigned i = 0; i < 256; i++)
            values[i] = MakeKeyName(i);
        return values;
    }();
    return names[std::min(key, 255u)];
}

inline const std::vector<unsigned> &VisibleKeys(const Preferences &p)
{
    struct Cache
    {
        std::array<uint32_t, 8> masks{};
        bool custom = false, full = false, valid = false;
        std::vector<unsigned> keys;
    };

    static thread_local std::array<Cache, 4> cache;
    static thread_local unsigned next = 0;
    for (auto &c : cache)
        if (c.valid && c.custom == p.customKeys && c.full == p.fullKeyboard &&
            std::equal(c.masks.begin(), c.masks.end(), p.keyMask))
            return c.keys;
    auto &c = cache[next++ % cache.size()];
    c.valid = true;
    c.custom = p.customKeys;
    c.full = p.fullKeyboard;
    std::copy(std::begin(p.keyMask), std::end(p.keyMask), c.masks.begin());
    c.keys.clear();
    if (p.customKeys)
    {
        for (unsigned k = 1; k < 256; k++)
            if (p.ShowKey(k))
                c.keys.push_back(k);
    }
    else if (p.fullKeyboard)
    {
        for (unsigned k = 1; k < 256; k++)
            if ((k >= 8 && k <= 46) || (k >= 48 && k <= 90) || (k >= 96 && k <= 123) ||
                (k >= 160 && k <= 165) || (k >= 186 && k <= 192) || (k >= 219 && k <= 226))
                c.keys.push_back(k);
    }
    else
        c.keys = {'W', 'A', 'S', 'D', 16, 17, 'R', 32};
    return c.keys;
}

inline bool Held(const Frame *f, unsigned key)
{
    if (!f)
        return false;
    if (key == 16)
        return f->Key(16) || f->Key(160) || f->Key(161);
    if (key == 17)
        return f->Key(17) || f->Key(162) || f->Key(163);
    if (key == 18)
        return f->Key(18) || f->Key(164) || f->Key(165);
    return f->Key(key);
}

inline bool KnownKey(const Frame *f, unsigned key)
{
    if (!f)
        return false;
    if (f->flags & 256)
        return true;
    if (!f->legacyKeys)
        return false;
    return key == 'W' || key == 'A' || key == 'S' || key == 'D' || key == 160 || key == 161 || key == 'R' ||
           key == 16;
}

inline bool ModuleVisible(int module, const Preferences &p)
{
    return module == 0   ? p.keyboard
           : module == 1 ? p.mouse
           : module == 2 ? p.controller
           : module == 3 ? p.states
                         : false;
}

inline ImVec2 ModuleSize(int module, const Preferences &p)
{
    if (module == 0)
    {
        if (!p.customKeys && !p.fullKeyboard)
            return {360, 112 + (p.heading ? 26.f : 0.f)};
        auto n = VisibleKeys(p).size();
        int cols = n > 16 ? 16 : n > 9 ? 8 : 5;
        return {float(cols * 57 + 24), float(((n + cols - 1) / cols) * 44 + 24 + (p.heading ? 26 : 0))};
    }
    if (module == 1)
    {
        int extra = std::popcount(unsigned(p.mouseMask & 124));
        return {170, extra ? float(140 + 40 * ((extra + 2) / 3)) : 140.f};
    }
    if (module == 2)
        return {310, 210};
    if (module == 3)
        return {350, p.events ? 148.f : 112.f};
    return {1, 1};
}

using Glow = std::array<float, 263>;

inline void Module(ImDrawList *d, int module, ImVec2 a, ImVec2 b, const Frame *f, const Preferences &p,
                   const std::vector<Change> &events, double now, ImFont *font, Palette c,
                   const Glow *glow = nullptr)
{
    auto base = ModuleSize(module, p);
    float u = std::min((b.x - a.x) / base.x, (b.y - a.y) / base.y);
    if (u <= 0)
        return;
    float alpha = p.contentOpacity;
    c.background = Fade(c.background | 0xff000000u, p.opacity * alpha);
    c.ink = Fade(c.ink, alpha);
    c.muted = Fade(c.muted, alpha);
    c.accent = Fade(c.accent, alpha);
    c.edge = Fade(c.edge, alpha);
    auto at = [&](float x, float y) { return ImVec2(a.x + x * u, a.y + y * u); };
    auto text = [&](float x, float y, float size, const std::string &s, ImU32 color) {
        Label(d, font, size * u, at(x, y), color, s.c_str());
    };
    d->PushClipRect(a, b, true);
    if (p.shadow && p.background)
        d->AddRectFilled(at(3, 4), {b.x, b.y}, IM_COL32(0, 0, 0, int(90 * alpha)), 10 * u);
    if (p.background)
        d->AddRectFilled(a, b, c.background, 10 * u);
    if (p.border)
        d->AddRect(a, b, c.edge, 10 * u, 0, 1.3f * u);
    auto key = [&](float x, float y, float w, const std::string &name, bool pressed, float flash = 0) {
        if (!p.animations)
            flash = 0;
        auto lo = at(x, y), hi = at(x + w, y + 36);
        if (flash > 0)
            d->AddRect({lo.x - 2 * u, lo.y - 2 * u}, {hi.x + 2 * u, hi.y + 2 * u},
                       Fade(c.accent, flash * .5f), 6 * u, 0, 3 * u);
        if (pressed || p.keyFill > 0)
            d->AddRectFilled(lo, hi,
                             pressed ? c.accent : Fade(Mix(c.background, c.accent, .16f), p.keyFill * alpha),
                             5 * u);
        if (p.keyOutline)
            d->AddRect(lo, hi, pressed ? c.accent : Fade(c.ink, .25f), 6 * u, 0, (pressed ? 1.8f : 1.f) * u);
        float fs = 18 * u;
        auto sz = font->CalcTextSizeA(fs, FLT_MAX, 0, name.c_str());
        if (sz.x > (w - 5) * u)
        {
            fs *= ((w - 5) * u) / sz.x;
            sz = font->CalcTextSizeA(fs, FLT_MAX, 0, name.c_str());
        }
        Label(d, font, fs, {(lo.x + hi.x - sz.x) * .5f, (lo.y + hi.y - sz.y) * .5f},
              pressed ? Fade(PressedInk(c.accent), alpha) : c.ink, name.c_str());
    };
    if (module == 0)
    {
        const auto &keys = VisibleKeys(p);
        float top = p.heading ? 36.f : 10.f;
        if (p.heading)
            text(12, 8, 13, "KEYBOARD", c.muted);
        auto drawKey = [&](unsigned vk, float x, float y, float width) {
            bool known = KnownKey(f, vk);
            char unknown[48];
            const char *label = KeyName(vk).c_str();
            if (!known)
            {
                std::snprintf(unknown, sizeof(unknown), "%s --", label);
                label = unknown;
            }
            key(x, top + y, width, label, known && Held(f, vk), glow ? (*glow)[vk] : 0);
        };
        if (!p.customKeys && !p.fullKeyboard)
        {
            drawKey('W', 64, 0, 44);
            drawKey('A', 14, 44, 44);
            drawKey('S', 64, 44, 44);
            drawKey('D', 114, 44, 44);
            drawKey(16, 180, 0, 76);
            drawKey(17, 264, 0, 82);
            drawKey('R', 180, 44, 40);
            drawKey(32, 228, 44, 118);
        }
        else
        {
            int cols = keys.size() > 16 ? 16 : keys.size() > 9 ? 8 : 5;
            for (size_t i = 0; i < keys.size(); i++)
                drawKey(keys[i], 12 + int(i % cols) * 57.f, int(i / cols) * 44.f, 50);
        }
    }
    else if (module == 1)
    {
        auto lo = at(43, 12), hi = at(127, 128);
        d->AddRect(lo, hi, c.edge, 28 * u, 0, 2 * u);
        if (f && (f->mouse & 1) && (p.mouseMask & 1))
            d->AddRectFilled(at(45, 14), at(83, 65), c.accent, 15 * u);
        if (f && (f->mouse & 2) && (p.mouseMask & 2))
            d->AddRectFilled(at(87, 14), at(125, 65), c.accent, 15 * u);
        auto buttonLabel = [&](float center, const char *label, int mask) {
            auto size = font->CalcTextSizeA(17 * u, FLT_MAX, 0, label);
            text(center - size.x / (2 * u), 39.5f - size.y / (2 * u), 17, label,
                 f && (f->mouse & mask) ? Fade(PressedInk(c.accent), alpha) : c.ink);
        };
        if (p.mouseMask & 1)
            buttonLabel(64, "L", 1);
        if (p.mouseMask & 2)
            buttonLabel(106, "R", 2);
        int j = 0;
        for (int i = 2; i < 7; i++)
            if (p.mouseMask & (1 << i))
            {
                key(10 + (j % 3) * 51.f, 132 + (j / 3) * 40.f, 45, "M" + std::to_string(i + 1),
                    f && (f->mouse & (1 << i)));
                j++;
            }
        auto center = at(85, 92);
        d->AddCircle(center, 17 * u, c.edge, 0, 1.2f * u);
        if (f)
            d->AddCircleFilled({center.x + std::clamp(f->lookX / 6, -1.f, 1.f) * 14 * u,
                                center.y + std::clamp(f->lookY / 6, -1.f, 1.f) * 14 * u},
                               4 * u, c.accent);
    }
    else if (module == 2)
    {
        int device = p.controllerIndex;
        if (device < 0 && f)
        {
            for (int i = 0; i < 4; i++)
                if (f->flags & (1u << i))
                {
                    device = i;
                    break;
                }
            if (device < 0)
                for (int i = 0; i < 8; i++)
                    if (f->flags & (1u << (16 + i)))
                    {
                        device = i;
                        break;
                    }
        }
        if (f && device >= 0 && device < 4 && (f->flags & (1u << device)))
        {
            auto q = f->pads[device];
            key(12, 12, 65, "LB", q.buttons & 256);
            key(233, 12, 65, "RB", q.buttons & 512);
            const char *names[] = {"X", "Y", "A", "B"};
            const char *ps[] = {"Square", "Tri", "Cross", "Circle"};
            int bits[] = {16384, 32768, 4096, 8192};
            for (int i = 0; i < 4; i++)
                key(190 + (i % 2) * 55.f, 70 + (i / 2) * 40.f, 50, (p.symbols ? ps : names)[i],
                    q.buttons & bits[i]);
            for (int i = 0; i < 4; i++)
                key(12 + (i % 2) * 43.f, 70 + (i / 2) * 40.f, 38, std::string(1, "UDLR"[i]),
                    q.buttons & (1 << i));
            auto stick = [&](float x, short sx, short sy, bool pressed) {
                auto center = at(x, 164);
                d->AddCircle(center, 21 * u, pressed ? c.accent : c.edge, 0, 2 * u);
                d->AddCircleFilled({center.x + sx / 32768.f * 16 * u, center.y - sy / 32768.f * 16 * u},
                                   6 * u, c.accent);
            };
            stick(45, q.lx, q.ly, q.buttons & 64);
            stick(115, q.rx, q.ry, q.buttons & 128);
            key(191, 159, 50, "View", q.buttons & 32);
            key(246, 159, 50, "Menu", q.buttons & 16);
            for (int i = 0; i < 2; i++)
            {
                float x = i ? 233 : 12;
                d->AddRectFilled(at(x, 51), at(x + 65, 61), c.edge, 3 * u);
                d->AddRectFilled(at(x, 51), at(x + 65 * (i ? q.rt : q.lt) / 255.f, 61), c.accent, 3 * u);
            }
        }
        else if (f && device >= 0 && device < 8 && (f->flags & (1u << (16 + device))))
        {
            for (int i = 0; i < 20; i++)
                key(12 + (i % 5) * 58.f, 15 + (i / 5) * 44.f, 52, "B" + std::to_string(i),
                    f->legacy[device] & (1u << i));
        }
        else
            text(16, 80, 18, f ? "No controller connected" : "No recorded controller data", c.muted);
    }
    else if (module == 3)
    {
        const char *names[] = {"Sprint", "Crouch", "Radar", "Light", "Clock"};
        unsigned bits[] = {4, 512, 8, 16, 1024};
        text(12, 10, 12, "PLAYER ACTIVITY", c.muted);
        char stamp[48];
        if (p.clock == 1)
            std::snprintf(stamp, sizeof(stamp), "FRAME %u", f ? f->frame : 0);
        else if (p.clock == 2)
            std::snprintf(stamp, sizeof(stamp), "TICK %u", f ? f->tick : 0);
        else
            std::snprintf(stamp, sizeof(stamp), "%02d:%06.3f", int(now / 60), std::fmod(now, 60.));
        if (f)
            text(241, 10, 12, stamp, c.muted);
        int j = 0;
        for (int i = 0; i < 5; i++)
            if (p.stateMask & (1 << i))
            {
                bool known = f && (f->knownStates & bits[i]), on = known && (f->state & bits[i]);
                float x = 12 + (j % 3) * 110.f, y = 34 + (j / 3) * 38.f;
                double latest = -100;
                if (p.events)
                    for (const auto &e : events)
                        if (e.seconds <= now && (e.changed & bits[i]))
                            latest = std::max(latest, e.seconds);
                float pulse = p.animations ? float(std::clamp(1 - (now - latest) / .55, 0., 1.)) : 0;
                auto lo = at(x, y), hi = at(x + 104, y + 31);
                d->AddRectFilled(lo, hi, Fade(c.accent, on ? .16f : .035f), 6 * u);
                d->AddRect(lo, hi, Fade(c.accent, on ? .65f : std::max(.12f, pulse * .8f)), 6 * u, 0,
                           (1 + pulse) * u);
                d->AddCircleFilled(at(x + 11, y + 15.5f), 3 * u, on ? c.accent : Fade(c.muted, .4f));
                text(x + 22, y + 7, 15, names[i], known ? (on ? c.ink : c.muted) : Fade(c.muted, .6f));
                if (!known)
                    text(x + 86, y + 8, 12, "--", c.muted);
                j++;
            }
        if (p.events)
        {
            const Change *latest = nullptr;
            unsigned mask = 32;
            for (int i = 0; i < 5; i++)
                if (p.stateMask & (1 << i))
                    mask |= bits[i];
            for (const auto &e : events)
                if (e.seconds <= now && now - e.seconds <= 2.5 && (e.changed & mask) &&
                    (!latest || e.seconds > latest->seconds))
                    latest = &e;
            if (latest)
            {
                float fade = float(std::clamp((2.5 - (now - latest->seconds)) / .3, 0., 1.));
                if (p.clock == 1)
                    std::snprintf(stamp, sizeof(stamp), "F%u", latest->frame);
                else if (p.clock == 2)
                    std::snprintf(stamp, sizeof(stamp), "T%u", latest->tick);
                else
                    std::snprintf(stamp, sizeof(stamp), "%02d:%06.3f", int(latest->seconds / 60),
                                  std::fmod(latest->seconds, 60.));
                auto filtered = *latest;
                filtered.changed &= mask;
                auto value = std::string(stamp) + "   " + ChangeText(filtered);
                float fs = 13.f;
                auto length = font->CalcTextSizeA(fs, FLT_MAX, 0, value.c_str()).x;
                if (length > 324)
                    value = std::string(stamp) + "   " + std::to_string(std::popcount(filtered.changed)) +
                            " states changed";
                text(12, 120, fs, value, Fade(c.ink, fade));
            }
            else
                text(12, 120, 13, f ? "Movement states / observed in game" : "Waiting for recorded input",
                     c.muted);
        }
    }
    d->PopClipRect();
}

// Shared modular arrangement for the live HUD. The exporter positions each
// module independently.
inline ImVec2 ModularSize(const Preferences &p)
{
    float width = 0, top = 0;
    for (int i = 0; i < 3; i++)
        if (ModuleVisible(i, p))
        {
            auto s = ModuleSize(i, p);
            float scale = i == 1 ? p.mouseScale : 1;
            width += s.x * scale + 12;
            top = std::max(top, s.y * scale);
        }
    width = std::max(180.f, width - 12);
    if (p.states)
        width = std::max(width, 350.f);
    return {width, std::max(40.f, top + (p.states ? ModuleSize(3, p).y + 12 : 0))};
}

inline void DrawModules(ImDrawList *d, ImVec2 a, ImVec2 b, const Frame *f, const Preferences &p,
                        const std::vector<Change> &events, double now, ImFont *font, Palette c,
                        const Glow *glow = nullptr)
{
    auto base = ModularSize(p);
    float u = std::min((b.x - a.x) / base.x, (b.y - a.y) / base.y), x = 0, top = 0;
    for (int i = 0; i < 3; i++)
        if (ModuleVisible(i, p))
        {
            auto s = ModuleSize(i, p);
            top = std::max(top, s.y * (i == 1 ? p.mouseScale : 1));
        }
    for (int i = 0; i < 3; i++)
        if (ModuleVisible(i, p))
        {
            auto s = ModuleSize(i, p);
            float scale = i == 1 ? p.mouseScale : 1;
            float y = (top - s.y * scale) * .5f;
            Module(d, i, {a.x + x * u, a.y + y * u},
                   {a.x + (x + s.x * scale) * u, a.y + (y + s.y * scale) * u}, f, p, events, now, font, c,
                   glow);
            x += s.x * scale + 12;
            top = std::max(top, s.y * scale);
        }
    if (p.states)
    {
        auto size = ModuleSize(3, p);
        float offset = std::max(0.f, (base.x - size.x) * .5f);
        Module(d, 3, {a.x + offset * u, a.y + (top + 12) * u},
               {a.x + (offset + size.x) * u, a.y + (top + 12 + size.y) * u}, f, p, events, now, font, c,
               glow);
    }
}

inline ImVec2 Size(const Preferences &p)
{
    return ModularSize(p);
}

inline void Draw(ImDrawList *d, ImVec2 a, ImVec2 b, const Frame *f, const Preferences &p,
                 const std::vector<Change> &events, double now, ImFont *font, const Palette &c = {})
{
    DrawModules(d, a, b, f, p, events, now, font, c);
}
} // namespace suite_input
