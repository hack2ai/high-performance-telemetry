#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace telemetry {
struct StatisticsSnapshot {
    std::uint64_t generated = 0;
    std::uint64_t enqueued = 0;
    std::uint64_t processed = 0;
    std::uint64_t full_retries = 0;
    std::uint64_t invalid_frames = 0;
    std::uint64_t total_latency_ns = 0;
    std::uint64_t peak_queue_depth = 0;
};

class TelemetryStats {
public:
    void record_generated() noexcept;
    void record_enqueued() noexcept;
    void record_processed(std::uint64_t latency_ns) noexcept;
    void record_full_retry() noexcept;
    void record_invalid() noexcept;
    void observe_queue_depth(std::size_t depth) noexcept;
    StatisticsSnapshot snapshot() const noexcept;

private:
    std::atomic<std::uint64_t> generated_{0};
    std::atomic<std::uint64_t> enqueued_{0};
    std::atomic<std::uint64_t> processed_{0};
    std::atomic<std::uint64_t> full_retries_{0};
    std::atomic<std::uint64_t> invalid_frames_{0};
    std::atomic<std::uint64_t> total_latency_ns_{0};
    std::atomic<std::uint64_t> peak_queue_depth_{0};
};
}
