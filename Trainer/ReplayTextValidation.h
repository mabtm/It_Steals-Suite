#pragma once
#include <string>
#include <cstdint>

namespace practice
{
inline bool ReplayUtf8(const std::string &text, size_t limit)
{
    if (text.size() > limit || text.find('\0') != std::string::npos)
        return false;
    for (size_t i = 0; i < text.size();)
    {
        uint32_t c = uint8_t(text[i++]);
        if (c < 128)
            continue;
        int n = c >= 0xc2 && c <= 0xdf ? 1 : c >= 0xe0 && c <= 0xef ? 2 : c >= 0xf0 && c <= 0xf4 ? 3 : -1;
        if (n < 0 || i + size_t(n) > text.size())
            return false;
        uint32_t value = c & ((1u << (6 - n)) - 1);
        for (int k = 0; k < n; k++)
        {
            c = uint8_t(text[i++]);
            if ((c & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (c & 63);
        }
        if (value < (n == 1   ? 128u
                     : n == 2 ? 2048u
                              : 65536u) ||
            value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return false;
    }
    return true;
}
} // namespace practice
