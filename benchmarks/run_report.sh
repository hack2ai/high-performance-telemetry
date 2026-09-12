#!/usr/bin/env bash
set -euo pipefail

BINARY="${1:-./build/telemetry}"
PACKETS="${PACKETS:-100000}"
WARMUP="${WARMUP:-1}"
RUNS="${RUNS:-3}"
OUTPUT_DIR="${OUTPUT_DIR:-build/benchmark-report}"
CSV="$OUTPUT_DIR/results.csv"
SUMMARY="$OUTPUT_DIR/summary.md"

if [[ ! -x "$BINARY" ]]; then
  echo "Benchmark binary not found or not executable: $BINARY" >&2
  exit 1
fi

mkdir -p "$OUTPUT_DIR"
"$BINARY" --matrix --packets "$PACKETS" --warmup "$WARMUP" --runs "$RUNS" --csv "$CSV"

awk -F',' '
NR == 1 { next }
{
  if ($5 > best) { best=$5; best_payload=$1 }
  if ($5 < worst || worst == "") { worst=$5; worst_payload=$1 }
  sum += $5
  count++
}
END {
  if (count == 0) exit 1
  printf "# Telemetry Benchmark Report\n\n"
  printf "- Packets per run: %s\n", ENVIRON["PACKETS"]
  printf "- Warm-up runs: %s\n", ENVIRON["WARMUP"]
  printf "- Measured runs: %s\n\n", ENVIRON["RUNS"]
  printf "| Metric | Value | Payload |\n|---|---:|---:|\n"
  printf "| Best mean throughput | %.2f packets/s | %s bytes |\n", best, best_payload
  printf "| Lowest mean throughput | %.2f packets/s | %s bytes |\n", worst, worst_payload
  printf "| Mean across payloads | %.2f packets/s | all |\n", sum/count
}' PACKETS="$PACKETS" WARMUP="$WARMUP" RUNS="$RUNS" "$CSV" > "$SUMMARY"

cat "$SUMMARY"
echo
printf 'CSV results: %s\n' "$CSV"
printf 'Markdown report: %s\n' "$SUMMARY"
