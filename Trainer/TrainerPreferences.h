#pragma once
#include "AppearancePreferences.h"
#include "ExperienceModel.h"
#include "InspectorPresentation.h"
#include "InputModel.h"
#include "MapLevels.h"
#include "MinimapOptions.h"
#include "OverlayConfiguration.h"
#include "OverlayPolicy.h"
#include "SeedSearchOptions.h"
#include "ReplayPreferences.h"
#include "TrainerHotkeys.h"
#include <algorithm>
#include <cmath>
#include <locale>
#include <sstream>
#include <string>

struct TrainerPreferences
{
    static constexpr int FormatVersion = 21;
    OverlayLayout hud{false}, speedrun{true};
    suite_input::Preferences input;
    MinimapOptions map;
    PlannerPreferences planner;
    ExperiencePreferences experience;
    inspector_fx::Preferences inspector;
    AppearancePreferences appearance;
    ReplayPreferences replay;
    suite_keys::Preferences hotkeys;
    bool reports = false;
    bool autoStartGame = true;
    bool skipGameIntro = true;
    bool captureOverlay = false;
    bool rawSpawn = false;
    bool compact = false, minimap = true, path = false, lockAspect = true;
    bool player = true, crystals = true, monster = true, visuals = true;
    int width = 250, height = 250, fps = 60, level = 0, seed = 0;
    int frameLimitMode = 0, gameFrameLimit = 240, vSyncMode = 0;
    float opacity = 1;
    float background[4] = {.055f, .067f, .086f, 1};
    float text[4] = {.94f, .96f, .98f, 1};
    float header[4] = {.48f, .83f, 1.f, 1};
    float accent[4] = {.98f, .74f, .46f, 1};

    void Validate()
    {
        input.Validate();
        experience.Validate();
        inspector.Validate();
        appearance.Validate();
        replay.Validate();
        hotkeys.Validate();
        hud.Validate();
        speedrun.Validate();
        map.Validate();
        planner.Validate();
        width = std::clamp(width, 100, 500);
        height = std::clamp(height, 100, 500);
        if (lockAspect)
            height = width;
        fps = ClampRenderRate(fps);
        level = std::clamp(level, 0, kMapLevelCount - 1);
        frameLimitMode = std::clamp(frameLimitMode, 0, 2);
        vSyncMode = std::clamp(vSyncMode, 0, 2);
        gameFrameLimit = std::clamp(gameFrameLimit, 30, 1000);
        opacity = std::isfinite(opacity) ? std::clamp(opacity, .1f, 1.f) : 1.f;
        for (float *color : {background, text, header, accent})
            for (int i = 0; i < 4; i++)
                color[i] = std::isfinite(color[i]) ? std::clamp(color[i], 0.f, 1.f) : 1.f;
        background[3] = 1; // Opaque readable menu; minimap has separate opacity.
    }

    std::string Encode() const
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out.precision(9);
        out << "version=" << FormatVersion << "\nframeLimitMode=" << frameLimitMode
            << "\ngameFrameLimit=" << gameFrameLimit << "\nvSyncMode=" << vSyncMode << "\nreports=" << reports
            << "\nrawSpawn=" << rawSpawn << "\ncompact=" << compact << "\nminimap=" << minimap
            << "\npath=" << path << "\nlockAspect=" << lockAspect << "\nplayer=" << player
            << "\ncrystals=" << crystals << "\nmonster=" << monster << "\nvisuals=" << visuals
            << "\nwidth=" << width << "\nheight=" << height << "\nfps=" << fps << "\nlevel=" << level
            << "\nseed=" << seed << "\nautoStartGame=" << autoStartGame
            << "\ncaptureOverlay=" << captureOverlay << "\nskipGameIntro=" << skipGameIntro
            << "\nopacity=" << opacity << '\n';
        const float *colors[] = {background, text, header, accent};
        const char *names[] = {"background", "text", "header", "accent"};
        for (int c = 0; c < 4; c++)
            for (int i = 0; i < 4; i++)
                out << names[c] << i << '=' << colors[c][i] << '\n';
        input.Encode(out);
        replay.Encode(out);
        hotkeys.Encode(out);
        experience.Encode(out);
        inspector.Encode(out);
        appearance.Encode(out);
        hud.Encode(out, "hud.");
        speedrun.Encode(out, "speedrun.");
        map.Encode(out);
        planner.Encode(out);
        return out.str();
    }

    static TrainerPreferences Decode(const std::string &input)
    {
        int version = 0;
        std::istringstream versions(input);
        std::string line;
        while (std::getline(versions, line))
            if (line.rfind("version=", 0) == 0)
            {
                std::istringstream value(line.substr(8));
                value >> version;
            }
        TrainerPreferences result;
        std::istringstream lines(input);
        while (std::getline(lines, line))
        {
            auto split = line.find('=');
            if (split == std::string::npos)
                continue;
            std::string key = line.substr(0, split);
            if (key == "appearance.fontId0" || key == "appearance.fontId1" || key == "appearance.fontId2")
            {
                auto id = line.substr(split + 1);
                if (suite_fonts::ValidId(id))
                    result.appearance.fontIds[size_t(key.back() - '0')] = std::move(id);
                continue;
            }
            std::istringstream number(line.substr(split + 1));
            number.imbue(std::locale::classic());
            double value = 0;
            if (!(number >> value) || !std::isfinite(value))
                continue;
            number >> std::ws;
            if (!number.eof())
                continue;
            int integer = static_cast<int>(std::clamp(value, -2147483648., 2147483647.));
            bool flag = value == 1;
            if (key.rfind("input.", 0) == 0)
                result.input.Decode(key.substr(6), value);
            else if (key.rfind("replay.", 0) == 0)
                result.replay.Decode(key.substr(7), value);
            else if (key.rfind("hotkey.", 0) == 0)
                result.hotkeys.Decode(key.substr(7), integer);
            else if (key.rfind("appearance.", 0) == 0)
                result.appearance.Decode(key.substr(11), value);
            else if (key.rfind("inspector.", 0) == 0)
                result.inspector.Decode(key.substr(10), value);
            else if (key.rfind("experience.", 0) == 0)
                result.experience.Decode(key.substr(11), value);
            else if (key.rfind("map.", 0) == 0)
                result.map.Decode(key.substr(4), value);
            else if (key.rfind("planner.", 0) == 0)
                result.planner.Decode(key.substr(8), value);

            else if (key.rfind("hud.", 0) == 0)
            {
                auto field = key.substr(4);
                if (version < 12 && field == "show25")
                    continue;
                result.hud.Decode(field,
                                  version < 12 && field.rfind("order", 0) == 0 && integer >= 25 ? -1 : value);
            }
            else if (key.rfind("speedrun.", 0) == 0)
            {
                auto field = key.substr(9);
                if (version < 12 && field == "show25")
                    continue;
                result.speedrun.Decode(
                    field, version < 12 && field.rfind("order", 0) == 0 && integer >= 25 ? -1 : value);
            }
            else if (key == "autoStartGame" && (value == 0 || value == 1))
                result.autoStartGame = flag;
            else if (key == "captureOverlay" && (value == 0 || value == 1))
                result.captureOverlay = flag;
            else if (key == "skipGameIntro" && (value == 0 || value == 1))
                result.skipGameIntro = flag;
            else if (key == "compact")
                result.compact = flag;
            else if (key == "frameLimitMode")
                result.frameLimitMode = integer;
            else if (key == "gameFrameLimit")
                result.gameFrameLimit = integer;
            else if (key == "vSyncMode")
                result.vSyncMode = integer;
            else if (key == "reports" && (value == 0 || value == 1))
                result.reports = flag;
            else if (key == "rawSpawn" && (value == 0 || value == 1))
                result.rawSpawn = flag;
            else if (key == "minimap")
                result.minimap = flag;
            else if (key == "path")
                result.path = flag;
            else if (key == "lockAspect")
                result.lockAspect = flag;
            else if (key == "player")
                result.player = flag;
            else if (key == "crystals")
                result.crystals = flag;
            else if (key == "monster")
                result.monster = flag;
            else if (key == "visuals")
                result.visuals = flag;
            else if (key == "width")
                result.width = integer;
            else if (key == "height")
                result.height = integer;
            else if (key == "fps")
                result.fps = integer;
            else if (key == "level")
                result.level = integer;
            else if (key == "seed")
                result.seed = integer;
            else if (key == "opacity")
                result.opacity = static_cast<float>(std::clamp(value, .1, 1.));
            else
            {
                float *colors[] = {result.background, result.text, result.header, result.accent};
                const char *names[] = {"background", "text", "header", "accent"};
                for (int c = 0; c < 4; c++)
                    for (int i = 0; i < 4; i++)
                        if (key == std::string(names[c]) + std::to_string(i))
                            colors[c][i] = static_cast<float>(std::clamp(value, 0., 1.));
            }
        }
        if (version < 13)
        {
            const float old[4][4] = {
                {.065f, .075f, .095f, 1}, {.9f, .93f, .97f, 1}, {.34f, .70f, .95f, 1}, {1, .64f, .29f, 1}};
            TrainerPreferences fresh;
            float *colors[] = {result.background, result.text, result.header, result.accent};
            const float *defaults[] = {fresh.background, fresh.text, fresh.header, fresh.accent};
            for (int k = 0; k < 4; k++)
            {
                bool same = true;
                for (int i = 0; i < 4; i++)
                    same &= std::abs(colors[k][i] - old[k][i]) < .00001f;
                if (same)
                    std::copy(defaults[k], defaults[k] + 4, colors[k]);
            }
        }
        if (version < 21)
        {
            // Upgrade only the complete untouched interface palette. A user
            // who customized even one color keeps their entire chosen theme.
            const float old[4][4] = {{.048f, .063f, .074f, 1}, {.91f, .95f, .95f, 1},
                                     {.37f, .83f, .77f, 1}, {1, .69f, .38f, 1}};
            const AppearancePreferences::RGB oldControls[] = {
                {.10f, .18f, .20f}, {.37f, .83f, .77f}, {.20f, .29f, .32f}};
            float *colors[] = {result.background, result.text, result.header, result.accent};
            bool untouched = true;
            for (int c = 0; c < 4; ++c)
                for (int i = 0; i < 4; ++i)
                    untouched &= std::abs(colors[c][i] - old[c][i]) < .00001f;
            for (int c = 0; c < 3; ++c)
                for (int i = 0; i < 3; ++i)
                    untouched &= std::abs(result.appearance.colors[c][i] - oldControls[c][i]) < .00001f;
            if (untouched)
            {
                TrainerPreferences fresh;
                const float *defaults[] = {fresh.background, fresh.text, fresh.header, fresh.accent};
                for (int c = 0; c < 4; ++c)
                    std::copy(defaults[c], defaults[c] + 4, colors[c]);
                for (int c = 0; c < 3; ++c)
                    result.appearance.colors[c] = fresh.appearance.colors[c];
            }
        }
        if (version < 15)
        {
            if (result.hud.width == 240)
                result.hud.width = 320;
            if (result.speedrun.width == 300)
                result.speedrun.width = 360;
        }
        result.planner.calculateRoutes = true;
        result.planner.minimapRoute = false;
        if (version < 17)
            result.input.mouse = result.input.keyboard;
        if (version < 18)
            result.input.states = result.input.states || result.input.events;
        result.Validate();
        return result;
    }
};
