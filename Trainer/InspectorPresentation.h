#pragma once
#include "ExperienceModel.h"
#include <string>
#include <unordered_map>
#include <ostream>

namespace inspector_fx
{
struct Preferences
{
    bool sounds = true, effects = true;
    float volume = .65f, opacity = .96f, outline = 2.f;

    void Validate()
    {
        volume = std::isfinite(volume) ? std::clamp(volume, 0.f, 1.f) : .65f;
        opacity = std::isfinite(opacity) ? std::clamp(opacity, .65f, 1.f) : .96f;
        outline = std::isfinite(outline) ? std::clamp(outline, 1.f, 4.f) : 2.f;
    }

    void Encode(std::ostream &out) const
    {
        out << "inspector.sounds=" << sounds << "\ninspector.effects=" << effects
            << "\ninspector.volume=" << volume << "\ninspector.opacity=" << opacity
            << "\ninspector.outline=" << outline << '\n';
    }

    void Decode(const std::string &key, double value)
    {
        if (!std::isfinite(value))
            return;
        if (key == "volume")
            volume = float(std::clamp(value, 0., 1.));
        else if (key == "opacity")
            opacity = float(std::clamp(value, .65, 1.));
        else if (key == "outline")
            outline = float(std::clamp(value, 1., 4.));
        else if (value == 0 || value == 1)
        {
            if (key == "sounds")
                sounds = value == 1;
            else if (key == "effects")
                effects = value == 1;
        }
    }
};

inline Preferences preferences;

struct Cue
{
    suite_ui::Tone tone = suite_ui::Tone::None;
    float gain = 0;
};

// One short, expiring event. Nothing is emitted by hovering or telemetry.
class Feedback
{
    Cue pending;
    double at = -100, audioUntil = -100;

  public:
    void Emit(suite_ui::Tone tone, double now, const Preferences &p, const ExperiencePreferences &master,
              bool running)
    {
        if (running || !master.sounds || master.volume <= 0 || !p.sounds || p.volume <= 0)
            return;
        pending = {tone, p.volume};
        at = now;
        audioUntil = now + .18;
    }

    bool Window(double now, const Preferences &p, const ExperiencePreferences &master) const
    {
        return master.sounds && master.volume > 0 && p.sounds && p.volume > 0 && now >= at &&
               now < audioUntil;
    }

    Cue Take(double now, bool allowed)
    {
        Cue result = allowed && now >= at && now - at < .18 ? pending : Cue{};
        pending = {};
        return result;
    }

    void Reset()
    {
        pending = {};
        at = audioUntil = -100;
    }
};

inline Feedback feedback;

// Bounded value cache; updated once per new detail publication, never per row.
class Changes
{
    struct Entry
    {
        std::string value;
        double at = -100;
    };

    std::unordered_map<uint64_t, Entry> entries;
    int pid = 0, selected = 0;
    uint32_t session = 0, epoch = 0, revision = 0;
    bool initialized = false;

  public:
    static constexpr size_t Capacity = 1024;

    template <class Detail> void Observe(const Detail &d, double now)
    {
        bool reset = !initialized || pid != d.header.pid || session != d.header.session ||
                     epoch != d.header.epoch || selected != d.selected;
        if (reset)
        {
            entries.clear();
            pid = d.header.pid;
            session = d.header.session;
            epoch = d.header.epoch;
            selected = d.selected;
            initialized = true;
        }
        else if (revision == d.header.revision)
            return;
        revision = d.header.revision;
        for (const auto &v : d.values)
        {
            auto found = entries.find(v.key);
            if (found != entries.end())
            {
                if (found->second.value != v.value)
                {
                    found->second.value = v.value;
                    found->second.at = now;
                }
            }
            else if (entries.size() < Capacity)
                entries.emplace(v.key, Entry{v.value, -100});
        }
    }

    float Pulse(uint64_t key, double now) const
    {
        auto found = entries.find(key);
        return found == entries.end() ? 0.f
                                      : std::clamp(1.f - float((now - found->second.at) / .65), 0.f, 1.f);
    }

    size_t Size() const
    {
        return entries.size();
    }

    void Reset()
    {
        entries.clear();
        initialized = false;
    }
};

inline Changes changes;
} // namespace inspector_fx
