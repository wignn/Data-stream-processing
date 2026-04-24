#!/bin/bash
set -euo pipefail

PROMETHEUS_URL="${PROMETHEUS_URL:-http://localhost:9090}"
SCRIPT_PATH="./grpc_bench.js"
RESULTS_DIR="./results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p "$RESULTS_DIR"

run_k6() {
    local target="$1"
    local output="$2"
    local vus="$3"
    local iterations="$4"
    local mode="${5:-fixed}"

    if command -v k6 &> /dev/null; then
        k6 run \
            -e TARGET="$target" \
            -e VUS="$vus" \
            -e ITERATIONS="$iterations" \
            -e MODE="$mode" \
            --summary-export "$output" \
            "$SCRIPT_PATH"
    else
        docker run --rm -i \
            --network host \
            -v "$(pwd):/work" \
            -w /work \
            grafana/k6 run \
            -e TARGET="$target" \
            -e VUS="$vus" \
            -e ITERATIONS="$iterations" \
            -e MODE="$mode" \
            --summary-export "$output" \
            "$SCRIPT_PATH"
    fi
}

snapshot_resources() {
    local label="$1"
    local output="$2"

    echo "--- Resource Snapshot: $label ---" >> "$output"
    echo "timestamp: $(date -Iseconds)" >> "$output"

    for service in rust-service go-service; do
        local job="$service"
        local cpu=$(curl -s "${PROMETHEUS_URL}/api/v1/query?query=rate(process_cpu_seconds_total{job=\"${job}\"}[30s])" 2>/dev/null | grep -o '"value":\[.*\]' | head -1)
        local rss=$(curl -s "${PROMETHEUS_URL}/api/v1/query?query=process_resident_memory_bytes{job=\"${job}\"}" 2>/dev/null | grep -o '"value":\[.*\]' | head -1)
        local vms=$(curl -s "${PROMETHEUS_URL}/api/v1/query?query=process_virtual_memory_bytes{job=\"${job}\"}" 2>/dev/null | grep -o '"value":\[.*\]' | head -1)
        local ticks=$(curl -s "${PROMETHEUS_URL}/api/v1/query?query=tick_processed_total{job=\"${job}\"}" 2>/dev/null | grep -o '"value":\[.*\]' | head -1)

        echo "${service}_cpu: ${cpu:-N/A}" >> "$output"
        echo "${service}_rss: ${rss:-N/A}" >> "$output"
        echo "${service}_vms: ${vms:-N/A}" >> "$output"
        echo "${service}_ticks: ${ticks:-N/A}" >> "$output"
    done
    echo "" >> "$output"
}

echo "========================================="
echo " Microservice Benchmark: Rust vs Go"
echo " XAUUSD Tick Processing"
echo " $(date)"
echo "========================================="
echo ""

RESOURCE_LOG="${RESULTS_DIR}/resources_${TIMESTAMP}.txt"

echo "[PHASE 0] Waiting for services to be ready..."
for port in 50051 50052; do
    for i in $(seq 1 30); do
        if curl -sf "http://localhost:${port}" &>/dev/null || true; then
            break
        fi
        sleep 1
    done
done
sleep 5

echo "[PHASE 1] Warm-up (1000 requests per service)..."
snapshot_resources "pre-warmup" "$RESOURCE_LOG"

run_k6 "localhost:50051" "/dev/null" 10 1000 fixed 2>/dev/null || true
run_k6 "localhost:50052" "/dev/null" 10 1000 fixed 2>/dev/null || true

echo "  Warm-up complete. Cooling down 10s..."
sleep 10

snapshot_resources "post-warmup" "$RESOURCE_LOG"

echo ""
echo "[PHASE 2] Fixed-iteration benchmark (10000 requests, 50 VUs)..."
echo ""

echo "  [2a] Benchmarking Rust service (localhost:50051)..."
snapshot_resources "pre-rust-fixed" "$RESOURCE_LOG"
run_k6 "localhost:50051" "${RESULTS_DIR}/rust_fixed_${TIMESTAMP}.json" 50 10000 fixed
snapshot_resources "post-rust-fixed" "$RESOURCE_LOG"

echo ""
echo "  Cooling down 10s..."
sleep 10

echo "  [2b] Benchmarking Go service (localhost:50052)..."
snapshot_resources "pre-go-fixed" "$RESOURCE_LOG"
run_k6 "localhost:50052" "${RESULTS_DIR}/go_fixed_${TIMESTAMP}.json" 50 10000 fixed
snapshot_resources "post-go-fixed" "$RESOURCE_LOG"

echo ""
echo "  Cooling down 10s..."
sleep 10

echo ""
echo "[PHASE 3] Sustained load benchmark (80s ramping, 50 peak VUs)..."
echo ""

echo "  [3a] Benchmarking Rust service (sustained)..."
snapshot_resources "pre-rust-sustained" "$RESOURCE_LOG"
run_k6 "localhost:50051" "${RESULTS_DIR}/rust_sustained_${TIMESTAMP}.json" 50 0 sustained
snapshot_resources "post-rust-sustained" "$RESOURCE_LOG"

echo ""
echo "  Cooling down 10s..."
sleep 10

echo "  [3b] Benchmarking Go service (sustained)..."
snapshot_resources "pre-go-sustained" "$RESOURCE_LOG"
run_k6 "localhost:50052" "${RESULTS_DIR}/go_sustained_${TIMESTAMP}.json" 50 0 sustained
snapshot_resources "post-go-sustained" "$RESOURCE_LOG"

snapshot_resources "final" "$RESOURCE_LOG"

echo ""
echo "========================================="
echo " Benchmark Complete"
echo "========================================="
echo " Results: ${RESULTS_DIR}/"
echo "   - rust_fixed_${TIMESTAMP}.json"
echo "   - go_fixed_${TIMESTAMP}.json"
echo "   - rust_sustained_${TIMESTAMP}.json"
echo "   - go_sustained_${TIMESTAMP}.json"
echo "   - resources_${TIMESTAMP}.txt"
echo ""
echo " Grafana Dashboard: http://localhost:3000"
echo " Prometheus: http://localhost:9090"
echo "========================================="
