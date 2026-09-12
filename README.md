# High-Performance Real-Time Telemetry

A C++20 systems project demonstrating a bounded single-producer/single-consumer (SPSC) ring buffer for low-latency synthetic telemetry ingestion.

## Features

- Fixed-size packet frames
- Pre-allocated circular storage
- Atomic producer/consumer indexes
- Acquire/release memory ordering
- Packet validation
- Overflow handling
- Throughput and latency benchmarking
- CMake build
- Unit tests

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
./build/telemetry
```

## Resume

**High-Performance Real-Time Telemetry Pipeline — C++20**

Developed a fixed-memory SPSC ring-buffer pipeline for low-latency telemetry ingestion using atomic synchronization, structured packet frames, validation, overflow handling, and throughput/latency benchmarking.
