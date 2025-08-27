# eBPF XDP rate limiter

This folder contains a minimal XDP eBPF program implementing a token-bucket rate limiter to throttle interface traffic.

Program: `traffic_limit_kern.c` (section `xdp`)

- Limits aggregate throughput by passing packets while tokens (bytes) are available, otherwise drops.
- Defaults: 1 Mbps rate, 64 KiB burst.

## Requirements

- Linux kernel with XDP/eBPF support
- clang/llvm, libbpf headers (for bpftool), and `bpftool`
- Root privileges to attach XDP

## Build

Using clang directly (no libbpf skeleton):

```bash
# Build object with rate 5 Mbps and burst 256 KiB (adjust as needed)
clang -O2 -g -target bpf -D__TARGET_ARCH_x86 \
  -DRATE_BPS=5000000 -DBURST_BYTES=262144 \
  -c traffic_limit_kern.c -o traffic_limit_kern.o
```

Note: Replace `-D__TARGET_ARCH_x86` according to your arch, e.g., `arm64`.

## Attach

Attach to interface (replace IFACE):

```bash
sudo ip link set dev IFACE xdpgeneric off 2>/dev/null || true
sudo ip link set dev IFACE xdpgeneric obj traffic_limit_kern.o sec xdp
```

To detach:

```bash
sudo ip link set dev IFACE xdpgeneric off
```

If your NIC supports native XDP, use `xdpdrv` (driver mode) instead of `xdpgeneric` for better performance.

## Tune at load time

- Change `RATE_BPS` and `BURST_BYTES` at compile time via `-D` flags.
- Map state is per-program; multi-queue sharing is handled automatically by the kernel for array maps.

## Verify

```bash
sudo bpftool prog show | grep xdp
sudo bpftool net
```
