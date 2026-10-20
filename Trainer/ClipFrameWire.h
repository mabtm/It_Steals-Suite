#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace suite_clip
{
inline constexpr int FrameVersion = 4, FrameSlots = 3, FrameHeader = 1024, FrameSlotBytes = 16 * 1024 * 1024;
inline constexpr size_t FrameCapacity = size_t(FrameHeader) + FrameSlots * size_t(FrameSlotBytes);

inline constexpr int FrameOffset(int slot)
{
    return FrameHeader + slot * FrameSlotBytes;
}

// Shared policy for the real bridge/runtime and portable reorder/backpressure
// tests. Completion may be out of order; consumption is always source order.
class FrameWindow
{
    std::array<bool, FrameSlots> complete{};
    uint64_t next = 0, first = 0, total = 0;

  public:
    void Reset(uint64_t count)
    {
        next = first = 0;
        total = count;
        complete.fill(false);
    }

    bool CanRequest() const
    {
        return next < total && next - first < FrameSlots;
    }

    uint64_t Request()
    {
        if (!CanRequest())
            throw std::logic_error("Frame window is full");
        return next++;
    }

    void Complete(uint64_t frame)
    {
        if (frame < first || frame >= next || complete[frame % FrameSlots])
            throw std::logic_error("Stale or duplicate frame completion");
        complete[frame % FrameSlots] = true;
    }

    bool Ready() const
    {
        return first < next && complete[first % FrameSlots];
    }

    uint64_t Consume()
    {
        if (!Ready())
            throw std::logic_error("Frame order is not ready");
        complete[first % FrameSlots] = false;
        return first++;
    }

    uint64_t Requested() const
    {
        return next;
    }

    uint64_t Consumed() const
    {
        return first;
    }

    uint64_t InFlight() const
    {
        return next - first;
    }
};
} // namespace suite_clip
