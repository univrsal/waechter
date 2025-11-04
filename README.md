## Wächter [*/ˈvɛçtɐ/*]

[![Build (Ubuntu)](https://github.com/univrsal/Waechter/actions/workflows/build.yml/badge.svg)](https://github.com/univrsal/Waechter/actions/workflows/build.yml)

![Demo](Meta/Demo.png)

A Linux traffic monitoring and shaping tool.

Wächter uses eBPF to monitor network traffic and enforce rules to block or throttle connections. It is
divided up into three parts:

- An eBPF program that hooks into the Linux kernel to gather traffic data and enforce rules
- A daemon that loads the eBPF program, reads data from it and keeps track of network usage statistics and rules
- A client GUI to edit and view rules and traffic statistics

Build requirements:

- [libbpf](https://github.com/libbpf/libbpf)
- [CMake](https://cmake.org/)
- A recent Linux kernel with eBPF support
- `bpftool` to create vmlinux.h
- `clang` and `llvm` to compile eBPF programs