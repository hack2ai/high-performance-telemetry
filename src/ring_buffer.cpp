#include "ring_buffer.hpp"
#include <cstring>

namespace telemetry {

bool SpscRingBuffer::try_push(std::uint64_t sequence,
                              std::uint16_t source,
                              std::uint16_t destination,
                              const std::uint8_t* data,
                              std::size_t length,
                              std::uint8_t flags) noexcept {
    if (data == nullptr || length == 0 || length > kMaxPayloadSize) return false;

    const auto write = write_.load(std::memory_order_relaxed);
    const auto next = (write + 1) & kRingBufferMask;
    const auto read = read_.load(std::memory_order_acquire);
    if (next == read) return false;

    auto& frame = buffer_[write];
    frame.header.magic = kMagicSignature;
    frame.header.timestamp_ns = now_nanoseconds();
    frame.header.sequence = sequence;
    frame.header.source = source;
    frame.header.destination = destination;
    frame.header.payload_len = static_cast<std::uint16_t>(length);
    frame.header.flags = flags;
    std::memcpy(frame.payload.data(), data, length);
    frame.header.checksum = checksum(frame.payload.data(), length);

    write_.store(next, std::memory_order_release);
    return true;
}

bool SpscRingBuffer::try_pop(Frame& output) noexcept {
    const auto read = read_.load(std::memory_order_relaxed);
    const auto write = write_.load(std::memory_order_acquire);
    if (read == write) return false;

    output = buffer_[read];
    read_.store((read + 1) & kRingBufferMask, std::memory_order_release);
    return true;
}

std::size_t SpscRingBuffer::pending() const noexcept {
    const auto write = write_.load(std::memory_order_acquire);
    const auto read = read_.load(std::memory_order_acquire);
    return (write - read) & kRingBufferMask;
}

} // namespace telemetry
