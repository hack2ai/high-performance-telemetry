#pragma once

#include "packet.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace telemetry {

class SpscRingBuffer {
public:
    bool try_push(std::uint64_t sequence,
                  std::uint16_t source,
                  std::uint16_t destination,
                  const std::uint8_t* data,
                  std::size_t length,
                  std::uint8_t flags) noexcept;
    bool try_pop(Frame& output) noexcept;
    std::size_t pending() const noexcept;
    constexpr std::size_t capacity() const noexcept { return kUsableQueueCapacity; }

private:
    alignas(64) std::array<Frame, kRingBufferCapacity> buffer_{};
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
};

} // namespace telemetry
