#!/bin/bash
# Sign macOS release artifacts with GPG
# Expects environment variables: GPG_PASSPHRASE

TAG_NAME="$1"
ARCH="$2"

echo "Generating SHA256 checksum..."
shasum -a 256 "waechter-${ARCH}-${TAG_NAME}.pkg" > "macos-${ARCH}-checksums.txt"
cat "macos-${ARCH}-checksums.txt"

echo "Checksum generated!"

if [ -n "$GPG_PASSPHRASE" ]; then
  echo "Signing macOS artifacts with GPG..."

  gpg --batch --yes --passphrase "$GPG_PASSPHRASE" \
    --pinentry-mode loopback --detach-sign --armor \
    "waechter-${ARCH}-macos-${TAG_NAME}.pkg"

  gpg --batch --yes --passphrase "$GPG_PASSPHRASE" \
    --pinentry-mode loopback --detach-sign --armor "macos-${ARCH}-checksums.txt"

  echo "GPG signing completed successfully!"
else
  echo "GPG signing skipped - GPG_PASSPHRASE not set"
fi
