#!/bin/bash
# Create macOS .app bundle structure
set -e

echo "Creating .app bundle structure..."

mkdir -p "Waechter.app/Contents/MacOS"
mkdir -p "Waechter.app/Contents/Resources"

# Copy executable
cp install/bin/waechter "Waechter.app/Contents/MacOS/"

# Copy icon (convert PNG to ICNS if needed)
if command -v sips &> /dev/null; then
  echo "Converting PNG to ICNS..."
  mkdir -p icon.iconset
  sips -z 16 16     Meta/Assets/Branding/Icon.png --out icon.iconset/icon_16x16.png
  sips -z 32 32     Meta/Assets/Branding/Icon.png --out icon.iconset/icon_16x16@2x.png
  sips -z 32 32     Meta/Assets/Branding/Icon.png --out icon.iconset/icon_32x32.png
  sips -z 64 64     Meta/Assets/Branding/Icon.png --out icon.iconset/icon_32x32@2x.png
  sips -z 128 128   Meta/Assets/Branding/Icon.png --out icon.iconset/icon_128x128.png
  sips -z 256 256   Meta/Assets/Branding/Icon.png --out icon.iconset/icon_128x128@2x.png
  sips -z 256 256   Meta/Assets/Branding/Icon.png --out icon.iconset/icon_256x256.png
  sips -z 512 512   Meta/Assets/Branding/Icon.png --out icon.iconset/icon_256x256@2x.png
  sips -z 512 512   Meta/Assets/Branding/Icon.png --out icon.iconset/icon_512x512.png
  cp Meta/Assets/Branding/Icon.png icon.iconset/icon_512x512@2x.png
  iconutil -c icns icon.iconset -o "Waechter.app/Contents/Resources/waechter.icns"
else
  echo "sips not available, copying PNG directly..."
  cp Meta/Assets/Branding/Icon.png "Waechter.app/Contents/Resources/"
fi

# Create Info.plist
cat > "Waechter.app/Contents/Info.plist" << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleExecutable</key>
    <string>waechter</string>
    <key>CFBundleIconFile</key>
    <string>waechter.icns</string>
    <key>CFBundleIdentifier</key>
    <string>st.waechter.client</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Waechter</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>0.1.3</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

echo ".app bundle created successfully!"
