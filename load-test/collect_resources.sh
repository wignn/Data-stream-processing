#!/bin/bash
set -euo pipefail

PROMETHEUS_URL="${PROMETHEUS_URL:-http://localhost:9090}"
OUTPUT_DIR="${1:-./results}"
DURATION="${2:-60}"
INTERVAL="${3:-5}"

mkdir -p "$OUTPUT_DIR"

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
CSV_FILE="${OUTPUT_DIR}/resource_monitor_${TIMESTAMP}.csv"

echo "timestamp,service,cpu_rate,rss_bytes,vms_bytes,open_fds,ticks_total" > "$CSV_FILE"

query_metric() {
    local query="$1"
    curl -s "${PROMETHEUS_URL}/api/v1/query?query=${query}" 2>/dev/null \
        | grep -oP '"value":\[\d+\.?\d*,"([^"]+)"\]' \
        | grep -oP ',"([^"]+)"\]' \
        | tr -d ',""]' || echo "0"
}

echo "Resource monitoring started (duration=${DURATION}s, interval=${INTERVAL}s)"
echo "Output: ${CSV_FILE}"

END_TIME=$(($(date +%s) + DURATION))

while [ "$(date +%s)" -lt "$END_TIME" ]; do
    TS=$(date -Iseconds)

    for service in rust-service go-service; do
        cpu=$(query_metric "rate(process_cpu_seconds_total{job=\"${service}\"}[30s])")
        rss=$(query_metric "process_resident_memory_bytes{job=\"${service}\"}")
        vms=$(query_metric "process_virtual_memory_bytes{job=\"${service}\"}")
        fds=$(query_metric "process_open_fds{job=\"${service}\"}")
        ticks=$(query_metric "tick_processed_total{job=\"${service}\"}")

        echo "${TS},${service},${cpu},${rss},${vms},${fds},${ticks}" >> "$CSV_FILE"
    done

    sleep "$INTERVAL"
done

echo "Resource monitoring complete: ${CSV_FILE}"
