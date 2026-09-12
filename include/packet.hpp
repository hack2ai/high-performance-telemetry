#pragma once
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace telemetry {
constexpr std::uint32_t kMagicSignature = 0x54454C45;
constexpr std::size_t kMaxPayloadSize = 1024;
constexpr std::size_t kRingBufferCapacity = 4096;
constexpr std::size_t kUsableQueueCapacity = kRingBufferCapacity - 1;
#pragma pack(push, 1)
struct PacketHeader {
    std::uint32_t magic = kMagicSignature;
    std::uint64_t timestamp_ns = 0;
    std::uint16_t source = 0;
    std::uint16_t destination = 0;
    std::uint16_t payload_len = 0;
    std::uint8_t flags = 0;
};
#pragma pack(pop)
static_assert(sizeof(PacketHeader) == 19);
struct Frame {
    PacketHeader header{};
    std::array<std::uint8_t, kMaxPayloadSize> payload{};
};
inline std::uint64_t now_nanoseconds() noexcept {
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}
inline bool is_valid(const Frame& frame) noexcept {
    return frame.header.magic == kMagicSignature && frame.header.payload_len <= kMaxPayloadSize;
}
}
