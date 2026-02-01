#!/bin/bash
# Build the project with Emscripten
set -e

echo "Building project with Emscripten..."
cmake --build build --config Release -j$(nproc)

echo "Build complete!"
echo "Preparing deployment artifacts..."
mkdir -p deploy
cp build/Source/Gui/waechter.js deploy/
cp build/Source/Gui/waechter.wasm deploy/
cp build/Source/Gui/waechter.html deploy/index.html

echo "Deployment artifacts prepared!"
ls -lh deploy/
