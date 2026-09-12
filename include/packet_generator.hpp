#pragma once
#include "packet.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace telemetry {
class PacketGenerator {
public:
    explicit PacketGenerator(std::size_t payload_size = 128) noexcept;
    void generate_payload(std::size_t sequence,
                          std::array<std::uint8_t, kMaxPayloadSize>& buffer) const noexcept;
    std::size_t payload_size() const noexcept { return payload_size_; }

private:
    std::size_t payload_size_;
};
}
