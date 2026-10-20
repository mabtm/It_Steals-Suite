#pragma once
#include <cmath>
#include <algorithm>
#include <ostream>
#include <string>

struct MinimapOptions
{
    bool follow = true, playerUp = true, walls = true, crystals = true, monster = true, trail = false;
    float zoom = 1, range = 40;
    int anchor = 2;

    void Validate()
    {
        zoom = std::isfinite(zoom) ? std::clamp(zoom, .5f, 4.f) : 1;
        range = std::isfinite(range) ? std::clamp(range, 10.f, 150.f) : 40;
        anchor = std::clamp(anchor, 0, 3);
    }

    void Encode(std::ostream &out) const
    {
        out << "map.follow=" << follow << "\nmap.playerUp=" << playerUp << "\nmap.walls=" << walls
            << "\nmap.crystals=" << crystals << "\nmap.monster=" << monster << "\nmap.trail=" << trail
            << "\nmap.zoom=" << zoom << "\nmap.range=" << range << "\nmap.anchor=" << anchor << '\n';
    }

    void Decode(const std::string &key, double value)
    {
        if (key == "zoom")
            zoom = static_cast<float>(std::clamp(value, .5, 4.));
        else if (key == "range")
            range = static_cast<float>(std::clamp(value, 10., 150.));
        else if (key == "anchor")
            anchor = static_cast<int>(std::clamp(value, 0., 3.));
        else if (value == 0 || value == 1)
        {
            bool flag = value == 1;
            if (key == "follow")
                follow = flag;
            else if (key == "playerUp")
                playerUp = flag;
            else if (key == "walls")
                walls = flag;
            else if (key == "crystals")
                crystals = flag;
            else if (key == "monster")
                monster = flag;
            else if (key == "trail")
                trail = flag;
        }
    }
};
