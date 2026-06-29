# Install dependencies via vcpkg on Windows
$ErrorActionPreference = "Stop"

Write-Host "Installing dependencies via vcpkg..."
.\vcpkg\vcpkg install libwebsockets:x64-windows sdl2:x64-windows

Write-Host "Dependencies installed successfully!"
