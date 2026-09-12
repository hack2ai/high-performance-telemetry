#include "ring_buffer.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <thread>

int main() {
    constexpr std::size_t iterations = 200000;
    telemetry::SpscRingBuffer queue;
    std::atomic<bool> failed{false};

    std::thread producer([&] {
        std::array<std::uint8_t, 32> payload{};
        for (std::size_t i = 0; i < iterations; ++i) {
            payload[0] = static_cast<std::uint8_t>(i & 0xFFU);
            payload[31] = static_cast<std::uint8_t>((i >> 8) & 0xFFU);
            while (!queue.try_push(i, 100, 200, payload.data(), payload.size(), 0x01)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        telemetry::Frame frame{};
        for (std::size_t expected = 0; expected < iterations; ++expected) {
            while (!queue.try_pop(frame)) {
                std::this_thread::yield();
            }
            if (!telemetry::is_valid(frame) ||
                frame.header.sequence != expected ||
                frame.header.payload_len != 32 ||
                frame.header.source != 100 ||
                frame.header.destination != 200 ||
                frame.payload[0] != static_cast<std::uint8_t>(expected & 0xFFU) ||
                frame.payload[31] != static_cast<std::uint8_t>((expected >> 8) & 0xFFU)) {
                failed.store(true, std::memory_order_relaxed);
                return;
            }
        }
    });

    producer.join();
    consumer.join();

    assert(!failed.load(std::memory_order_relaxed));
    assert(queue.pending() == 0);
    return 0;
}
