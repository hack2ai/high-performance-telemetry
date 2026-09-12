# High-Performance Real-Time Telemetry

A C++20 systems project demonstrating a bounded single-producer/single-consumer (SPSC) ring buffer for low-latency synthetic telemetry ingestion.

## Features

- Fixed-size packet frames
- Pre-allocated circular storage
- Atomic producer/consumer indexes
- Acquire/release memory ordering
- Power-of-two ring indexing with bit masking
- Optional modulo-indexing baseline for performance comparison
- Packet validation
- Overflow handling with full-queue retry tracking
- Configurable packet count and payload size
- Throughput and latency benchmarking
- P50/P95/P99 latency percentiles
- Static queue-memory reporting
- Payload-size benchmark matrix (64/128/256/512/1024 bytes)
- Machine-readable CSV benchmark export
- CMake build
- Unit tests
- GitHub Actions CI

## Scope

This is a local synthetic telemetry benchmark. It does not capture network traffic, perform packet interception, decrypt traffic, or collect credentials.

## Architecture

```text
Synthetic Packet Generator
          |
          v
   +----------------+
   |   SPSC Queue   |
   | 4095 usable    |
   | frame slots    |
   +-------+--------+
           |
           v
     Packet Consumer
           |
     +-----+-----+
     |           |
 Validation   Statistics
                 |
              Benchmark
```

## Ring Buffer Design

The queue uses **4096 physical slots**, giving **4095 usable frame slots** because one slot is reserved to distinguish full from empty. Since 4096 is a power of two, the default implementation uses a bit mask for index wrapping:

```cpp
next = (write + 1) & kRingBufferMask;
```

A CMake option can build the same queue with modulo-based wrapping for a controlled comparison:

```bash
cmake -S . -B build-modulo -DCMAKE_BUILD_TYPE=Release -DTELEMETRY_USE_MODULO_INDEXING=ON
```

The comparison is an empirical benchmark, not a universal claim: results depend on the CPU, compiler, operating system, and system load. The implementation is intentionally **SPSC only**: exactly one producer thread and one consumer thread are supported.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Run

Default benchmark:

```bash
./build/telemetry
```

Custom packet count:

```bash
./build/telemetry --packets 1000000
```

Custom payload size:

```bash
./build/telemetry --payload 512
```

Benchmark matrix:

```bash
./build/telemetry --matrix --packets 100000
```

Export matrix results to CSV:

```bash
./build/telemetry --matrix --packets 100000 --csv build/benchmark.csv
```

Show command-line help:

```bash
./build/telemetry --help
```

Payload size must be between 1 and 1024 bytes. The benchmark uses retry-on-full semantics, so `Full retries` measures producer backpressure events rather than dropped packets.

## Benchmark Metrics

Each benchmark reports:

- **Throughput:** processed packets per second.
- **Average latency:** mean producer-to-consumer queue latency.
- **P50/P95/P99 latency:** percentile latency measurements, useful for observing typical and tail behavior.
- **Peak queue:** maximum observed queue depth.
- **Queue memory:** static storage reserved by the ring buffer (`4096 * sizeof(Frame)`).
- **Full retries:** producer backpressure events caused by a full queue.

Percentile samples are collected only by the consumer thread and do not change the SPSC synchronization model. Benchmark results are machine-dependent and should be compared under the same CPU/compiler/system-load conditions.

## Benchmark Matrix

Run the reproducible payload-size matrix with the helper script:

```bash
PACKETS=100000 ./benchmarks/run_matrix.sh
```

Or provide a custom binary path:

```bash
PACKETS=500000 ./benchmarks/run_matrix.sh ./build/telemetry
```

The matrix runs payload sizes of **64, 128, 256, 512, and 1024 bytes** and reports throughput, average latency, P50/P95/P99 latency, producer full-queue retries, and peak queue depth. The script parses the program's own benchmark output, so it does not hard-code performance numbers.

## Indexing Performance Comparison

Build and benchmark both indexing strategies with the same source tree:

```bash
PACKETS=100000 ./benchmarks/compare_indexing.sh
```

The script creates separate Release builds for:

- **Optimized:** power-of-two bit-mask indexing (`TELEMETRY_USE_MODULO_INDEXING=OFF`)
- **Baseline:** modulo indexing (`TELEMETRY_USE_MODULO_INDEXING=ON`)

It exports both result sets to CSV and prints an observed throughput ratio for each payload size. Run multiple times on the same machine if you want a more stable comparison.

## CSV Schema

CSV exports contain:

```text
payload_bytes,packets,throughput_packets_per_sec,avg_latency_us,p50_latency_us,p95_latency_us,p99_latency_us,full_retries,peak_queue,queue_memory_bytes,integrity_errors,ordering_errors
```

Performance numbers should be generated from the benchmark on the target machine rather than copied into the README as fixed claims.

## Resume

**High-Performance Real-Time Telemetry Pipeline — C++20**

Developed a fixed-memory SPSC ring-buffer pipeline for low-latency synthetic telemetry ingestion using atomic synchronization, structured packet frames, validation, overflow handling, configurable benchmarks, power-of-two index wrapping, percentile latency analysis, static memory reporting, and reproducible throughput benchmarking with CSV export and indexing comparison.
