#!/bin/bash
# Install bpftool for the current architecture
set -e

# Detect architecture
ARCH=$(uname -m)
VERSION="v7.6.0"

case "$ARCH" in
    x86_64)
        BPFTOOL_ARCH="amd64"
        ;;
    aarch64)
        BPFTOOL_ARCH="arm64"
        ;;
    *)
        echo "Error: Unsupported architecture: $ARCH"
        echo "Supported architectures: x86_64 (amd64), aarch64 (arm64)"
        exit 1
        ;;
esac

echo "Detected architecture: $ARCH"
echo "Installing bpftool $VERSION for $BPFTOOL_ARCH..."

# Download and install bpftool
BPFTOOL_URL="https://github.com/libbpf/bpftool/releases/download/${VERSION}/bpftool-${VERSION}-${BPFTOOL_ARCH}.tar.gz"
wget "$BPFTOOL_URL"
tar -xzf "bpftool-${VERSION}-${BPFTOOL_ARCH}.tar.gz"
sudo install -m 755 bpftool /usr/local/bin/bpftool

# Clean up
rm -f "bpftool-${VERSION}-${BPFTOOL_ARCH}.tar.gz" bpftool

# Verify installation
bpftool --version
echo "bpftool installed successfully!"
