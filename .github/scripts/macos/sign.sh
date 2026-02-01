#!/bin/bash
# Code sign macOS application
# Expects environment variables: MACOS_CERTIFICATE, MACOS_CERTIFICATE_PWD, MACOS_SIGNING_IDENTITY

if [ -n "$MACOS_CERTIFICATE" ] && [ -n "$MACOS_CERTIFICATE_PWD" ]; then
  echo "Setting up code signing..."
  echo "$MACOS_CERTIFICATE" | base64 --decode > certificate.p12
  security create-keychain -p actions build.keychain
  security default-keychain -s build.keychain
  security unlock-keychain -p actions build.keychain
  security import certificate.p12 -k build.keychain -P "$MACOS_CERTIFICATE_PWD" -T /usr/bin/codesign
  security set-key-partition-list -S apple-tool:,apple:,codesign: -s -k actions build.keychain

  # Sign the app
  codesign --force --deep --sign "$MACOS_SIGNING_IDENTITY" --options runtime "Waechter.app"

  # Sign the PKG
  productsign --sign "$MACOS_INSTALLER_SIGNING_IDENTITY" \
              "waechter-${TARGET_ARCH}-${SHORT_SHA}.pkg" \
              "waechter-${TARGET_ARCH}-${SHORT_SHA}-signed.pkg"

  mv "waechter-${TARGET_ARCH}-${SHORT_SHA}-signed.pkg" \
     "waechter-${TARGET_ARCH}-${SHORT_SHA}.pkg"

  # Verify
  codesign --verify --verbose "Waechter.app"

  # Cleanup
  rm certificate.p12
  security delete-keychain build.keychain
  echo "Code signing completed"
else
  echo "Code signing skipped - certificate secrets not configured"
fi
