#pragma once
#include "PracticeMapData.h"
#include <bit>
#include <functional>
#include <mutex>
#include <numeric>
#include <queue>
#include <set>

namespace practice
{
// Immutable spatial indexes shared by all workers. The game thread only
// supplies collider footprints; no Physics queries run during a seed search.
class PhysicalNavigation
{
    struct Box
    {
        double x0 = 1e30, z0 = 1e30, x1 = -1e30, z1 = -1e30;

        void Add(Vec3 p)
        {
            x0 = std::min(x0, double(p.x));
            z0 = std::min(z0, double(p.z));
            x1 = std::max(x1, double(p.x));
            z1 = std::max(z1, double(p.z));
        }

        void Add(const Box &b)
        {
            x0 = std::min(x0, b.x0);
            z0 = std::min(z0, b.z0);
            x1 = std::max(x1, b.x1);
            z1 = std::max(z1, b.z1);
        }
    };

    struct Index
    {
        struct Node
        {
            Box bounds;
            int begin = 0, end = 0, left = -1, right = -1;
        };

        std::vector<Box> boxes;
        std::vector<int> order;
        std::vector<Node> nodes;

        int Build(int begin, int end)
        {
            int id = int(nodes.size());
            nodes.push_back({});
            Box box;
            for (int i = begin; i < end; i++)
                box.Add(boxes[order[i]]);
            nodes[id].bounds = box;
            nodes[id].begin = begin;
            nodes[id].end = end;
            if (end - begin > 8)
            {
                bool x = box.x1 - box.x0 > box.z1 - box.z0;
                int mid = (begin + end) / 2;
                std::nth_element(order.begin() + begin, order.begin() + mid, order.begin() + end,
                                 [&](int a, int b) {
                                     return x ? boxes[a].x0 + boxes[a].x1 < boxes[b].x0 + boxes[b].x1
                                              : boxes[a].z0 + boxes[a].z1 < boxes[b].z0 + boxes[b].z1;
                                 });
                int left = Build(begin, mid), right = Build(mid, end);
                nodes[id].left = left;
                nodes[id].right = right;
            }
            return id;
        }

        void Finish()
        {
            order.resize(boxes.size());
            std::iota(order.begin(), order.end(), 0);
            if (!order.empty())
                Build(0, int(order.size()));
        }

        static bool Meets(const Box &box, Vec3 a, Vec3 b, double pad)
        {
            double lo = 0, hi = 1;
            for (int k = 0; k < 2; k++)
            {
                double start = k ? a.z : a.x, delta = k ? b.z - a.z : b.x - a.x,
                       min = (k ? box.z0 : box.x0) - pad, max = (k ? box.z1 : box.x1) + pad;
                if (std::abs(delta) < 1e-12)
                {
                    if (start < min || start > max)
                        return false;
                }
                else
                {
                    double x = (min - start) / delta, y = (max - start) / delta;
                    if (x > y)
                        std::swap(x, y);
                    lo = std::max(lo, x);
                    hi = std::min(hi, y);
                    if (lo > hi)
                        return false;
                }
            }
            return true;
        }

        template <class F> bool Query(int id, Vec3 a, Vec3 b, double pad, F &visit) const
        {
            if (id < 0 || !Meets(nodes[id].bounds, a, b, pad))
                return true;
            const auto &n = nodes[id];
            if (n.left < 0)
            {
                for (int i = n.begin; i < n.end; i++)
                    if (Meets(boxes[order[i]], a, b, pad) && !visit(order[i]))
                        return false;
                return true;
            }
            return Query(n.left, a, b, pad, visit) && Query(n.right, a, b, pad, visit);
        }

        template <class F> bool Query(Vec3 a, Vec3 b, double pad, F visit) const
        {
            return nodes.empty() || Query(0, a, b, pad, visit);
        }
    } solids, floors;

    std::vector<std::vector<Vec3>> polygons;
    std::vector<std::array<Vec3, 3>> triangles;
    bool enabled = false;
    double radius = 0;
    // Geometry is immutable. Repeated corridor repairs and pickup tables ask
    // about the same segments; retain exact endpoint bits in a bounded cache.
    struct Segment
    {
        std::array<uint32_t, 6> endpoints{};
        uint8_t flags = 0; // valid, capsule clear, floor covered
    };
    mutable std::mutex segmentGate;
    mutable std::vector<Segment> segments;

    uint8_t CheckSegment(Vec3 a, Vec3 b) const
    {
        std::array<uint32_t, 6> key{std::bit_cast<uint32_t>(a.x), std::bit_cast<uint32_t>(a.y),
            std::bit_cast<uint32_t>(a.z), std::bit_cast<uint32_t>(b.x), std::bit_cast<uint32_t>(b.y), std::bit_cast<uint32_t>(b.z)};
        uint64_t hash = 14695981039346656037ULL;
        for (auto bits : key) { hash ^= bits; hash *= 1099511628211ULL; }
        size_t slot = size_t(hash ^ (hash >> 32)) & 65535;
        {
            std::lock_guard lock(segmentGate);
            if (segments.empty()) segments.resize(65536);
            const auto &entry = segments[slot];
            if (entry.flags && entry.endpoints == key) return entry.flags;
        }
        bool clear = Clear(a, b);
        uint8_t flags = 1 | (clear ? 2 : 0) | (clear && OnFloor(a, b) ? 4 : 0);
        { std::lock_guard lock(segmentGate); segments[slot] = {key, flags}; }
        return flags;
    }

    static double Cross(Vec3 a, Vec3 b, Vec3 p)
    {
        return (b.x - a.x) * double(p.z - a.z) - (b.z - a.z) * double(p.x - a.x);
    }

    static double PointSegment(Vec3 p, Vec3 a, Vec3 b)
    {
        double x = b.x - a.x, z = b.z - a.z, s = x * x + z * z,
               t = s ? std::clamp(((p.x - a.x) * x + (p.z - a.z) * z) / s, 0., 1.) : 0;
        return std::hypot(p.x - a.x - t * x, p.z - a.z - t * z);
    }

    static bool Intersects(Vec3 a, Vec3 b, Vec3 c, Vec3 d)
    {
        double x = Cross(a, b, c), y = Cross(a, b, d), z = Cross(c, d, a), w = Cross(c, d, b);
        if (((x > 1e-9 && y < -1e-9) || (x < -1e-9 && y > 1e-9)) &&
            ((z > 1e-9 && w < -1e-9) || (z < -1e-9 && w > 1e-9)))
            return true;
        return PointSegment(a, c, d) < 1e-7 || PointSegment(b, c, d) < 1e-7 || PointSegment(c, a, b) < 1e-7 ||
               PointSegment(d, a, b) < 1e-7;
    }

    static bool Inside(Vec3 p, const std::vector<Vec3> &poly)
    {
        if (poly.size() < 3)
            return false;
        bool pos = false, neg = false;
        for (size_t i = 0; i < poly.size(); i++)
        {
            double c = Cross(poly[i], poly[(i + 1) % poly.size()], p);
            pos |= c > 1e-7;
            neg |= c < -1e-7;
        }
        return !(pos && neg);
    }

    bool Blocked(int id, Vec3 a, Vec3 b) const
    {
        const auto &p = polygons[id];
        if (Inside(a, p) || Inside(b, p))
            return true;
        for (size_t i = 0; i < p.size(); i++)
        {
            Vec3 c = p[i], d = p[(i + 1) % p.size()];
            if (Intersects(a, b, c, d) ||
                std::min({PointSegment(a, c, d), PointSegment(b, c, d), PointSegment(c, a, b),
                          PointSegment(d, a, b)}) < radius - 1e-5)
                return true;
        }
        return false;
    }

    bool Surface(Vec3 &p) const
    {
        bool found = false;
        double nearest = .35, targetY = p.y;
        floors.Query(p, p, .001, [&](int id) {
            auto t = triangles[id];
            double area = Cross(t[0], t[1], t[2]);
            if (std::abs(area) < 1e-9)
                return true;
            double u = Cross(t[1], t[2], p) / area, v = Cross(t[2], t[0], p) / area, w = 1 - u - v;
            if (u < -1e-5 || v < -1e-5 || w < -1e-5)
                return true;
            double y = u * t[0].y + v * t[1].y + w * t[2].y, d = std::abs(y - targetY);
            if (d < nearest)
            {
                p.y = float(y);
                nearest = d;
                found = true;
            }
            return true;
        });
        return found;
    }

  public:
    PhysicalNavigation(const MapData &d, std::vector<std::array<Vec3, 3>> floor)
        : triangles(std::move(floor)), enabled(d.physicalCaptured), radius(d.agentRadius + .008)
    {
        std::vector<std::array<Vec3, 3>> ground;
        for (const auto &part : d.physicalParts)
            if ((part.flags & 8) && part.points.size() >= 3)
                for (size_t i = 2; i < part.points.size(); i++)
                    ground.push_back({part.points[0], part.points[i - 1], part.points[i]});
        if (!ground.empty())
            triangles = std::move(ground);
        if (enabled)
            for (const auto &p : d.physicalParts)
                if ((p.flags & 2) && p.points.size() >= 2)
                {
                    Box b;
                    for (auto v : p.points)
                        b.Add(v);
                    solids.boxes.push_back(b);
                    polygons.push_back(p.points);
                }
        solids.Finish();
        for (const auto &t : triangles)
        {
            Box b;
            for (auto v : t)
                b.Add(v);
            floors.boxes.push_back(b);
        }
        floors.Finish();
    }

    bool Adjust(Vec3 &p) const
    {
        if (!enabled)
            return true;
        for (int iteration = 0; iteration < 16; iteration++)
        {
            bool moved = false;
            solids.Query(p, p, radius, [&](int id) {
                if (!Blocked(id, p, p))
                    return true;
                const auto &polygon = polygons[id];
                bool inside = Inside(p, polygon);
                double best = 1e30;
                Vec3 closest;
                double nx = 0, nz = 0;
                for (size_t i = 0; i < polygon.size(); i++)
                {
                    Vec3 a = polygon[i], b = polygon[(i + 1) % polygon.size()];
                    double x = b.x - a.x, z = b.z - a.z, length = x * x + z * z,
                           t = length ? std::clamp(((p.x - a.x) * x + (p.z - a.z) * z) / length, 0., 1.) : 0;
                    Vec3 q{float(a.x + t * x), p.y, float(a.z + t * z)};
                    double distance = std::hypot(p.x - q.x, p.z - q.z);
                    if (distance < best && length > 1e-12)
                    {
                        best = distance;
                        closest = q;
                        double norm = std::sqrt(length);
                        nx = z / norm;
                        nz = -x / norm;
                    }
                }
                if (best == 1e30)
                    return false;
                if (!inside && best > 1e-8)
                {
                    nx = (p.x - closest.x) / best;
                    nz = (p.z - closest.z) / best;
                }
                p.x = float(closest.x + nx * (radius + .012));
                p.z = float(closest.z + nz * (radius + .012));
                moved = true;
                return false;
            });
            if (!moved)
                return true;
        }
        return Clear(p, p);
    }

    bool AdjustOnFloor(Vec3 &p) const
    {
        return Adjust(p) && Surface(p);
    }

    bool Enabled() const
    {
        return enabled;
    }

    bool Clear(Vec3 a, Vec3 b) const
    {
        return !enabled || solids.Query(a, b, radius, [&](int id) { return !Blocked(id, a, b); });
    }

    bool OnFloor(Vec3 a, Vec3 b) const
    {
        std::vector<std::pair<double, double>> spans;
        bool covered = false;
        floors.Query(a, b, .0001, [&](int id) {
            const auto &t = triangles[id];
            double area = Cross(t[0], t[1], t[2]), sign = area < 0 ? -1 : 1, lo = 0, hi = 1;
            for (int e = 0; e < 3; e++)
            {
                double begin = Cross(t[e], t[(e + 1) % 3], a) * sign,
                       end = Cross(t[e], t[(e + 1) % 3], b) * sign, delta = end - begin;
                if (begin < -.0001 && end < -.0001)
                    return true;
                if (std::abs(delta) > 1e-10)
                {
                    double at = (-.0001 - begin) / delta;
                    if (delta > 0)
                        lo = std::max(lo, at);
                    else
                        hi = std::min(hi, at);
                }
            }
            if (lo > hi || lo > 1 || hi < 0)
                return true;
            double at = (std::max(lo, 0.) + std::min(hi, 1.)) * .5;
            Vec3 probe{float(a.x + (b.x - a.x) * at), float(a.y + (b.y - a.y) * at),
                       float(a.z + (b.z - a.z) * at)};
            double u = Cross(t[1], t[2], probe) / area, v = Cross(t[2], t[0], probe) / area,
                   y = u * t[0].y + v * t[1].y + (1 - u - v) * t[2].y;
            if (std::abs(y - probe.y) < .3)
            {
                if (lo <= 0 && hi >= 1) { covered = true; return false; }
                spans.emplace_back(std::max(lo, 0.), std::min(hi, 1.));
            }
            return true;
        });
        if (covered) return true;
        std::sort(spans.begin(), spans.end());
        double end = 0;
        for (auto s : spans)
        {
            if (s.first > end + .0001)
                return false;
            end = std::max(end, s.second);
            if (end >= .9999)
                return true;
        }
        return false;
    }

    bool Visible(Vec3 a, Vec3 b) const
    {
        return (CheckSegment(a, b) & 4) != 0;
    }

    // Small local visibility repairs around physical obstacles, on the same
    // floor triangles as every other mode. Work is bounded and cached in the
    // seed pair table; an unsolved leg remains explicitly unverified.
    std::vector<Vec3> Repair(Vec3 a, Vec3 b, const std::function<bool()> &canceled) const
    {
        if (!Clear(a, a) || !Clear(b, b))
            return {};
        std::vector<Vec3> nodes{a, b};
        std::set<int> added, pending;
        auto collect = [&](Vec3 x, Vec3 y) {
            solids.Query(x, y, radius, [&](int id) {
                if (!added.contains(id) && !pending.contains(id) && Blocked(id, x, y))
                    pending.insert(id);
                return true;
            });
        };
        collect(a, b);
        auto candidate = [&](Vec3 q) {
            double length = std::hypot(b.x - a.x, b.z - a.z),
                   at = length ? std::clamp(((q.x - a.x) * (b.x - a.x) + (q.z - a.z) * (b.z - a.z)) /
                                                (length * length),
                                            0., 1.)
                               : 0;
            q.y = float(a.y + (b.y - a.y) * at);
            if (nodes.size() < 1024 && Surface(q) && Clear(q, q) &&
                std::none_of(nodes.begin(), nodes.end(), [&](Vec3 n) { return Distance(n, q) < .002; }))
                nodes.push_back(q);
        };
        std::vector<std::vector<double>> edges;
        for (int round = 0; round < 8 && !pending.empty(); round++)
        {
            if (canceled())
                throw std::runtime_error("Canceled");
            auto current = std::move(pending);
            pending.clear();
            for (int id : current)
            {
                if (added.size() >= 192 || nodes.size() >= 1024)
                    return {};
                added.insert(id);
                const auto &p = polygons[id];
                // A clipped vertical mesh triangle projects to a segment, not
                // an area. Inflate its endpoints by the same player radius as
                // every polygon corner; otherwise it blocks paths but can
                // never contribute a valid detour to this visibility graph.
                if (p.size() == 2)
                {
                    double distance = (radius + .012) / std::cos(3.141592653589793 / 16);
                    for (auto v : p)
                        for (int k = 0; k < 16; k++)
                        {
                            double angle = k * .39269908169872414;
                            candidate({float(v.x + std::cos(angle) * distance), float(a.y),
                                       float(v.z + std::sin(angle) * distance)});
                        }
                    continue;
                }
                double winding = 0;
                for (size_t k = 0; k < p.size(); k++)
                    winding +=
                        p[k].x * double(p[(k + 1) % p.size()].z) - p[k].z * double(p[(k + 1) % p.size()].x);
                if (p.size() < 3 || winding <= 1e-8)
                    continue;
                for (size_t k = 0; k < p.size(); k++)
                {
                    Vec3 prev = p[(k + p.size() - 1) % p.size()], v = p[k], next = p[(k + 1) % p.size()];
                    double angle = std::atan2(-(v.x - prev.x), v.z - prev.z),
                           finish = std::atan2(-(next.x - v.x), next.z - v.z);
                    while (finish < angle)
                        finish += 6.283185307179586;
                    double arc = finish - angle;
                    if (arc > 3.1416)
                        continue;
                    int steps = std::max(1, int(std::ceil(arc / .3926990817)));
                    double distance = (radius + .012) / std::cos(arc / (2 * steps));
                    for (int step = 0; step <= steps; step++)
                    {
                        double turn = angle + arc * step / steps;
                        candidate({float(v.x + std::cos(turn) * distance), float(a.y),
                                   float(v.z + std::sin(turn) * distance)});
                    }
                }
            }
            size_t old = edges.size();
            edges.resize(nodes.size());
            for (auto &row : edges)
                row.resize(nodes.size(), -1);
            for (size_t i = 0; i < nodes.size(); i++)
                for (size_t j = std::max(i + 1, old); j < nodes.size(); j++)
                {
                    if ((j & 31) == 0 && canceled())
                        throw std::runtime_error("Canceled");
                    auto segment = CheckSegment(nodes[i], nodes[j]);
                    bool clear = (segment & 2) != 0;
                    edges[i][j] = edges[j][i] = (segment & 4) != 0
                                                    ? Distance(nodes[i], nodes[j])
                                                    : std::numeric_limits<double>::infinity();
                    if (!clear)
                        collect(nodes[i], nodes[j]);
                }
            std::vector<double> costs(nodes.size(), std::numeric_limits<double>::infinity());
            std::vector<int> parent(nodes.size(), -1);
            using Item = std::pair<double, int>;
            std::priority_queue<Item, std::vector<Item>, std::greater<Item>> queue;
            costs[0] = 0;
            queue.push({0, 0});
            while (!queue.empty())
            {
                auto [cost, id] = queue.top();
                queue.pop();
                if (cost != costs[id])
                    continue;
                if (id == 1)
                    break;
                for (size_t j = 0; j < nodes.size(); j++)
                    if (j != size_t(id) && edges[id][j] >= 0 && cost + edges[id][j] < costs[j])
                    {
                        costs[j] = cost + edges[id][j];
                        parent[j] = id;
                        queue.push({costs[j], int(j)});
                    }
            }
            if (std::isfinite(costs[1]))
            {
                std::vector<Vec3> result;
                for (int id = 1; id >= 0; id = parent[id])
                    result.push_back(nodes[id]);
                std::reverse(result.begin(), result.end());
                return result;
            }
        }
        return {};
    }
};
} // namespace practice
