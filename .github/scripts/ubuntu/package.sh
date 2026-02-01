#!/bin/bash
# Package the Ubuntu build
set -e

echo "Installing to artifacts directory..."
cmake --install build --prefix artifacts || true

echo "Creating DEB package..."
cd build
cpack -G DEB

echo "Packaging complete!"
