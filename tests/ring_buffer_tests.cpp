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
    const std::array<std::uint8_t, 4> payload{1, 2, 3, 4};

    assert(queue.pending() == 0);
    assert(!queue.try_pop(frame));
    assert(!queue.try_push(0, 1, 2, nullptr, payload.size(), 0));
    assert(!queue.try_push(0, 1, 2, payload.data(), 0, 0));
    assert(!queue.try_push(0, 1, 2, payload.data(), telemetry::kMaxPayloadSize + 1, 0));

    for (std::size_t i = 0; i < queue.capacity(); ++i) {
        assert(queue.try_push(i, 10, 20, payload.data(), payload.size(), 0x01));
    }
    assert(queue.pending() == queue.capacity());
    assert(!queue.try_push(queue.capacity(), 10, 20, payload.data(), payload.size(), 0x01));

    assert(queue.try_pop(frame));
    assert(frame.header.sequence == 0);
    assert(frame.header.source == 10);
    assert(frame.header.destination == 20);
    assert(frame.header.payload_len == payload.size());
    assert(frame.payload[0] == 1 && frame.payload[3] == 4);
    assert(telemetry::is_valid(frame));

    frame.payload[0] ^= 0xFFU;
    assert(!telemetry::is_valid(frame));

    // Exercise multiple complete wraparound cycles.
    SpscRingBuffer wrap_queue;
    constexpr std::size_t wrap_cycles = 8;
    const std::size_t total = wrap_cycles * wrap_queue.capacity();
    for (std::size_t i = 0; i < total; ++i) {
        while (!wrap_queue.try_push(i, 3, 4, payload.data(), payload.size(), 0)) {
            assert(wrap_queue.try_pop(frame));
            assert(frame.header.sequence == i - wrap_queue.capacity() + 1);
        }
        if ((i + 1) % wrap_queue.capacity() == 0) {
            assert(wrap_queue.pending() == wrap_queue.capacity());
            assert(wrap_queue.try_pop(frame));
        }
    }
    while (wrap_queue.pending() != 0) {
        assert(wrap_queue.try_pop(frame));
    }
    assert(wrap_queue.pending() == 0);

    constexpr std::size_t iterations = 50000;
    SpscRingBuffer concurrent_queue;
    std::atomic<bool> failed{false};

    std::thread producer([&] {
        std::array<std::uint8_t, 8> data{};
        for (std::size_t i = 0; i < iterations; ++i) {
            data[0] = static_cast<std::uint8_t>(i & 0xFFU);
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
            if (!telemetry::is_valid(out) || out.header.sequence != count ||
                out.header.payload_len != 8 || out.header.source != 1 ||
                out.header.destination != 2 || out.payload[0] != (count & 0xFFU)) {
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
