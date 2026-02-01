#!/bin/bash
# Install bpftool for ARM64 architecture
set -e

echo "Installing bpftool for ARM64..."
wget https://github.com/libbpf/bpftool/releases/download/v7.6.0/bpftool-v7.6.0-arm64.tar.gz
tar -xzf bpftool-v7.6.0-arm64.tar.gz
sudo install -m 755 bpftool /usr/local/bin/bpftool
bpftool --version
echo "bpftool installed successfully!"
