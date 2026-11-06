#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <ostream>
#include <string>
#include <utility>

struct ReplayPreferences
{
    int mode = 0, crystals = 3;
    float seconds = 10;
    int libraryLevel = -1, libraryCategory = -1, libraryStatus = 0, librarySort = 0;
    bool libraryAscending = false, libraryFavorites = false, liveSplits = true;
    int libraryView = 0;
    int splitPlacement = 1, splitRows = 1; // timer / independent / both
    float splitX = .5f, splitY = 1.f, splitScale = 1.f, splitWidth = 480, splitOpacity = .9f;
    bool splitPace = false, splitTheory = false;
    int splitStyle = 0, splitMetric = 0, splitAnimation = 2;
    float splitTextSize = 52, splitDuration = 3, splitBackgroundOpacity = .9f;
    std::array<float,4> splitAhead{.42f,.92f,.64f,1}, splitBehind{1,.44f,.49f,1}, splitEqual{.83f,.87f,.91f,1};

    void Validate()
    {
        mode = std::clamp(mode, 0, 1);
        splitPlacement = std::clamp(splitPlacement, 0, 2);
        splitRows = 1; // Live display deliberately shows only the latest pickup.
        splitStyle = std::clamp(splitStyle, 0, 1); splitMetric = std::clamp(splitMetric, 0, 2);
        splitAnimation = std::clamp(splitAnimation, 0, 3);
        auto bound = [](float value, float low, float high, float fallback) { return std::isfinite(value) ? std::clamp(value, low, high) : fallback; };
        splitTextSize = bound(splitTextSize, 24, 96, 52); splitDuration = bound(splitDuration, 0, 10, 3);
        splitBackgroundOpacity = bound(splitBackgroundOpacity, 0, 1, .9f);
        for (auto *color : {&splitAhead,&splitBehind,&splitEqual})
            for (auto &v : *color) v = bound(v, 0, 1, 1);
        splitX = bound(splitX, 0, 1, .5f); splitY = bound(splitY, 0, 1, 1);
        splitScale = bound(splitScale, .6f, 1.8f, 1); splitWidth = bound(splitWidth, 320, 720, 480);
        splitOpacity = bound(splitOpacity, .1f, 1, .9f);
        crystals = std::clamp(crystals, 1, 1000);
        seconds = std::isfinite(seconds) ? std::clamp(seconds, 0.f, 600.f) : 10;
        libraryLevel = std::clamp(libraryLevel, -1, 7);
        libraryCategory = std::clamp(libraryCategory, -1, 2);
        libraryStatus = std::clamp(libraryStatus, 0, 4);
        librarySort = std::clamp(librarySort, 0, 6);
        libraryView = std::clamp(libraryView, 0, 1);
    }

    void Encode(std::ostream &out) const
    {
        out << "replay.mode=" << mode << "\nreplay.seconds=" << seconds << "\nreplay.crystals=" << crystals
            << '\n';
        out << "replay.splitStyle=" << splitStyle << "\nreplay.splitMetric=" << splitMetric << "\nreplay.splitAnimation=" << splitAnimation
            << "\nreplay.splitTextSize=" << splitTextSize << "\nreplay.splitDuration=" << splitDuration
            << "\nreplay.splitBackgroundOpacity=" << splitBackgroundOpacity << '\n';
        for (int i=0;i<4;i++) out << "replay.splitAhead" << i << '=' << splitAhead[i] << "\nreplay.splitBehind" << i << '=' << splitBehind[i]
            << "\nreplay.splitEqual" << i << '=' << splitEqual[i] << '\n';
        out << "replay.splitPlacement=" << splitPlacement << "\nreplay.splitRows=" << splitRows
            << "\nreplay.splitX=" << splitX << "\nreplay.splitY=" << splitY << "\nreplay.splitScale=" << splitScale
            << "\nreplay.splitWidth=" << splitWidth << "\nreplay.splitOpacity=" << splitOpacity
            << "\nreplay.splitPace=" << int(splitPace) << "\nreplay.splitTheory=" << int(splitTheory) << '\n';
        out << "replay.liveSplits=" << int(liveSplits) << "\nreplay.libraryView=" << libraryView << '\n';
        out << "replay.libraryLevel=" << libraryLevel << "\nreplay.libraryCategory=" << libraryCategory
            << "\nreplay.libraryStatus=" << libraryStatus << "\nreplay.librarySort=" << librarySort
            << "\nreplay.libraryAscending=" << int(libraryAscending) << "\nreplay.libraryFavorites=" << int(libraryFavorites) << '\n';
    }

    void Decode(const std::string &key, double v)
    {
        if (!std::isfinite(v))
            return;
        for (auto item : {std::pair{"splitAhead", &splitAhead}, std::pair{"splitBehind", &splitBehind}, std::pair{"splitEqual", &splitEqual}})
        {
            auto length = std::char_traits<char>::length(item.first);
            if (key.starts_with(item.first) && key.size()==length+1 && key.back()>='0' && key.back()<='3')
            { (*item.second)[size_t(key.back()-'0')] = float(std::clamp(v,0.,1.)); return; }
        }
        if (key == "splitStyle") splitStyle = int(std::clamp(v,0.,1.));
        else if (key == "splitMetric") splitMetric = int(std::clamp(v,0.,2.));
        else if (key == "splitAnimation") splitAnimation = int(std::clamp(v,0.,3.));
        else if (key == "splitTextSize") splitTextSize = float(std::clamp(v,24.,96.));
        else if (key == "splitDuration") splitDuration = float(std::clamp(v,0.,10.));
        else if (key == "splitBackgroundOpacity") splitBackgroundOpacity = float(std::clamp(v,0.,1.));
        else if (key == "mode")
            mode = int(std::clamp(v, 0., 1.));
        else if (key == "crystals")
            crystals = int(std::clamp(v, 1., 1000.));
        else if (key == "seconds")
            seconds = float(std::clamp(v, 0., 600.));
        else if (key == "splitPlacement") splitPlacement = int(std::clamp(v, 0., 2.));
        else if (key == "splitRows") splitRows = int(std::clamp(v, 1., 6.));
        else if (key == "splitX") splitX = float(std::clamp(v, 0., 1.));
        else if (key == "splitY") splitY = float(std::clamp(v, 0., 1.));
        else if (key == "splitScale") splitScale = float(std::clamp(v, .6, 1.8));
        else if (key == "splitWidth") splitWidth = float(std::clamp(v, 320., 720.));
        else if (key == "splitOpacity") splitOpacity = float(std::clamp(v, .1, 1.));
        else if (key == "splitPace" && (v == 0 || v == 1)) splitPace = v == 1;
        else if (key == "splitTheory" && (v == 0 || v == 1)) splitTheory = v == 1;
        else if (key == "liveSplits" && (v == 0 || v == 1)) liveSplits = v == 1;
        else if (key == "libraryView") libraryView = int(std::clamp(v, 0., 1.));
        else if (key == "libraryLevel") libraryLevel = int(std::clamp(v, -1., 7.));
        else if (key == "libraryCategory") libraryCategory = int(std::clamp(v, -1., 2.));
        else if (key == "libraryStatus") libraryStatus = int(std::clamp(v, 0., 4.));
        else if (key == "librarySort") librarySort = int(std::clamp(v, 0., 6.));
        else if (key == "libraryAscending" && (v == 0 || v == 1)) libraryAscending = v == 1;
        else if (key == "libraryFavorites" && (v == 0 || v == 1)) libraryFavorites = v == 1;
    }
};
