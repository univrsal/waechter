#!/usr/bin/env bash
set -euo pipefail

IFACE=${1:-}
RATE=${2:-1000000}   # bytes per second
BURST=${3:-65536}    # bytes

if [[ -z "$IFACE" ]]; then
  echo "Usage: $0 <iface> [RATE_BPS] [BURST_BYTES]" >&2
  exit 1
fi

cd "$(dirname "$0")"

clang -O2 -g -target bpf -D__TARGET_ARCH_x86 -DRATE_BPS=${RATE} -DBURST_BYTES=${BURST} \
  -c traffic_limit_kern.c -o traffic_limit_kern.o

# Try to detach first, ignore errors
sudo ip link set dev "$IFACE" xdpgeneric off || true

# Attach in generic mode (works on most NICs)
sudo ip link set dev "$IFACE" xdpgeneric obj traffic_limit_kern.o sec xdp

echo "Attached XDP rate limiter to $IFACE (RATE_BPS=$RATE, BURST_BYTES=$BURST)"
