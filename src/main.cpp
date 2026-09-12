#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>

namespace telemetry {
constexpr std::uint32_t MAGIC = 0xDEADBEEF;
constexpr std::size_t MAX_PAYLOAD = 1024;
constexpr std::size_t CAPACITY = 4096;

#pragma pack(push, 1)
struct PacketHeader {
    std::uint32_t magic = MAGIC;
    std::uint64_t timestamp_ns = 0;
    std::uint16_t source = 0;
    std::uint16_t destination = 0;
    std::uint16_t payload_len = 0;
    std::uint8_t flags = 0;
};
#pragma pack(pop)

struct Frame { PacketHeader header{}; std::array<std::uint8_t, MAX_PAYLOAD> payload{}; };

inline std::uint64_t now_ns() noexcept {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

class SpscRingBuffer {
public:
    bool push(std::uint16_t src, std::uint16_t dst, const std::uint8_t* data, std::size_t len, std::uint8_t flags) noexcept {
        if (!data || len > MAX_PAYLOAD) return false;
        const auto w = write_.load(std::memory_order_relaxed);
        const auto next = (w + 1) % CAPACITY;
        const auto r = read_.load(std::memory_order_acquire);
        if (next == r) return false;
        auto& f = buffer_[w];
        f.header.magic = MAGIC; f.header.timestamp_ns = now_ns(); f.header.source = src;
        f.header.destination = dst; f.header.payload_len = static_cast<std::uint16_t>(len); f.header.flags = flags;
        std::memcpy(f.payload.data(), data, len);
        write_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(Frame& out) noexcept {
        const auto r = read_.load(std::memory_order_relaxed);
        const auto w = write_.load(std::memory_order_acquire);
        if (r == w) return false;
        out = buffer_[r];
        read_.store((r + 1) % CAPACITY, std::memory_order_release);
        return true;
    }
    std::size_t pending() const noexcept {
        const auto w = write_.load(std::memory_order_acquire);
        const auto r = read_.load(std::memory_order_acquire);
        return w >= r ? w - r : CAPACITY - r + w;
    }
    std::size_t capacity() const noexcept { return CAPACITY - 1; }
private:
    alignas(64) std::array<Frame, CAPACITY> buffer_{};
    alignas(64) std::atomic<std::size_t> write_{0};
    alignas(64) std::atomic<std::size_t> read_{0};
};
}

int main() {
    using clock = std::chrono::steady_clock;
    telemetry::SpscRingBuffer queue;
    constexpr std::size_t packets = 100000;
    std::array<std::uint8_t, 128> payload{};
    std::atomic<std::uint64_t> received{0}, dropped{0}, processed{0}, total_latency_ns{0}, peak_depth{0};

    std::cout << "================================================\n";
    std::cout << "   HIGH-PERFORMANCE REAL-TIME TELEMETRY\n";
    std::cout << "================================================\n";
    std::cout << "Queue capacity : " << queue.capacity() << " frames\n";
    std::cout << "Packets        : " << packets << "\n";
    std::cout << "Payload size   : " << payload.size() << " bytes\n\n";

    const auto start = clock::now();
    std::thread producer([&] {
        for (std::size_t i = 0; i < packets; ++i) {
            for (std::size_t j = 0; j < payload.size(); ++j) payload[j] = static_cast<std::uint8_t>((i + j) & 0xFFU);
            while (!queue.push(100, 8080, payload.data(), payload.size(), 0x03)) { dropped.fetch_add(1, std::memory_order_relaxed); std::this_thread::yield(); }
            received.fetch_add(1, std::memory_order_relaxed);
            const auto depth = static_cast<std::uint64_t>(queue.pending());
            auto old = peak_depth.load(std::memory_order_relaxed);
            while (old < depth && !peak_depth.compare_exchange_weak(old, depth, std::memory_order_relaxed)) {}
        }
    });

    std::thread consumer([&] {
        telemetry::Frame frame{}; bool sample = false;
        while (processed.load(std::memory_order_relaxed) < packets) {
            if (!queue.pop(frame)) { std::this_thread::yield(); continue; }
            if (frame.header.magic != telemetry::MAGIC || frame.header.payload_len > telemetry::MAX_PAYLOAD) continue;
            const auto now = telemetry::now_ns();
            total_latency_ns.fetch_add(now >= frame.header.timestamp_ns ? now - frame.header.timestamp_ns : 0, std::memory_order_relaxed);
            processed.fetch_add(1, std::memory_order_relaxed);
            if (!sample) {
                std::cout << "--- SAMPLE FRAME ---\n";
                std::cout << "Magic          : 0x" << std::hex << frame.header.magic << std::dec << "\n";
                std::cout << "Route          : Node " << frame.header.source << " -> Node " << frame.header.destination << "\n";
                std::cout << "Payload length : " << frame.header.payload_len << " bytes\n";
                std::cout << "Flags          : 0x" << std::hex << static_cast<unsigned>(frame.header.flags) << std::dec << "\n";
                sample = true;
            }
        }
    });
    producer.join(); consumer.join();

    const double seconds = std::chrono::duration<double>(clock::now() - start).count();
    const auto done = processed.load();
    const double throughput = seconds > 0 ? static_cast<double>(done) / seconds : 0;
    const double avg_us = done ? static_cast<double>(total_latency_ns.load()) / static_cast<double>(done) / 1000.0 : 0;
    std::cout << "\n================================================\n";
    std::cout << "                BENCHMARK RESULT\n";
    std::cout << "================================================\n";
    std::cout << "Received       : " << received.load() << "\n";
    std::cout << "Processed      : " << done << "\n";
    std::cout << "Dropped        : " << dropped.load() << "\n";
    std::cout << "Peak queue     : " << peak_depth.load() << "\n";
    std::cout << "Throughput     : " << std::fixed << std::setprecision(2) << throughput << " packets/s\n";
    std::cout << "Avg latency    : " << std::fixed << std::setprecision(3) << avg_us << " us\n";
    std::cout << "================================================\n";
    return done == packets ? 0 : 1;
}
