#!/bin/bash
# Build the project on macOS
set -e

echo "Building project..."
cmake --build build --config Release
echo "Build complete!"

echo "Installing project..."
cmake --install build --prefix install
echo "Installation complete!"
