#!/bin/bash
# Create PKG installer for macOS
set -e

TARGET_ARCH="$1"
SHORT_SHA="$2"

echo "Creating PKG installer for ${TARGET_ARCH}..."

# Create package structure
mkdir -p pkgroot/Applications
cp -R Waechter.app pkgroot/Applications/

# Create component package
pkgbuild --root pkgroot \
         --identifier st.waechter.client \
         --version ${VERSION} \
         --install-location / \
         component.pkg

# Create distribution XML
cat > distribution.xml << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Waechter Network Monitor</title>
    <organization>st.waechter</organization>
    <domains enable_localSystem="true"/>
    <options customize="never" require-scripts="false"/>
    <pkg-ref id="st.waechter.client"/>
    <choices-outline>
        <line choice="default">
            <line choice="st.waechter.client"/>
        </line>
    </choices-outline>
    <choice id="default"/>
    <choice id="st.waechter.client" visible="false">
        <pkg-ref id="st.waechter.client"/>
    </choice>
    <pkg-ref id="st.waechter.client" version="${VERSION}" onConclusion="none">component.pkg</pkg-ref>
</installer-gui-script>
EOF

# Create product archive (final PKG)
productbuild --distribution distribution.xml \
             --package-path . \
             "waechter-${TARGET_ARCH}-${SHORT_SHA}.pkg"

echo "PKG installer created successfully!"
