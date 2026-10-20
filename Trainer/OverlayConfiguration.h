#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

enum class OverlayField : int
{
    Fps,
    Timer,
    Seed,
    SeedType,
    Attempts,
    Resets,
    Completed,
    CompletionRate,
    RunNumber,
    SessionTime,
    PersonalBest,
    AverageRun,
    AverageReset,
    Progress,
    Furthest,
    AttemptsToday,
    CompletedToday,
    ResetsToday,
    SessionAttempts,
    SessionCompleted,
    SessionResets,
    SessionRate,
    Deaths,
    Speed,
    Mode,
    Coordinates,
    Rotation,
    PlayerState,
    MonsterState,
    MonsterSpeed,
    Noise,
    Health,
    Count
};
inline constexpr int kOverlayFields = static_cast<int>(OverlayField::Count);
inline constexpr const char *kOverlayFieldNames[] = {"Game FPS",
                                                     "Run timer",
                                                     "Seed",
                                                     "Seed type",
                                                     "Total attempts",
                                                     "Total resets",
                                                     "Completed runs",
                                                     "Completion rate",
                                                     "Run number",
                                                     "Session time",
                                                     "Local PB",
                                                     "Average completed run",
                                                     "Average reset point",
                                                     "Run progress",
                                                     "Furthest progress",
                                                     "Attempts today",
                                                     "Completed today",
                                                     "Resets today",
                                                     "Session attempts",
                                                     "Session completed",
                                                     "Session resets",
                                                     "Session success rate",
                                                     "Deaths",
                                                     "Movement speed",
                                                     "Level / mode",
                                                     "Coordinates (X / Y / Z)",
                                                     "Rotation",
                                                     "Player state",
                                                     "Monster state",
                                                     "Monster speed",
                                                     "Player noise",
                                                     "Health"};

struct OverlayLayout
{
    bool enabled = true;
    bool progressBar = true;
    std::array<bool, kOverlayFields> visible{};
    std::array<int, kOverlayFields> order{};
    float scale = 1, opacity = .86f, width = 260, padding = 12, gap = 6;
    float x = .02f, y = .02f; // Fraction of remaining viewport after panel size.
    int anchor = 0;           // top left/right, bottom left/right, custom

    explicit OverlayLayout(bool speedrun = false)
    {
        for (int i = 0; i < 64; i++)
            decodedOrder[i] = i;
        for (int i = 0; i < kOverlayFields; i++)
        {
            visible[i] = false;
            order[i] = i;
        }
        auto show = [&](std::initializer_list<OverlayField> fields) {
            int i = 0;
            for (auto field : fields)
            {
                visible[static_cast<int>(field)] = true;
                order[i++] = static_cast<int>(field);
            }
            for (int f = 0; f < kOverlayFields; f++)
                if (!visible[f])
                    order[i++] = f;
        };
        if (speedrun)
            show({OverlayField::Mode, OverlayField::Timer, OverlayField::PersonalBest,
                  OverlayField::RunNumber, OverlayField::SeedType, OverlayField::Seed, OverlayField::Attempts,
                  OverlayField::Resets, OverlayField::Completed, OverlayField::CompletionRate});
        else
            show({OverlayField::Mode, OverlayField::Progress, OverlayField::Speed, OverlayField::Fps,
                  OverlayField::PlayerState, OverlayField::Rotation});
        width = speedrun ? 360.f : 320.f;
        anchor = speedrun ? 0 : 1;
        scale = 1;
        opacity = .86f;
        padding = 12;
        gap = 6;
        x = y = .02f;
        for (int i = 0; i < kOverlayFields; i++)
            decodedOrder[i] = order[i];
    }

    std::array<int, 64> decodedOrder{};
    bool hasDecodedOrder = false;

    void Validate()
    {
        if (hasDecodedOrder)
        {
            std::array<bool, kOverlayFields> included{};
            int n = 0;
            for (int field : decodedOrder)
                if (field >= 0 && field < kOverlayFields && !included[field])
                {
                    order[n++] = field;
                    included[field] = true;
                }
            for (int field = 0; field < kOverlayFields; field++)
                if (!included[field])
                    order[n++] = field;
            hasDecodedOrder = false;
        }
        auto sane = [](float value, float low, float high, float fallback) {
            return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
        };
        scale = sane(scale, .85f, 1.75f, 1);
        opacity = sane(opacity, .25f, 1, .86f);
        width = sane(width, 220, 560, 320);
        padding = sane(padding, 6, 24, 12);
        gap = sane(gap, 2, 14, 6);
        x = sane(x, 0, 1, .02f);
        y = sane(y, 0, 1, .02f);
        anchor = std::clamp(anchor, 0, 4);
        std::array<bool, kOverlayFields> used{};
        for (int &field : order)
        {
            if (field < 0 || field >= kOverlayFields || used[field])
            {
                field = 0;
                while (used[field])
                    field++;
            }
            used[field] = true;
        }
    }

    void Encode(std::ostream &out, const char *prefix) const
    {
        out << prefix << "enabled=" << enabled << '\n';
        out << prefix << "progressBar=" << progressBar << '\n';
        out << prefix << "scale=" << scale << '\n'
            << prefix << "opacity=" << opacity << '\n'
            << prefix << "width=" << width << '\n'
            << prefix << "padding=" << padding << '\n'
            << prefix << "gap=" << gap << '\n'
            << prefix << "anchor=" << anchor << '\n'
            << prefix << "x=" << x << '\n'
            << prefix << "y=" << y << '\n';
        for (int i = 0; i < kOverlayFields; i++)
            out << prefix << "show" << i << '=' << visible[i] << '\n'
                << prefix << "order" << i << '=' << order[i] << '\n';
    }

    void Decode(const std::string &key, double value)
    {
        int n = static_cast<int>(std::clamp(value, -2147483648., 2147483647.));
        if (key == "enabled")
            enabled = value == 1;
        else if (key == "progressBar" && (value == 0 || value == 1))
            progressBar = value == 1;
        else if (key == "anchor")
            anchor = n;
        else if (key == "scale")
            scale = static_cast<float>(std::clamp(value, .85, 1.75));
        else if (key == "opacity")
            opacity = static_cast<float>(std::clamp(value, .25, 1.));
        else if (key == "width")
            width = static_cast<float>(std::clamp(value, 220., 560.));
        else if (key == "padding")
            padding = static_cast<float>(std::clamp(value, 6., 24.));
        else if (key == "gap")
            gap = static_cast<float>(std::clamp(value, 2., 14.));
        else if (key == "x")
            x = static_cast<float>(std::clamp(value, 0., 1.));
        else if (key == "y")
            y = static_cast<float>(std::clamp(value, 0., 1.));
        else
            for (int i = 0; i < 64; i++)
            {
                if (i < kOverlayFields && key == "show" + std::to_string(i))
                    visible[i] = value == 1;
                if (key == "order" + std::to_string(i))
                {
                    if (!hasDecodedOrder)
                        decodedOrder.fill(-1);
                    decodedOrder[i] = n;
                    hasDecodedOrder = true;
                }
            }
    }
};
