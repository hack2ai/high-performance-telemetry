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

constexpr std::array<std::size_t, 5> kBenchmarkPayloads{64, 128, 256, 512, 1024};

struct Config {
    std::size_t packets = 100000;
    std::size_t payload_size = 128;
    bool matrix = false;
};

struct BenchmarkResult {
    std::uint64_t generated = 0;
    std::uint64_t enqueued = 0;
    std::uint64_t processed = 0;
    std::uint64_t full_retries = 0;
    std::uint64_t invalid_frames = 0;
    std::uint64_t integrity_errors = 0;
    std::uint64_t ordering_errors = 0;
    std::uint64_t peak_queue_depth = 0;
    double seconds = 0.0;
    double throughput = 0.0;
    double avg_latency_us = 0.0;
};

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n\n"
              << "Options:\n"
              << "  --packets N    Number of synthetic packets (default: 100000)\n"
              << "  --payload N    Payload size in bytes, 1-1024 (default: 128)\n"
              << "  --matrix       Run payload matrix: 64, 128, 256, 512, 1024 bytes\n"
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
        if (arg == "--matrix") {
            config.matrix = true;
            continue;
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
    if (config.matrix && config.payload_size != 128) {
        throw std::invalid_argument("--matrix cannot be combined with a custom --payload value");
    }
    return config;
}

BenchmarkResult run_benchmark(std::size_t packets, std::size_t payload_size) {
    using clock = std::chrono::steady_clock;
    telemetry::SpscRingBuffer queue;
    telemetry::TelemetryStats stats;
    telemetry::PacketGenerator generator(payload_size);
    std::array<std::uint8_t, telemetry::kMaxPayloadSize> payload{};
    std::atomic<bool> producer_done{false};
    std::atomic<std::uint64_t> integrity_errors{0};
    std::atomic<std::uint64_t> ordering_errors{0};

    const auto start = clock::now();

    std::thread producer([&] {
        for (std::size_t i = 0; i < packets; ++i) {
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
        }
    });

    producer.join();
    consumer.join();

    const double seconds = std::chrono::duration<double>(clock::now() - start).count();
    const auto result = stats.snapshot();
    const auto integrity = integrity_errors.load(std::memory_order_relaxed);
    const auto ordering = ordering_errors.load(std::memory_order_relaxed);

    BenchmarkResult benchmark;
    benchmark.generated = result.generated;
    benchmark.enqueued = result.enqueued;
    benchmark.processed = result.processed;
    benchmark.full_retries = result.full_retries;
    benchmark.invalid_frames = result.invalid_frames;
    benchmark.integrity_errors = integrity;
    benchmark.ordering_errors = ordering;
    benchmark.peak_queue_depth = result.peak_queue_depth;
    benchmark.seconds = seconds;
    benchmark.throughput = seconds > 0.0 ? static_cast<double>(result.processed) / seconds : 0.0;
    benchmark.avg_latency_us = result.processed > 0
        ? static_cast<double>(result.total_latency_ns) / result.processed / 1000.0 : 0.0;
    return benchmark;
}

void print_single_result(std::size_t packets, std::size_t payload_size, const BenchmarkResult& result) {
    std::cout << "================================================\n"
              << "   HIGH-PERFORMANCE REAL-TIME TELEMETRY\n"
              << "================================================\n"
              << "Configuration\n"
              << "Queue capacity : " << telemetry::kUsableQueueCapacity << " frames\n"
              << "Packets        : " << packets << "\n"
              << "Payload size   : " << payload_size << " bytes\n"
              << "Frame size     : " << sizeof(telemetry::Frame) << " bytes\n"
              << "Physical slots : " << telemetry::kRingBufferCapacity << "\n\n"
              << "                BENCHMARK RESULT\n"
              << "Generated       : " << result.generated << "\n"
              << "Enqueued        : " << result.enqueued << "\n"
              << "Processed       : " << result.processed << "\n"
              << "Full retries    : " << result.full_retries << "\n"
              << "Invalid frames  : " << result.invalid_frames << "\n"
              << "Integrity errors : " << result.integrity_errors << "\n"
              << "Ordering errors  : " << result.ordering_errors << "\n"
              << "Peak queue      : " << result.peak_queue_depth << "\n"
              << "Elapsed         : " << std::fixed << std::setprecision(6) << result.seconds << " s\n"
              << "Throughput      : " << std::fixed << std::setprecision(2) << result.throughput << " packets/s\n"
              << "Avg latency     : " << std::fixed << std::setprecision(3) << result.avg_latency_us << " us\n"
              << "================================================\n";
}

void print_matrix(const Config& config) {
    std::cout << "===============================================================\n"
              << "                 TELEMETRY BENCHMARK MATRIX\n"
              << "===============================================================\n"
              << "Packets per run: " << config.packets << "\n\n";
    std::cout << std::left << std::setw(10) << "Payload"
              << std::setw(18) << "Throughput"
              << std::setw(16) << "Avg Latency"
              << std::setw(16) << "Full Retries"
              << std::setw(14) << "Peak Queue" << '\n';
    std::cout << std::left << std::setw(10) << "(bytes)"
              << std::setw(18) << "(packets/s)"
              << std::setw(16) << "(us)"
              << std::setw(16) << "(count)"
              << std::setw(14) << "(frames)" << '\n';

    for (const auto payload_size : kBenchmarkPayloads) {
        const auto result = run_benchmark(config.packets, payload_size);
        std::cout << std::left << std::setw(10) << payload_size
                  << std::setw(18) << std::fixed << std::setprecision(2) << result.throughput
                  << std::setw(16) << std::fixed << std::setprecision(3) << result.avg_latency_us
                  << std::setw(16) << result.full_retries
                  << std::setw(14) << result.peak_queue_depth << '\n';
        if (result.processed != config.packets || result.integrity_errors != 0 ||
            result.ordering_errors != 0) {
            throw std::runtime_error("Benchmark validation failed for payload " +
                                     std::to_string(payload_size) + " bytes");
        }
    }
    std::cout << "===============================================================\n";
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        const Config config = parse_args(argc, argv);
        if (config.matrix) {
            print_matrix(config);
            return 0;
        }

        const auto result = run_benchmark(config.packets, config.payload_size);
        print_single_result(config.packets, config.payload_size, result);
        return result.processed == config.packets && result.integrity_errors == 0 &&
                       result.ordering_errors == 0
                   ? 0
                   : 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        std::cerr << "Use --help for usage.\n";
        return 2;
    }
}
