#include "packet_generator.hpp"
#include <algorithm>

namespace telemetry {
PacketGenerator::PacketGenerator(std::size_t payload_size) noexcept
    : payload_size_(std::min(payload_size, kMaxPayloadSize)) {}

void PacketGenerator::generate_payload(
    std::size_t sequence,
    std::array<std::uint8_t, kMaxPayloadSize>& buffer) const noexcept {
    for (std::size_t i = 0; i < payload_size_; ++i) {
        buffer[i] = static_cast<std::uint8_t>((sequence + i) & 0xFFU);
    }
}
}
