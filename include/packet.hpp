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
    std::uint64_t sequence = 0;
    std::uint16_t source = 0;
    std::uint16_t destination = 0;
    std::uint16_t payload_len = 0;
    std::uint8_t flags = 0;
    std::uint32_t checksum = 0;
};
#pragma pack(pop)

static_assert(sizeof(PacketHeader) == 31, "PacketHeader layout changed unexpectedly");

struct Frame {
    PacketHeader header{};
    std::array<std::uint8_t, kMaxPayloadSize> payload{};
};

inline std::uint64_t now_nanoseconds() noexcept {
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
}

// Small, deterministic checksum used only for local data-integrity verification.
inline std::uint32_t checksum(const std::uint8_t* data, std::size_t length) noexcept {
    std::uint32_t hash = 2166136261U;
    for (std::size_t i = 0; i < length; ++i) {
        hash ^= data[i];
        hash *= 16777619U;
    }
    return hash;
}

inline bool is_valid(const Frame& frame) noexcept {
    if (frame.header.magic != kMagicSignature ||
        frame.header.payload_len > kMaxPayloadSize) {
        return false;
    }
    return checksum(frame.payload.data(), frame.header.payload_len) == frame.header.checksum;
}

} // namespace telemetry
