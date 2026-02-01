# Build the project on Windows
$ErrorActionPreference = "Stop"

Write-Host "Building project..."
cmake --build build --config Release

Write-Host "Build complete!"
