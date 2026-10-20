#pragma once
#include <algorithm>
#include <cmath>
#include <ostream>
#include <string>

struct ReplayPreferences
{
    int mode = 0, crystals = 3;
    float seconds = 10;

    void Validate()
    {
        mode = std::clamp(mode, 0, 1);
        crystals = std::clamp(crystals, 1, 1000);
        seconds = std::isfinite(seconds) ? std::clamp(seconds, 0.f, 600.f) : 10;
    }

    void Encode(std::ostream &out) const
    {
        out << "replay.mode=" << mode << "\nreplay.seconds=" << seconds << "\nreplay.crystals=" << crystals
            << '\n';
    }

    void Decode(const std::string &key, double v)
    {
        if (!std::isfinite(v))
            return;
        if (key == "mode")
            mode = int(std::clamp(v, 0., 1.));
        else if (key == "crystals")
            crystals = int(std::clamp(v, 1., 1000.));
        else if (key == "seconds")
            seconds = float(std::clamp(v, 0., 600.));
    }
};
