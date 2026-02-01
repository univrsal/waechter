#!/bin/bash
# Create Windows distribution package for release
set -e

TAG_NAME="$1"

echo "Creating Windows distribution package..."
mkdir -p dist
cp build/Source/Gui/Release/waechter.exe dist/
cp Meta/Assets/Branding/Icon.png dist/
echo "Distribution package created!"
