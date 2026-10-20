#pragma once
#include "ReplayExperience.h"
#include <array>
#include <fstream>
#include <limits>
#include <streambuf>

namespace practice
{
inline ReplayAudioInfo VerifyRecordedAudio(std::istream &file, uint64_t size, int64_t id, int scene, int seed,
                                           const std::string &gameHash,
                                           const ReplayAudioInfo *expected = nullptr)
{
    if (size < AudioHeaderBytes + 8 || size > AudioCapacity)
        throw std::runtime_error("Recorded audio exceeds file limit");
    auto read = [&]<class T>() {
        T v{};
        file.read(reinterpret_cast<char *>(&v), sizeof(v));
        if (!file)
            throw std::runtime_error("Recorded audio is truncated");
        return v;
    };
    if (read.operator()<uint32_t>() != 0x55415349u || read.operator()<int32_t>() != 1 ||
        read.operator()<int64_t>() != id || read.operator()<int32_t>() != scene ||
        read.operator()<int32_t>() != seed)
        throw std::runtime_error("Recorded audio identity mismatch");
    ReplayAudioInfo i;
    i.recorded = true;
    i.rate = read.operator()<int32_t>();
    i.channels = read.operator()<int32_t>();
    i.frames = read.operator()<int64_t>();
    i.flags = read.operator()<int32_t>();
    i.gaps = read.operator()<int32_t>();
    i.bytes = int64_t(size);
    int n = read.operator()<int32_t>();
    if (n < 0 || n > 65)
        throw std::runtime_error("Invalid audio game hash");
    std::string hash(size_t(n), '\0');
    if (n)
        file.read(hash.data(), n);
    if (!file || hash != gameHash || i.rate < 8000 || i.rate > 192000 || i.channels < 1 || i.channels > 8 ||
        i.frames < 1 ||
        uint64_t(i.frames) > (AudioCapacity - AudioHeaderBytes - 8) / (uint64_t(i.channels) * 4) ||
        size != AudioHeaderBytes + uint64_t(i.frames) * i.channels * 4 + 8 || i.flags < 0 ||
        (i.flags & ~31) || i.gaps < 0)
        throw std::runtime_error("Invalid recorded audio header");
    file.seekg(std::streamoff(size - 8));
    i.checksum = read.operator()<uint64_t>();
    file.seekg(0);
    uint64_t sum = 14695981039346656037ull, left = size - 8;
    std::array<char, 65536> buffer{};
    while (left)
    {
        size_t bytes = size_t(std::min<uint64_t>(left, buffer.size()));
        file.read(buffer.data(), std::streamsize(bytes));
        if (!file)
            throw std::runtime_error("Recorded audio is truncated");
        for (size_t j = 0; j < bytes; j++)
        {
            sum ^= uint8_t(buffer[j]);
            sum *= 1099511628211ull;
        }
        left -= bytes;
    }
    if (sum != i.checksum)
        throw std::runtime_error("Recorded audio checksum mismatch");
    if (expected &&
        (!expected->recorded || i.rate != expected->rate || i.channels != expected->channels ||
         i.frames != expected->frames || i.bytes != expected->bytes || i.checksum != expected->checksum ||
         i.flags != expected->flags || i.gaps != expected->gaps))
        throw std::runtime_error("Recorded audio does not match input recording");
    return i;
}

inline ReplayAudioInfo VerifyRecordedAudio(const std::filesystem::path &file, int64_t id, int scene, int seed,
                                           const std::string &hash, const ReplayAudioInfo *expected = nullptr)
{
    std::ifstream f(file, std::ios::binary);
    if (!f)
        throw std::runtime_error("Recorded audio is missing");
    return VerifyRecordedAudio(f, std::filesystem::file_size(file), id, scene, seed, hash, expected);
}

inline ReplayAudioInfo VerifyRecordedAudio(const std::vector<uint8_t> &bytes, int64_t id, int scene, int seed,
                                           const std::string &hash, const ReplayAudioInfo *expected = nullptr)
{
    struct View : std::streambuf
    {
        View(const std::vector<uint8_t> &b)
        {
            char *p = const_cast<char *>(reinterpret_cast<const char *>(b.data()));
            setg(p, p, p + b.size());
        }

        pos_type seekoff(off_type off, std::ios_base::seekdir way, std::ios_base::openmode which) override
        {
            if (!(which & std::ios_base::in))
                return pos_type(off_type(-1));
            off_type base = way == std::ios_base::beg   ? 0
                            : way == std::ios_base::cur ? gptr() - eback()
                                                        : egptr() - eback();
            off_type size = egptr() - eback();
            if (off < -base || off > size - base)
                return pos_type(off_type(-1));
            auto *p = eback() + base + off;
            setg(eback(), p, egptr());
            return p - eback();
        }

        pos_type seekpos(pos_type off, std::ios_base::openmode mode) override
        {
            return seekoff(off_type(off), std::ios_base::beg, mode);
        }
    } view(bytes);

    std::istream input(&view);
    return VerifyRecordedAudio(input, bytes.size(), id, scene, seed, hash, expected);
}

class RecordedAudioReader
{
    std::ifstream input;
    ReplayAudioInfo info;
    std::vector<float> cache;
    int64_t first = -1;
    size_t count = 0;

  public:
    RecordedAudioReader(const std::filesystem::path &path, const ReplayAudioInfo &metadata, int64_t id,
                        int scene, int seed, const std::string &hash)
        : input(path, std::ios::binary), info(metadata)
    {
        if (!input)
        {
            throw std::runtime_error("Recorded audio is missing");
        }
        VerifyRecordedAudio(input, std::filesystem::file_size(path), id, scene, seed, hash, &info);
        cache.resize(size_t(16384) * metadata.channels);
        input.clear();
    }

    float Sample(int64_t frame, int channel)
    {
        if (frame < 0 || frame >= info.frames)
            return 0;
        channel = std::clamp(channel, 0, info.channels - 1);
        if (first < 0 || frame < first || frame >= first + int64_t(count))
        {
            first = (frame / 8192) * 8192;
            count = size_t(std::min<int64_t>(16384, info.frames - first));
            input.clear();
            input.seekg(std::streamoff(AudioHeaderBytes + uint64_t(first) * info.channels * 4));
            input.read(reinterpret_cast<char *>(cache.data()), std::streamsize(count * info.channels * 4));
            if (!input)
                throw std::runtime_error("Recorded audio read failed");
        }
        float v = cache[size_t(frame - first) * info.channels + size_t(channel)];
        return std::isfinite(v) ? v : 0;
    }

    std::array<float, 2> Stereo(double frame)
    {
        auto at = int64_t(std::floor(frame));
        float mix = float(frame - at);
        auto value = [&](int channel) {
            float a = Sample(at, channel), b = Sample(at + 1, channel);
            return a + (b - a) * mix;
        };
        if (info.channels == 1)
        {
            float a = value(0);
            return {a, a};
        }
        float l = value(0), r = value(1);
        if (info.channels >= 3)
        {
            l += value(2) * .70710678f;
            r += value(2) * .70710678f;
        }
        if (info.channels == 4)
        {
            l = value(0) + value(2) * .70710678f;
            r = value(1) + value(3) * .70710678f;
        }
        if (info.channels >= 5)
        {
            l += value(info.channels == 5 ? 3 : 4) * .70710678f;
            r += value(info.channels == 5 ? 4 : 5) * .70710678f;
        }
        if (info.channels >= 8)
        {
            l += value(6) * .70710678f;
            r += value(7) * .70710678f;
        }
        return {l, r};
    }
};
} // namespace practice
