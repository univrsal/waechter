#!/bin/bash
# Configure CMake for Emscripten build
set -e

echo "Configuring CMake with Emscripten..."
emcmake cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DWAECHTER_BUILD_DAEMON=OFF

echo "Configuration complete!"
