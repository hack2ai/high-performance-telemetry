# High-Performance Real-Time Telemetry

[![C++ CI](https://github.com/hack2ai/high-performance-telemetry/actions/workflows/ci.yml/badge.svg)](https://github.com/hack2ai/high-performance-telemetry/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)
![CMake](https://img.shields.io/badge/CMake-3.16%2B-blue.svg)

**Low-latency C++20 systems programming with a bounded Single-Producer/Single-Consumer (SPSC) ring buffer.**

A portfolio-oriented systems project focused on concurrency correctness, predictable memory usage, queue backpressure, and reproducible performance measurement under a controlled synthetic workload.

> **Scope:** This repository generates and processes synthetic telemetry locally. It does not capture live network traffic, intercept communications, decrypt traffic, or collect credentials.

## Key Features

- Fixed-size, pre-allocated frame storage
- SPSC concurrency with atomic acquire/release ordering
- Power-of-two ring indexing with bit masking
- Configurable compile-time queue capacity
- Sequence and checksum validation
- Boundary, wraparound, concurrent, and stress tests
- AddressSanitizer + UndefinedBehaviorSanitizer CI
- Warm-up and repeated benchmark runs
- Average and P50/P95/P99 latency metrics
- Payload-size benchmark matrix
- CSV export and benchmark reporting
- CMake install and package support

## Architecture

```text
Synthetic Workload
       |
       v
+-------------------+
| Packet Generator  |
+---------+---------+
          |
          v
+---------------------------+
|      SPSC Ring Buffer     |
|---------------------------|
| Fixed-memory frame array  |
| Atomic read/write indexes |
| Acquire/release ordering  |
| Power-of-two masking      |
+-------------+-------------+
              |
              v
+---------------------------+
| Consumer / Validation     |
| Sequence + checksum       |
+-------------+-------------+
              |
              v
+---------------------------+
| Statistics / Benchmarking |
+---------------------------+
```

## Technical Highlights

| Area | Implementation |
|---|---|
| Language | C++20 |
| Concurrency | SPSC producer/consumer |
| Synchronization | `std::atomic`, acquire/release |
| Queue storage | Pre-allocated fixed-size frame array |
| Optimization | Power-of-two index masking |
| Integrity | Sequence numbers + deterministic checksum |
| Configuration | Compile-time queue capacity |
| Metrics | Throughput, average latency, P50/P95/P99 |
| Testing | Boundary, FIFO, wraparound, concurrent, stress |
| Sanitizers | AddressSanitizer + UndefinedBehaviorSanitizer |
| Build | CMake + CTest |
| CI | GitHub Actions |
| Reporting | CSV + benchmark reports |
| License | MIT |

## Repository Structure

```text
high-performance-telemetry/
├── .github/workflows/       # CI pipeline
├── benchmarks/              # benchmark and reporting tools
├── cmake/                   # CMake package configuration
├── include/                 # public headers
├── src/                     # implementation and CLI
├── tests/                   # unit/concurrency/stress tests
├── CMakeLists.txt
├── CONTRIBUTING.md
├── LICENSE
├── RELEASE_NOTES.md
├── SECURITY.md
└── README.md
```

## Requirements

- C++20-compatible compiler
- CMake 3.16+
- POSIX-like shell for benchmark helper scripts

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

### Configure Queue Capacity

The default configuration uses **4096 physical slots** and **4095 usable slots**. One slot remains unused so full and empty states are distinguishable.

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DTELEMETRY_RING_BUFFER_CAPACITY=8192

cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The capacity must be a power of two and at least 2.

## Run

### Default workload

```bash
./build/telemetry
```

### Custom workload

```bash
./build/telemetry --packets 1000000 --payload 512
```

### Repeated benchmark

```bash
./build/telemetry \
  --packets 100000 \
  --payload 256 \
  --warmup 2 \
  --runs 5
```

### Payload-size matrix

```bash
./build/telemetry \
  --matrix \
  --packets 100000 \
  --warmup 2 \
  --runs 5
```

The matrix covers **64, 128, 256, 512, and 1024 bytes**.

### CSV export

```bash
./build/telemetry \
  --matrix \
  --packets 100000 \
  --warmup 2 \
  --runs 5 \
  --csv build/benchmark.csv
```

## Benchmark Methodology

The benchmark separates warm-up runs from measured runs and supports repeated measurements to reduce over-interpreting a single noisy run.

Reported metrics include:

- **Throughput** — processed packets per second
- **Average latency** — mean producer-to-consumer latency
- **P50 / P95 / P99 latency** — typical and tail behavior
- **Full retries** — producer backpressure events when the queue is full
- **Peak queue depth** — maximum observed occupancy
- **Queue memory** — static storage reserved by the ring buffer
- **Integrity / ordering errors** — correctness checks during processing

Performance is machine-dependent. CPU model, compiler, operating system, scheduler behavior, background load, and benchmark configuration can affect results. Keep the environment and workload consistent when comparing runs.

## Performance Comparison

The project supports a controlled comparison between power-of-two bit-mask indexing and modulo indexing.

```text
Optimized: (index + 1) & mask
Baseline:  (index + 1) % capacity
```

Run:

```bash
PACKETS=100000 ./benchmarks/compare_indexing.sh
```

This is an empirical comparison, not a universal performance claim.

## Testing

Coverage includes:

- Empty queue and invalid input
- Minimum and maximum payload sizes
- Full-queue behavior
- FIFO sequence ordering
- Checksum corruption detection
- Multiple wraparound cycles
- Concurrent SPSC operation
- Long-running stress execution

Run all tests:

```bash
ctest --test-dir build --output-on-failure
```

## CI

GitHub Actions validates:

1. Standard Release build and tests
2. Configurable-capacity build
3. AddressSanitizer + UndefinedBehaviorSanitizer build
4. Benchmark smoke tests
5. CMake installation smoke test

Treat the live workflow status as authoritative. Do not claim CI is passing until GitHub reports a completed successful run.

## Install

```bash
cmake --install build --prefix ./install
```

The CMake package exports the `HighPerformanceTelemetry::` target namespace.

## Design Notes

### Why SPSC?

The queue is intentionally designed for exactly one producer and one consumer. This keeps ownership and synchronization rules explicit and avoids presenting an SPSC structure as an MPMC queue.

### Why Power-of-Two Capacity?

A power-of-two capacity allows efficient wraparound with a bit mask:

```cpp
next = (write + 1) & kRingBufferMask;
```

The invariant is enforced at compile time.

### Why Integrity Checks?

Sequence numbers verify ordering, while the checksum detects payload corruption. The benchmark can therefore measure performance while continuously checking correctness.

## Security and Scope

See [SECURITY.md](SECURITY.md) for the security policy and [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines.

## Release Notes

See [RELEASE_NOTES.md](RELEASE_NOTES.md) for the v1.1.0 release summary.

## Resume-Ready Description

**High-Performance Real-Time Telemetry Pipeline — C++20**

Built a fixed-memory SPSC ring-buffer pipeline using atomic acquire/release synchronization and power-of-two index masking; implemented sequence/checksum validation, configurable capacity, concurrent/stress testing, ASan/UBSan CI, repeated throughput and P50/P95/P99 latency benchmarking, CSV reporting, and CMake installation/package support.

## Interview Topics

- SPSC vs MPMC queue design
- C++ atomic memory ordering
- Full/empty ring-buffer invariants
- Cache-aware data layout
- Backpressure under queue saturation
- Tail-latency measurement
- Benchmark reproducibility
- Modulo vs bit-mask indexing
- Sanitizer-backed concurrency testing

## License

MIT — see [LICENSE](LICENSE).
