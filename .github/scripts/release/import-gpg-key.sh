#!/bin/bash
# Import GPG key for signing
# Expects environment variable: GPG_PRIVATE_KEY

if [ -n "$GPG_PRIVATE_KEY" ]; then
  echo "Importing GPG key..."
  echo "$GPG_PRIVATE_KEY" | gpg --batch --import
  gpg --list-secret-keys
  echo "GPG key imported successfully!"
else
  echo "GPG key import skipped - GPG_PRIVATE_KEY not set"
fi
