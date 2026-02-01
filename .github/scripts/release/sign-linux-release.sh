#!/bin/bash
# Sign Linux release artifacts with GPG
# Expects environment variables: GPG_PASSPHRASE

TAG_NAME="$1"

if [ -n "$GPG_PASSPHRASE" ]; then
  echo "Signing artifacts with GPG..."

  # Sign the DEB package
  for deb in build/*.deb; do
    gpg --batch --yes --passphrase "$GPG_PASSPHRASE" \
      --pinentry-mode loopback --detach-sign --armor "$deb"
  done

  # Sign the tarball
  gpg --batch --yes --passphrase "$GPG_PASSPHRASE" \
    --pinentry-mode loopback --detach-sign --armor \
    "waechter-${TAG_NAME}-amd64-linux.tar.gz"

  echo "GPG signing completed successfully!"
else
  echo "GPG signing skipped - GPG_PASSPHRASE not set"
fi
