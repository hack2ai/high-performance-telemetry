#pragma once

#include "packet.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace telemetry {

class SpscRingBuffer {
public:
    SpscRingBuffer()
        : buffer_(std::make_unique<Frame[]>(kRingBufferCapacity)) {}

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
    std::unique_ptr<Frame[]> buffer_;
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
};

} // namespace telemetry
