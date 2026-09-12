#include "packet.hpp"
#include "packet_generator.hpp"
#include "ring_buffer.hpp"
#include "telemetry_logger.hpp"
#include "telemetry_stats.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr std::array<std::size_t, 5> kBenchmarkPayloads{64, 128, 256, 512, 1024};
struct Config { std::size_t packets = 100000; std::size_t payload_size = 128; bool matrix = false; std::string csv_path; };
struct BenchmarkResult {
    std::uint64_t generated = 0, enqueued = 0, processed = 0, full_retries = 0, invalid_frames = 0;
    std::uint64_t integrity_errors = 0, ordering_errors = 0, peak_queue_depth = 0;
    std::size_t queue_memory_bytes = 0;
    double seconds = 0.0, throughput = 0.0, avg_latency_us = 0.0, p50_latency_us = 0.0, p95_latency_us = 0.0, p99_latency_us = 0.0;
};

double percentile_us(std::vector<std::uint64_t>& samples, double percentile) {
    if (samples.empty()) return 0.0;
    const auto index = static_cast<std::size_t>((percentile / 100.0) * static_cast<double>(samples.size() - 1));
    std::nth_element(samples.begin(), samples.begin() + index, samples.end());
    return static_cast<double>(samples[index]) / 1000.0;
}

void print_usage(const char* program) {
    std::cout << "Usage: " << program << " [options]\n\nOptions:\n"
              << "  --packets N    Number of synthetic packets (default: 100000)\n"
              << "  --payload N    Payload size in bytes, 1-1024 (default: 128)\n"
              << "  --matrix       Run payload matrix: 64, 128, 256, 512, 1024 bytes\n"
              << "  --csv PATH     Write matrix results to a CSV file (requires --matrix)\n"
              << "  --help         Show this help message\n";
}

std::size_t parse_size(const std::string& value, const char* option) {
    try { std::size_t parsed = 0; const auto result = std::stoull(value, &parsed); if (parsed != value.size()) throw std::invalid_argument("trailing characters"); return static_cast<std::size_t>(result); }
    catch (const std::exception&) { throw std::invalid_argument(std::string("Invalid value for ") + option + ": " + value); }
}

Config parse_args(int argc, char* argv[]) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") { print_usage(argv[0]); std::exit(0); }
        if (arg == "--matrix") { config.matrix = true; continue; }
        if (arg == "--packets" || arg == "--payload" || arg == "--csv") {
            if (i + 1 >= argc) throw std::invalid_argument("Missing value for " + arg);
            if (arg == "--csv") config.csv_path = argv[++i];
            else if (arg == "--packets") config.packets = parse_size(argv[++i], arg.c_str());
            else config.payload_size = parse_size(argv[++i], arg.c_str());
            continue;
        }
        throw std::invalid_argument("Unknown option: " + arg);
    }
    if (config.packets == 0) throw std::invalid_argument("--packets must be greater than 0");
    if (config.payload_size == 0 || config.payload_size > telemetry::kMaxPayloadSize) throw std::invalid_argument("--payload must be between 1 and " + std::to_string(telemetry::kMaxPayloadSize));
    if (config.matrix && config.payload_size != 128) throw std::invalid_argument("--matrix cannot be combined with a custom --payload value");
    if (!config.csv_path.empty() && !config.matrix) throw std::invalid_argument("--csv requires --matrix");
    return config;
}

BenchmarkResult run_benchmark(std::size_t packets, std::size_t payload_size) {
    using clock = std::chrono::steady_clock;
    telemetry::SpscRingBuffer queue;
    telemetry::TelemetryStats stats;
    telemetry::PacketGenerator generator(payload_size);
    std::array<std::uint8_t, telemetry::kMaxPayloadSize> payload{};
    std::vector<std::uint64_t> latencies_ns; latencies_ns.reserve(packets);
    std::atomic<bool> producer_done{false};
    std::atomic<std::uint64_t> integrity_errors{0}, ordering_errors{0};
    const auto start = clock::now();

    std::thread producer([&] {
        for (std::size_t i = 0; i < packets; ++i) {
            generator.generate_payload(i, payload); stats.record_generated();
            while (!queue.try_push(i, 100, 8080, payload.data(), generator.payload_size(), 0x03)) { stats.record_full_retry(); std::this_thread::yield(); }
            stats.record_enqueued(); stats.observe_queue_depth(queue.pending());
        }
        producer_done.store(true, std::memory_order_release);
    });
    std::thread consumer([&] {
        telemetry::Frame frame{}; std::size_t expected_sequence = 0;
        while (!producer_done.load(std::memory_order_acquire) || queue.pending() != 0) {
            if (!queue.try_pop(frame)) { std::this_thread::yield(); continue; }
            if (!telemetry::is_valid(frame)) { integrity_errors.fetch_add(1, std::memory_order_relaxed); stats.record_invalid(); continue; }
            if (frame.header.sequence != expected_sequence) { ordering_errors.fetch_add(1, std::memory_order_relaxed); expected_sequence = static_cast<std::size_t>(frame.header.sequence); }
            ++expected_sequence;
            const auto now = telemetry::now_nanoseconds();
            const auto latency = now >= frame.header.timestamp_ns ? now - frame.header.timestamp_ns : 0;
            latencies_ns.push_back(latency); stats.record_processed(latency);
        }
    });
    producer.join(); consumer.join();

    const double seconds = std::chrono::duration<double>(clock::now() - start).count();
    const auto result = stats.snapshot();
    BenchmarkResult benchmark;
    benchmark.generated = result.generated; benchmark.enqueued = result.enqueued; benchmark.processed = result.processed;
    benchmark.full_retries = result.full_retries; benchmark.invalid_frames = result.invalid_frames;
    benchmark.integrity_errors = integrity_errors.load(std::memory_order_relaxed); benchmark.ordering_errors = ordering_errors.load(std::memory_order_relaxed);
    benchmark.peak_queue_depth = result.peak_queue_depth; benchmark.queue_memory_bytes = sizeof(telemetry::Frame) * telemetry::kRingBufferCapacity;
    benchmark.seconds = seconds; benchmark.throughput = seconds > 0.0 ? static_cast<double>(result.processed) / seconds : 0.0;
    benchmark.avg_latency_us = result.processed > 0 ? static_cast<double>(result.total_latency_ns) / result.processed / 1000.0 : 0.0;
    benchmark.p50_latency_us = percentile_us(latencies_ns, 50.0); benchmark.p95_latency_us = percentile_us(latencies_ns, 95.0); benchmark.p99_latency_us = percentile_us(latencies_ns, 99.0);
    return benchmark;
}

void print_single_result(std::size_t packets, std::size_t payload_size, const BenchmarkResult& r) {
    std::cout << "================================================\nHIGH-PERFORMANCE REAL-TIME TELEMETRY\n================================================\n"
              << "Packets        : " << packets << "\nPayload size   : " << payload_size << " bytes\nFrame size     : " << sizeof(telemetry::Frame) << " bytes\nQueue memory   : " << r.queue_memory_bytes / 1024.0 << " KiB\n\n"
              << "Processed       : " << r.processed << "\nFull retries    : " << r.full_retries << "\nInvalid frames  : " << r.invalid_frames << "\nIntegrity errors : " << r.integrity_errors << "\nOrdering errors  : " << r.ordering_errors << "\nPeak queue      : " << r.peak_queue_depth << "\n"
              << "Elapsed         : " << std::fixed << std::setprecision(6) << r.seconds << " s\nThroughput      : " << std::fixed << std::setprecision(2) << r.throughput << " packets/s\n"
              << "Avg latency     : " << std::fixed << std::setprecision(3) << r.avg_latency_us << " us\nP50 latency     : " << r.p50_latency_us << " us\nP95 latency     : " << r.p95_latency_us << " us\nP99 latency     : " << r.p99_latency_us << " us\n================================================\n";
}

void print_matrix(const Config& config) {
    std::ofstream csv;
    if (!config.csv_path.empty()) { csv.open(config.csv_path); if (!csv) throw std::runtime_error("Unable to open CSV output: " + config.csv_path); csv << "payload_bytes,packets,throughput_packets_per_sec,avg_latency_us,p50_latency_us,p95_latency_us,p99_latency_us,full_retries,peak_queue,queue_memory_bytes,integrity_errors,ordering_errors\n"; }
    std::cout << "=========================================================================\nTELEMETRY BENCHMARK MATRIX\n=========================================================================\nPackets per run: " << config.packets << "\n\n";
    std::cout << std::left << std::setw(10) << "Payload" << std::setw(18) << "Throughput" << std::setw(14) << "Avg(us)" << std::setw(12) << "P50(us)" << std::setw(12) << "P95(us)" << std::setw(12) << "P99(us)" << std::setw(14) << "Peak Queue" << '\n';
    for (const auto payload_size : kBenchmarkPayloads) {
        const auto r = run_benchmark(config.packets, payload_size);
        std::cout << std::left << std::setw(10) << payload_size << std::setw(18) << std::fixed << std::setprecision(2) << r.throughput << std::setw(14) << std::setprecision(3) << r.avg_latency_us << std::setw(12) << r.p50_latency_us << std::setw(12) << r.p95_latency_us << std::setw(12) << r.p99_latency_us << std::setw(14) << r.peak_queue_depth << '\n';
        if (csv) csv << payload_size << ',' << config.packets << ',' << std::fixed << std::setprecision(3) << r.throughput << ',' << std::setprecision(6) << r.avg_latency_us << ',' << r.p50_latency_us << ',' << r.p95_latency_us << ',' << r.p99_latency_us << ',' << r.full_retries << ',' << r.peak_queue_depth << ',' << r.queue_memory_bytes << ',' << r.integrity_errors << ',' << r.ordering_errors << '\n';
        if (r.processed != config.packets || r.integrity_errors != 0 || r.ordering_errors != 0) throw std::runtime_error("Benchmark validation failed for payload " + std::to_string(payload_size) + " bytes");
    }
    std::cout << "=========================================================================\n";
    if (csv) std::cout << "CSV results      : " << config.csv_path << '\n';
}
} // namespace

int main(int argc, char* argv[]) {
    try {
        const Config config = parse_args(argc, argv);
        telemetry::log(telemetry::LogLevel::info, "startup", "telemetry benchmark initialized");
        if (config.matrix) { telemetry::log(telemetry::LogLevel::info, "benchmark_matrix", "running payload-size matrix"); print_matrix(config); telemetry::log(telemetry::LogLevel::info, "shutdown", "benchmark completed"); return 0; }
        const auto result = run_benchmark(config.packets, config.payload_size);
        print_single_result(config.packets, config.payload_size, result);
        telemetry::log(result.integrity_errors == 0 && result.ordering_errors == 0 ? telemetry::LogLevel::info : telemetry::LogLevel::error, "benchmark_result", result.processed == config.packets ? "validation passed" : "validation failed");
        return result.processed == config.packets && result.integrity_errors == 0 && result.ordering_errors == 0 ? 0 : 1;
    } catch (const std::exception& error) { telemetry::log(telemetry::LogLevel::error, "exception", error.what()); std::cerr << "Error: " << error.what() << "\nUse --help for usage.\n"; return 2; }
}
