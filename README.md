![header](Meta/Assets/Branding/Header.png)

![Demo](Meta/Assets/Demo.png)

![Dev time spent](https://vrsal.cc/waechter_badge.svg) [![Build (Ubuntu)](https://github.com/univrsal/waechter/actions/workflows/build.yml/badge.svg)](https://github.com/univrsal/waechter/actions/workflows/build.yml) [![Build (Windows Client)](https://github.com/univrsal/waechter/actions/workflows/build-windows.yml/badge.svg)](https://github.com/univrsal/waechter/actions/workflows/build-windows.yml) [![Build (macOS Client)](https://github.com/univrsal/waechter/actions/workflows/build-macos.yml/badge.svg)](https://github.com/univrsal/waechter/actions/workflows/build-macos.yml)

A Linux traffic monitoring and shaping tool.

Wächter uses [eBPF](https://ebpf.io/) to monitor network traffic and enforce rules to block or throttle connections. It
is
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

If you have questions, you can use [discussions](https://github.com/univrsal/waechter/discussions), the IRC
channel [#waechter](https://rizon.net/chat) on irc.rizon.net or the [discord server](https://discord.gg/kZsHuncu3q).

### Roadmap

Wächter is still in development, currently implemented features are:

- Monitor per-connection network traffic (upload and download)
- View connection information (local/remote endpoints, hostname)
- Block upload/download per connection
- Throttle upload/download per connection
- Connection history to log what connections are made by which application

Planned features:

- Persistent rules that are saved and loaded on startup
- Long-term traffic statistics and graphs

Potential future features:

- Priority-based traffic shaping
- Traffic quotas
- Global rules per ip/port etc.
