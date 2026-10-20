#pragma once
#include "SharedProtocol.h"
#include "imgui.h"
#include <vector>
#include <map>
#include <utility>
#include <array>
#include "MinimapOptions.h"

inline float PlayerMapHeading(float bodyYaw)
{
    // MouseLook.FixedUpdate moves forward along playerBody.right, not
    // playerBody.forward. Telemetry contains the body's unmodified yaw.
    return std::remainder(bodyYaw + 90.f, 360.f);
}

struct MapTransform
{
    ImVec2 center;
    float worldX = 0, worldZ = 0, scale = 1, cosine = 1, sine = 0;

    MapTransform(ImVec2 screen, float radius, const MinimapOptions &options, float px, float pz,
                 float bodyYaw, float minX, float maxX, float minZ, float maxZ)
        : center(screen)
    {
        worldX = options.follow ? px : (minX + maxX) * .5f;
        worldZ = options.follow ? pz : (minZ + maxZ) * .5f;
        float extent =
            options.follow ? options.range : std::max(1.f, std::hypot(maxX - minX, maxZ - minZ) * .5f);
        scale = radius * options.zoom / extent;
        float angle = options.playerUp ? PlayerMapHeading(bodyYaw) * 3.14159265358979323846f / 180 : 0;
        cosine = std::cos(angle);
        sine = std::sin(angle);
    }

    ImVec2 Project(float x, float z) const
    {
        float dx = x - worldX, dz = z - worldZ;
        return {center.x + (dx * cosine - dz * sine) * scale, center.y - (dx * sine + dz * cosine) * scale};
    }
};

class MapTrail
{
    std::array<ImVec2, 128> points{};
    size_t head = 0, count = 0;
    uint32_t session = 0, epoch = 0, teleport = 0;
    double sampled = 0;

  public:
    void Clear()
    {
        head = count = 0;
        sampled = 0;
    }

    size_t Size() const
    {
        return count;
    }

    ImVec2 At(size_t index) const
    {
        return points[(head + points.size() - count + index) % points.size()];
    }

    void Push(const GameData &data, uint32_t currentSession, double gameTime)
    {
        if (!data.playerReady || !std::isfinite(gameTime))
        {
            Clear();
            return;
        }
        bool reset = session != currentSession || epoch != static_cast<uint32_t>(data.sceneEpoch) ||
                     teleport != static_cast<uint32_t>(data.lastTeleportSerial) || gameTime < sampled ||
                     (count && gameTime - sampled > .5);
        session = currentSession;
        epoch = static_cast<uint32_t>(data.sceneEpoch);
        teleport = static_cast<uint32_t>(data.lastTeleportSerial);
        if (reset)
            Clear();
        if (count && gameTime - sampled < .1)
            return;
        ImVec2 next(data.playerX, data.playerZ);
        if (count)
        {
            ImVec2 before = At(count - 1);
            float dx = next.x - before.x, dz = next.y - before.y;
            if (dx * dx + dz * dz > 144)
                Clear();
            else if (dx * dx + dz * dz < .04f)
            {
                sampled = gameTime;
                return;
            }
        }
        points[head] = next;
        head = (head + 1) % points.size();
        count = std::min(count + 1, points.size());
        sampled = gameTime;
    }
};

inline bool ClipLineToCircle(ImVec2 &a, ImVec2 &b, const ImVec2 &center, float radius)
{
    auto inside = [&](const ImVec2 &p) {
        return (p.x - center.x) * (p.x - center.x) + (p.y - center.y) * (p.y - center.y) <= radius * radius;
    };
    if (!std::isfinite(radius) || radius <= 0 || !ValidCoordinate(a.x) || !ValidCoordinate(a.y) ||
        !ValidCoordinate(b.x) || !ValidCoordinate(b.y))
        return false;
    bool aIn = inside(a), bIn = inside(b);
    if (aIn && bIn)
        return true;
    if (!aIn && !bIn)
    {
        float dx = b.x - a.x, dy = b.y - a.y;
        float fx = a.x - center.x, fy = a.y - center.y;
        float A = dx * dx + dy * dy;
        float B = 2.0f * (fx * dx + fy * dy);
        float C = fx * fx + fy * fy - radius * radius;
        float disc = B * B - 4.0f * A * C;
        if (disc <= 0.0f)
            return false;
        float t1 = (-B - sqrt(disc)) / (2.0f * A);
        float t2 = (-B + sqrt(disc)) / (2.0f * A);
        if (t1 > t2)
            std::swap(t1, t2);
        if (t2 < 0.0f || t1 > 1.0f)
            return false;
        float t1c = std::max(0.0f, t1);
        float t2c = std::min(1.0f, t2);
        float ox = a.x, oy = a.y;
        a = ImVec2(ox + t1c * dx, oy + t1c * dy);
        b = ImVec2(ox + t2c * dx, oy + t2c * dy);
        return true;
    }
    ImVec2 insidePt = aIn ? a : b;
    ImVec2 outsidePt = aIn ? b : a;
    float dx = outsidePt.x - insidePt.x, dy = outsidePt.y - insidePt.y;
    float fx = insidePt.x - center.x, fy = insidePt.y - center.y;
    float A = dx * dx + dy * dy;
    float B = 2.0f * (fx * dx + fy * dy);
    float C = fx * fx + fy * fy - radius * radius;
    float disc = B * B - 4.0f * A * C;
    if (disc <= 0.0f)
        return false;
    float t = (-B + sqrt(disc)) / (2.0f * A);
    if (t < 0.0f || t > 1.0f)
        return false;
    ImVec2 hit = ImVec2(insidePt.x + t * dx, insidePt.y + t * dy);
    if (aIn)
        b = hit;
    else
        a = hit;
    return true;
}

inline void ClipPolygonToCircleReusable(const std::vector<ImVec2> &polygon, const ImVec2 &center,
                                        float radius, std::vector<ImVec2> &current, std::vector<ImVec2> &next)
{
    // Clip against a 32-sided disk. This also handles a wall rectangle covering
    // the whole disk, where none of its four corners lies inside the circle.
    current.clear();
    next.clear();
    if (!std::isfinite(radius) || radius <= 0 || !ValidCoordinate(center.x) || !ValidCoordinate(center.y))
        return;
    for (ImVec2 point : polygon)
        if (!ValidCoordinate(point.x) || !ValidCoordinate(point.y))
            return;
    current.assign(polygon.begin(), polygon.end());
    current.reserve(40);
    next.reserve(40);
    constexpr int sides = 32;
    constexpr float pi = 3.14159265358979323846f;
    static const auto normals = [] {
        std::array<ImVec2, sides> result{};
        for (int side = 0; side < sides; side++)
        {
            float angle = (side + .5f) * 2.f * pi / sides;
            result[side] = ImVec2(std::cos(angle), std::sin(angle));
        }
        return result;
    }();
    float limit = radius * std::cos(pi / sides);
    for (int side = 0; side < sides && !current.empty(); side++)
    {
        float nx = normals[side].x, ny = normals[side].y;
        auto distance = [&](ImVec2 p) { return (p.x - center.x) * nx + (p.y - center.y) * ny - limit; };
        next.clear();
        ImVec2 previous = current.back();
        float before = distance(previous);
        for (ImVec2 point : current)
        {
            float after = distance(point);
            if ((before <= 0) != (after <= 0))
            {
                float t = before / (before - after);
                next.emplace_back(previous.x + t * (point.x - previous.x),
                                  previous.y + t * (point.y - previous.y));
            }
            if (after <= 0)
                next.push_back(point);
            previous = point;
            before = after;
        }
        current.swap(next);
    }
}

inline std::vector<ImVec2> ClipPolygonToCircle(const std::vector<ImVec2> &polygon, const ImVec2 &center,
                                               float radius)
{
    std::vector<ImVec2> current, next;
    ClipPolygonToCircleReusable(polygon, center, radius, current, next);
    return current;
}

inline constexpr float kInvQuant = 100.0f;

inline int Quant(float f)
{
    return std::isfinite(f) ? (int)std::lround(std::clamp(f, -1000000.0f, 1000000.0f) * kInvQuant) : 0;
}

inline float Unquant(int i)
{
    return i / kInvQuant;
}

struct EdgeKey
{
    int type;
    int fixed;
    int start, end;

    bool operator<(const EdgeKey &o) const
    {
        if (type != o.type)
            return type < o.type;
        if (fixed != o.fixed)
            return fixed < o.fixed;
        if (start != o.start)
            return start < o.start;
        return end < o.end;
    }
};

inline void ExtractUniqueEdges(const std::vector<float> &wallBoxes, int wallCount,
                               std::vector<std::pair<ImVec2, ImVec2>> &outEdges)
{
    outEdges.clear();
    wallCount = std::clamp(wallCount, 0, std::min(200, (int)(wallBoxes.size() / 4)));
    std::vector<int> x_coords, z_coords;
    x_coords.reserve(wallCount * 2);
    z_coords.reserve(wallCount * 2);

    for (int i = 0; i < wallCount; i++)
    {
        const float *w = &wallBoxes[i * 4];
        if (w[0] == 0.0f && w[2] == 0.0f && w[1] == 0.0f && w[3] == 0.0f)
            continue;
        int minX = Quant(w[0]), maxX = Quant(w[2]);
        int minZ = Quant(w[1]), maxZ = Quant(w[3]);
        if (minX > maxX)
            std::swap(minX, maxX);
        if (minZ > maxZ)
            std::swap(minZ, maxZ);
        x_coords.push_back(minX);
        x_coords.push_back(maxX);
        z_coords.push_back(minZ);
        z_coords.push_back(maxZ);
    }

    std::sort(x_coords.begin(), x_coords.end());
    x_coords.erase(std::unique(x_coords.begin(), x_coords.end()), x_coords.end());
    std::sort(z_coords.begin(), z_coords.end());
    z_coords.erase(std::unique(z_coords.begin(), z_coords.end()), z_coords.end());

    if (x_coords.size() < 2 || z_coords.size() < 2)
        return;
    std::map<EdgeKey, int> edgeCount;

    for (int i = 0; i < wallCount; i++)
    {
        const float *w = &wallBoxes[i * 4];
        if (w[0] == 0.0f && w[2] == 0.0f && w[1] == 0.0f && w[3] == 0.0f)
            continue;
        int minX = Quant(w[0]), maxX = Quant(w[2]);
        int minZ = Quant(w[1]), maxZ = Quant(w[3]);
        if (minX > maxX)
            std::swap(minX, maxX);
        if (minZ > maxZ)
            std::swap(minZ, maxZ);

        auto x_start = std::lower_bound(x_coords.begin(), x_coords.end(), minX);
        for (auto it = x_start; it != x_coords.end() && (it + 1) != x_coords.end(); ++it)
        {
            int x1 = *it, x2 = *(it + 1);
            if (x2 > maxX)
                break;
            EdgeKey h1 = {0, minZ, x1, x2};
            EdgeKey h2 = {0, maxZ, x1, x2};
            edgeCount[h1]++;
            edgeCount[h2]++;
        }

        auto z_start = std::lower_bound(z_coords.begin(), z_coords.end(), minZ);
        for (auto it = z_start; it != z_coords.end() && (it + 1) != z_coords.end(); ++it)
        {
            int z1 = *it, z2 = *(it + 1);
            if (z2 > maxZ)
                break;
            EdgeKey v1 = {1, minX, z1, z2};
            EdgeKey v2 = {1, maxX, z1, z2};
            edgeCount[v1]++;
            edgeCount[v2]++;
        }
    }

    for (const auto &kv : edgeCount)
    {
        if (kv.second == 1)
        {
            const EdgeKey &e = kv.first;
            ImVec2 a, b;
            if (e.type == 0)
            {
                a = ImVec2(Unquant(e.start), Unquant(e.fixed));
                b = ImVec2(Unquant(e.end), Unquant(e.fixed));
            }
            else
            {
                a = ImVec2(Unquant(e.fixed), Unquant(e.start));
                b = ImVec2(Unquant(e.fixed), Unquant(e.end));
            }
            outEdges.emplace_back(a, b);
        }
    }
}
