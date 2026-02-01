#!/bin/bash
# Configure CMake for release build on Ubuntu
set -e

TAG_NAME="$1"
WORKSPACE="${2:-$(pwd)}"

echo "Configuring CMake for release build..."
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${WORKSPACE}/artifacts" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

echo "Configuration complete!"
