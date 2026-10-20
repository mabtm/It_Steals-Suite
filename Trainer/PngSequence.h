#pragma once
#include "ClipModel.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace suite_clip
{
inline uint32_t PngCrc(const uint8_t *data, size_t n)
{
    static const auto table = []() {
        std::array<uint32_t, 256> values{};
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int j = 0; j < 8; j++)
                c = (c >> 1) ^ (0xedb88320u & uint32_t(-int(c & 1)));
            values[i] = c;
        }
        return values;
    }();
    uint32_t c = ~0u;
    for (size_t i = 0; i < n; i++)
        c = (c >> 8) ^ table[(c ^ data[i]) & 255];
    return ~c;
}

inline void PngWord(std::vector<uint8_t> &out, uint32_t v)
{
    for (int i = 3; i >= 0; i--)
        out.push_back(uint8_t(v >> (i * 8)));
}

inline void PngChunk(std::ofstream &out, const char *name, const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> bytes;
    PngWord(bytes, uint32_t(payload.size()));
    bytes.insert(bytes.end(), name, name + 4);
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    PngWord(bytes, PngCrc(bytes.data() + 4, bytes.size() - 4));
    out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
}

// Stored DEFLATE blocks keep encoding memory/CPU bounded, and preserve RGBA exactly.
inline void WritePng(const std::filesystem::path &path, const std::vector<uint8_t> &bgra, int width,
                     int height)
{
    if (bgra.size() != size_t(width) * height * 4)
        throw std::runtime_error("Invalid PNG frame dimensions");
    std::ofstream out(path, std::ios::binary);
    if (!out)
        throw std::runtime_error("Cannot write PNG frame");
    const uint8_t signature[] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.write(reinterpret_cast<const char *>(signature), 8);
    std::vector<uint8_t> header;
    PngWord(header, uint32_t(width));
    PngWord(header, uint32_t(height));
    header.insert(header.end(), {8, 6, 0, 0, 0});
    PngChunk(out, "IHDR", header);
    std::vector<uint8_t> raw((size_t(width) * 4 + 1) * height);
    size_t at = 0;
    for (int y = 0; y < height; y++)
    {
        raw[at++] = 0;
        for (int x = 0; x < width; x++)
        {
            auto p = bgra.data() + (size_t(y) * width + x) * 4;
            raw[at++] = p[2];
            raw[at++] = p[1];
            raw[at++] = p[0];
            raw[at++] = p[3];
        }
    }
    std::vector<uint8_t> z;
    z.reserve(raw.size() + raw.size() / 65535 * 5 + 16);
    z.insert(z.end(), {0x78, 0x01});
    uint32_t a = 1, b = 0;
    for (auto v : raw)
    {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t offset = 0; offset < raw.size();)
    {
        size_t count = std::min(size_t(65535), raw.size() - offset);
        z.push_back(offset + count == raw.size() ? 1 : 0);
        uint16_t n = uint16_t(count), inv = uint16_t(~n);
        z.push_back(uint8_t(n));
        z.push_back(uint8_t(n >> 8));
        z.push_back(uint8_t(inv));
        z.push_back(uint8_t(inv >> 8));
        z.insert(z.end(), raw.begin() + std::ptrdiff_t(offset), raw.begin() + std::ptrdiff_t(offset + count));
        offset += count;
    }
    PngWord(z, (b << 16) | a);
    PngChunk(out, "IDAT", z);
    PngChunk(out, "IEND", {});
    out.close();
    if (!out)
        throw std::runtime_error("PNG frame write failed; check available disk space");
}

inline void StraightAlpha(std::vector<uint8_t> &bgra)
{
    for (size_t i = 0; i < bgra.size(); i += 4)
    {
        unsigned a = bgra[i + 3];
        for (int c = 0; c < 3; c++)
            bgra[i + c] = a ? uint8_t(std::min(255u, (unsigned(bgra[i + c]) * 255 + a / 2) / a)) : 0;
    }
}

class PngSequence
{
    std::filesystem::path folder;
    Options options;
    uint64_t count = 0;

  public:
    PngSequence(const std::filesystem::path &p, const Options &o) : folder(p), options(o) {}

    void Write(const std::vector<uint8_t> &bytes)
    {
        char name[40];
        std::snprintf(name, sizeof(name), "frame-%08llu.png", static_cast<unsigned long long>(count++));
        WritePng(folder / name, bytes, options.width, options.height);
    }

    void Finish()
    {
        std::ofstream out(folder / "sequence.json");
        out.imbue(std::locale::classic());
        out.precision(17);
        out << "{\n  \"format\": 1,\n  \"frames\": " << count << ",\n  \"fps\": " << options.fps
            << ",\n  \"width\": " << options.width << ",\n  \"height\": " << options.height
            << ",\n  \"sourceStart\": " << options.a << ",\n  \"sourceEnd\": " << options.b
            << ",\n  \"playbackRate\": " << options.rate << ",\n  \"alpha\": \""
            << (options.transparent ? "straight" : "opaque")
            << "\",\n  \"pattern\": \"frame-%08d.png\",\n  \"audio\": false";
        if (options.timing && options.speedKeys)
        {
            out << ",\n  \"variableRate\": true,\n  \"sourceTimes\": [";
            for (uint64_t i = 0; i < count; i++)
            {
                if (i)
                    out << ",";
                out << options.At(i);
            }
            out << "]";
        }
        out << "\n}\n";
        out.close();
        if (!out)
            throw std::runtime_error("Cannot write PNG sequence manifest");
    }
};
} // namespace suite_clip
