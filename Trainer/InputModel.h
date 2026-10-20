#pragma once
#include "SharedProtocol.h"
#include <array>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace suite_input
{
struct Pad
{
    uint16_t buttons = 0;
    uint8_t lt = 0, rt = 0;
    int16_t lx = 0, ly = 0, rx = 0, ry = 0;
};

struct Frame
{
    double seconds = 0;
    uint32_t frame = 0, tick = 0, state = 0, flags = 0, mouse = 0, knownStates = 2047;
    bool legacyKeys = false;
    std::array<uint64_t, 4> keys{};
    std::array<uint32_t, 8> legacy{};
    std::array<Pad, 4> pads{};
    float sprint = 0, flash = 0, moveX = 0, moveY = 0, lookX = 0, lookY = 0, joyX = 0, joyY = 0;

    bool Key(unsigned vk) const
    {
        return vk < 256 && (keys[vk / 64] & (uint64_t(1) << (vk % 64)));
    }

    void Key(unsigned vk)
    {
        if (vk < 256)
            keys[vk / 64] |= uint64_t(1) << (vk % 64);
    }
};

struct Change
{
    double seconds = 0;
    uint32_t frame = 0, tick = 0, changed = 0, state = 0;
};

inline Frame Live(const GameData &d)
{
    Frame f;
    f.seconds = d.inputSeconds;
    f.frame = uint32_t(d.inputFrame);
    f.tick = uint32_t(d.inputTick);
    f.state = uint32_t(d.inputState);
    f.flags = uint32_t(d.inputFlags);
    std::memcpy(f.keys.data(), d.inputKeys, 32);
    f.mouse = uint32_t(d.inputMouse);
    for (int i = 0; i < 8; i++)
        f.legacy[i] = uint32_t(d.inputLegacy[i]);
    for (int i = 0; i < 4; i++)
    {
        auto &p = f.pads[i];
        p.buttons = uint16_t(d.inputPadButtons[i]);
        auto a = d.inputPadAxes + i * 6;
        p.lt = uint8_t(std::clamp(a[0], 0.f, 1.f) * 255);
        p.rt = uint8_t(std::clamp(a[1], 0.f, 1.f) * 255);
        p.lx = int16_t(std::clamp(a[2], -1.f, 1.f) * 32767);
        p.ly = int16_t(std::clamp(a[3], -1.f, 1.f) * 32767);
        p.rx = int16_t(std::clamp(a[4], -1.f, 1.f) * 32767);
        p.ry = int16_t(std::clamp(a[5], -1.f, 1.f) * 32767);
    }
    f.sprint = d.inputSprint;
    f.flash = d.inputFlash;
    f.moveX = d.inputGameAxes[0];
    f.moveY = d.inputGameAxes[1];
    f.lookX = d.inputGameAxes[2];
    f.lookY = d.inputGameAxes[3];
    f.joyX = d.inputGameAxes[4];
    f.joyY = d.inputGameAxes[5];
    return f;
}

inline std::vector<Change> Changes(const GameData &d)
{
    std::vector<Change> r;
    for (int i = 0; i < std::clamp(d.inputEventCount, 0, 8); i++)
    {
        Change c;
        std::memcpy(&c.seconds, d.inputEvents + i * 24, 8);
        std::memcpy(&c.frame, d.inputEvents + i * 24 + 8, 4);
        std::memcpy(&c.tick, d.inputEvents + i * 24 + 12, 4);
        std::memcpy(&c.changed, d.inputEvents + i * 24 + 16, 4);
        std::memcpy(&c.state, d.inputEvents + i * 24 + 20, 4);
        if (std::isfinite(c.seconds) && c.seconds >= 0)
            r.push_back(c);
    }
    return r;
}

inline constexpr uint32_t VisibleChanges = 4 | 8 | 16 | 32 | 512 | 1024;

inline std::string ChangeText(const Change &c)
{
    std::string s;
    for (auto pair : {std::pair{4u, "Sprint"}, std::pair{512u, "Crouch"}, std::pair{8u, "Radar"},
                      std::pair{16u, "Light"}, std::pair{32u, "Flash"}, std::pair{1024u, "Clock"}})
        if (c.changed & pair.first)
        {
            if (!s.empty())
                s += " / ";
            s += pair.second;
            s += (c.state & pair.first) ? " ON" : " OFF";
        }
    return s;
}

struct Preferences
{
    bool enabled = false, keyboard = true, mouse = true, controller = true, states = true, events = true,
         fullKeyboard = false;
    bool background = true, border = true, shadow = false, heading = false, customKeys = false,
         keyOutline = true, animations = true;
    int controllerIndex = -1, clock = 0, symbols = 0, mouseMask = 127, stateMask = 31;
    float x = .5f, y = 1, scale = 1, opacity = .86f, contentOpacity = 1, mouseScale = 1, keyFill = .18f;
    uint32_t keyMask[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    bool operator==(const Preferences &) const = default;

    Preferences()
    {
        for (unsigned key : {87u, 65u, 83u, 68u, 16u, 17u, 82u, 32u, 18u})
            ShowKey(key, true);
    }

    bool ShowKey(unsigned key) const
    {
        return key < 256 && (keyMask[key / 32] & (1u << (key % 32)));
    }

    void ShowKey(unsigned key, bool value)
    {
        if (key < 256)
        {
            auto bit = 1u << (key % 32);
            if (value)
                keyMask[key / 32] |= bit;
            else
                keyMask[key / 32] &= ~bit;
        }
    }

    void Validate()
    {
        auto safe = [](float v, float lo, float hi, float def) {
            return std::isfinite(v) ? std::clamp(v, lo, hi) : def;
        };
        x = safe(x, 0, 1, .5f);
        y = safe(y, 0, 1, 1);
        scale = safe(scale, .5f, 2.5f, 1);
        opacity = safe(opacity, 0, 1, .86f);
        contentOpacity = safe(contentOpacity, 0, 1, 1);
        mouseScale = safe(mouseScale, .5f, 2.5f, 1);
        keyFill = safe(keyFill, 0, 1, .18f);
        controllerIndex = std::clamp(controllerIndex, -1, 7);
        clock = std::clamp(clock, 0, 2);
        symbols = std::clamp(symbols, 0, 1);
        mouseMask &= 127;
        stateMask &= 31;
    }

    void Encode(std::ostream &o) const
    {
        for (auto p :
             {std::pair{"enabled", enabled}, std::pair{"keyboard", keyboard}, std::pair{"mouse", mouse},
              std::pair{"controller", controller}, std::pair{"states", states}, std::pair{"events", events},
              std::pair{"fullKeyboard", fullKeyboard}, std::pair{"background", background},
              std::pair{"border", border}, std::pair{"shadow", shadow}, std::pair{"heading", heading},
              std::pair{"customKeys", customKeys}, std::pair{"keyOutline", keyOutline},
              std::pair{"animations", animations}})
            o << "input." << p.first << '=' << p.second << '\n';
        o << "input.controllerIndex=" << controllerIndex << "\ninput.clock=" << clock
          << "\ninput.symbols=" << symbols << "\ninput.x=" << x << "\ninput.y=" << y
          << "\ninput.scale=" << scale << "\ninput.opacity=" << opacity
          << "\ninput.contentOpacity=" << contentOpacity << "\ninput.mouseScale=" << mouseScale
          << "\ninput.keyFill=" << keyFill << "\ninput.mouseMask=" << mouseMask
          << "\ninput.stateMask=" << stateMask << '\n';
        for (int i = 0; i < 8; i++)
            o << "input.keys" << i << '=' << keyMask[i] << '\n';
    }

    void Decode(const std::string &k, double v)
    {
        if (!std::isfinite(v))
            return;
        if (k == "animations")
            animations = v == 1;
        else if (k == "enabled")
            enabled = v == 1;
        else if (k == "keyboard")
            keyboard = v == 1;
        else if (k == "mouse")
            mouse = v == 1;
        else if (k == "controller")
            controller = v == 1;
        else if (k == "states")
            states = v == 1;
        else if (k == "events")
            events = v == 1;
        else if (k == "fullKeyboard")
            fullKeyboard = v == 1;
        else if (k == "background")
            background = v == 1;
        else if (k == "border")
            border = v == 1;
        else if (k == "shadow")
            shadow = v == 1;
        else if (k == "heading")
            heading = v == 1;
        else if (k == "customKeys")
            customKeys = v == 1;
        else if (k == "keyOutline")
            keyOutline = v == 1;
        else if (k == "controllerIndex")
            controllerIndex = int(std::clamp(v, -1., 7.));
        else if (k == "clock")
            clock = int(std::clamp(v, 0., 2.));
        else if (k == "symbols")
            symbols = int(std::clamp(v, 0., 1.));
        else if (k == "x")
            x = float(v);
        else if (k == "y")
            y = float(v);
        else if (k == "scale")
            scale = float(v);
        else if (k == "opacity")
            opacity = float(v);
        else if (k == "contentOpacity")
            contentOpacity = float(v);
        else if (k == "mouseScale")
            mouseScale = float(v);
        else if (k == "keyFill")
            keyFill = float(v);
        else if (k == "mouseMask")
            mouseMask = int(std::clamp(v, 0., 127.));
        else if (k == "stateMask")
            stateMask = int(std::clamp(v, 0., 31.));
        else if (k.size() == 5 && k.rfind("keys", 0) == 0 && k[4] >= '0' && k[4] <= '7')
            keyMask[k[4] - '0'] = uint32_t(std::clamp(v, 0., 4294967295.));
    }
};

inline Preferences preferences;
} // namespace suite_input
