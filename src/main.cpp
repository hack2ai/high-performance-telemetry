#include "packet.hpp"
#include "packet_generator.hpp"
#include "ring_buffer.hpp"
#include "telemetry_stats.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <thread>

int main() {
    using clock = std::chrono::steady_clock;
    telemetry::SpscRingBuffer queue;
    telemetry::TelemetryStats stats;
    telemetry::PacketGenerator generator(128);
    constexpr std::size_t packets = 100000;
    std::array<std::uint8_t, telemetry::kMaxPayloadSize> payload{};
    std::atomic<bool> producer_done{false};

    std::cout << "================================================\n";
    std::cout << "   HIGH-PERFORMANCE REAL-TIME TELEMETRY\n";
    std::cout << "================================================\n";
    std::cout << "Queue capacity : " << queue.capacity() << " frames\n";
    std::cout << "Packets        : " << packets << "\n";
    std::cout << "Payload size   : " << generator.payload_size() << " bytes\n";
    std::cout << "Frame size     : " << sizeof(telemetry::Frame) << " bytes\n\n";

    const auto start = clock::now();

    std::thread producer([&] {
        for (std::size_t i = 0; i < packets; ++i) {
            generator.generate_payload(i, payload);
            stats.record_generated();
            while (!queue.try_push(100, 8080, payload.data(), generator.payload_size(), 0x03)) {
                stats.record_full_retry();
                std::this_thread::yield();
            }
            stats.record_enqueued();
            stats.observe_queue_depth(queue.pending());
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&] {
        telemetry::Frame frame{};
        bool sample_printed = false;
        while (!producer_done.load(std::memory_order_acquire) || queue.pending() != 0) {
            if (!queue.try_pop(frame)) {
                std::this_thread::yield();
                continue;
            }
            if (!telemetry::is_valid(frame)) {
                stats.record_invalid();
                continue;
            }
            const auto now = telemetry::now_nanoseconds();
            const auto latency = now >= frame.header.timestamp_ns
                ? now - frame.header.timestamp_ns : 0;
            stats.record_processed(latency);

            if (!sample_printed) {
                std::cout << "--- SAMPLE FRAME ---\n";
                std::cout << "Magic          : 0x" << std::hex << frame.header.magic << std::dec << "\n";
                std::cout << "Route          : Node " << frame.header.source
                          << " -> Node " << frame.header.destination << "\n";
                std::cout << "Payload length : " << frame.header.payload_len << " bytes\n";
                std::cout << "Flags          : 0x" << std::hex
                          << static_cast<unsigned>(frame.header.flags) << std::dec << "\n";
                sample_printed = true;
            }
        }
    });

    producer.join();
    consumer.join();

    const double seconds = std::chrono::duration<double>(clock::now() - start).count();
    const auto result = stats.snapshot();
    const double throughput = seconds > 0.0
        ? static_cast<double>(result.processed) / seconds : 0.0;
    const double avg_us = result.processed > 0
        ? static_cast<double>(result.total_latency_ns) / result.processed / 1000.0 : 0.0;

    std::cout << "\n================================================\n";
    std::cout << "                BENCHMARK RESULT\n";
    std::cout << "================================================\n";
    std::cout << "Generated       : " << result.generated << "\n";
    std::cout << "Enqueued        : " << result.enqueued << "\n";
    std::cout << "Processed       : " << result.processed << "\n";
    std::cout << "Full retries    : " << result.full_retries << "\n";
    std::cout << "Invalid frames  : " << result.invalid_frames << "\n";
    std::cout << "Peak queue      : " << result.peak_queue_depth << "\n";
    std::cout << "Throughput      : " << std::fixed << std::setprecision(2)
              << throughput << " packets/s\n";
    std::cout << "Avg latency     : " << std::fixed << std::setprecision(3)
              << avg_us << " us\n";
    std::cout << "================================================\n";

    return result.processed == packets ? 0 : 1;
}
