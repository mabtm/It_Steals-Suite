#pragma once
#include "ReplayMotion.h"
#include "SuiteBranding.h"
#include "SuiteUpdateModel.h"
#include "ReplayAnnotations.h"
#include <sstream>
#include <iomanip>
#include <map>
#include <fstream>

namespace practice
{
// Small, platform-independent SHA-256 for portable package verification.
// These are unsigned consistency hashes, never an authenticity claim.
template <class Reader> inline std::string ReplaySha256(size_t size, Reader read)
{
    constexpr uint32_t k[] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                    0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    auto rr = [](uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); };
    size_t blocks = (size + 9 + 63) / 64;
    uint64_t bits = uint64_t(size) * 8;
    for (size_t b = 0; b < blocks; b++)
    {
        uint32_t w[64]{};
        std::array<uint8_t, 64> chunk{};
        size_t count = b * 64 < size ? std::min(size_t(64), size - b * 64) : 0;
        if (count)
            read(chunk.data(), count);
        for (size_t i = 0; i < 64; i++)
        {
            size_t p = b * 64 + i;
            uint8_t v = p < size               ? chunk[i]
                        : p == size            ? 0x80
                        : p >= blocks * 64 - 8 ? uint8_t(bits >> (8 * (blocks * 64 - 1 - p)))
                                               : 0;
            w[i / 4] |= uint32_t(v) << (24 - (i % 4) * 8);
        }
        for (int i = 16; i < 64; i++)
        {
            uint32_t a = w[i - 15], z = w[i - 2];
            w[i] = w[i - 16] + (rr(a, 7) ^ rr(a, 18) ^ (a >> 3)) + w[i - 7] +
                   (rr(z, 17) ^ rr(z, 19) ^ (z >> 10));
        }
        uint32_t a = h[0], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], z = h[7], bb = h[1];
        for (int i = 0; i < 64; i++)
        {
            uint32_t t1 = z + (rr(e, 6) ^ rr(e, 11) ^ rr(e, 25)) + ((e & f) ^ (~e & g)) + k[i] + w[i],
                     t2 = (rr(a, 2) ^ rr(a, 13) ^ rr(a, 22)) + ((a & bb) ^ (a & c) ^ (bb & c));
            z = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = bb;
            bb = a;
            a = t1 + t2;
        }
        h[0] += a;
        h[1] += bb;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += z;
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto v : h)
        out << std::setw(8) << v;
    return out.str();
}

inline std::string ReplaySha256(const std::vector<uint8_t> &data)
{
    size_t at = 0;
    return ReplaySha256(data.size(), [&](uint8_t *out, size_t count) {
        std::memcpy(out, data.data() + at, count);
        at += count;
    });
}

inline std::string ReplayFileSha256(const std::filesystem::path &path, uint64_t limit)
{
    auto size = std::filesystem::file_size(path);
    if (size > limit)
        throw std::runtime_error("Replay hash exceeds file limit");
    std::ifstream file(path, std::ios::binary);
    return ReplaySha256(size_t(size), [&](uint8_t *out, size_t count) {
        file.read(reinterpret_cast<char *>(out), std::streamsize(count));
        if (!file)
            throw std::runtime_error("Replay file changed during verification");
    });
}

inline uint32_t ReplayCrc(const std::vector<uint8_t> &b)
{
    static const auto table = []() {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int j = 0; j < 8; j++)
                c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
            t[i] = c;
        }
        return t;
    }();
    uint32_t c = ~uint32_t{0};
    for (auto v : b)
        c = (c >> 8) ^ table[(c ^ v) & 255];
    return ~c;
}

inline std::string ReplayJson(const std::string &s)
{
    std::ostringstream o;
    o << '"';
    for (unsigned char c : s)
    {
        if (c == '"' || c == '\\')
            o << '\\' << c;
        else if (c < 32)
            o << "\\u00" << std::hex << std::setfill('0') << std::setw(2) << int(c) << std::dec;
        else
            o << c;
    }
    o << '"';
    return o.str();
}

inline constexpr size_t ReplayPackageCapacity = MotionCapacity + MapCapacity + AudioCapacity + 196608;
using ReplayFiles = std::map<std::string, std::vector<uint8_t>>;

inline std::vector<uint8_t> ReplayText(const std::string &s)
{
    return {s.begin(), s.end()};
}

inline std::string ReplayManifest(const MapData &d, bool inputs, bool validation,
                                  const std::string &exporter = suite_brand::Version,
                                  bool annotations = false)
{
    const auto &r = *d.review;
    auto version = suite_update::Version::Parse(exporter);
    bool categoryMetadata = version && version->parts >= std::array<unsigned, 3>{7, 22, 0};
    std::ostringstream o;
    o << std::setprecision(17) << "{\n  \"packageFormat\": " << (annotations ? 2 : 1)
      << ",\n  \"exportedBy\": " << ReplayJson((categoryMetadata ? "Elite-Suite " : "PracticeSuite ") + exporter + " / mab")
      << ",\n  \"runId\": " << ReplayJson(std::to_string(r.id)) << ",\n  \"scene\": " << d.scene
      << ",\n  \"mode\": " << ReplayJson(kMapLevelNames[MapLevelIndex(d.scene)])
      << ",\n  \"layoutSeed\": " << d.seed << ",\n  \"seconds\": " << r.seconds
      << ",\n  \"outcome\": " << r.outcome << ",\n  \"localValid\": " << (r.valid ? "true" : "false")
      << ",\n  \"validationReason\": " << ReplayJson(r.validityReason)
      << ",\n  \"gameAssemblyFileSha256\": " << ReplayJson(d.hash)
      << ",\n  \"inputsIncluded\": " << (inputs ? "true" : "false")
      << ",\n  \"originalValidationReportIncluded\": " << (validation ? "true" : "false");
    if (categoryMetadata)
        o << ",\n  \"speedrunCategory\": "
          << ReplayJson(r.category == 1 ? "SET-SEED" : r.category == 2 ? "PRACTICE" : "RANDOM SEED")
          << ",\n  \"selectedSeed\": " << (r.category == 1 ? std::to_string(r.selectedSeed) : "null")
          << ",\n  \"seedRecipe\": "
          << ReplayJson(r.category == 1 ? r.rawSpawnSeed ? "RAW-FIRST-SPAWN-V1" : "DERIVED-V1"
                                       : "NOT A SET-SEED RUN");
    if (annotations)
        o << ",\n  \"annotationsIncluded\": true";
    if (r.motion)
        o << ",\n  \"recordingTool\": " << ReplayJson(r.motion->tool)
          << ",\n  \"gameReportedVersion\": " << ReplayJson(r.motion->game)
          << ",\n  \"unityVersion\": " << ReplayJson(r.motion->unity)
          << ",\n  \"cameraFrames\": " << r.motion->frames << ",\n  \"physicsTicks\": " << r.motion->ticks
          << ",\n  \"inputEvents\": " << r.motion->events << ",\n  \"captureFlags\": " << r.motion->flags
          << ",\n  \"droppedSamples\": " << r.motion->dropped;
    o << ",\n  \"scope\": \"Local observations with unsigned consistency hashes. No physical-key or tamper-proof attestation. Replay fidelity depends on the recording channels and their reported coverage.\"\n}\n";
    return o.str();
}

// Explicit exporter compatibility: 7.2 packages predate replay annotations.
// Keep each version and its format support together when adding a release.
struct ReplayPackageExporter
{
    const char *version;
    bool annotations;
};

inline constexpr ReplayPackageExporter ReplayPackageExporters[] = {
    {"7.22.0", true},
    {"7.21.0", true},
    {"7.20.2", true},
    {"7.20.1", true},
    {"7.20.0", true},
    {"7.19.0", true},
    {"7.15.0", true},
    {"7.16.0", true},
    {"7.16.1", true},
    {"7.16.2", true},
    {"7.18.1", true},
    {"7.18.0", true},
    {"7.17.0", true},
    {"7.14.2", true},
    {"7.14.1", true},
    {"7.14.0", true},
    {suite_brand::Version, true},
    {"7.13.1", true},
    {"7.13.0", true},
    {"7.12.6", true},
    {"7.12.5", true},
    {"7.12.4", true},
    {"7.12.3", true},
    {"7.12.2", true},
    {"7.12.1", true},
    {"7.12.0", true},
    {"7.11.5", true},
    {"7.11.4", true},
    {"7.11.3", true},
    {"7.11.2", true},
    {"7.11.1", true},
    {"7.11.0", true},
    {"7.10.1", true},
    {"7.10.0", true},
    {"7.9.0", true},
    {"7.8.1", true},
    {"7.8.0", true},
    {"7.7.0", true},
    {"7.6.0", true},
    {"7.5.2", true},
    {"7.5.1", true},
    {"7.5.0", true},
    {"7.4.0", true},
    {"7.3.5", true},
    {"7.3.4", true},
    {"7.3.3", true},
    {"7.3.2", true},
    {"7.3.1", true},
    {"7.3.0", true},
    {"7.2.1", false},
    {"7.2.0", false},
};

inline bool ReplayManifestMatches(const std::vector<uint8_t> &manifest, const MapData &data, bool inputs,
                                  bool validation, bool annotations)
{
    for (const auto &exporter : ReplayPackageExporters)
        if ((!annotations || exporter.annotations) &&
            manifest == ReplayText(ReplayManifest(data, inputs, validation, exporter.version, annotations)))
            return true;
    return false;
}

// Stored ZIP avoids platform-specific codecs and permits inspection with any
// ordinary ZIP reader. Work runs exclusively on the archive IO worker.
inline std::vector<uint8_t> EncodeReplayZip(const ReplayFiles &files)
{
    if (files.empty() || files.size() > 8)
        throw std::runtime_error("Invalid package entry count");
    std::vector<uint8_t> b, central;
    auto put = [](auto &out, auto v) {
        auto p = reinterpret_cast<const uint8_t *>(&v);
        out.insert(out.end(), p, p + sizeof(v));
    };
    for (const auto &[name, data] : files)
    {
        if (name.size() > 200 || name.find_first_of("/\\") != std::string::npos ||
            data.size() > (name.ends_with(".isaudio") ? AudioCapacity : MotionCapacity))
            throw std::runtime_error("Invalid package entry");
        uint32_t crc = ReplayCrc(data), n = uint32_t(data.size()), offset = uint32_t(b.size());
        uint16_t len = uint16_t(name.size());
        put(b, uint32_t{0x04034b50});
        for (auto v : {20, 0, 0, 0, 33})
            put(b, uint16_t(v));
        put(b, crc);
        put(b, n);
        put(b, n);
        put(b, len);
        put(b, uint16_t{0});
        b.insert(b.end(), name.begin(), name.end());
        b.insert(b.end(), data.begin(), data.end());
        put(central, uint32_t{0x02014b50});
        for (auto v : {20, 20, 0, 0, 0, 33})
            put(central, uint16_t(v));
        put(central, crc);
        put(central, n);
        put(central, n);
        put(central, len);
        for (int i = 0; i < 4; i++)
            put(central, uint16_t{0});
        put(central, uint32_t{0});
        put(central, offset);
        central.insert(central.end(), name.begin(), name.end());
    }
    uint32_t offset = uint32_t(b.size());
    b.insert(b.end(), central.begin(), central.end());
    put(b, uint32_t{0x06054b50});
    put(b, uint16_t{0});
    put(b, uint16_t{0});
    put(b, uint16_t(files.size()));
    put(b, uint16_t(files.size()));
    put(b, uint32_t(central.size()));
    put(b, offset);
    put(b, uint16_t{0});
    return b;
}

inline ReplayFiles DecodeReplayZip(const std::vector<uint8_t> &bytes)
{
    if (bytes.size() < 22 || bytes.size() > ReplayPackageCapacity)
        throw std::runtime_error("Package exceeds size limit");
    size_t pos = 0;
    auto get = [&]<class T>() {
        if (pos > bytes.size() || sizeof(T) > bytes.size() - pos)
            throw std::runtime_error("Truncated package");
        T v;
        std::memcpy(&v, bytes.data() + pos, sizeof(v));
        pos += sizeof(v);
        return v;
    };

    struct Entry
    {
        uint32_t crc = 0, n = 0, offset = 0;
    };

    std::map<std::string, Entry> entries;
    ReplayFiles files;
    auto name = [&](uint16_t n) {
        if (n == 0 || n > 200 || pos + n > bytes.size())
            throw std::runtime_error("Invalid package filename");
        std::string s(reinterpret_cast<const char *>(bytes.data() + pos), n);
        pos += n;
        if (s.find_first_of("/\\") != std::string::npos || s.find('\0') != std::string::npos ||
            s.find("..") != std::string::npos)
            throw std::runtime_error("Unsafe package filename");
        return s;
    };
    while (true)
    {
        size_t start = pos;
        uint32_t sig = get.operator()<uint32_t>();
        if (sig == 0x02014b50)
        {
            pos = start;
            break;
        }
        if (sig != 0x04034b50 || files.size() >= 9)
            throw std::runtime_error("Unsupported package ZIP");
        if (get.operator()<uint16_t>() != 20 || get.operator()<uint16_t>() != 0 ||
            get.operator()<uint16_t>() != 0)
            throw std::runtime_error("Import a stored PracticeSuite run package");
        get.operator()<uint16_t>();
        get.operator()<uint16_t>();
        Entry e;
        e.offset = uint32_t(start);
        e.crc = get.operator()<uint32_t>();
        e.n = get.operator()<uint32_t>();
        if (get.operator()<uint32_t>() != e.n || e.n > AudioCapacity)
            throw std::runtime_error("Invalid package entry size");
        uint16_t len = get.operator()<uint16_t>();
        if (get.operator()<uint16_t>() != 0)
            throw std::runtime_error("Unsupported package extra field");
        auto s = name(len);
        if (!s.ends_with(".isaudio") && e.n > MotionCapacity)
            throw std::runtime_error("Invalid package entry size");
        if (files.count(s) || pos + e.n > bytes.size())
            throw std::runtime_error("Duplicate/truncated package entry");
        auto &data = files[s];
        data.assign(bytes.begin() + std::ptrdiff_t(pos), bytes.begin() + std::ptrdiff_t(pos + e.n));
        pos += e.n;
        if (ReplayCrc(data) != e.crc)
            throw std::runtime_error("Package CRC mismatch");
        entries.emplace(s, e);
    }
    size_t central = pos;
    std::map<std::string, bool> seen;
    for (size_t k = 0; k < entries.size(); k++)
    {
        if (get.operator()<uint32_t>() != 0x02014b50)
            throw std::runtime_error("Invalid package directory");
        get.operator()<uint16_t>();
        if (get.operator()<uint16_t>() != 20 || get.operator()<uint16_t>() || get.operator()<uint16_t>())
            throw std::runtime_error("Invalid package directory flags");
        get.operator()<uint16_t>();
        get.operator()<uint16_t>();
        uint32_t crc = get.operator()<uint32_t>(), n = get.operator()<uint32_t>(),
                 size = get.operator()<uint32_t>();
        uint16_t len = get.operator()<uint16_t>();
        for (int i = 0; i < 4; i++)
            if (get.operator()<uint16_t>())
                throw std::runtime_error("Unsupported package directory metadata");
        get.operator()<uint32_t>();
        uint32_t offset = get.operator()<uint32_t>();
        auto s = name(len);
        auto it = entries.find(s);
        if (it == entries.end() || seen[s] || it->second.crc != crc || it->second.n != n || n != size ||
            it->second.offset != offset)
            throw std::runtime_error("Package directory mismatch");
        seen[s] = true;
    }
    size_t centralSize = pos - central;
    if (get.operator()<uint32_t>() != 0x06054b50 || get.operator()<uint16_t>() ||
        get.operator()<uint16_t>() || get.operator()<uint16_t>() != entries.size() ||
        get.operator()<uint16_t>() != entries.size() || get.operator()<uint32_t>() != centralSize ||
        get.operator()<uint32_t>() != central || get.operator()<uint16_t>() || pos != bytes.size())
        throw std::runtime_error("Invalid package trailer");
    return files;
}
} // namespace practice

