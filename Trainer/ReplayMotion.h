#pragma once
#include "InputModel.h"
#include "PracticeMapData.h"
#include "ReplayColliders.h"
#include "ReplayExperience.h"
#include <climits>
#include <set>
#include <tuple>
#include "ReplayCoverage.h"
namespace practice {
inline constexpr size_t MotionCapacity = 128 * 1024 * 1024;
struct ReplayQuaternion {
    float x = 0, y = 0, z = 0, w = 1;
};
struct ReplayViewFrame {
    double seconds = 0;
    uint32_t frame = 0, tick = 0, state = 0, keys = 0, reads = 0;
    float horizontal = 0, vertical = 0, mouseX = 0, mouseY = 0, joystickX = 0, joystickY = 0;
    Vec3 body, camera;
    ReplayQuaternion bodyRotation, cameraRotation, lampRotation;
    float fov = 60;
    Vec3 lampPosition, lampColor;
    float lampIntensity = 0, lampRange = 0, lampAngle = 0;
    uint32_t lampType = 0;
};
struct ReplayPhysicsFrame {
    double seconds = 0;
    uint32_t frame = 0, tick = 0, state = 0;
    float horizontal = 0, vertical = 0;
    Vec3 position;
    ReplayQuaternion rotation;
};
struct ReplayInputEvent {
    double seconds = 0;
    uint32_t frame = 0, tick = 0, keys = 0, down = 0, up = 0, actions = 0, state = 0;
};
struct RecordingCheck;
struct ReplayMotion {
    int64_t id = 0;
    int scene = 0, seed = 0, inputFormat = 1;
    double seconds = 0;
    std::string hash;
    std::shared_ptr<const ReplayMotionInfo> info;
    ReplayColliderData colliders;
    ReplayExperience experience;
    ReplayCoverage coverage;
    mutable std::shared_ptr<const RecordingCheck> recordingCheck;
    std::filesystem::path audioFile;
    std::string audioError;
    std::vector<suite_input::Frame> devices;
    double deviceThrough = 0;
    int deviceDropped = 0;
    bool visualsRecorded = false;
    int visualTracks = 0, visualSamples = 0, visualDropped = 0;
    std::vector<ReplayViewFrame> frames;
    std::vector<ReplayPhysicsFrame> ticks;
    std::vector<ReplayInputEvent> events;
    std::vector<uint8_t> bytes;
    bool Matches(const MapData &d) const {
        const auto *i = d.review && d.review->motion ? d.review->motion.get() : nullptr;
        return i && info && id == d.review->id && scene == d.scene && seed == d.seed && hash == d.hash &&
               std::abs(seconds - d.review->seconds) < .000001 && i->frames == info->frames &&
               i->ticks == info->ticks && i->events == info->events && i->dropped == info->dropped &&
               i->flags == info->flags && i->tool == info->tool && i->game == info->game &&
               i->unity == info->unity;
    }
};
inline uint64_t ReplayHash(const std::vector<uint8_t> &bytes, size_t n) {
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < n; i++) {
        h ^= bytes[i];
        h *= 1099511628211ull;
    }
    return h;
}
inline std::shared_ptr<const ReplayMotion> DecodeMotion(std::vector<uint8_t> bytes, std::string &error) {
    try {
        if (bytes.size() < 64 || bytes.size() > MotionCapacity)
            throw std::runtime_error("Input recording exceeds size limit");
        uint64_t sum = 0;
        std::memcpy(&sum, bytes.data() + bytes.size() - 8, 8);
        if (sum != ReplayHash(bytes, bytes.size() - 8))
            throw std::runtime_error("Input recording checksum mismatch");
        MapReader r(bytes);
        if (r.Read<uint32_t>() != 0x4e495349)
            throw std::runtime_error("Unsupported input recording");
        int format = r.Read<int32_t>();
        if (format < 1 || format > 6)
            throw std::runtime_error("Unsupported input recording");
        auto d = std::make_shared<ReplayMotion>();
        d->inputFormat = format;
        d->id = r.Read<int64_t>();
        d->scene = r.Read<int32_t>();
        d->seed = r.Read<int32_t>();
        d->seconds = r.Read<double>();
        d->hash = r.Text(65);
        d->info = ReadMotionInfo(r);
        if (d->id <= 0 || MapLevelIndex(d->scene) < 0 || !std::isfinite(d->seconds) || d->seconds < 0 ||
            d->seconds > 315360000 || !d->info)
            throw std::runtime_error("Invalid input identity");
        double previous = 0;
        uint32_t frame = 0, tick = 0;
        auto stamp = [&](auto &f) {
            f.seconds = r.Read<double>();
            f.frame = r.Read<uint32_t>();
            f.tick = r.Read<uint32_t>();
            if (!std::isfinite(f.seconds) || f.seconds < previous || f.seconds < 0 ||
                f.seconds > d->seconds + .000001 || f.frame < frame || f.tick < tick)
                throw std::runtime_error("Invalid input timeline");
            previous = f.seconds;
            frame = f.frame;
            tick = f.tick;
        };
        auto vec = [&]() { return Vec3{r.Coordinate(), r.Coordinate(), r.Coordinate()}; };
        auto quat = [&]() {
            ReplayQuaternion q{r.Coordinate(), r.Coordinate(), r.Coordinate(), r.Coordinate()};
            double n = double(q.x) * q.x + double(q.y) * q.y + double(q.z) * q.z + double(q.w) * q.w;
            if (n < .98 || n > 1.02)
                throw std::runtime_error("Invalid camera rotation");
            return q;
        };
        auto bits = [format](uint32_t state, uint32_t keys, uint32_t reads = 0) {
            if ((state & ~(format >= 3 ? 2047u : 511u)) || (keys & ~127u) || (reads & ~63u))
                throw std::runtime_error("Invalid input flags");
        };
        int n = r.Count(262144);
        if (n != d->info->frames || bytes.size() < size_t(n) * 168 + 64)
            throw std::runtime_error("Input frame count mismatch");
        d->frames.reserve(n);
        for (int k = 0; k < n; k++) {
            ReplayViewFrame f;
            stamp(f);
            f.state = r.Read<uint32_t>();
            f.keys = r.Read<uint32_t>();
            f.reads = r.Read<uint32_t>();
            bits(f.state, f.keys, f.reads);
            f.horizontal = r.Coordinate();
            f.vertical = r.Coordinate();
            f.mouseX = r.Coordinate();
            f.mouseY = r.Coordinate();
            f.joystickX = r.Coordinate();
            f.joystickY = r.Coordinate();
            f.body = vec();
            f.bodyRotation = quat();
            f.camera = vec();
            f.cameraRotation = quat();
            f.fov = r.Coordinate();
            if (f.fov <= 0 || f.fov > 179)
                throw std::runtime_error("Invalid camera FOV");
            f.lampRotation = quat();
            f.lampPosition = vec();
            f.lampColor = vec();
            f.lampIntensity = r.Coordinate();
            f.lampRange = r.Coordinate();
            f.lampAngle = r.Coordinate();
            f.lampType = r.Read<uint32_t>();
            if (f.lampIntensity < 0 || f.lampRange < 0 || f.lampAngle < 0 || f.lampAngle > 179 ||
                f.lampType > 4)
                throw std::runtime_error("Invalid recorded light");
            d->frames.push_back(f);
        }
        n = r.Count(524288);
        if (n != d->info->ticks || bytes.size() < size_t(n) * 56 + 64)
            throw std::runtime_error("Input physics count mismatch");
        previous = 0;
        frame = tick = 0;
        d->ticks.reserve(n);
        for (int k = 0; k < n; k++) {
            ReplayPhysicsFrame f;
            stamp(f);
            f.state = r.Read<uint32_t>();
            bits(f.state, 0);
            f.horizontal = r.Coordinate();
            f.vertical = r.Coordinate();
            f.position = vec();
            f.rotation = quat();
            d->ticks.push_back(f);
        }
        n = r.Count(131072);
        if (n != d->info->events || bytes.size() < size_t(n) * 36 + 64)
            throw std::runtime_error("Input event count mismatch");
        previous = 0;
        frame = tick = 0;
        uint32_t keys = 0;
        d->events.reserve(n);
        for (int k = 0; k < n; k++) {
            ReplayInputEvent e;
            stamp(e);
            e.keys = r.Read<uint32_t>();
            e.down = r.Read<uint32_t>();
            e.up = r.Read<uint32_t>();
            e.actions = r.Read<uint32_t>();
            e.state = r.Read<uint32_t>();
            bits(e.state, e.keys);
            if ((e.down & ~127u) || (e.up & ~127u) || (e.down & e.up) || (e.actions & ~15u) ||
                (k > 0 && ((keys | e.down) & ~e.up) != e.keys) || (e.down & ~e.keys) || (e.up & e.keys))
                throw std::runtime_error("Invalid input transitions");
            keys = e.keys;
            d->events.push_back(e);
        }
        if (format >= 2)
            d->colliders = ReadReplayColliders(r, d->seconds, bytes.size());
        if (format >= 3) {
            d->deviceThrough = r.Read<double>();
            d->deviceDropped = r.Read<int32_t>();
            if (!std::isfinite(d->deviceThrough) || d->deviceThrough < 0 ||
                d->deviceThrough > d->seconds + .000001 || d->deviceDropped < 0)
                throw std::runtime_error("Invalid device recording span");
            n = r.Count(131072);
            if (bytes.size() < size_t(n) * 172 + 80)
                throw std::runtime_error("Invalid device observation count");
            previous = 0;
            frame = tick = 0;
            d->devices.reserve(n);
            for (int k = 0; k < n; k++) {
                suite_input::Frame f;
                stamp(f);
                f.state = r.Read<uint32_t>();
                f.flags = r.Read<uint32_t>();
                bits(f.state, 0);
                if (f.flags & ~0x00ff070fu)
                    throw std::runtime_error("Invalid input sources");
                for (auto &key : f.keys)
                    key = r.Read<uint64_t>();
                f.mouse = r.Read<uint32_t>();
                if (f.mouse & ~127u)
                    throw std::runtime_error("Invalid mouse buttons");
                for (auto &joy : f.legacy) {
                    joy = r.Read<uint32_t>();
                    if (joy & ~0xfffffu)
                        throw std::runtime_error("Invalid joystick buttons");
                }
                for (auto &p : f.pads) {
                    p.buttons = r.Read<uint16_t>();
                    p.lt = r.Read<uint8_t>();
                    p.rt = r.Read<uint8_t>();
                    p.lx = r.Read<int16_t>();
                    p.ly = r.Read<int16_t>();
                    p.rx = r.Read<int16_t>();
                    p.ry = r.Read<int16_t>();
                }
                f.sprint = r.Coordinate();
                f.flash = r.Coordinate();
                if (std::abs(f.sprint) > 1.001 || std::abs(f.flash) > 1.001)
                    throw std::runtime_error("Invalid game input axis");
                f.moveX = r.Coordinate();
                f.moveY = r.Coordinate();
                f.lookX = r.Coordinate();
                f.lookY = r.Coordinate();
                f.joyX = r.Coordinate();
                f.joyY = r.Coordinate();
                d->devices.push_back(f);
            }
            if (previous > d->deviceThrough + .000001)
                throw std::runtime_error("Device span ends before samples");
        }

        if (format >= 4) {
            auto visualStart = r.Offset();
            int recorded = r.Read<uint8_t>();
            if (recorded > 1)
                throw std::runtime_error("Invalid visual flag");
            d->visualsRecorded = recorded;
            d->visualDropped = r.Count(INT_MAX);
            int assets = r.Count(4096);
            std::set<std::string> assetKeys;
            for (int i = 0; i < assets; i++) {
                auto key = r.Text(1024);
                if (key.empty() || !assetKeys.insert(key).second)
                    throw std::runtime_error("Duplicate visual asset");
            }
            int owners = r.Count(512);
            std::set<std::pair<int, int>> ownerKeys;
            for (int i = 0; i < owners; i++) {
                int group = r.Read<int32_t>(), id = r.Read<int32_t>(), kind = r.Read<int32_t>();
                auto root = r.Text(1024);
                if (group < 0 || group > 3 || id < 0 || kind < 0 || kind > 4 ||
                    !ownerKeys.insert({group, id}).second)
                    throw std::runtime_error("Invalid visual owner");
                int tracks = r.Count(8192 - d->visualTracks);
                d->visualTracks += tracks;
                std::set<std::tuple<int, int, std::string>> paths;
                for (int j = 0; j < tracks; j++) {
                    int type = r.Read<int32_t>(), slot = r.Read<int32_t>();
                    auto path = r.Text(1024);
                    if (type < 1 || type > 4 || slot < 0 || slot > 31 ||
                        !paths.insert({type, slot, path}).second)
                        throw std::runtime_error("Invalid visual track");
                    int frames = r.Count(393216 - d->visualSamples);
                    d->visualSamples += frames;
                    double last = -1;
                    for (int k = 0; k < frames; k++) {
                        double t = r.Read<double>();
                        if (!std::isfinite(t) || t < 0 || t < last || t > d->seconds + .000001)
                            throw std::runtime_error("Invalid visual time");
                        last = t;
                        if (type == 1) {
                            vec();
                            quat();
                            vec();
                        } else {
                            if (r.Read<uint32_t>() & ~31u)
                                throw std::runtime_error("Invalid visual state");
                            for (int channel = 0; channel < 4; channel++)
                                r.Coordinate();
                            if (type == 4) {
                                vec();
                                quat();
                                float intensity = r.Coordinate(), range = r.Coordinate(),
                                      angle = r.Coordinate(), inner = r.Coordinate();
                                if (intensity < 0 || range < 0 || angle < 0 || angle > 179 || inner < 0 ||
                                    inner > 179)
                                    throw std::runtime_error("Invalid visual light");
                            } else {
                                r.Count(assets);
                                r.Count(assets);
                            }
                        }
                    }
                }
            }
            if (r.Offset() - visualStart > 24 * 1024 * 1024)
                throw std::runtime_error("Visual recording exceeds byte limit");
            if (!d->visualsRecorded && (owners || d->visualSamples))
                throw std::runtime_error("Unexpected visual observations");
        }
        if(format>=5)d->experience=ReadReplayExperience(r,d->seconds);
        if(format>=6)d->coverage=ReadCoverage(r,d->seconds);
        r.Read<uint64_t>();
        if (!r.Done())
            throw std::runtime_error("Trailing input data");
        d->bytes = std::move(bytes);
        error.clear();
        return d;
    } catch (const std::exception &e) {
        error = e.what();
        return {};
    }
}
template <class T> inline const T *ReplayAt(const std::vector<T> &values, double seconds) {
    if (!std::isfinite(seconds))
        return nullptr;
    auto p = std::upper_bound(values.begin(), values.end(), seconds,
                              [](double t, const T &v) { return t < v.seconds; });
    return p == values.begin() ? nullptr : &*--p;
}
inline double ReplayFrameStep(const ReplayMotion &d, double seconds, bool forward) {
    if (d.frames.empty() || !std::isfinite(seconds))
        return std::clamp(std::isfinite(seconds) ? seconds : 0., 0., d.seconds);
    if (forward) {
        auto i = std::upper_bound(d.frames.begin(), d.frames.end(), seconds + 1e-7,
                                  [](double t, const auto &f) { return t < f.seconds; });
        return i == d.frames.end() ? d.seconds : i->seconds;
    }
    auto i = std::lower_bound(d.frames.begin(), d.frames.end(), seconds - 1e-7,
                              [](const auto &f, double t) { return f.seconds < t; });
    return i == d.frames.begin() ? 0 : std::prev(i)->seconds;
}
inline const ReplayViewFrame *ReplayViewAt(const ReplayMotion &d, double seconds) {
    auto f = ReplayAt(d.frames, seconds);
    return f && seconds - f->seconds <= .25 ? f : nullptr;
}
inline const ReplayInputEvent *ReplayInputAt(const ReplayMotion &d, double seconds) {
    if (!std::isfinite(seconds) || seconds < 0 || seconds > d.seconds || d.events.empty() ||
        (d.events.size() >= 131072 && seconds > d.events.back().seconds))
        return nullptr;
    return ReplayAt(d.events, seconds);
}
} // namespace practice
