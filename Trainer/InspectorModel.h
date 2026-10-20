#pragma once
#include "PracticeRouting.h"
#include <unordered_set>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <optional>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace inspector
{
inline constexpr uint32_t Magic = 0x504e5349, Version = 3;
inline constexpr size_t CatalogOffset = 256, CatalogCapacity = 4194304,
                        DetailOffset = CatalogOffset + CatalogCapacity, DetailCapacity = 1048576,
                        HoverOffset = DetailOffset + DetailCapacity, HoverCapacity = 16384,
                        Capacity = HoverOffset + HoverCapacity;

struct Header
{
    int pid = 0, scene = -1, frame = 0;
    uint32_t session = 0, epoch = 0, revision = 0;
    int64_t utc = 0;
    double seconds = 0;
};

struct Object
{
    int id = 0, parent = 0, scene = 0, layer = 0;
    uint32_t flags = 0;
    std::string name;
};

struct Value
{
    uint64_t key = 0;
    int component = 0;
    uint32_t flags = 0;
    std::string group, name, source, value;
};

// Merge noncontiguous Object/Transform fields into one visible section.
// Each section and row keeps its own ID scope when groups recur in the wire.
struct ValueGroup
{
    std::string name;
    std::vector<size_t> values;
};

inline std::vector<ValueGroup> GroupValues(const std::vector<Value> &values)
{
    std::vector<ValueGroup> groups;
    std::unordered_map<std::string, size_t> index;
    for (size_t i = 0; i < values.size(); i++)
    {
        const auto &name = values[i].group;
        auto found = index.find(name);
        if (found == index.end())
        {
            index.emplace(name, groups.size());
            groups.push_back({name, {i}});
        }
        else
            groups[found->second].values.push_back(i);
    }
    return groups;
}

struct Change
{
    uint64_t key = 0;
    int frame = 0;
    double seconds = 0;
    std::string before, after;
};

struct Hit
{
    int id = 0, kind = 0;
    float distance = 0;
    std::string name;
};

struct Geometry
{
    int id = 0, frame = -1;
    uint32_t flags = 0;
    std::string type, reason;
    double seconds = 0;
    float low = 0, high = 0, floor = 0, radius = 0, height = 0;
    bool grounded = false, inactiveAccepted = false, partial = false;
    std::vector<practice::PhysicalPart> parts;
};

struct Catalog
{
    Header header;
    std::string sceneName;
    bool truncated = false;
    std::vector<Object> objects;
};

struct Detail
{
    Header header;
    bool active = false, ackSuccess = false, scanning = false, truncated = false, hasA = false, hasB = false;
    uint32_t ack = 0, options = 0, geometryRevision = 0;
    int ackAction = 0, selected = 0, scanned = 0, changesDropped = 0, valuesDropped = 0;
    std::string status, name, path, pickSource;
    double scanMs = 0, detailMs = 0;
    practice::Vec3 camera, rotation, a, b;
    float fov = 0;
    std::vector<Value> values;
    std::vector<Change> changes;
    std::vector<Hit> hits;
    std::vector<Geometry> geometry;
};

struct Command
{
    uint32_t serial = 0, epoch = 0, flags = 0;
    int action = 0, object = 0;
    float x = 0, y = 0;
    uint64_t key = 0;

    bool Valid() const
    {
        return action >= 0 && action <= 9 && !(flags & ~63u) && std::isfinite(x) && std::isfinite(y) &&
               x >= 0 && x <= 1 && y >= 0 && y <= 1;
    }
};

struct Pointer
{
    uint32_t serial = 0, epoch = 0;
    bool active = false;
    float x = 0, y = 0;
    int width = 0, height = 0, action = 3, preferred = 0;

    bool Valid() const
    {
        return serial && std::isfinite(x) && std::isfinite(y) && x >= 0 && x <= 1 && y >= 0 && y <= 1 &&
               width > 0 && width <= 32768 && height > 0 && height <= 32768 &&
               (action == 3 || action == 7 || action == 8);
    }
};

inline std::optional<Pointer> NormalizePointer(double screenX, double screenY, double left, double top,
                                               int width, int height)
{
    if (!std::isfinite(screenX) || !std::isfinite(screenY) || !std::isfinite(left) || !std::isfinite(top) ||
        width <= 0 || height <= 0 || width > 32768 || height > 32768)
        return {};
    double x = (screenX - left) / width, y = (screenY - top) / height;
    if (x < 0 || x >= 1 || y < 0 || y >= 1)
        return {};
    Pointer p;
    p.x = float(x);
    p.y = float(1 - y);
    p.width = width;
    p.height = height;
    return p;
}

struct PickTarget
{
    int id = 0, kind = 0;
    float distance = 0;
    std::string name, type;
};

struct Hover
{
    Header header;
    uint32_t serial = 0;
    bool active = false, outline = false;
    int object = 0, kind = 0, cameraId = 0, width = 0, height = 0;
    float x = 0, y = 0, distance = 0, left = 0, top = 0, right = 0, bottom = 0;
    std::array<float, 4> viewport{};
    std::string name, type, status, camera;
    bool partial = false, scanning = false;
    std::vector<PickTarget> targets;
};

inline uint64_t Hash(const std::vector<uint8_t> &bytes, size_t n)
{
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < n; i++)
    {
        h ^= bytes[i];
        h *= 1099511628211ull;
    }
    return h;
}

inline std::string Text(practice::MapReader &r, int maximum)
{
    auto s = r.Text(maximum);
    size_t at = 0;
    while (at < s.size())
    {
        uint8_t a = uint8_t(s[at++]);
        if (a < 128)
            continue;
        int n = 0;
        uint32_t cp = 0, minimum = 0;
        if (a >= 0xc2 && a <= 0xdf)
        {
            n = 1;
            cp = a & 31;
            minimum = 128;
        }
        else if (a >= 0xe0 && a <= 0xef)
        {
            n = 2;
            cp = a & 15;
            minimum = 2048;
        }
        else if (a >= 0xf0 && a <= 0xf4)
        {
            n = 3;
            cp = a & 7;
            minimum = 65536;
        }
        else
            throw std::runtime_error("Invalid inspector UTF-8");
        for (int i = 0; i < n; i++)
        {
            if (at == s.size() || (uint8_t(s[at]) & 0xc0) != 0x80)
                throw std::runtime_error("Invalid inspector UTF-8");
            cp = (cp << 6) | (uint8_t(s[at++]) & 63);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            throw std::runtime_error("Invalid inspector Unicode");
    }
    return s;
}

inline double Number(practice::MapReader &r)
{
    double n = r.Read<double>();
    if (!std::isfinite(n) || n < 0 || n > 1e15)
        throw std::runtime_error("Invalid inspector time");
    return n;
}

inline practice::Vec3 Vector(practice::MapReader &r)
{
    return {r.Coordinate(), r.Coordinate(), r.Coordinate()};
}

inline Header ReadHeader(practice::MapReader &r, int kind)
{
    if (r.Read<uint32_t>() != Magic || r.Read<int32_t>() != int(Version) || r.Read<int32_t>() != kind)
        throw std::runtime_error("Unsupported inspector packet");
    Header h;
    h.pid = r.Read<int32_t>();
    h.session = r.Read<uint32_t>();
    h.scene = r.Read<int32_t>();
    h.epoch = r.Read<uint32_t>();
    h.revision = r.Read<uint32_t>();
    h.utc = r.Read<int64_t>();
    h.seconds = Number(r);
    h.frame = r.Read<int32_t>();
    if (h.pid <= 0 || h.scene < -1 || h.scene > 10000 || h.frame < 0 || h.utc < 0 ||
        h.utc > 3155378975999999999LL)
        throw std::runtime_error("Invalid inspector identity");
    return h;
}

inline void CheckBytes(const std::vector<uint8_t> &b, size_t capacity)
{
    if (b.size() < 60 || b.size() > capacity)
        throw std::runtime_error("Invalid inspector publication size");
    uint64_t h = 0;
    std::memcpy(&h, b.data() + b.size() - 8, 8);
    if (h != Hash(b, b.size() - 8))
        throw std::runtime_error("Inspector checksum mismatch");
}

inline std::shared_ptr<const Catalog> DecodeCatalog(const std::vector<uint8_t> &bytes)
{
    CheckBytes(bytes, CatalogCapacity);
    practice::MapReader r(bytes);
    auto d = std::make_shared<Catalog>();
    d->header = ReadHeader(r, 1);
    d->sceneName = Text(r, 512);
    d->truncated = r.Flag();
    int n = r.Count(20000);
    std::unordered_set<int> ids;
    d->objects.reserve(n);
    for (int i = 0; i < n; i++)
    {
        Object o;
        o.id = r.Read<int32_t>();
        o.parent = r.Read<int32_t>();
        o.scene = r.Read<int32_t>();
        o.layer = r.Read<int32_t>();
        o.flags = r.Read<uint32_t>();
        o.name = Text(r, 768);
        if (!o.id || o.id == o.parent || o.layer < 0 || o.layer > 31 || (o.flags & ~31u) ||
            !ids.insert(o.id).second)
            throw std::runtime_error("Invalid inspector object");
        d->objects.push_back(std::move(o));
    }
    r.Read<uint64_t>();
    if (!r.Done())
        throw std::runtime_error("Unexpected inspector catalog tail");
    return d;
}

inline std::shared_ptr<const Detail> DecodeDetail(const std::vector<uint8_t> &bytes)
{
    CheckBytes(bytes, DetailCapacity);
    practice::MapReader r(bytes);
    auto d = std::make_shared<Detail>();
    d->header = ReadHeader(r, 2);
    d->active = r.Flag();
    d->ack = r.Read<uint32_t>();
    d->ackAction = r.Read<int32_t>();
    d->ackSuccess = r.Flag();
    if (d->ackAction < 0 || d->ackAction > 9)
        throw std::runtime_error("Invalid inspector acknowledgement");
    d->selected = r.Read<int32_t>();
    d->status = Text(r, 2048);
    d->name = Text(r, 768);
    d->path = Text(r, 4096);
    d->pickSource = Text(r, 1024);
    d->scanning = r.Flag();
    d->truncated = r.Flag();
    d->scanned = r.Count(20000);
    d->scanMs = Number(r);
    d->detailMs = Number(r);
    d->options = r.Read<uint32_t>();
    d->changesDropped = r.Count(2147483647);
    d->valuesDropped = r.Count(512);
    d->geometryRevision = r.Read<uint32_t>();
    if (d->options & ~63u)
        throw std::runtime_error("Invalid inspector options");
    d->camera = Vector(r);
    d->rotation = Vector(r);
    d->fov = r.Coordinate();
    if (d->fov < 0 || d->fov >= 180)
        throw std::runtime_error("Invalid inspector FOV");
    int n = r.Count(512), watchCount = 0;
    std::unordered_set<uint64_t> keys;
    d->values.reserve(n);
    for (int i = 0; i < n; i++)
    {
        Value v;
        v.key = r.Read<uint64_t>();
        v.component = r.Read<int32_t>();
        v.flags = r.Read<uint32_t>();
        v.group = Text(r, 512);
        v.name = Text(r, 512);
        v.source = Text(r, 1024);
        v.value = Text(r, 2048);
        watchCount += bool(v.flags & 4);
        if ((v.flags & ~7u) || !keys.insert(v.key).second || watchCount > 8)
            throw std::runtime_error("Invalid inspector field");
        d->values.push_back(std::move(v));
    }
    n = r.Count(128);
    double previous = 0;
    for (int i = 0; i < n; i++)
    {
        Change c;
        c.key = r.Read<uint64_t>();
        c.frame = r.Read<int32_t>();
        c.seconds = Number(r);
        c.before = Text(r, 2048);
        c.after = Text(r, 2048);
        if (c.frame < 0 || c.frame > d->header.frame || c.seconds < previous ||
            c.seconds > d->header.seconds + .001)
            throw std::runtime_error("Invalid inspector observation");
        previous = c.seconds;
        d->changes.push_back(std::move(c));
    }
    n = r.Count(32);
    for (int i = 0; i < n; i++)
    {
        Hit h;
        h.id = r.Read<int32_t>();
        h.kind = r.Read<int32_t>();
        h.distance = r.Coordinate();
        h.name = Text(r, 768);
        if (!h.id || h.kind < 1 || h.kind > 4 || h.distance < 0)
            throw std::runtime_error("Invalid inspector pick");
        d->hits.push_back(std::move(h));
    }
    d->hasA = r.Flag();
    d->a = Vector(r);
    d->hasB = r.Flag();
    d->b = Vector(r);
    n = r.Count(32);
    int points = 0, parts = 0;
    for (int i = 0; i < n; i++)
    {
        Geometry g;
        g.id = r.Read<int32_t>();
        g.type = Text(r, 512);
        g.flags = r.Read<uint32_t>();
        g.reason = Text(r, 2048);
        g.frame = r.Read<int32_t>();
        g.seconds = Number(r);
        g.low = r.Coordinate();
        g.high = r.Coordinate();
        g.floor = r.Coordinate();
        g.radius = r.Coordinate();
        g.height = r.Coordinate();
        g.grounded = r.Flag();
        g.inactiveAccepted = r.Flag();
        if ((g.flags & ~7u) || g.frame < -1 || g.frame > d->header.frame ||
            g.seconds > d->header.seconds + .001 || g.radius < 0 || g.height < 0 || g.high < g.low)
            throw std::runtime_error("Invalid inspector capture metadata");
        int count = r.Count(512);
        parts += count;
        if (parts > 512)
            throw std::runtime_error("Inspector footprint count exceeds limit");
        g.partial = r.Flag();
        for (int k = 0; k < count; k++)
        {
            practice::PhysicalPart p;
            p.flags = r.Read<uint32_t>();
            p.points = r.Vectors(64);
            points += int(p.points.size());
            if ((p.flags & ~7u) || p.points.size() < 2 || points > 4096)
                throw std::runtime_error("Invalid inspector footprint");
            g.parts.push_back(std::move(p));
        }
        d->geometry.push_back(std::move(g));
    }
    r.Read<uint64_t>();
    if (!r.Done() || (!d->active && (!d->values.empty() || !d->geometry.empty())))
        throw std::runtime_error("Invalid inspector detail tail");
    return d;
}

inline std::shared_ptr<const Hover> DecodeHover(const std::vector<uint8_t> &bytes)
{
    CheckBytes(bytes, HoverCapacity);
    practice::MapReader r(bytes);
    auto h = std::make_shared<Hover>();
    h->header = ReadHeader(r, 3);
    h->serial = r.Read<uint32_t>();
    h->active = r.Flag();
    h->object = r.Read<int32_t>();
    h->kind = r.Read<int32_t>();
    h->x = r.Coordinate();
    h->y = r.Coordinate();
    h->distance = r.Coordinate();
    h->outline = r.Flag();
    h->left = r.Coordinate();
    h->top = r.Coordinate();
    h->right = r.Coordinate();
    h->bottom = r.Coordinate();
    h->name = Text(r, 768);
    h->type = Text(r, 256);
    h->status = Text(r, 1024);
    h->cameraId = r.Read<int32_t>();
    h->camera = Text(r, 512);
    h->width = r.Count(32768);
    h->height = r.Count(32768);
    for (auto &v : h->viewport)
        v = r.Coordinate();
    auto normalized = [](float v) { return v >= 0 && v <= 1; };
    if (!normalized(h->x) || !normalized(h->y) || !normalized(h->left) || !normalized(h->top) ||
        !normalized(h->right) || !normalized(h->bottom) || h->distance < 0 || h->kind < 0 || h->kind > 4 ||
        (!h->active && h->object) || (!h->object && (h->kind || h->outline)) || (h->object && !h->kind) ||
        (h->active && (!h->serial || !h->width || !h->height)) || h->viewport[2] < 0 || h->viewport[3] < 0 ||
        (h->outline && (h->right <= h->left || h->bottom <= h->top)))
        throw std::runtime_error("Invalid inspector hover");
    h->partial = r.Flag();
    h->scanning = r.Flag();
    int count = r.Count(32);
    std::unordered_set<int> ids;
    bool selected = false;
    for (int i = 0; i < count; i++)
    {
        PickTarget t;
        t.id = r.Read<int32_t>();
        t.kind = r.Read<int32_t>();
        t.distance = r.Coordinate();
        t.name = Text(r, 256);
        t.type = Text(r, 128);
        if (!t.id || t.kind < 1 || t.kind > 4 || t.distance < 0 || t.distance > 10000 ||
            !ids.insert(t.id).second)
            throw std::runtime_error("Invalid hover target");
        if (t.id == h->object)
        {
            if (t.kind != h->kind || std::abs(t.distance - h->distance) > .001f)
                throw std::runtime_error("Hover target mismatch");
            selected = true;
        }
        h->targets.push_back(std::move(t));
    }
    if ((h->object && !selected) || (!h->active && !h->targets.empty()))
        throw std::runtime_error("Invalid hover selection");
    r.Read<uint64_t>();
    if (!r.Done())
        throw std::runtime_error("Unexpected hover tail");
    return h;
}

inline bool SameScene(const Header &a, const Header &b)
{
    return a.pid == b.pid && a.session == b.session && a.scene == b.scene && a.epoch == b.epoch;
}

struct Difference
{
    std::string group, name, source, before, after;
};

inline std::vector<Difference> Compare(const Detail &a, const Detail &b)
{
    if (!SameScene(a.header, b.header) || a.selected != b.selected || !a.selected || (a.options & 16) == 0 ||
        (b.options & 16) == 0)
        throw std::runtime_error("Snapshots must belong to the same live object and scene");
    std::unordered_map<uint64_t, const Value *> old;
    for (const auto &v : a.values)
        old[v.key] = &v;
    std::vector<Difference> out;
    for (const auto &v : b.values)
    {
        auto it = old.find(v.key);
        if (it == old.end())
            out.push_back({v.group, v.name, v.source, "<not observed>", v.value});
        else
        {
            if (it->second->value != v.value)
                out.push_back({v.group, v.name, v.source, it->second->value, v.value});
            old.erase(it);
        }
    }
    for (const auto &item : old)
    {
        const auto &v = *item.second;
        out.push_back({v.group, v.name, v.source, v.value, "<not observed>"});
    }
    return out;
}

inline bool SamePoint(practice::Vec3 a, practice::Vec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

struct Probe
{
    bool ready = false, physical = false, projectedA = false, projectedB = false, sameComponent = false;
    practice::Vec3 a, b, start, end;
    practice::NavPath path;
    std::string message;
    int64_t mapUtc = 0;
};

inline Probe ProbeRoute(
    const practice::MapData &data, practice::Vec3 a, practice::Vec3 b,
    const practice::Cancel &cancel = [] { return false; })
{
    Probe p;
    p.ready = true;
    p.physical = data.physicalCaptured && data.physicalOmitted == 0;
    p.a = a;
    p.b = b;
    p.mapUtc = data.capturedUtc;
    // Deliberately use the common floor model, not captured-corridor node IDs.
    practice::NavGraph graph(data, cancel, true);
    auto from = graph.FloorProject(a), to = graph.FloorProject(b);
    p.projectedA = from.triangle >= 0;
    p.projectedB = to.triangle >= 0;
    p.start = from.point;
    p.end = to.point;
    if (!p.projectedA || !p.projectedB)
    {
        p.message = "An endpoint has no supported standing-floor projection";
        return p;
    }
    p.sameComponent = graph.Component(from) == graph.Component(to);
    if (!p.sameComponent)
    {
        p.message = "Projected endpoints belong to disconnected floor components";
        return p;
    }
    p.path = graph.Path(graph.Search(from, cancel), to, cancel);
    if (!std::isfinite(p.path.length))
        p.message = "No checked route found within the common floor / clearance repair budget";
    else
        p.message = p.physical ? "Route checked against captured floor and static player clearance"
                               : "Navigation route only: physical capture is absent or incomplete";
    return p;
}

inline std::string Quote(const std::string &s)
{
    std::ostringstream o;
    o << '"';
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\')
            o << '\\' << char(c);
        else if (c < 32)
            o << "\\u00" << "0123456789abcdef"[c >> 4] << "0123456789abcdef"[c & 15];
        else
            o << char(c);
    }
    o << '"';
    return o.str();
}

struct ReportContext
{
    std::string version, game, unity, assemblyHash;
    int seed = 0, seedKind = 0;
    bool seedKnown = false;
    const Probe *probe = nullptr;
};

inline void WriteVector(std::ostream &o, practice::Vec3 v)
{
    o << '[' << v.x << ',' << v.y << ',' << v.z << ']';
}

inline std::string Report(const Detail &d, const Detail *baseline, const std::string &diagnostics,
                          const ReportContext &context)
{
    std::ostringstream o;
    o.imbue(std::locale::classic());
    o << std::setprecision(17);
    o << "{\n  \"format\": 1,\n  \"tool\": " << Quote(context.version)
      << ", \"author\": \"mab\",\n  \"scope\": \"Read-only live observations; runtime IDs are session-local, not a save state or attestation\",\n  \"pid\": "
      << d.header.pid << ", \"session\": " << d.header.session << ", \"scene\": " << d.header.scene
      << ", \"epoch\": " << d.header.epoch
      << ", \"seed\": " << (context.seedKnown ? std::to_string(context.seed) : "null")
      << ", \"seedKind\": " << (context.seedKnown ? context.seedKind : 0)
      << ",\n  \"game\": " << Quote(context.game) << ", \"unity\": " << Quote(context.unity)
      << ", \"assemblyFileSha256\": " << Quote(context.assemblyHash)
      << ",\n  \"capturedUtcTicks\": " << d.header.utc << ", \"observedFrame\": " << d.header.frame
      << ", \"seconds\": " << d.header.seconds << ",\n  \"objectId\": " << d.selected
      << ", \"name\": " << Quote(d.name) << ", \"hierarchy\": " << Quote(d.path)
      << ",\n  \"pickSource\": " << Quote(d.pickSource)
      << ", \"catalogPartial\": " << (d.truncated ? "true" : "false") << ", \"scannedObjects\": " << d.scanned
      << ", \"scanMilliseconds\": " << d.scanMs << ", \"previousDetailBuildMilliseconds\": " << d.detailMs
      << ", \"geometryRevision\": " << d.geometryRevision << ",\n  \"camera\": [" << d.camera.x << ','
      << d.camera.y << ',' << d.camera.z << ',' << d.rotation.x << ',' << d.rotation.y << ',' << d.rotation.z
      << ',' << d.fov << "],\n  \"values\": [";
    bool comma = false;
    for (const auto &v : d.values)
    {
        if (comma)
            o << ',';
        comma = true;
        o << "\n    {\"key\": " << Quote(std::to_string(v.key)) << ", \"component\": " << v.component
          << ", \"group\": " << Quote(v.group) << ", \"name\": " << Quote(v.name)
          << ", \"source\": " << Quote(v.source) << ", \"value\": " << Quote(v.value)
          << ", \"flags\": " << v.flags << '}';
    }
    o << "\n  ],\n  \"detailLevel\": " << Quote(d.options & 16 ? "full" : "summary")
      << ",\n  \"valuesDropped\": " << d.valuesDropped << ",\n  \"geometry\": [";
    comma = false;
    for (const auto &g : d.geometry)
    {
        if (comma)
            o << ',';
        comma = true;
        o << "\n    {\"collider\": " << g.id << ", \"type\": " << Quote(g.type)
          << ", \"reason\": " << Quote(g.reason) << ", \"liveFlags\": " << g.flags
          << ", \"partial\": " << (g.partial ? "true" : "false") << ", \"captureFrame\": " << g.frame
          << ", \"captureSeconds\": " << g.seconds << ", \"low\": " << g.low << ", \"high\": " << g.high
          << ", \"floor\": " << g.floor << ", \"playerRadius\": " << g.radius
          << ", \"playerHeight\": " << g.height
          << ", \"groundedAtCapture\": " << (g.grounded ? "true" : "false")
          << ", \"inactiveAcceptedAtCapture\": " << (g.inactiveAccepted ? "true" : "false")
          << ", \"parts\": [";
        bool partComma = false;
        for (const auto &p : g.parts)
        {
            if (partComma)
                o << ',';
            partComma = true;
            o << "{\"flags\":" << p.flags << ",\"points\":[";
            bool pointComma = false;
            for (auto v : p.points)
            {
                if (pointComma)
                    o << ',';
                pointComma = true;
                WriteVector(o, v);
            }
            o << "]}";
        }
        o << "]}";
    }
    o << "\n  ],\n  \"observedChanges\": [";
    comma = false;
    for (const auto &c : d.changes)
    {
        if (comma)
            o << ',';
        comma = true;
        o << "{\"key\":" << Quote(std::to_string(c.key)) << ",\"frame\":" << c.frame
          << ",\"seconds\":" << c.seconds << ",\"before\":" << Quote(c.before)
          << ",\"after\":" << Quote(c.after) << '}';
    }
    o << "],\n  \"changesDropped\": " << d.changesDropped
      << ",\n  \"comparisonBaselineFrame\": " << (baseline ? std::to_string(baseline->header.frame) : "null")
      << ",\n  \"comparison\": [";
    comma = false;
    if (baseline)
        for (const auto &c : Compare(*baseline, d))
        {
            if (comma)
                o << ',';
            comma = true;
            o << "{\"source\":" << Quote(c.source) << ",\"before\":" << Quote(c.before)
              << ",\"after\":" << Quote(c.after) << '}';
        }
    o << "],\n  \"probeStart\": ";
    if (d.hasA)
        WriteVector(o, d.a);
    else
        o << "null";
    o << ", \"probeEnd\": ";
    if (d.hasB)
        WriteVector(o, d.b);
    else
        o << "null";
    o << ",\n  \"probeResult\": ";
    if (context.probe)
    {
        const auto &p = *context.probe;
        o << "{\"mapUtcTicks\":" << p.mapUtc << ",\"message\":" << Quote(p.message)
          << ",\"physical\":" << (p.physical ? "true" : "false") << ",\"projectedStart\":";
        if (p.projectedA)
            WriteVector(o, p.start);
        else
            o << "null";
        o << ",\"projectedEnd\":";
        if (p.projectedB)
            WriteVector(o, p.end);
        else
            o << "null";
        o << ",\"length\":";
        if (std::isfinite(p.path.length))
            o << p.path.length;
        else
            o << "null";
        o << ",\"path\":[";
        comma = false;
        for (auto v : p.path.points)
        {
            if (comma)
                o << ',';
            comma = true;
            WriteVector(o, v);
        }
        o << "]}";
    }
    else
        o << "null";
    o << ",\n  \"diagnostics\": " << Quote(diagnostics) << "\n}\n";
    return o.str();
}

inline std::filesystem::path SaveReport(const std::filesystem::path &folder, const std::string &body,
                                        int64_t utc)
{
    if (body.size() > 4 * 1024 * 1024)
        throw std::runtime_error("Inspector report exceeds its file budget");
    std::filesystem::create_directories(folder);
    for (int n = 0; n < 1000; n++)
    {
        auto path = folder / ("inspector-" + std::to_string(utc) + "-" + std::to_string(n) + ".json");
        size_t done = 0;
        bool failed = false;
#ifdef _WIN32
        HANDLE file =
            CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
        {
            auto error = GetLastError();
            if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
                continue;
            throw std::runtime_error("Could not create inspector report");
        }
        while (done < body.size())
        {
            DWORD written = 0;
            if (!WriteFile(file, body.data() + done, DWORD(body.size() - done), &written, nullptr) ||
                !written)
            {
                failed = true;
                break;
            }
            done += written;
        }
        if (!CloseHandle(file))
            failed = true;
#else
        int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (file < 0)
        {
            if (errno == EEXIST)
                continue;
            throw std::runtime_error("Could not create inspector report");
        }
        while (done < body.size())
        {
            auto written = ::write(file, body.data() + done, body.size() - done);
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0)
            {
                failed = true;
                break;
            }
            done += size_t(written);
        }
        if (::close(file) != 0)
            failed = true;
#endif
        if (failed)
        {
            std::error_code ec;
            std::filesystem::remove(path, ec);
            throw std::runtime_error("Could not finish inspector report");
        }
        return path;
    }
    throw std::runtime_error("No unused inspector report filename");
}
} // namespace inspector
