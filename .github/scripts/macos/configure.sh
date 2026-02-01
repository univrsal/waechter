#!/bin/bash
# Configure CMake for macOS build
set -e

echo "Configuring CMake for ${TARGET_ARCH}..."

if [[ "${TARGET_ARCH}" == "x86_64" ]]; then
  # Use x86_64 Homebrew prefix for finding libraries
  export CMAKE_PREFIX_PATH="/usr/local"
  cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="${TARGET_ARCH}" \
    -DCMAKE_INSTALL_PREFIX="${WORKSPACE}/install" \
    -DWAECHTER_BUILD_DAEMON=OFF \
    -DWAECHTER_BUILD_CLIENT=ON \
    -DCMAKE_PREFIX_PATH="/usr/local"
else
  cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="${TARGET_ARCH}" \
    -DCMAKE_INSTALL_PREFIX="${WORKSPACE}/install" \
    -DWAECHTER_BUILD_DAEMON=OFF \
    -DWAECHTER_BUILD_CLIENT=ON
fi

echo "Configuration complete!"
