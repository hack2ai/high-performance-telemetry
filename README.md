# High-Performance Real-Time Telemetry

A C++20 systems project demonstrating a bounded single-producer/single-consumer (SPSC) ring buffer for low-latency synthetic telemetry ingestion.

## Features

- Fixed-size packet frames
- Pre-allocated circular storage
- Atomic producer/consumer indexes
- Acquire/release memory ordering
- Packet validation
- Overflow handling with full-queue retry tracking
- Configurable packet count and payload size
- Throughput and latency benchmarking
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

Combine options:

```bash
./build/telemetry --packets 500000 --payload 256
```

Show command-line help:

```bash
./build/telemetry --help
```

Payload size must be between 1 and 1024 bytes. The benchmark uses retry-on-full semantics, so `Full retries` measures producer backpressure events rather than dropped packets.

## Resume

**High-Performance Real-Time Telemetry Pipeline — C++20**

Developed a fixed-memory SPSC ring-buffer pipeline for low-latency synthetic telemetry ingestion using atomic synchronization, structured packet frames, validation, overflow handling, configurable benchmarks, and throughput/latency measurement.
