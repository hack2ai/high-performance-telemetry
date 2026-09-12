#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/telemetry}"
PACKETS="${PACKETS:-100000}"

if [[ ! -x "$BINARY" ]]; then
  echo "Benchmark binary not found or not executable: $BINARY" >&2
  echo "Build first with: cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --parallel" >&2
  exit 1
fi

printf '%-12s %-16s %-16s %-16s %-16s\n' "Payload" "Throughput" "Avg Latency" "Full Retries" "Peak Queue"
printf '%-12s %-16s %-16s %-16s %-16s\n' "(bytes)" "(packets/s)" "(us)" "(count)" "(frames)"

for payload in 64 128 256 512 1024; do
  output="$($BINARY --packets "$PACKETS" --payload "$payload")"
  throughput="$(awk -F': ' '/Throughput/{print $2}' <<< "$output")"
  latency="$(awk -F': ' '/Avg latency/{print $2}' <<< "$output")"
  retries="$(awk -F': ' '/Full retries/{print $2}' <<< "$output")"
  peak="$(awk -F': ' '/Peak queue/{print $2}' <<< "$output")"
  printf '%-12s %-16s %-16s %-16s %-16s\n' "$payload" "$throughput" "$latency" "$retries" "$peak"
done
