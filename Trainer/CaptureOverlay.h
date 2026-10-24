#pragma once
#include "LayeredHud.h"
#include <string>
#include <vector>

// The game composites these exact HUD pixels, so window/game capture includes
// the overlay. The normal desktop overlay remains the fallback until acknowledged.
class CaptureOverlay
{
    static constexpr uint32_t Magic = 0x48505349, Capacity = 32 * 1024 * 1024 + 256;
    HANDLE mapping = nullptr, mutex = nullptr;
    uint8_t *data = nullptr;
    DWORD pid = 0;
    uint32_t acknowledged = 0;
    ULONGLONG lastAck = 0, nextConnect = 0;
    const LayeredHud *lastSource = nullptr;
    uint64_t lastFrame = 0;

    static const std::array<uint8_t, 65536> &StraightAlpha()
    {
        static const auto values = [] {
            std::array<uint8_t, 65536> result{};
            for (unsigned a = 1; a < 256; ++a)
                for (unsigned c = 0; c < 256; ++c)
                    result[a * 256 + c] = uint8_t(std::min(255u, (c * 255 + a / 2) / a));
            return result;
        }();
        return values;
    }

    bool Lock()
    {
        DWORD result = WaitForSingleObject(mutex, 0);
        if (result == WAIT_ABANDONED)
            ReleaseMutex(mutex);
        return result == WAIT_OBJECT_0;
    }

    template <class T> T Read(size_t at) const
    {
        T value{};
        std::memcpy(&value, data + at, sizeof(value));
        return value;
    }

    template <class T> void Write(size_t at, T value)
    {
        std::memcpy(data + at, &value, sizeof(value));
    }

  public:
    bool enabled = false;

    ~CaptureOverlay()
    {
        Close();
    }

    bool Active() const
    {
        return enabled && lastAck && GetTickCount64() - lastAck < 350;
    }

    void ResetGraphics()
    {
        lastAck = 0;
        lastSource = nullptr;
        lastFrame = 0;
    }

    void Close()
    {
        ResetGraphics();
        if (data)
            UnmapViewOfFile(data);
        if (mapping)
            CloseHandle(mapping);
        if (mutex)
            CloseHandle(mutex);
        data = nullptr;
        mapping = mutex = nullptr;
        pid = acknowledged = 0;
        lastAck = 0;
    }

    void Tick(DWORD process, bool visible)
    {
        if (!enabled || pid != process)
        {
            if (data && Lock())
            {
                Write(36, 0u);
                ReleaseMutex(mutex);
            }
            Close();
        }
        if (!enabled || !process)
            return;
        if (!data && GetTickCount64() >= nextConnect)
        {
            nextConnect = GetTickCount64() + 1000;
            auto name = L"Local\\ItSteals.Capture.v1." + std::to_wstring(process);
            mapping = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, name.c_str());
            mutex = OpenMutexW(SYNCHRONIZE | MUTEX_MODIFY_STATE, FALSE, (name + L".Lock").c_str());
            if (mapping && mutex)
                data = static_cast<uint8_t *>(
                    MapViewOfFile(mapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, Capacity));
            if (!data)
                Close();
            else
                pid = process;
        }
        if (!data || !Lock())
            return;
        bool valid = Read<uint32_t>(0) == Magic && Read<uint32_t>(4) == 1 && Read<uint32_t>(8) == Capacity &&
                     Read<DWORD>(12) == process;
        if (valid)
        {
            auto ack = Read<uint32_t>(20);
            if (ack && ack != acknowledged)
            {
                acknowledged = ack;
                lastAck = GetTickCount64();
            }
            if (!visible)
            {
                Write(36, 0u);
                lastAck = 0;
            }
        }
        ReleaseMutex(mutex);
        if (!valid)
            Close();
    }

    void Publish(const LayeredHud &source)
    {
        if (!enabled || !data || !source.Generation() ||
            (lastSource == &source && lastFrame == source.Generation()) || !Lock())
            return;
        const auto &alphaTable = StraightAlpha();
        uint32_t count = 0, offset = 256;
        bool valid = true;
        source.VisitPixels([&](HudRegion region, const uint8_t *pixels) {
            uint64_t size = uint64_t(region.Width()) * region.Height() * 4;
            if (count == 4 || size > Capacity - offset)
            {
                valid = false;
                return;
            }
            size_t desc = 64 + count * 24;
            Write(desc, region.left);
            Write(desc + 4, region.top);
            Write(desc + 8, region.Width());
            Write(desc + 12, region.Height());
            Write(desc + 16, offset);
            Write(desc + 20, uint32_t(size));
            // ImGui's target is premultiplied; Unity's GUI expects straight alpha.
            for (size_t i = 0; i < size; i += 4)
            {
                auto alpha = pixels[i + 3];
                if (alpha == 255)
                {
                    std::memcpy(data + offset + i, pixels + i, 4);
                    continue;
                }
                const auto *row = alphaTable.data() + unsigned(alpha) * 256;
                for (int c = 0; c < 3; c++)
                    data[offset + i + c] = row[pixels[i + c]];
                data[offset + i + 3] = alpha;
            }
            offset += uint32_t(size);
            count++;
        });
        Write(24, source.Width());
        Write(28, source.Height());
        Write(32, count);
        Write(36, valid && count ? 1u : 0u);
        auto serial = Read<uint32_t>(16) + 1;
        Write(16, serial ? serial : 1u);
        lastSource = &source;
        lastFrame = source.Generation();
        ReleaseMutex(mutex);
    }
};
