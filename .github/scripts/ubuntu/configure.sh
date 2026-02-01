#!/bin/bash
# Configure CMake for Ubuntu build
set -e

INSTALL_PREFIX="${1:-$GITHUB_WORKSPACE/artifacts}"

echo "Configuring CMake with Ninja..."
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache

echo "Configuration complete!"
