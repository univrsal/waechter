#!/bin/bash
# Sign Windows release artifacts with GPG
# Expects environment variables: GPG_PASSPHRASE

TAG_NAME="$1"

echo "Generating SHA256 checksum..."
sha256sum "waechter-${TAG_NAME}-windows-client.zip" > windows-checksums.txt
cat windows-checksums.txt
echo "Checksum generated!"

if [ -n "$GPG_PASSPHRASE" ]; then
  echo "Signing Windows artifacts with GPG..."

  gpg --batch --yes --passphrase "$GPG_PASSPHRASE" \
    --pinentry-mode loopback --detach-sign --armor \
    "waechter-${TAG_NAME}-windows-client.zip"

  gpg --batch --yes --passphrase "$GPG_PASSPHRASE" \
    --pinentry-mode loopback --detach-sign --armor windows-checksums.txt

  echo "GPG signing completed successfully!"
else
  echo "GPG signing skipped - GPG_PASSPHRASE not set"
fi
