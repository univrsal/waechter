# waechter

Minimal C++ CMake project structure with sources in `Source/`.

This repo also includes an optional eBPF XDP rate limiter under `ebpf/` that can limit network throughput per interface using a token-bucket.

## Build

```bash
# Configure & build (out-of-source)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Run

```bash
./build/waechter
```

## Notes

- Requires CMake >= 3.16 and a C++17-capable compiler.
- Modify `Source/main.cpp` to add your code. Add more source files under `Source/` and list them in `CMakeLists.txt` or use `target_sources()` / `file(GLOB ...)` if preferred.

## eBPF XDP rate limiter (optional)

See `ebpf/` for a minimal XDP program. Quick start:

```bash
cd ebpf
chmod +x attach.sh detach.sh
# Attach with 5 Mbps rate and 256 KiB burst on interface eth0
./attach.sh eth0 5000000 262144

# Detach when done
./detach.sh eth0
```
