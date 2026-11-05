#pragma once
#include <algorithm>
#include <cmath>
#include <ostream>
#include <string>

struct ReplayPreferences
{
    int mode = 0, crystals = 3;
    float seconds = 10;
    int libraryLevel = -1, libraryCategory = -1, libraryStatus = 0, librarySort = 0;
    bool libraryAscending = false, libraryFavorites = false;

    void Validate()
    {
        mode = std::clamp(mode, 0, 1);
        crystals = std::clamp(crystals, 1, 1000);
        seconds = std::isfinite(seconds) ? std::clamp(seconds, 0.f, 600.f) : 10;
        libraryLevel = std::clamp(libraryLevel, -1, 7);
        libraryCategory = std::clamp(libraryCategory, -1, 2);
        libraryStatus = std::clamp(libraryStatus, 0, 4);
        librarySort = std::clamp(librarySort, 0, 6);
    }

    void Encode(std::ostream &out) const
    {
        out << "replay.mode=" << mode << "\nreplay.seconds=" << seconds << "\nreplay.crystals=" << crystals
            << '\n';
        out << "replay.libraryLevel=" << libraryLevel << "\nreplay.libraryCategory=" << libraryCategory
            << "\nreplay.libraryStatus=" << libraryStatus << "\nreplay.librarySort=" << librarySort
            << "\nreplay.libraryAscending=" << int(libraryAscending) << "\nreplay.libraryFavorites=" << int(libraryFavorites) << '\n';
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
        else if (key == "libraryLevel") libraryLevel = int(std::clamp(v, -1., 7.));
        else if (key == "libraryCategory") libraryCategory = int(std::clamp(v, -1., 2.));
        else if (key == "libraryStatus") libraryStatus = int(std::clamp(v, 0., 4.));
        else if (key == "librarySort") librarySort = int(std::clamp(v, 0., 6.));
        else if (key == "libraryAscending" && (v == 0 || v == 1)) libraryAscending = v == 1;
        else if (key == "libraryFavorites" && (v == 0 || v == 1)) libraryFavorites = v == 1;
    }
};
