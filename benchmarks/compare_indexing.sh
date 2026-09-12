#!/usr/bin/env bash
set -euo pipefail

PACKETS="${PACKETS:-100000}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OPT_BUILD="${ROOT_DIR}/build-bench-opt"
MOD_BUILD="${ROOT_DIR}/build-bench-modulo"
OPT_BINARY="${OPT_BUILD}/telemetry"
MOD_BINARY="${MOD_BUILD}/telemetry"
OPT_CSV="${ROOT_DIR}/build-bench-opt.csv"
MOD_CSV="${ROOT_DIR}/build-bench-modulo.csv"

cmake -S "${ROOT_DIR}" -B "${OPT_BUILD}" -DCMAKE_BUILD_TYPE=Release -DTELEMETRY_USE_MODULO_INDEXING=OFF
cmake --build "${OPT_BUILD}" --parallel
cmake -S "${ROOT_DIR}" -B "${MOD_BUILD}" -DCMAKE_BUILD_TYPE=Release -DTELEMETRY_USE_MODULO_INDEXING=ON
cmake --build "${MOD_BUILD}" --parallel

"${OPT_BINARY}" --matrix --packets "${PACKETS}" --csv "${OPT_CSV}"
"${MOD_BINARY}" --matrix --packets "${PACKETS}" --csv "${MOD_CSV}"

printf '\nIndexing comparison (same machine; run more than once for stable observations)\n'
printf '%-12s %-18s %-18s %-14s\n' "Payload" "Optimized" "Modulo" "Opt/Modulo"
printf '%-12s %-18s %-18s %-14s\n' "(bytes)" "(packets/s)" "(packets/s)" "ratio"

awk -F',' 'NR > 1 {opt[$1]=$3} END {for (p in opt) print p, opt[p]}' "${OPT_CSV}" | sort -n > /tmp/telemetry-opt.txt
awk -F',' 'NR > 1 {mod[$1]=$3} END {for (p in mod) print p, mod[p]}' "${MOD_CSV}" | sort -n > /tmp/telemetry-mod.txt

paste /tmp/telemetry-opt.txt /tmp/telemetry-mod.txt | while read -r opayload opt mpayload mod; do
    ratio=$(awk -v o="$opt" -v m="$mod" 'BEGIN { if (m > 0) printf "%.3fx", o/m; else print "n/a" }')
    printf '%-12s %-18s %-18s %-14s\n' "$opayload" "$opt" "$mod" "$ratio"
done

rm -f /tmp/telemetry-opt.txt /tmp/telemetry-mod.txt
printf '\nCSV files:\n  %s\n  %s\n' "${OPT_CSV}" "${MOD_CSV}"
printf 'Note: benchmark results depend on CPU, compiler, OS, and system load.\n'
