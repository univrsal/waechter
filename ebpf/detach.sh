#!/usr/bin/env bash
set -euo pipefail
IFACE=${1:-}
if [[ -z "$IFACE" ]]; then
  echo "Usage: $0 <iface>" >&2
  exit 1
fi
sudo ip link set dev "$IFACE" xdpgeneric off
