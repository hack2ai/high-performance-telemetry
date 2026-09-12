#include "packet.hpp"
#include "packet_generator.hpp"
#include "ring_buffer.hpp"
#include "telemetry_stats.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

struct Config {
    std::size_t packets = 100000;
    std::size_t payload_size = 128;
};

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n\n"
              << "Options:\n"
              << "  --packets N    Number of synthetic packets (default: 100000)\n"
              << "  --payload N    Payload size in bytes, 1-1024 (default: 128)\n"
              << "  --help         Show this help message\n";
}

std::size_t parse_size(const std::string& value, const char* option) {
    try {
        std::size_t parsed = 0;
        const auto result = std::stoull(value, &parsed);
        if (parsed != value.size()) throw std::invalid_argument("trailing characters");
        return static_cast<std::size_t>(result);
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string("Invalid value for ") + option + ": " + value);
    }
}

Config parse_args(int argc, char* argv[]) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (arg == "--packets" || arg == "--payload") {
            if (i + 1 >= argc) throw std::invalid_argument("Missing value for " + arg);
            const auto value = parse_size(argv[++i], arg.c_str());
            if (arg == "--packets") config.packets = value;
            else config.payload_size = value;
            continue;
        }
        throw std::invalid_argument("Unknown option: " + arg);
    }
    if (config.packets == 0) throw std::invalid_argument("--packets must be greater than 0");
    if (config.payload_size == 0 || config.payload_size > telemetry::kMaxPayloadSize) {
        throw std::invalid_argument("--payload must be between 1 and " +
                                    std::to_string(telemetry::kMaxPayloadSize));
    }
    return config;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const Config config = parse_args(argc, argv);
        using clock = std::chrono::steady_clock;

        telemetry::SpscRingBuffer queue;
        telemetry::TelemetryStats stats;
        telemetry::PacketGenerator generator(config.payload_size);
        std::array<std::uint8_t, telemetry::kMaxPayloadSize> payload{};
        std::atomic<bool> producer_done{false};
        std::atomic<std::size_t> integrity_errors{0};
        std::atomic<std::size_t> ordering_errors{0};

        std::cout << "================================================\n";
        std::cout << "   HIGH-PERFORMANCE REAL-TIME TELEMETRY\n";
        std::cout << "================================================\n";
        std::cout << "Configuration\n";
        std::cout << "Queue capacity : " << queue.capacity() << " frames\n";
        std::cout << "Packets        : " << config.packets << "\n";
        std::cout << "Payload size   : " << generator.payload_size() << " bytes\n";
        std::cout << "Frame size     : " << sizeof(telemetry::Frame) << " bytes\n";
        std::cout << "Queue storage  : " << queue.capacity() * sizeof(telemetry::Frame) << " bytes\n\n";

        const auto start = clock::now();

        std::thread producer([&] {
            for (std::size_t i = 0; i < config.packets; ++i) {
                generator.generate_payload(i, payload);
                stats.record_generated();
                while (!queue.try_push(i, 100, 8080, payload.data(), generator.payload_size(), 0x03)) {
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
            std::size_t expected_sequence = 0;
            bool sample_printed = false;

            while (!producer_done.load(std::memory_order_acquire) || queue.pending() != 0) {
                if (!queue.try_pop(frame)) {
                    std::this_thread::yield();
                    continue;
                }

                if (!telemetry::is_valid(frame)) {
                    integrity_errors.fetch_add(1, std::memory_order_relaxed);
                    stats.record_invalid();
                    continue;
                }

                if (frame.header.sequence != expected_sequence) {
                    ordering_errors.fetch_add(1, std::memory_order_relaxed);
                    expected_sequence = static_cast<std::size_t>(frame.header.sequence);
                }
                ++expected_sequence;

                const auto now = telemetry::now_nanoseconds();
                const auto latency = now >= frame.header.timestamp_ns
                    ? now - frame.header.timestamp_ns : 0;
                stats.record_processed(latency);

                if (!sample_printed) {
                    std::cout << "--- SAMPLE FRAME ---\n";
                    std::cout << "Magic          : 0x" << std::hex << frame.header.magic << std::dec << "\n";
                    std::cout << "Sequence       : " << frame.header.sequence << "\n";
                    std::cout << "Checksum       : 0x" << std::hex << frame.header.checksum << std::dec << "\n";
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

        const auto integrity_failures = integrity_errors.load(std::memory_order_relaxed);
        const auto order_failures = ordering_errors.load(std::memory_order_relaxed);

        std::cout << "\n================================================\n";
        std::cout << "                BENCHMARK RESULT\n";
        std::cout << "================================================\n";
        std::cout << "Generated       : " << result.generated << "\n";
        std::cout << "Enqueued        : " << result.enqueued << "\n";
        std::cout << "Processed       : " << result.processed << "\n";
        std::cout << "Full retries    : " << result.full_retries << "\n";
        std::cout << "Invalid frames  : " << result.invalid_frames << "\n";
        std::cout << "Integrity errors : " << integrity_failures << "\n";
        std::cout << "Ordering errors  : " << order_failures << "\n";
        std::cout << "Peak queue      : " << result.peak_queue_depth << "\n";
        std::cout << "Elapsed         : " << std::fixed << std::setprecision(6) << seconds << " s\n";
        std::cout << "Throughput      : " << std::fixed << std::setprecision(2)
                  << throughput << " packets/s\n";
        std::cout << "Avg latency     : " << std::fixed << std::setprecision(3)
                  << avg_us << " us\n";
        std::cout << "================================================\n";

        return result.processed == config.packets && integrity_failures == 0 && order_failures == 0 ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        std::cerr << "Use --help for usage.\n";
        return 2;
    }
}
