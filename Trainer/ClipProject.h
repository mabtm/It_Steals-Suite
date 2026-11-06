#pragma once
#include "ClipComposition.h"
#include <atomic>
#include <fstream>
#include <iomanip>

namespace suite_clip
{
struct Project
{
    int64_t run = 0;
    double duration = 0;
    std::string gameHash, name = "Default";
    Options options;
    Content content;
};

inline uint64_t ProjectChecksum(const std::string &bytes)
{
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : bytes)
    {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}

inline void WritePose(std::ostream &o, const CameraPose &p)
{
    o << p.position.x << ' ' << p.position.y << ' ' << p.position.z << ' ' << p.rotation.x << ' '
      << p.rotation.y << ' ' << p.rotation.z << ' ' << p.rotation.w << ' ' << p.fov << ' ';
}

inline void ReadPose(std::istream &i, CameraPose &p)
{
    i >> p.position.x >> p.position.y >> p.position.z >> p.rotation.x >> p.rotation.y >> p.rotation.z >>
        p.rotation.w >> p.fov;
    p.Validate();
}

inline std::string EncodeProject(const Project &p)
{
    auto o = p.options;
    ConfigureSpeed(o.timing, p.content.track, p.duration, o.rate, o.camera == 3 && o.speedKeys);
    o.Validate(p.duration);
    p.content.track.Validate(p.duration);
    auto layout = p.content.layout;
    layout.Validate();
    std::ostringstream s;
    s.imbue(std::locale::classic());
    s << std::setprecision(17) << "ISCLIP 9\n"
      << p.run << ' ' << p.duration << ' ' << std::quoted(p.gameHash) << ' ' << std::quoted(p.name) << '\n';
    s << o.a << ' ' << o.b << ' ' << o.rate << ' ' << o.width << ' ' << o.height << ' ' << o.fps << ' '
      << o.format << ' ' << o.camera << '\n';
    for (bool flag : {o.scene3d, o.inputs, o.speed, o.time, o.seed, o.events, o.route, o.monster, o.colliders,
                      o.fullKeyboard, o.hideCeiling, o.fadeWalls, o.transparent, o.simplified, o.fullbright})
    {
        s << int(flag) << ' ';
    }
    s << '\n' << int(o.speedKeys) << ' ' << o.quality << ' ' << o.preset << ' ' << o.maxFileBytes << '\n';
    s << int(o.audio) << ' ' << o.audioVolume << '\n';
    s << int(o.splits) << '\n';
    std::ostringstream split;
    p.content.splitPresentation.Encode(split);
    s << std::quoted(split.str()) << '\n';
    for (auto b : layout.blocks)
        s << b.x << ' ' << b.y << ' ' << b.width << ' ' << b.opacity << '\n';
    std::ostringstream input;
    p.content.input.Encode(input);
    s << std::quoted(input.str()) << '\n';
    auto orbit = p.content.orbit;
    s << orbit.yaw << ' ' << orbit.pitch << ' ' << orbit.radius << ' ' << orbit.pan.x << ' ' << orbit.pan.y
      << ' ' << orbit.pan.z << ' ' << orbit.ready << '\n';
    s << p.content.freeReady << ' ';
    WritePose(s, p.content.freePose);
    s << '\n' << p.content.track.keys.size() << '\n';
    for (const auto &key : p.content.track.keys)
    {
        s << key.seconds << ' ' << int(key.next) << ' ' << std::quoted(key.name) << ' ';
        WritePose(s, key.pose);
        s << key.mode << ' ' << key.layerMask << ' ' << key.layers << ' ' << key.follow.yaw << ' '
          << key.follow.pitch << ' ' << key.follow.radius << ' ' << key.follow.pan.x << ' '
          << key.follow.pan.y << ' ' << key.follow.pan.z << ' ' << int(key.speedOverride) << ' '
          << key.playbackSpeed << ' ' << int(key.speedCurve) << '\n';
    }
    auto bytes = s.str();
    return bytes + std::to_string(ProjectChecksum(bytes)) + "\n";
}

inline Project DecodeProject(const std::string &bytes, int64_t expectedRun, double duration,
                             const std::string &gameHash)
{
    if (bytes.size() > 1024 * 1024 || bytes.size() < 32)
        throw std::runtime_error("Invalid clip project size");
    auto last = bytes.rfind('\n', bytes.size() - 2);
    if (last == std::string::npos)
        throw std::runtime_error("Incomplete clip project");
    std::string data = bytes.substr(0, last + 1);
    uint64_t hash = 0;
    std::istringstream checksum(bytes.substr(last + 1));
    if (!(checksum >> hash) || !(checksum >> std::ws).eof() || hash != ProjectChecksum(data))
        throw std::runtime_error("Clip project checksum mismatch");
    std::istringstream s(data);
    s.imbue(std::locale::classic());
    std::string magic;
    int version = 0;
    s >> magic >> version;
    if (magic != "ISCLIP" || version < 1 || version > 9)
        throw std::runtime_error("Unsupported clip project version");
    Project p;
    s >> p.run >> p.duration >> std::quoted(p.gameHash) >> std::quoted(p.name);
    if (p.run != expectedRun || !std::isfinite(p.duration) || std::abs(p.duration - duration) > .000001 ||
        p.gameHash != gameHash || p.name.size() > 80)
        throw std::runtime_error("Clip project belongs to a different recording");
    auto &o = p.options;
    s >> o.a >> o.b >> o.rate >> o.width >> o.height >> o.fps >> o.format >> o.camera;
    for (bool *flag : {&o.scene3d, &o.inputs, &o.speed, &o.time, &o.seed, &o.events, &o.route, &o.monster,
                       &o.colliders, &o.fullKeyboard, &o.hideCeiling, &o.fadeWalls, &o.transparent})
    {
        int v = -1;
        s >> v;
        if (v < 0 || v > 1)
            throw std::runtime_error("Invalid clip project flags");
        *flag = v == 1;
    }
    if (version >= 4)
    {
        int simple = -1;
        s >> simple;
        if (simple < 0 || simple > 1)
            throw std::runtime_error("Invalid graphics profile");
        o.simplified = simple == 1;
    }
    if (version >= 5)
    {
        int bright = -1;
        s >> bright;
        if (bright < 0 || bright > 1)
            throw std::runtime_error("Invalid replay brightness");
        o.fullbright = bright == 1;
    }
    if (version >= 6)
    {
        int enabled = -1;
        s >> enabled >> o.quality >> o.preset >> o.maxFileBytes;
        if (enabled < 0 || enabled > 1)
            throw std::runtime_error("Invalid speed-keypoint setting");
        o.speedKeys = enabled == 1;
    }
    if (version >= 7)
    {
        int audio = -1;
        s >> audio >> o.audioVolume;
        if (audio < 0 || audio > 1)
            throw std::runtime_error("Invalid audio setting");
        o.audio = audio == 1;
    }
    if (version >= 8)
    {
        int splits = -1; s >> splits;
        if (splits < 0 || splits > 1) throw std::runtime_error("Invalid split setting");
        o.splits = splits == 1;
    }
    if (version >= 9)
    {
        std::string style; s >> std::quoted(style);
        if (style.size() > 16384) throw std::runtime_error("Invalid split appearance");
        std::istringstream lines(style); std::string line;
        while (std::getline(lines,line))
        {
            auto equal = line.find('=');
            if (equal == std::string::npos || !line.starts_with("replay.")) throw std::runtime_error("Invalid split appearance key");
            double value = 0;
            std::istringstream number(line.substr(equal + 1)); number.imbue(std::locale::classic());
            if (!(number >> value) || !(number >> std::ws).eof() || !std::isfinite(value)) throw std::runtime_error("Invalid split appearance value");
            p.content.splitPresentation.Decode(line.substr(7,equal-7),value);
        }
        p.content.splitPresentation.Validate();
    }
    else p.content.splitPresentation.splitStyle = 1; // Existing projects retain their panel appearance.
    for (int i = 0; i < (version >= 8 ? BlockCount : 6); i++)
    {
        auto &b = p.content.layout.blocks[i];
        s >> b.x >> b.y >> b.width >> b.opacity;
    }
    p.content.layout.Validate();
    std::string input;
    s >> std::quoted(input);
    if (input.size() > 16384)
        throw std::runtime_error("Invalid input layout");
    std::istringstream lines(input);
    std::string line;
    while (std::getline(lines, line))
    {
        auto eq = line.find('=');
        if (eq == std::string::npos || line.rfind("input.", 0) != 0)
            throw std::runtime_error("Invalid input preference");
        double v = 0;
        std::istringstream value(line.substr(eq + 1));
        if (!(value >> v) || !std::isfinite(v))
            throw std::runtime_error("Invalid input preference value");
        p.content.input.Decode(line.substr(6, eq - 6), v);
    }
    p.content.input.Validate();
    auto &orbit = p.content.orbit;
    s >> orbit.yaw >> orbit.pitch >> orbit.radius >> orbit.pan.x >> orbit.pan.y >> orbit.pan.z >> orbit.ready;
    for (float v : {orbit.yaw, orbit.pitch, orbit.radius, orbit.pan.x, orbit.pan.y, orbit.pan.z})
        if (!std::isfinite(v))
            throw std::runtime_error("Invalid orbit settings");
    if (orbit.radius < .5f || orbit.radius > 100 || std::abs(orbit.pitch) > 85)
        throw std::runtime_error("Invalid orbit range");
    s >> p.content.freeReady;
    ReadPose(s, p.content.freePose);
    size_t count = 0;
    s >> count;
    if (count > 256)
        throw std::runtime_error("Camera keypoint limit exceeded");
    for (size_t i = 0; i < count; i++)
    {
        CameraKey key;
        int mode = 0;
        s >> key.seconds >> mode >> std::quoted(key.name);
        key.next = Transition(mode);
        ReadPose(s, key.pose);
        if (version >= 3)
        {
            s >> key.mode >> key.layerMask >> key.layers >> key.follow.yaw >> key.follow.pitch >>
                key.follow.radius >> key.follow.pan.x >> key.follow.pan.y >> key.follow.pan.z;
            key.follow.ready = true;
        }
        if (version >= 4)
        {
            int enabled = -1, curve = -1;
            s >> enabled >> key.playbackSpeed >> curve;
            if (enabled < 0 || enabled > 1)
                throw std::runtime_error("Invalid speed override");
            key.speedOverride = enabled == 1;
            key.speedCurve = Transition(curve);
        }
        p.content.track.keys.push_back(key);
    }
    if (!s)
        throw std::runtime_error("Incomplete clip project fields");
    s >> std::ws;
    if (!s.eof())
        throw std::runtime_error("Unexpected clip project data");
    if (version == 1)
    {
        p.content.input.states = p.content.input.states || p.content.input.events;
        p.content.layout.blocks[States] = Layout().blocks[States];
    }
    if (version < 3 && !p.content.track.keys.empty())
        o.camera = 3;
    ConfigureSpeed(o.timing, p.content.track, duration, o.rate, o.camera == 3 && o.speedKeys);
    o.Validate(duration);
    p.content.track.Validate(duration);
    return p;
}

inline std::string ReadProjectFile(const std::filesystem::path &path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot read clip project");
    auto size = std::filesystem::file_size(path);
    if (size > 1024 * 1024)
        throw std::runtime_error("Clip project is too large");
    return {std::istreambuf_iterator<char>(in), {}};
}

inline void StoreProject(const std::filesystem::path &path, const Project &project)
{
    auto bytes = EncodeProject(project);
    if (std::filesystem::exists(path))
    {
        auto previous = ReadProjectFile(path);
        std::istringstream header(previous);
        std::string magic;
        int version = 0;
        header >> magic >> version;
        if (magic != "ISCLIP" || version < 1 || version > 9)
            throw std::runtime_error("Project from another version was preserved");
        DecodeProject(previous, project.run, project.duration, project.gameHash);
    }
    std::filesystem::create_directories(path.parent_path());
    static std::atomic<unsigned> serial{0};
    auto temporary = path;
    temporary += L".tmp-" + std::to_wstring(++serial);
    bool owned = false;
    try
    {
#ifdef _WIN32
        HANDLE f = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE)
            throw std::runtime_error("Cannot reserve clip project");
        owned = true;
        DWORD written = 0;
        BOOL ok = WriteFile(f, bytes.data(), DWORD(bytes.size()), &written, nullptr);
        BOOL flushed = ok && written == bytes.size() && FlushFileBuffers(f);
        CloseHandle(f);
        if (!flushed)
            throw std::runtime_error("Cannot write clip project");
        if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot save clip project");
#else
        if (std::filesystem::exists(temporary))
            throw std::runtime_error("Clip project temporary file exists");
        {
            std::ofstream out(temporary, std::ios::binary);
            owned = true;
            out << bytes;
            out.close();
            if (!out)
                throw std::runtime_error("Cannot write clip project");
        }
        std::filesystem::rename(temporary, path);
#endif
    }
    catch (...)
    {
        if (owned)
        {
            std::error_code ec;
            std::filesystem::remove(temporary, ec);
        }
        throw;
    }
}

inline void ApplyProject(const Project &p, Options &o, Content &c, bool profile = false)
{
    auto analysis = c.analysis;
    auto motion = c.motion;
    auto markers = c.markers;
    auto reference = c.splitReference;
    auto splits = c.splits;
    bool splitMotion = c.splitMotion;
    auto font = c.font;
    auto splitFont = c.splitFont;
    auto palette = c.palette;
    double a = o.a, b = o.b;
    bool scene = o.scene3d;
    int camera = o.camera;
    auto track = c.track;
    auto orbit = c.orbit;
    auto free = c.freePose;
    bool ready = c.freeReady;
    o = p.options;
    c = p.content;
    c.analysis = analysis;
    c.motion = motion;
    c.markers = markers;
    c.splitReference = reference;
    c.splits = splits;
    c.splitMotion = splitMotion;
    c.font = font;
    c.splitFont = splitFont;
    c.palette = palette;
    if (profile)
    {
        o.a = a;
        o.b = b;
        o.scene3d = scene;
        o.camera = camera;
        c.track = track;
        c.orbit = orbit;
        c.freePose = free;
        c.freeReady = ready;
        if (o.camera == 3 && c.track.keys.empty())
            o.camera = 0;
    }
}
} // namespace suite_clip
