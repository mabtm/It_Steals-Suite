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
    InspectorClose
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
            *slot = {id, frame, appear && enabled ? 0 : target, target, now, true};
        }
        auto &e = *slot;
        if (frame < e.frame || frame > e.frame + 1)
        {
            e.from = appear && enabled ? 0 : target;
            e.target = target;
            e.began = now;
        }
        float current =
            enabled ? e.from + (e.target - e.from) * Ease(float((now - e.began) / duration)) : e.target;
        if (e.target != target)
        {
            e.from = current;
            e.target = target;
            e.began = now;
        }
        e.frame = frame;
        return enabled ? e.from + (e.target - e.from) * Ease(float((now - e.began) / duration)) : target;
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

// Original quiet PCM cues. Gain is applied to these samples, never to the
// system/game mixer. Smooth ends avoid artificial clicks from cut waveforms.
inline std::vector<int16_t> ToneSamples(Tone kind, float volume, int rate = 44100)
{
    if (kind == Tone::None || rate < 8000 || rate > 192000 || !std::isfinite(volume))
        return {};
    volume = std::clamp(volume, 0.f, 1.f);
    double duration = kind == Tone::InspectorOpen     ? .075
                      : kind == Tone::InspectorSelect ? .045
                      : kind == Tone::InspectorPin    ? .09
                      : kind == Tone::InspectorClose  ? .06
                      : kind == Tone::PersonalBest    ? .185
                      : kind == Tone::Complete        ? .115
                      : kind == Tone::Confirm         ? .095
                      : kind == Tone::Tab             ? .038
                                                      : .028;
    std::vector<int16_t> pcm(size_t(std::ceil(duration * rate)) + 1, 0);
    constexpr double pi = 3.14159265358979323846;
    auto note = [&](double at, double frequency, double length, double gain) {
        for (size_t i = size_t(at * rate); i < pcm.size(); i++)
        {
            double t = double(i) / rate - at;
            if (t < 0 || t >= length)
                continue;
            double envelope = std::pow(std::sin(pi * t / length), 2) * std::exp(-t * 20);
            double wave = std::sin(2 * pi * frequency * t) + .14 * std::sin(2 * pi * frequency * 2 * t);
            int sample = int(pcm[i]) + int(32767 * .16 * volume * gain * envelope * wave);
            pcm[i] = int16_t(std::clamp(sample, -32767, 32767));
        }
    };
    if (kind == Tone::InspectorOpen)
    {
        note(0, 420, .055, .48);
        note(.024, 680, .05, .34);
    }
    else if (kind == Tone::InspectorSelect)
    {
        note(0, 780, .024, .6);
        note(.016, 1040, .028, .28);
    }
    else if (kind == Tone::InspectorPin)
    {
        note(0, 660, .038, .55);
        note(.032, 990, .055, .4);
    }
    else if (kind == Tone::InspectorClose)
    {
        note(0, 510, .035, .42);
        note(.022, 340, .037, .3);
    }
    else if (kind == Tone::PersonalBest)
    {
        note(0, 610, .05, .7);
        note(.04, 810, .05, .6);
        note(.08, 970, .05, .5);
        note(.125, 1220, .06, .4);
    }
    else if (kind == Tone::Confirm || kind == Tone::Complete)
    {
        note(0, 610, .048, .8);
        note(.038, 810, .05, .65);
        if (kind == Tone::Complete)
            note(.069, 970, .044, .45);
    }
    else
        note(0,
             kind == Tone::Tab   ? 680
             : kind == Tone::On  ? 610
             : kind == Tone::Off ? 470
                                 : 530,
             duration, .8);
    pcm.front() = pcm.back() = 0;
    return pcm;
}
} // namespace suite_ui
