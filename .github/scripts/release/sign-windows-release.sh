#!/bin/bash
# Sign Windows release artifacts with GPG
# Expects environment variables: GPG_PASSPHRASE

TAG_NAME="$1"

if [ -n "$GPG_PASSPHRASE" ]; then
  echo "Signing Windows artifacts with GPG..."

  gpg --batch --yes --passphrase "$GPG_PASSPHRASE" \
    --pinentry-mode loopback --detach-sign --armor \
    "waechter-${TAG_NAME}-amd64-windows.zip"

  echo "GPG signing completed successfully!"
else
  echo "GPG signing skipped - GPG_PASSPHRASE not set"
fi
