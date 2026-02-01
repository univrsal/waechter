#!/bin/bash
# Create release tarball and generate checksums for Linux
set -e

TAG_NAME="$1"

echo "Creating tarball of installed files..."
cd artifacts
tar -czf "../waechter-${TAG_NAME}-linux-x86_64.tar.gz" .
cd ..

echo "Generating SHA256 checksums..."
sha256sum build/*.deb > checksums.txt
sha256sum "waechter-${TAG_NAME}-linux-x86_64.tar.gz" >> checksums.txt
cat checksums.txt

echo "Tarball and checksums created successfully!"
