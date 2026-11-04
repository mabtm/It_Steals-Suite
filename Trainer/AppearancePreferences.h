#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <ostream>
#include <string>
#include "FontCatalog.h"

// Retain the original indices for existing settings; discovered faces use
// stable catalog IDs so adding/removing a font cannot change a selection.
inline constexpr const char *kSuiteFonts[] = {
    "Segoe UI", "Segoe UI Semibold", "Bahnschrift", "Arial",    "Verdana",
    "Tahoma",   "Trebuchet MS",      "Calibri",     "Consolas", "Georgia"};
inline constexpr const wchar_t *kSuiteFontFiles[] = {
    L"segoeui.ttf", L"seguisb.ttf", L"bahnschrift.ttf", L"arial.ttf",   L"verdana.ttf",
    L"tahoma.ttf",  L"trebuc.ttf",  L"calibri.ttf",     L"consola.ttf", L"georgia.ttf"};
inline constexpr int kSuiteFontCount = 10;

struct AppearancePreferences
{
    enum Color
    {
        Button,
        Animation,
        Outline,
        HudBackground,
        HudText,
        HudAccent,
        MapBackground,
        MapWalls,
        MapPlayer,
        MapRoute,
        MapCrystal,
        Count
    };

    using RGB = std::array<float, 3>;
    std::array<RGB, Count> colors = {RGB{.10f, .18f, .20f},    RGB{.37f, .83f, .77f}, RGB{.20f, .29f, .32f},
                                     RGB{.048f, .063f, .074f}, RGB{.91f, .95f, .95f}, RGB{.37f, .83f, .77f},
                                     RGB{.045f, .065f, .08f},  RGB{.48f, .60f, .65f}, RGB{.55f, .92f, .94f},
                                     RGB{.37f, .83f, .77f},    RGB{1, .72f, .39f}};
    bool hudFollowsTheme = true, introBlur = true;
    int menuFont = 0, hudFont = 0, timerFont = 8;
    std::array<std::string, 3> fontIds;

    static float Luminance(const RGB &c)
    {
        auto linear = [](float v) {
            return v <= .04045f ? v / 12.92f : std::pow((v + .055f) / 1.055f, 2.4f);
        };
        return .2126f * linear(c[0]) + .7152f * linear(c[1]) + .0722f * linear(c[2]);
    }

    static float Contrast(const RGB &a, const RGB &b)
    {
        float x = Luminance(a), y = Luminance(b);
        return (std::max(x, y) + .05f) / (std::min(x, y) + .05f);
    }

    void Validate()
    {
        AppearancePreferences defaults;
        for (int c = 0; c < Count; c++)
            for (int i = 0; i < 3; i++)
                colors[c][i] =
                    std::isfinite(colors[c][i]) ? std::clamp(colors[c][i], 0.f, 1.f) : defaults.colors[c][i];
        for (int *f : {&menuFont, &hudFont, &timerFont})
            *f = std::clamp(*f, 0, kSuiteFontCount - 1);
        for (auto &id : fontIds)
            if (!suite_fonts::ValidId(id))
                id.clear();
    }

    void Encode(std::ostream &out) const
    {
        out << "appearance.menuFont=" << menuFont << "\nappearance.hudFont=" << hudFont
            << "\nappearance.timerFont=" << timerFont << "\nappearance.hudFollowsTheme=" << hudFollowsTheme
            << "\nappearance.introBlur=" << introBlur << '\n';
        for (size_t slot = 0; slot < fontIds.size(); ++slot)
            out << "appearance.fontId" << slot << '=' << fontIds[slot] << '\n';
        for (int c = 0; c < Count; c++)
            for (int i = 0; i < 3; i++)
                out << "appearance.color" << c << '.' << i << '=' << colors[c][i] << '\n';
    }

    void Decode(const std::string &key, double v)
    {
        if (!std::isfinite(v))
            return;
        if (key == "menuFont" || key == "hudFont" || key == "timerFont")
        {
            int n = int(std::clamp(v, 0., double(kSuiteFontCount - 1)));
            if (key == "menuFont")
                menuFont = n;
            else if (key == "hudFont")
                hudFont = n;
            else
                timerFont = n;
        }
        else if (key == "hudFollowsTheme" && (v == 0 || v == 1))
            hudFollowsTheme = v == 1;
        else if (key == "introBlur" && (v == 0 || v == 1))
            introBlur = v == 1;
        else
            for (int c = 0; c < Count; c++)
                for (int i = 0; i < 3; i++)
                    if (key == "color" + std::to_string(c) + '.' + std::to_string(i))
                        colors[c][i] = float(std::clamp(v, 0., 1.));
    }
};
