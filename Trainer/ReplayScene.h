#pragma once
#include "PracticeMapData.h"
#include "ReplayTextValidation.h"
#include <set>
#include <climits>
#include <tuple>

namespace practice
{
struct ReplaySceneInfo
{
    bool recorded = false;
    int dropped = 0, tracks = 0, samples = 0, cameras = 0, ui = 0, particles = 0;
    double through = 0;
};

inline ReplaySceneInfo ReadReplayScene(MapReader &r, double duration)
{
    const size_t begin = r.Offset();
    ReplaySceneInfo d;
    d.recorded = r.Flag();
    d.dropped = r.Count(INT_MAX);
    d.through = r.Read<double>();
    if (!std::isfinite(d.through) || d.through < 0 || d.through > duration + .000001)
        throw std::runtime_error("Invalid scene coverage");
    auto text = [&](int max) {
        auto value = r.Text(max);
        if (!ReplayUtf8(value, size_t(max)))
            throw std::runtime_error("Invalid scene UTF-8");
        return value;
    };
    auto floats = [&](int count) {
        while (count--)
            r.Coordinate();
    };
    auto rotation = [&]() {
        double norm = 0;
        for (int i = 0; i < 4; i++)
        {
            float f = r.Coordinate();
            norm += double(f) * f;
        }
        if (norm < .98 || norm > 1.02)
            throw std::runtime_error("Invalid scene rotation");
    };
    auto graphic = [&]() {
        r.Flag();
        floats(4);
        text(1024);
        if (r.Read<uint32_t>() & ~15u)
            throw std::runtime_error("Invalid UI material properties");
        floats(6);
    };
    d.tracks = r.Count(2048);
    std::set<std::tuple<int, std::string, std::string>> unique;
    for (int i = 0; i < d.tracks; i++)
    {
        int kind = r.Count(7);
        auto path = text(4096), prefab = text(1024), spawn = text(4096);
        if (!kind ||
            ((!prefab.empty() || !spawn.empty()) && (kind != 7 || prefab.empty() || spawn.empty())) ||
            !unique.insert({kind, spawn, path}).second)
            throw std::runtime_error("Invalid scene track identity");
        if (kind == 1)
            d.cameras++;
        else if (kind == 7)
            d.particles++;
        else
            d.ui++;
        int count = r.Count(262144 - d.samples);
        d.samples += count;
        double previous = -1;
        for (int j = 0; j < count; j++)
        {
            double at = r.Read<double>();
            if (!std::isfinite(at) || at < 0 || at < previous || at > duration + .000001)
                throw std::runtime_error("Invalid scene timestamp");
            previous = at;
            int bytes = r.Count(1024 * 1024);
            const size_t start = r.Offset();
            if (start - begin + size_t(bytes) > 32 * 1024 * 1024)
                throw std::runtime_error("Scene recording exceeds byte limit");
            if (kind == 1)
            {
                r.Flag();
                floats(3);
                rotation();
                floats(3);
                float fov = r.Coordinate(), clipNear = r.Coordinate(), clipFar = r.Coordinate();
                floats(3);
                r.Read<int32_t>();
                int clear = r.Count(4);
                r.Flag();
                floats(4);
                if (fov < 1 || fov > 179 || clipNear <= 0 || clipFar <= clipNear || !clear)
                    throw std::runtime_error("Invalid recorded camera");
            }
            else if (kind == 2)
            {
                r.Flag();
                floats(11);
                rotation();
                floats(3);
            }
            else if (kind == 3)
            {
                graphic();
                text(16384);
                r.Count(1000);
                r.Count(3);
                r.Count(8);
                r.Coordinate();
            }
            else if (kind == 4)
            {
                graphic();
                text(1024);
                float fill = r.Coordinate();
                if (fill < 0 || fill > 1)
                    throw std::runtime_error("Invalid image fill");
            }
            else if (kind == 5)
            {
                graphic();
                text(1024);
                floats(4);
            }
            else if (kind == 6)
            {
                float alpha = r.Coordinate();
                r.Flag();
                if (alpha < 0 || alpha > 1)
                    throw std::runtime_error("Invalid group alpha");
            }
            else
            {
                if (r.Read<uint32_t>() & ~7u)
                    throw std::runtime_error("Invalid particle state");
                float time = r.Read<float>();
                if (!std::isfinite(time) || time < 0 || time > 315360000)
                    throw std::runtime_error("Invalid particle time");
                r.Read<uint32_t>();
                floats(3);
                rotation();
                floats(3);
                int particles = r.Count(8192);
                for (int k = 0; k < particles; k++)
                {
                    floats(18);
                    r.Read<uint32_t>();
                    float life = r.Coordinate(), startLife = r.Coordinate();
                    r.Read<uint32_t>();
                    if (life < 0 || startLife <= 0 || life > startLife + .001f)
                        throw std::runtime_error("Invalid particle lifetime");
                }
            }
            if (r.Offset() - start != size_t(bytes))
                throw std::runtime_error("Invalid scene payload size");
        }
    }
    if (r.Offset() - begin > 32 * 1024 * 1024 ||
        (!d.recorded && (d.tracks || d.samples || d.dropped || d.through)))
        throw std::runtime_error("Unexpected scene observations");
    return d;
}
} // namespace practice
