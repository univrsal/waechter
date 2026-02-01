#!/bin/bash
# Build the project on Ubuntu
set -e

echo "Building project..."
cmake --build build --config Release -- -k 0

echo "Build complete!"
