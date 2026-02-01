# Install dependencies via vcpkg on Windows
$ErrorActionPreference = "Stop"

Write-Host "Installing dependencies via vcpkg..."
.\vcpkg\vcpkg install curl:x64-windows zlib:x64-windows libwebsockets:x64-windows

Write-Host "Dependencies installed successfully!"
