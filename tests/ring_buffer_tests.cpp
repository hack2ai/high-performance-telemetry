#include "ring_buffer.hpp"

#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <thread>

using telemetry::Frame;
using telemetry::SpscRingBuffer;

int main() {
    SpscRingBuffer queue;
    Frame frame{};

    // Empty queue and invalid-input checks.
    assert(queue.pending() == 0);
    const std::array<std::uint8_t, 4> small{1, 2, 3, 4};
    assert(!queue.try_pop(frame));
    assert(!queue.try_push(0, 1, 2, nullptr, small.size(), 0));
    assert(!queue.try_push(0, 1, 2, small.data(), 0, 0));
    assert(!queue.try_push(0, 1, 2, small.data(), telemetry::kMaxPayloadSize + 1, 0));

    // Boundary payload sizes: 1 byte and maximum payload.
    const std::array<std::uint8_t, 1> one_byte{0xAB};
    assert(queue.try_push(1, 10, 20, one_byte.data(), one_byte.size(), 0x01));
    assert(queue.try_pop(frame));
    assert(frame.header.payload_len == 1);
    assert(frame.payload[0] == 0xAB);
    assert(telemetry::is_valid(frame));

    std::array<std::uint8_t, telemetry::kMaxPayloadSize> max_payload{};
    max_payload.front() = 0x11;
    max_payload.back() = 0xEE;
    assert(queue.try_push(2, 30, 40, max_payload.data(), max_payload.size(), 0x03));
    assert(queue.try_pop(frame));
    assert(frame.header.payload_len == telemetry::kMaxPayloadSize);
    assert(frame.payload.front() == 0x11);
    assert(frame.payload.back() == 0xEE);
    assert(telemetry::is_valid(frame));

    // FIFO and capacity boundary.
    for (std::size_t i = 0; i < queue.capacity(); ++i) {
        assert(queue.try_push(i, 10, 20, small.data(), small.size(), 0x01));
    }
    assert(queue.pending() == queue.capacity());
    assert(!queue.try_push(queue.capacity(), 10, 20, small.data(), small.size(), 0x01));

    for (std::size_t expected = 0; expected < queue.capacity(); ++expected) {
        assert(queue.try_pop(frame));
        assert(frame.header.sequence == expected);
        assert(telemetry::is_valid(frame));
    }
    assert(queue.pending() == 0);
    assert(!queue.try_pop(frame));

    // Checksum corruption must invalidate a frame.
    assert(queue.try_push(999, 1, 2, small.data(), small.size(), 0));
    assert(queue.try_pop(frame));
    assert(telemetry::is_valid(frame));
    frame.payload[0] ^= 0xFFU;
    assert(!telemetry::is_valid(frame));

    // Multiple complete wraparound cycles.
    SpscRingBuffer wrap_queue;
    constexpr std::size_t wrap_cycles = 8;
    const std::size_t total = wrap_cycles * wrap_queue.capacity();
    for (std::size_t i = 0; i < total; ++i) {
        if (wrap_queue.pending() == wrap_queue.capacity()) {
            assert(wrap_queue.try_pop(frame));
        }
        assert(wrap_queue.try_push(i, 3, 4, small.data(), small.size(), 0));
    }
    while (wrap_queue.pending() != 0) {
        assert(wrap_queue.try_pop(frame));
    }

    // Concurrent SPSC FIFO + integrity test.
    constexpr std::size_t iterations = 50000;
    SpscRingBuffer concurrent_queue;
    std::atomic<bool> failed{false};

    std::thread producer([&] {
        std::array<std::uint8_t, 8> data{};
        for (std::size_t i = 0; i < iterations; ++i) {
            data[0] = static_cast<std::uint8_t>(i & 0xFFU);
            data[7] = static_cast<std::uint8_t>((i >> 8) & 0xFFU);
            while (!concurrent_queue.try_push(i, 1, 2, data.data(), data.size(), 0)) {
                std::this_thread::yield();
            }
        }
    });

    std::thread consumer([&] {
        Frame out{};
        std::size_t count = 0;
        while (count < iterations) {
            if (!concurrent_queue.try_pop(out)) {
                std::this_thread::yield();
                continue;
            }
            if (!telemetry::is_valid(out) ||
                out.header.sequence != count ||
                out.header.payload_len != 8 ||
                out.header.source != 1 ||
                out.header.destination != 2 ||
                out.payload[0] != static_cast<std::uint8_t>(count & 0xFFU) ||
                out.payload[7] != static_cast<std::uint8_t>((count >> 8) & 0xFFU)) {
                failed.store(true, std::memory_order_relaxed);
                return;
            }
            ++count;
        }
    });

    producer.join();
    consumer.join();

    assert(!failed.load(std::memory_order_relaxed));
    assert(concurrent_queue.pending() == 0);
    return 0;
}
