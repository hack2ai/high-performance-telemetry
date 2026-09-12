#include "telemetry_stats.hpp"

namespace telemetry {
void TelemetryStats::record_generated() noexcept {
    generated_.fetch_add(1, std::memory_order_relaxed);
}

void TelemetryStats::record_enqueued() noexcept {
    enqueued_.fetch_add(1, std::memory_order_relaxed);
}

void TelemetryStats::record_processed(std::uint64_t latency_ns) noexcept {
    processed_.fetch_add(1, std::memory_order_relaxed);
    total_latency_ns_.fetch_add(latency_ns, std::memory_order_relaxed);
}

void TelemetryStats::record_full_retry() noexcept {
    full_retries_.fetch_add(1, std::memory_order_relaxed);
}

void TelemetryStats::record_invalid() noexcept {
    invalid_frames_.fetch_add(1, std::memory_order_relaxed);
}

void TelemetryStats::observe_queue_depth(std::size_t depth) noexcept {
    auto peak = peak_queue_depth_.load(std::memory_order_relaxed);
    const auto value = static_cast<std::uint64_t>(depth);
    while (peak < value &&
           !peak_queue_depth_.compare_exchange_weak(
               peak, value, std::memory_order_relaxed)) {}
}

StatisticsSnapshot TelemetryStats::snapshot() const noexcept {
    return {
        generated_.load(std::memory_order_relaxed),
        enqueued_.load(std::memory_order_relaxed),
        processed_.load(std::memory_order_relaxed),
        full_retries_.load(std::memory_order_relaxed),
        invalid_frames_.load(std::memory_order_relaxed),
        total_latency_ns_.load(std::memory_order_relaxed),
        peak_queue_depth_.load(std::memory_order_relaxed)
    };
}
}
