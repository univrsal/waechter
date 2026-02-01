#!/bin/bash
# Package Windows distribution
set -e

SHORT_SHA="${1:-unknown}"

echo "Creating distribution directory..."
mkdir -p dist
cp build/Source/Gui/Release/waechter.exe dist/
cp build/Source/Gui/Release/waechter.pdb dist/ || echo "No PDB file found"
cp Meta/Assets/Branding/Icon.png dist/

# Copy vcpkg DLLs
VCPKG_BIN="vcpkg/installed/x64-windows/bin"
if [ -d "$VCPKG_BIN" ]; then
    echo "Copying DLLs from vcpkg..."
    cp "$VCPKG_BIN"/curl*.dll dist/ 2>/dev/null || true
    cp "$VCPKG_BIN"/zlib*.dll dist/ 2>/dev/null || true
    cp "$VCPKG_BIN"/libcrypto*.dll dist/ 2>/dev/null || true
    cp "$VCPKG_BIN"/libssl*.dll dist/ 2>/dev/null || true
    cp "$VCPKG_BIN"/websockets*.dll dist/ 2>/dev/null || true
    cp "$VCPKG_BIN"/libwebsockets*.dll dist/ 2>/dev/null || true
    # Copy any other DLLs that might be needed
    cp "$VCPKG_BIN"/*.dll dist/ 2>/dev/null || true
else
    echo "Warning: vcpkg bin directory not found at $VCPKG_BIN"
fi

# List what we're including
echo "Package contents:"
ls -lh dist/

echo "Packaging complete!"
