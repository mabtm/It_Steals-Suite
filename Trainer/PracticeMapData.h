#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include <array>
#include "MapLevels.h"

namespace practice
{
inline constexpr size_t MapCapacity = 4194304;

struct Vec3
{
    float x = 0, y = 0, z = 0;
};

struct Wall
{
    float minX = 0, minZ = 0, maxX = 0, maxZ = 0;
};

struct Selection
{
    int spawn = -1;
    std::vector<int> crystals;
};

struct WallOutline
{
    std::array<Vec3, 4> corners;
    bool approximate = false;
};

struct PhysicalPart
{
    uint32_t flags = 0;
    std::vector<Vec3> points;
};

struct PickupRegion
{
    bool verified = false;
    Vec3 center;
    float radius = 0;
};

struct RunPose
{
    double seconds = 0;
    Vec3 point;
    bool continuous = false;
};

struct ReplayPath
{
    double seconds = 0;
    std::vector<Vec3> corners;
};

struct ReplayMonster
{
    int id = 0, kind = 0;
    std::vector<RunPose> trace;
    std::vector<ReplayPath> paths;
};

struct ReplayFlash
{
    double seconds = 0;
    Vec3 player;
    int kind = 0, actor = 0;
};

struct ReplayMotionInfo
{
    int frames = 0, ticks = 0, events = 0, dropped = 0;
    uint32_t flags = 0;
    std::string tool, game, unity;
};

struct RunReview
{
    int64_t id = 0;
    int outcome = 0;
    double seconds = 0;
    bool reduced = false;
    std::shared_ptr<const ReplayMotionInfo> motion;
    uint32_t lightSources = 0;
    int lightRejected = 0;
    bool validationChecked = false, valid = false, observedFromStart = false, seedKnown = false,
         monstersReduced = false;
    int seedKind = 0, category = 0, selectedSeed = 0, runSeed = 0;
    bool rawSpawnSeed = false;
    uint32_t initialAids = 0;
    uint32_t aids = 0, warnings = 0;
    std::string validityReason = "Older recording; validation unavailable";
    std::vector<ReplayMonster> monsters;
    std::vector<ReplayFlash> flashes;
    std::vector<RunPose> trace;
    std::vector<int> collected;
    std::vector<double> pickupSeconds;
};

struct MovementData
{
    int scene = -1, revision = 0, rejected = 0;
    float tick = 0, radius = 0, height = 0;
    std::string hash;
    std::array<int, 5> counts{};
    std::array<float, 15> speeds{};
    std::array<float, 5> lengths{};

    bool Ready() const
    {
        return counts[0] >= 24;
    }
};

inline bool ValidMovement(const MovementData &m)
{
    if (MapLevelIndex(m.scene) < 0 || m.revision < 0 || m.rejected < 0 || !std::isfinite(m.tick) ||
        m.tick <= 0 || m.tick > 1 || !std::isfinite(m.radius) || m.radius <= 0 || m.radius > 5 ||
        !std::isfinite(m.height) || m.height <= 0 || m.height > 10)
        return false;
    for (int k = 0; k < 5; k++)
    {
        if (m.counts[k] < 0 || m.counts[k] > 20000 || !std::isfinite(m.lengths[k]) || m.lengths[k] < 0 ||
            m.lengths[k] > 5)
            return false;
        for (int q = 0; q < 3; q++)
        {
            float v = m.speeds[k * 3 + q];
            if (!std::isfinite(v) || v < 0 || v > 25.6f || (q && v < m.speeds[k * 3 + q - 1]))
                return false;
        }
        if (m.counts[k] == 0)
        {
            if (m.lengths[k] != 0 || m.speeds[k * 3] != 0 || m.speeds[k * 3 + 1] != 0 ||
                m.speeds[k * 3 + 2] != 0)
                return false;
        }
        else if (m.lengths[k] <= 0 || m.speeds[k * 3] <= 0)
            return false;
    }
    return true;
}

struct MapData
{
    int pid = 0, scene = 0, epoch = 0, seed = 0, recipe = 0, sourceKind = 0, spawn = -1, keep = 0, flags = 0,
        stage = 0;
    int requested = 0, completed = 0, liveSpawn = -1, liveSeed = 0, liveRecipe = 0;
    int64_t capturedUtc = 0;
    uint32_t requestSerial = 0;
    std::string hash, message;
    std::vector<Vec3> spawns, points, vertices;
    std::vector<int> indices, selected, liveSelected;
    std::vector<Vec3> navigationNodes;
    std::vector<int> navigationEdges, navigationAnchors;
    int navigationStatus = 0, navigationQueries = 0, navigationTotal = 0;
    int navigationModel = 0;
    std::string navigationNote;
    int rngStatus = 0, rngInit = 0, rngRange = 0, rngAdvance = 0, rngSite = 0, rngProbes = 0,
        searchBackend = 0;
    std::string rngNote;
    bool physicalCaptured = false;
    int physicalOmitted = 0;
    std::vector<PhysicalPart> physicalParts;
    std::vector<PickupRegion> pickupRegions;
    std::vector<Wall> walls;
    std::vector<WallOutline> wallOutlines, obstacles;
    std::shared_ptr<const RunReview> review;
    std::shared_ptr<const MovementData> movement;
    float agentRadius = 0, agentHeight = 0, agentStep = 0, agentSlope = 0, agentFloor = 0;
    float movementTick = 0, movementSprintStep = 0;
    std::vector<Selection> samples; // Decode legacy v1/v2 caches only; no sampling or statistics feature.
    // Keep the original checked wire bytes for the background cache writer.
    std::vector<uint8_t> bytes;
    float minX = -1, maxX = 1, minZ = -1, maxZ = 1;

    bool Verified() const
    {
        return (flags & 1) && !(flags & 64);
    }

    void CalculateBounds()
    {
        minX = minZ = std::numeric_limits<float>::max();
        maxX = maxZ = -minX;
        auto include = [&](Vec3 p) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minZ = std::min(minZ, p.z);
            maxZ = std::max(maxZ, p.z);
        };
        // Bounds must include the captured mesh even when its baked height
        // differs from the physical controller bottom. Surface selection is
        // performed by NavGraph; a height band here could crop that surface.
        for (Vec3 p : vertices)
            include(p);
        for (Vec3 p : navigationNodes)
            include(p);
        for (Vec3 p : spawns)
            include(p);
        for (Vec3 p : points)
            include(p);
        for (auto w : walls)
        {
            include({w.minX, 0, w.minZ});
            include({w.maxX, 0, w.maxZ});
        }
        for (const auto &part : physicalParts)
            for (Vec3 p : part.points)
                include(p);
        if (review)
            for (const auto &p : review->trace)
                include(p.point);
        if (minX > maxX)
        {
            minX = minZ = -1;
            maxX = maxZ = 1;
        }
    }
};

inline bool SameNavigationCatalog(const MapData &a, const MapData &b)
{
    if (a.physicalCaptured != b.physicalCaptured || a.physicalOmitted != b.physicalOmitted ||
        a.physicalParts.size() != b.physicalParts.size() || a.pickupRegions.size() != b.pickupRegions.size())
        return false;
    for (size_t i = 0; i < a.physicalParts.size(); i++)
    {
        const auto &x = a.physicalParts[i];
        const auto &y = b.physicalParts[i];
        if (x.flags != y.flags || x.points.size() != y.points.size() ||
            (!x.points.empty() &&
             std::memcmp(x.points.data(), y.points.data(), x.points.size() * sizeof(Vec3)) != 0))
            return false;
    }
    for (size_t i = 0; i < a.pickupRegions.size(); i++)
    {
        const auto &x = a.pickupRegions[i];
        const auto &y = b.pickupRegions[i];
        if (x.verified != y.verified || x.radius != y.radius ||
            std::memcmp(&x.center, &y.center, sizeof(Vec3)) != 0)
            return false;
    }
    if (a.agentRadius != b.agentRadius || a.agentHeight != b.agentHeight || a.agentFloor != b.agentFloor ||
        a.agentStep != b.agentStep || a.agentSlope != b.agentSlope ||
        a.obstacles.size() != b.obstacles.size())
        return false;
    for (size_t i = 0; i < a.obstacles.size(); i++)
        if (a.obstacles[i].approximate != b.obstacles[i].approximate ||
            std::memcmp(a.obstacles[i].corners.data(), b.obstacles[i].corners.data(), sizeof(Vec3) * 4) != 0)
            return false;
    return a.scene == b.scene && a.hash == b.hash && a.spawns.size() == b.spawns.size() &&
           a.points.size() == b.points.size() &&
           (a.spawns.empty() ||
            std::memcmp(a.spawns.data(), b.spawns.data(), a.spawns.size() * sizeof(Vec3)) == 0) &&
           (a.points.empty() ||
            std::memcmp(a.points.data(), b.points.data(), a.points.size() * sizeof(Vec3)) == 0) &&
           a.vertices.size() == b.vertices.size() && a.indices == b.indices &&
           a.navigationModel == b.navigationModel && a.navigationStatus == b.navigationStatus &&
           a.navigationAnchors == b.navigationAnchors && a.navigationEdges == b.navigationEdges &&
           a.navigationNodes.size() == b.navigationNodes.size() &&
           (a.navigationNodes.empty() || std::memcmp(a.navigationNodes.data(), b.navigationNodes.data(),
                                                     a.navigationNodes.size() * sizeof(Vec3)) == 0) &&
           (a.vertices.empty() ||
            std::memcmp(a.vertices.data(), b.vertices.data(), a.vertices.size() * sizeof(Vec3)) == 0);
}

// Slope is independent of controller height. In particular, older captures
// without a controller profile must still display their ground geometry.
inline bool FloorSlope(const MapData &d, Vec3 a, Vec3 b, Vec3 c)
{
    double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z, vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
    double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx,
           n = std::sqrt(nx * nx + ny * ny + nz * nz);
    double slope = d.agentHeight > 0 ? d.agentSlope : 45;
    return n > 1e-9 && std::abs(ny) / n + 1e-6 >= std::cos(slope * 3.141592653589793 / 180);
}

class MapReader
{
    const std::vector<uint8_t> &data;
    size_t position = 0;

  public:
    explicit MapReader(const std::vector<uint8_t> &bytes) : data(bytes) {}

    template <class T> T Read()
    {
        if (position > data.size() || sizeof(T) > data.size() - position)
            throw std::runtime_error("Truncated map packet");
        T value;
        std::memcpy(&value, data.data() + position, sizeof(T));
        position += sizeof(T);
        return value;
    }

    int Count(int maximum)
    {
        int count = Read<int32_t>();
        if (count < 0 || count > maximum)
            throw std::runtime_error("Map count exceeds limit");
        return count;
    }

    float Coordinate()
    {
        float value = Read<float>();
        if (!std::isfinite(value) || std::abs(value) > 100000)
            throw std::runtime_error("Invalid map coordinate");
        return value;
    }

    std::string Text(int maximum)
    {
        size_t count = static_cast<size_t>(Count(maximum));
        if (count > data.size() - position)
            throw std::runtime_error("Truncated map text");
        std::string value(reinterpret_cast<const char *>(data.data() + position), count);
        position += count;
        if (value.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid map text");
        return value;
    }

    std::vector<Vec3> Vectors(int maximum)
    {
        int count = Count(maximum);
        std::vector<Vec3> result;
        result.reserve(count);
        for (int i = 0; i < count; i++)
            result.push_back({Coordinate(), Coordinate(), Coordinate()});
        return result;
    }

    std::vector<int> Ints(int maximum)
    {
        int count = Count(maximum);
        std::vector<int> result;
        result.reserve(count);
        for (int i = 0; i < count; i++)
            result.push_back(Read<int32_t>());
        return result;
    }

    bool Flag()
    {
        auto flag = Read<uint8_t>();
        if (flag > 1)
            throw std::runtime_error("Invalid recorded flag");
        return flag != 0;
    }

    size_t Offset() const
    {
        return position;
    }

    bool Done() const
    {
        return position == data.size();
    }
};

inline std::shared_ptr<const ReplayMotionInfo> ReadMotionInfo(MapReader &r)
{
    if (!r.Flag())
        return {};
    auto i = std::make_shared<ReplayMotionInfo>();
    i->frames = r.Count(262144);
    i->ticks = r.Count(524288);
    i->events = r.Count(131072);
    i->dropped = r.Count(2147483647);
    i->flags = r.Read<uint32_t>();
    i->tool = r.Text(64);
    i->game = r.Text(64);
    i->unity = r.Text(64);
    if (i->flags & ~uint32_t{7})
        throw std::runtime_error("Invalid input metadata");
    return i;
}

inline bool ValidSelection(const std::vector<int> &values, size_t count)
{
    std::vector<bool> seen(count, false);
    for (int i : values)
    {
        if (i < 0 || static_cast<size_t>(i) >= count || seen[i])
            return false;
        seen[i] = true;
    }
    return true;
}

inline std::shared_ptr<const MapData> DecodeMap(std::vector<uint8_t> bytes, std::string &error)
{
    try
    {
        if (bytes.size() > MapCapacity - 64)
            throw std::runtime_error("Map packet exceeds channel capacity");
        MapReader r(bytes);
        if (r.Read<uint32_t>() != 0x504D5349u)
            throw std::runtime_error("Unsupported map format");
        int format = r.Read<int32_t>();
        if (format < 1 || format > 15 || format == 5)
            throw std::runtime_error("Unsupported map format");
        auto d = std::make_shared<MapData>();
        for (int *value : {&d->pid, &d->scene, &d->epoch, &d->seed, &d->recipe, &d->sourceKind, &d->spawn,
                           &d->keep, &d->flags, &d->stage, &d->requested, &d->completed, &d->liveSpawn,
                           &d->liveSeed, &d->liveRecipe})
            *value = r.Read<int32_t>();
        d->capturedUtc = r.Read<int64_t>();
        if (format >= 2)
            d->requestSerial = r.Read<uint32_t>();
        d->hash = r.Text(65);
        d->message = r.Text(1024);
        d->spawns = r.Vectors(128);
        d->points = r.Vectors(1024);
        d->selected = r.Ints(64);
        d->liveSelected = r.Ints(64);
        d->vertices = r.Vectors(30000);
        d->indices = r.Ints(180000);
        int walls = r.Count(20000);
        d->walls.reserve(walls);
        for (int i = 0; i < walls; i++)
        {
            Wall w{r.Coordinate(), r.Coordinate(), r.Coordinate(), r.Coordinate()};
            if (w.minX > w.maxX || w.minZ > w.maxZ)
                throw std::runtime_error("Invalid map wall");
            d->walls.push_back(w);
        }
        int samples = r.Count(2000);
        d->samples.reserve(samples);
        for (int i = 0; i < samples; i++)
        {
            Selection s;
            s.spawn = r.Read<int32_t>();
            s.crystals = r.Ints(64);
            d->samples.push_back(std::move(s));
        }
        if (format >= 3)
        {
            d->navigationStatus = r.Read<int32_t>();
            d->navigationQueries = r.Read<int32_t>();
            d->navigationTotal = r.Read<int32_t>();
            d->navigationNote = r.Text(1024);
            d->navigationNodes = r.Vectors(10000);
            d->navigationEdges = r.Ints(60000);
            d->navigationAnchors = r.Ints(1152);
            if (d->navigationStatus < 0 || d->navigationStatus > 3 || d->navigationQueries < 0 ||
                d->navigationQueries > d->navigationTotal || d->navigationTotal > 1000000 ||
                d->navigationEdges.size() % 2)
                throw std::runtime_error("Invalid captured Unity navigation metadata");
            if (bool(d->flags & 128) != (d->navigationStatus == 1))
                throw std::runtime_error("Invalid navigation pending flag");
            if (d->navigationStatus == 2 &&
                (d->navigationQueries != d->navigationTotal ||
                 d->navigationTotal !=
                     static_cast<int>(
                         d->spawns.size() * d->points.size() +
                         (d->points.empty() ? 0 : d->points.size() * (d->points.size() - 1) / 2)) ||
                 d->navigationAnchors.size() != d->spawns.size() + d->points.size()))
                throw std::runtime_error("Navigation anchors do not match the catalog");
            if (d->navigationStatus != 2 &&
                (!d->navigationNodes.empty() || !d->navigationEdges.empty() || !d->navigationAnchors.empty()))
                throw std::runtime_error("Incomplete navigation graph published");
            for (int id : d->navigationAnchors)
                if (id < -1 || id >= static_cast<int>(d->navigationNodes.size()))
                    throw std::runtime_error("Invalid navigation anchor");
            for (int id : d->navigationEdges)
                if (id < 0 || id >= static_cast<int>(d->navigationNodes.size()))
                    throw std::runtime_error("Invalid corridor edge");
            for (size_t i = 0; i < d->navigationEdges.size(); i += 2)
                if (d->navigationEdges[i] == d->navigationEdges[i + 1])
                    throw std::runtime_error("Degenerate corridor edge");
        }
        if (format >= 4)
        {
            for (int *value : {&d->rngStatus, &d->rngInit, &d->rngRange, &d->rngAdvance, &d->rngSite,
                               &d->rngProbes, &d->searchBackend})
            {
                *value = r.Read<int32_t>();
            }
            d->rngNote = r.Text(1024);
            if (d->rngStatus < 0 || d->rngStatus > 3 || d->rngInit < 0 || d->rngInit > 3 || d->rngRange < 0 ||
                d->rngRange > 4 || d->rngAdvance < 0 || d->rngAdvance > 1 || d->rngProbes < 0 ||
                d->rngProbes > 4160 || (d->rngStatus == 2 && d->rngProbes != 4160) || d->searchBackend < 0 ||
                d->searchBackend > 1 || (d->searchBackend == 1 && (d->rngStatus != 2 || !d->Verified())))
                throw std::runtime_error("Unverified CPU generation descriptor");
        }
        if (format >= 6)
        {
            d->navigationModel = r.Read<int32_t>();
            if (d->navigationModel < 0 || d->navigationModel > 1 ||
                (d->navigationModel == 1 && (!IsHideSeekScene(d->scene) || d->navigationStatus != 2)))
                throw std::runtime_error("Invalid player navigation provenance");
        }
        if (format >= 7)
        {
            auto outlines = [&](std::vector<WallOutline> &target) {
                int count = r.Count(20000);
                target.reserve(count);
                for (int i = 0; i < count; i++)
                {
                    int approximate = r.Read<uint8_t>();
                    if (approximate > 1)
                        throw std::runtime_error("Invalid outline flag");
                    auto corners = r.Vectors(4);
                    if (corners.size() != 4)
                        throw std::runtime_error("Invalid wall outline");
                    WallOutline outline;
                    outline.approximate = approximate;
                    std::copy(corners.begin(), corners.end(), outline.corners.begin());
                    target.push_back(outline);
                }
            };
            outlines(d->wallOutlines);
            for (float *value :
                 {&d->agentRadius, &d->agentHeight, &d->agentStep, &d->agentSlope, &d->agentFloor})
                *value = r.Coordinate();
            outlines(d->obstacles);
            if (d->agentRadius < 0 || d->agentRadius > 5 || d->agentHeight < 0 || d->agentHeight > 10 ||
                d->agentStep < 0 || d->agentStep > 10 || d->agentSlope < 0 || d->agentSlope > 90)
                throw std::runtime_error("Invalid captured player dimensions");
            int hasReview = r.Read<uint8_t>();
            if (hasReview > 1)
                throw std::runtime_error("Invalid review flag");
            if (hasReview)
            {
                auto review = std::make_shared<RunReview>();
                review->id = r.Read<int64_t>();
                review->outcome = r.Read<int32_t>();
                review->seconds = r.Read<double>();
                review->reduced = r.Flag();
                if (review->id <= 0 || review->outcome < 1 || review->outcome > 5 ||
                    !std::isfinite(review->seconds) || review->seconds < 0 || review->seconds > 315360000)
                    throw std::runtime_error("Invalid recorded run identity");
                int count = r.Count(32768);
                double last = 0;
                for (int i = 0; i < count; i++)
                {
                    RunPose pose;
                    pose.seconds = r.Read<double>();
                    pose.point = {r.Coordinate(), r.Coordinate(), r.Coordinate()};
                    if (!std::isfinite(pose.seconds) || pose.seconds < last ||
                        pose.seconds > review->seconds + .1)
                        throw std::runtime_error("Invalid recorded time sequence");
                    last = pose.seconds;
                    review->trace.push_back(pose);
                }
                review->collected = r.Ints(64);
                if (!ValidSelection(review->collected, d->points.size()))
                    throw std::runtime_error("Invalid pickup sequence");
                for (int id : review->collected)
                    if (std::find(d->selected.begin(), d->selected.end(), id) == d->selected.end())
                        throw std::runtime_error("Pickup is outside the recorded layout");
                count = r.Count(64);
                last = 0;
                for (int i = 0; i < count; i++)
                {
                    double time = r.Read<double>();
                    if (!std::isfinite(time) || time < last || time > review->seconds + .1)
                        throw std::runtime_error("Invalid pickup time");
                    last = time;
                    review->pickupSeconds.push_back(time);
                }
                if (review->pickupSeconds.size() != review->collected.size())
                    throw std::runtime_error("Pickup times do not match the recorded goals");
                if (format >= 8)
                {
                    review->validationChecked = r.Flag();
                    review->valid = r.Flag();
                    review->observedFromStart = r.Flag();
                    review->seedKnown = r.Flag();
                    review->seedKind = r.Read<int32_t>();
                    review->aids = r.Read<uint32_t>();
                    review->warnings = r.Read<uint32_t>();
                    review->validityReason = r.Text(1024);
                    if (format >= 15)
                    {
                        review->category = r.Read<uint8_t>();
                        review->selectedSeed = r.Read<int32_t>();
                        review->runSeed = r.Read<int32_t>();
                        review->rawSpawnSeed = r.Flag();
                        review->initialAids = r.Read<uint32_t>();
                        if (review->category > 2)
                            throw std::runtime_error("Invalid recorded run category");
                    }
                    else
                        review->category = (review->aids & 3u) ? 2 : 0;
                    // Exact local validation rule, preserved with the run.
                    constexpr uint32_t invalidWarnings = ~uint32_t{64};
                    bool seedMatches = review->category == 0 ? review->seedKind == 1 && (review->aids | review->initialAids) == 0 :
                                       review->category == 1 && review->seedKind == 2 && review->aids == 2 &&
                                       review->initialAids == 2 &&
                                       review->runSeed == review->selectedSeed;
                    bool valid = review->validationChecked && review->observedFromStart &&
                                 review->seedKnown && seedMatches &&
                                 (review->warnings & invalidWarnings) == 0;
                    if (review->seedKind < 0 || review->seedKind > 2 || review->valid != valid)
                        throw std::runtime_error("Inconsistent recorded validation");
                    review->monstersReduced = r.Flag();
                    count = r.Count(128);
                    std::vector<int> ids;
                    int poses = 0, paths = 0;
                    auto time = [&](double &previous) {
                        double at = r.Read<double>();
                        if (!std::isfinite(at) || at < previous || at > review->seconds + .1)
                            throw std::runtime_error("Invalid monster event time");
                        previous = at;
                        return at;
                    };
                    for (int i = 0; i < count; i++)
                    {
                        ReplayMonster m;
                        m.id = r.Read<int32_t>();
                        m.kind = r.Read<int32_t>();
                        if (m.id <= 0 || std::find(ids.begin(), ids.end(), m.id) != ids.end() || m.kind < 1 ||
                            m.kind > 4)
                            throw std::runtime_error("Invalid monster identity");
                        ids.push_back(m.id);
                        int n = r.Count(32768);
                        poses += n;
                        if (poses > 32768)
                            throw std::runtime_error("Monster sample limit exceeded");
                        last = 0;
                        for (int j = 0; j < n; j++)
                        {
                            double at = time(last);
                            m.trace.push_back(
                                {at, {r.Coordinate(), r.Coordinate(), r.Coordinate()}, r.Flag()});
                        }
                        n = r.Count(1024);
                        paths += n;
                        if (paths > 1024)
                            throw std::runtime_error("Monster path limit exceeded");
                        last = 0;
                        for (int j = 0; j < n; j++)
                        {
                            double at = time(last);
                            m.paths.push_back({at, r.Vectors(64)});
                        }
                        review->monsters.push_back(std::move(m));
                    }
                    count = r.Count(4096);
                    last = 0;
                    for (int i = 0; i < count; i++)
                    {
                        double at = time(last);
                        Vec3 p{r.Coordinate(), r.Coordinate(), r.Coordinate()};
                        int kind = r.Read<int32_t>(), actor = r.Read<int32_t>();
                        if (kind < 1 || kind > 2 ||
                            (!(format >= 9 && actor == 0) &&
                             std::find(ids.begin(), ids.end(), actor) == ids.end()))
                            throw std::runtime_error("Invalid light event");
                        review->flashes.push_back({at, p, kind, actor});
                    }
                    if (format >= 10)
                    {
                        review->lightSources = r.Read<uint32_t>();
                        review->lightRejected = r.Count(2147483647);
                        if (review->lightSources & ~uint32_t{255})
                            throw std::runtime_error("Invalid light observer sources");
                    }
                }
                d->review = std::move(review);
            }
        }
        if (format >= 10)
        {
            d->movementTick = r.Coordinate();
            d->movementSprintStep = r.Coordinate();
            if (d->movementTick < 0 || d->movementTick > 1 || d->movementSprintStep < 0 ||
                d->movementSprintStep > 1 || (d->movementTick == 0) != (d->movementSprintStep == 0))
                throw std::runtime_error("Invalid captured movement model");
        }
        if (format >= 11 && r.Flag())
        {
            auto m = std::make_shared<MovementData>();
            m->scene = r.Read<int32_t>();
            m->revision = r.Read<int32_t>();
            m->rejected = r.Read<int32_t>();
            m->tick = r.Read<float>();
            m->radius = r.Read<float>();
            m->height = r.Read<float>();
            m->hash = d->hash;
            for (int k = 0; k < 5; k++)
            {
                m->counts[k] = r.Read<int32_t>();
                for (int q = 0; q < 3; q++)
                    m->speeds[k * 3 + q] = r.Read<float>();
                m->lengths[k] = r.Read<float>();
            }
            if (!ValidMovement(*m) || m->scene != d->scene || d->movementTick <= 0 ||
                std::abs(m->tick - d->movementTick) >= .000001f ||
                std::abs(m->radius - d->agentRadius) >= .0001f ||
                std::abs(m->height - d->agentHeight) >= .0001f)
                throw std::runtime_error("Invalid cumulative movement model");
            d->movement = std::move(m);
        }
        if (format >= 12)
        {
            auto info = ReadMotionInfo(r);
            if (info)
            {
                if (!d->review)
                    throw std::runtime_error("Input metadata without a run");
                auto review = std::make_shared<RunReview>(*d->review);
                review->motion = std::move(info);
                d->review = std::move(review);
            }
        }
        if (format >= 13)
        {
            d->physicalCaptured = r.Flag();
            d->physicalOmitted = r.Count(2147483647);
            int count = r.Count(20000), total = 0;
            for (int i = 0; i < count; i++)
            {
                PhysicalPart part;
                part.flags = r.Read<uint32_t>();
                int points = r.Count(64);
                total += points;
                if (points < 2 || total > 60000 || (part.flags & ~(format >= 14 ? 15u : 7u)))
                    throw std::runtime_error("Invalid physical footprint");
                if ((part.flags & 8) && (part.flags != 8 || points != 3))
                    throw std::runtime_error("Invalid physical floor triangle");
                for (int j = 0; j < points; j++)
                    part.points.push_back({r.Coordinate(), r.Coordinate(), r.Coordinate()});
                if (points == 2)
                {
                    if (std::hypot(part.points[0].x - part.points[1].x, part.points[0].z - part.points[1].z) <
                        1e-6)
                        throw std::runtime_error("Degenerate physical segment");
                }
                else
                {
                    double sign = 0, area = 0;
                    for (int k = 0; k < points; k++)
                    {
                        auto a = part.points[k], b = part.points[(k + 1) % points],
                             c = part.points[(k + 2) % points];
                        double cross = (b.x - a.x) * double(c.z - a.z) - (b.z - a.z) * double(c.x - a.x);
                        if (std::abs(cross) > 1e-8)
                        {
                            if (sign * cross < 0)
                                throw std::runtime_error("Non-convex physical footprint");
                            sign = cross;
                        }
                        area += a.x * double(b.z) - a.z * double(b.x);
                    }
                    if (area <= 1e-8)
                        throw std::runtime_error("Degenerate or reversed physical footprint");
                }
                d->physicalParts.push_back(std::move(part));
            }
            count = r.Count(1024);
            if (count && static_cast<size_t>(count) != d->points.size())
                throw std::runtime_error("Pickup regions do not match the catalog");
            for (int i = 0; i < count; i++)
            {
                PickupRegion p;
                p.verified = r.Flag();
                p.center = {r.Coordinate(), r.Coordinate(), r.Coordinate()};
                p.radius = r.Coordinate();
                if (p.radius < 0 || p.radius > 10 || (p.verified && p.radius <= 0))
                    throw std::runtime_error("Invalid pickup region");
                d->pickupRegions.push_back(p);
            }
            if (!d->physicalCaptured && !d->physicalParts.empty())
                throw std::runtime_error("Unexpected physical geometry");
        }
        if (format >= 8)
        {
            uint64_t checksum = 14695981039346656037ull;
            for (size_t i = 0; i < bytes.size() - 8; i++)
            {
                checksum ^= bytes[i];
                checksum *= 1099511628211ull;
            }
            if (r.Read<uint64_t>() != checksum)
                throw std::runtime_error("Map checksum mismatch");
        }
        if (!r.Done() || MapLevelIndex(d->scene) < 0 || d->pid < 0 || d->epoch < 0 || d->recipe < 0 ||
            d->recipe > 2 || d->sourceKind < 0 || d->sourceKind > 2 || d->liveRecipe < 0 ||
            d->liveRecipe > 2 || d->stage < 0 || d->stage > 3 || d->flags < 0 ||
            (d->flags & ~(format >= 3 ? 255 : 127)) || d->keep < 0 || d->keep > 64 || d->requested < 0 ||
            d->requested > 2000 || d->completed < 0 || d->completed > d->requested ||
            samples != d->completed || d->indices.size() % 3)
            throw std::runtime_error("Invalid map metadata");
        for (int i : d->indices)
            if (i < 0 || static_cast<size_t>(i) >= d->vertices.size())
                throw std::runtime_error("Invalid NavMesh index");
        auto validSpawn = [&](int spawn) {
            return spawn >= 0 && static_cast<size_t>(spawn) < d->spawns.size();
        };
        bool unknownAllowed = d->stage == 3 || (d->flags & 64);
        if (!d->spawns.empty() && ((!validSpawn(d->spawn) && !(unknownAllowed && d->spawn == -1)) ||
                                   (!validSpawn(d->liveSpawn) && !(unknownAllowed && d->liveSpawn == -1))))
            throw std::runtime_error("Invalid spawn index");
        if (!ValidSelection(d->selected, d->points.size()) ||
            !ValidSelection(d->liveSelected, d->points.size()))
            throw std::runtime_error("Invalid crystal selection");
        for (const auto &s : d->samples)
            if (!validSpawn(s.spawn) || !ValidSelection(s.crystals, d->points.size()) ||
                s.crystals.size() != d->selected.size())
                throw std::runtime_error("Invalid sampled selection");
        if (d->stage != 3 && (d->spawns.empty() || d->selected.empty() ||
                              d->selected.size() != std::min(d->points.size(), static_cast<size_t>(d->keep))))
            throw std::runtime_error("Incomplete spawn catalog");
        d->CalculateBounds();
        d->bytes = std::move(bytes);
        error.clear();
        return d;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return {};
    }
}

inline double Distance(Vec3 a, Vec3 b)
{
    return std::sqrt(double(a.x - b.x) * (a.x - b.x) + double(a.y - b.y) * (a.y - b.y) +
                     double(a.z - b.z) * (a.z - b.z));
}

inline Vec3 Midpoint(Vec3 a, Vec3 b)
{
    return {(a.x + b.x) * .5f, (a.y + b.y) * .5f, (a.z + b.z) * .5f};
}
} // namespace practice
