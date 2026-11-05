#pragma once
#include <algorithm>
#include <cmath>
#include <ostream>
#include <string>
#include <array>
#include <cstdint>
#include <vector>

struct ExperiencePreferences
{
    bool sounds = true, motion = true, launchGraphic = true;
    float volume = .16f;

    void Validate()
    {
        volume = std::isfinite(volume) ? std::clamp(volume, 0.f, 1.f) : .16f;
    }

    void Encode(std::ostream &out) const
    {
        out << "experience.sounds=" << sounds << "\nexperience.motion=" << motion
            << "\nexperience.launchGraphic=" << launchGraphic << "\nexperience.volume=" << volume << '\n';
    }

    void Decode(const std::string &key, double value)
    {
        if (key == "volume")
            volume = float(std::clamp(value, 0., 1.));
        else if (value == 0 || value == 1)
        {
            if (key == "sounds")
                sounds = value == 1;
            else if (key == "motion")
                motion = value == 1;
            else if (key == "launchGraphic")
                launchGraphic = value == 1;
        }
    }
};

namespace suite_ui
{
enum class Tone
{
    None,
    Click,
    Tab,
    On,
    Off,
    Confirm,
    Complete,
    PersonalBest,
    InspectorOpen,
    InspectorSelect,
    InspectorPin,
    InspectorClose,
    Hover,
    Warning
};

inline int ToneImportance(Tone tone)
{
    switch (tone)
    {
    case Tone::PersonalBest: return 6;
    case Tone::Complete: return 5;
    case Tone::Confirm:
    case Tone::Warning: return 4;
    case Tone::On:
    case Tone::Off:
    case Tone::InspectorPin: return 3;
    case Tone::Tab:
    case Tone::InspectorOpen:
    case Tone::InspectorClose: return 2;
    case Tone::Click:
    case Tone::InspectorSelect: return 1;
    case Tone::Hover: return 0;
    default: return -1;
    }
}

// UI events can arrive more often than audio is useful. This clock-based gate
// keeps pointer travel quiet and repeated warnings restrained at any frame rate.
class ToneCadence
{
    Tone last = Tone::None;
    double lastAt = -10, hoverAt = -10, actionAt = -10, warningAt = -10, bestAt = -10;

  public:
    bool Accept(Tone tone, double now)
    {
        if (ToneImportance(tone) < 0 || !std::isfinite(now) || now < 0)
            return false;
        if (now < lastAt)
            Reset();
        if (tone == last && now - lastAt < .035)
            return false;
        if ((tone == Tone::Warning && now - warningAt < .35) ||
            (tone == Tone::PersonalBest && now - bestAt < .5))
            return false;
        if (tone == Tone::Hover && (now - hoverAt < .10 || now - actionAt < .08))
            return false;
        last = tone;
        lastAt = now;
        (tone == Tone::Hover ? hoverAt : actionAt) = now;
        if (tone == Tone::Warning) warningAt = now;
        if (tone == Tone::PersonalBest) bestAt = now;
        return true;
    }

    void Reset()
    {
        last = Tone::None;
        lastAt = hoverAt = actionAt = warningAt = bestAt = -10;
    }
};

inline bool AudioAllowed(const ExperiencePreferences &p, bool menu, bool focused, bool running)
{
    return p.sounds && p.volume > 0 && menu && focused && !running;
}

inline float Ease(float x)
{
    x = std::clamp(x, 0.f, 1.f);
    return 1 - (1 - x) * (1 - x) * (1 - x);
}

// Fixed memory, four-way associative animation table. No allocation per widget.
class MotionTable
{
    struct Entry
    {
        uint32_t id = 0;
        int frame = -2;
        float from = 0, target = 0;
        double began = 0;
        bool used = false;
    };

    std::array<Entry, 512> entries{};

  public:
    float Value(uint32_t id, float target, int frame, double now, bool enabled, bool appear = false,
                float duration = .16f)
    {
        // Invalid timing and reduced motion snap to the requested value. Keep
        // that value as the next transition's origin when motion is re-enabled.
        target = std::isfinite(target) ? target : 0;
        bool animate = enabled && std::isfinite(now) && std::isfinite(duration) && duration > 0;
        now = std::isfinite(now) ? now : 0;
        size_t base = (id % 128) * 4;
        Entry *slot = nullptr;
        for (size_t i = base; i < base + 4; i++)
            if (entries[i].used && entries[i].id == id)
            {
                slot = &entries[i];
                break;
            }
        if (!slot)
        {
            slot = &entries[base];
            for (size_t i = base; i < base + 4; i++)
                if (!entries[i].used || entries[i].frame < slot->frame)
                    slot = &entries[i];
            *slot = {id, frame, appear && animate ? 0 : target, target, now, true};
        }
        auto &e = *slot;
        if (!animate)
        {
            e = {id, frame, target, target, now, true};
            return target;
        }
        if (frame < e.frame || int64_t(frame) > int64_t(e.frame) + 1 || now < e.began)
        {
            e.from = appear ? 0 : target;
            e.target = target;
            e.began = now;
        }
        float current = e.from + (e.target - e.from) * Ease(float((now - e.began) / duration));
        if (e.target != target)
        {
            e.from = current;
            e.target = target;
            e.began = now;
        }
        e.frame = frame;
        return e.from + (e.target - e.from) * Ease(float((now - e.began) / duration));
    }

    void Reset()
    {
        entries = {};
    }

    static constexpr size_t Capacity()
    {
        return 512;
    }
};

struct Lifecycle
{
    static constexpr double StartupDuration = 2.6, ExitDuration = .24;
    bool considered = false, presented = false;
    double start = -1, exit = -1, pausedAt = -1;

    double StartupAge(double now) const
    {
        return presented ? std::max(0., (pausedAt >= 0 ? pausedAt : now) - start) : 0;
    }

    void Consider(double now, bool visible, const ExperiencePreferences &p, bool run)
    {
        // Loading, focus changes and an already active run defer the intro;
        // they must not consume its one opportunity to appear.
        if (!visible || run || !p.launchGraphic)
        {
            if (presented && pausedAt < 0)
                pausedAt = now;
            return;
        }
        if (pausedAt >= 0)
        {
            start += std::max(0., now - pausedAt);
            pausedAt = -1;
        }
        if (!considered)
        {
            considered = true;
            start = now;
        }
    }

    bool Startup(double now, const ExperiencePreferences &p, bool run) const
    {
        return p.launchGraphic && !run && considered && StartupAge(now) < StartupDuration;
    }

    bool WantsStartup(double now, const ExperiencePreferences &p, bool run) const
    {
        return p.launchGraphic && !run && (!considered || Startup(now, p, false));
    }

    void Presented(double now)
    {
        if (considered && !presented)
        {
            presented = true;
            start = now;
        }
    }

    bool BeginExit(double now, const ExperiencePreferences &p, bool visible, bool run)
    {
        if (exit >= 0)
            return true;
        if (!p.motion || !p.launchGraphic || !visible || run)
            return false;
        exit = now;
        return true;
    }

    bool Exiting() const
    {
        return exit >= 0;
    }

    bool Done(double now) const
    {
        return exit >= 0 && now - exit >= ExitDuration;
    }
};

// Original short, quiet PCM cues: a soft transient for actions, a light glass
// accent for navigation and gently rising notes for completion. Gain affects
// only these samples. No sound assets, system mixer changes or random noise.
inline std::vector<int16_t> ToneSamples(Tone kind, float volume, int rate = 44100)
{
    if (ToneImportance(kind) < 0 || rate < 8000 || rate > 192000 || !std::isfinite(volume))
        return {};
    volume = std::clamp(volume, 0.f, 1.f);
    double duration = kind == Tone::Hover             ? .020
                      : kind == Tone::InspectorOpen   ? .076
                      : kind == Tone::InspectorSelect ? .040
                      : kind == Tone::InspectorPin    ? .088
                      : kind == Tone::InspectorClose  ? .06
                      : kind == Tone::PersonalBest    ? .195
                      : kind == Tone::Complete || kind == Tone::Warning ? .128
                      : kind == Tone::Confirm         ? .096
                      : kind == Tone::On || kind == Tone::Off ? .064
                      : kind == Tone::Tab             ? .048
                                                      : .032;
    std::vector<int16_t> pcm(size_t(std::ceil(duration * rate)) + 1, 0);
    constexpr double pi = 3.14159265358979323846;
    auto note = [&](double at, double frequency, double length, double gain, double glide = 0) {
        for (size_t i = size_t(at * rate); i < pcm.size(); i++)
        {
            double t = double(i) / rate - at;
            if (t < 0 || t >= length)
                continue;
            double edge = std::sin(pi * t / length);
            double envelope = edge * edge * std::exp(-t * 11);
            double phase = 2 * pi * (frequency * t + .5 * glide * t * t / length);
            double wave = .78 * std::sin(phase) + .16 * std::sin(phase * 2) + .06 * std::sin(phase * 3);
            int sample = int(pcm[i]) + int(32767 * .16 * volume * gain * envelope * wave);
            pcm[i] = int16_t(std::clamp(sample, -32767, 32767));
        }
    };
    if (kind == Tone::Hover)
        note(0, 960, .019, .28, 90);
    else if (kind == Tone::Click)
        note(0, 390, .031, .82, 110);
    else if (kind == Tone::Tab)
    {
        note(0, 600, .032, .64);
        note(.017, 900, .030, .32);
    }
    else if (kind == Tone::On || kind == Tone::Off)
    {
        bool on = kind == Tone::On;
        note(0, on ? 480 : 640, .037, .66);
        note(.026, on ? 720 : 420, .037, .46);
    }
    else if (kind == Tone::Warning)
    {
        note(0, 350, .054, .58);
        note(.068, 300, .059, .48);
    }
    else if (kind == Tone::InspectorOpen)
    {
        note(0, 440, .050, .48, 40);
        note(.026, 720, .049, .34);
    }
    else if (kind == Tone::InspectorSelect)
    {
        note(0, 780, .024, .60, 60);
        note(.015, 1040, .024, .28);
    }
    else if (kind == Tone::InspectorPin)
    {
        note(0, 600, .038, .55);
        note(.032, 900, .055, .40);
    }
    else if (kind == Tone::InspectorClose)
    {
        note(0, 540, .035, .42);
        note(.022, 360, .037, .30);
    }
    else if (kind == Tone::PersonalBest)
    {
        note(0, 430, .054, .70);
        note(.041, 645, .054, .60);
        note(.086, 860, .054, .50);
        note(.131, 1075, .063, .40);
    }
    else if (kind == Tone::Confirm || kind == Tone::Complete)
    {
        note(0, 540, .051, .80);
        note(.039, 810, .056, .65);
        if (kind == Tone::Complete)
            note(.079, 1080, .048, .45);
    }
    pcm.front() = pcm.back() = 0;
    return pcm;
}
} // namespace suite_ui
