#pragma once
#include "ClipModel.h"
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace suite_clip
{
// Streaming GIF89a, fixed palette, per-frame LZW; no full-clip image storage.
class GifWriter
{
    std::ofstream out;
    int width = 0, height = 0, fps = 0;
    uint64_t frame = 0;
    bool finished = false;

    void Byte(int v)
    {
        out.put(char(v));
    }

    void Word(int v)
    {
        Byte(v);
        Byte(v >> 8);
    }

    void Check()
    {
        if (!out)
            throw std::runtime_error("GIF write failed; check available disk space");
        if (out.tellp() > std::streampos(2ull * 1024 * 1024 * 1024))
            throw std::runtime_error("GIF exceeds 2 GiB; export a shorter clip or MP4");
    }

  public:
    GifWriter(const std::filesystem::path &path, int w, int h, int f)
        : out(path, std::ios::binary), width(w), height(h), fps(f)
    {
        out.write("GIF89a", 6);
        Word(w);
        Word(h);
        Byte(247);
        Byte(0);
        Byte(0);
        for (int i = 0; i < 256; i++)
        {
            uint8_t r, g, b;
            PaletteColor(i, r, g, b);
            Byte(r);
            Byte(g);
            Byte(b);
        }
        const uint8_t loop[] = {0x21, 0xff, 11,  'N', 'E', 'T', 'S', 'C', 'A', 'P',
                                'E',  '2',  '.', '0', 3,   1,   0,   0,   0};
        out.write(reinterpret_cast<const char *>(loop), sizeof(loop));
        Check();
    }

    void Write(const std::vector<uint8_t> &bgra)
    {
        if (finished || bgra.size() != size_t(width) * height * 4)
            throw std::runtime_error("Invalid GIF frame");
        Byte(0x21);
        Byte(0xf9);
        Byte(4);
        Byte(4);
        Word(int((frame + 1) * 100 / fps - frame * 100 / fps));
        Byte(0);
        Byte(0);
        Byte(0x2c);
        Word(0);
        Word(0);
        Word(width);
        Word(height);
        Byte(0);
        Byte(8);
        std::unordered_map<uint32_t, uint16_t> dict;
        dict.reserve(4096);
        std::vector<uint8_t> bytes;
        bytes.reserve(size_t(width) * height / 3 + 512);
        uint32_t bits = 0;
        int count = 0, size = 9, next = 258;
        auto emit = [&](int code) {
            bits |= uint32_t(code) << count;
            count += size;
            while (count >= 8)
            {
                bytes.push_back(uint8_t(bits));
                bits >>= 8;
                count -= 8;
            }
            if (next > (1 << size) - 1 && size < 12)
                size++;
        };
        emit(256);
        auto pixel = [&](size_t i) {
            auto p = bgra.data() + i * 4;
            return Index(p[2], p[1], p[0]);
        };
        int prefix = pixel(0);
        size_t pixels = size_t(width) * height;
        for (size_t i = 1; i < pixels; i++)
        {
            int k = pixel(i);
            uint32_t key = (uint32_t(prefix) << 8) | uint32_t(k);
            auto found = dict.find(key);
            if (found != dict.end())
            {
                prefix = found->second;
                continue;
            }
            emit(prefix);
            if (next < 4096)
                dict.emplace(key, uint16_t(next++));
            else
            {
                emit(256);
                dict.clear();
                size = 9;
                next = 258;
            }
            prefix = k;
        }
        emit(prefix);
        emit(257);
        if (count)
            bytes.push_back(uint8_t(bits));
        for (size_t i = 0; i < bytes.size(); i += 255)
        {
            size_t n = std::min(size_t(255), bytes.size() - i);
            Byte(int(n));
            out.write(reinterpret_cast<const char *>(bytes.data() + i), std::streamsize(n));
        }
        Byte(0);
        frame++;
        Check();
    }

    void Finish()
    {
        if (finished)
            return;
        Byte(0x3b);
        out.flush();
        Check();
        out.close();
        finished = true;
    }
};
} // namespace suite_clip
