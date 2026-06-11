# Configure CMake for Windows build
$ErrorActionPreference = "Stop"

Write-Host "Configuring CMake..."
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE="vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DWAECHTER_BUILD_DAEMON=OFF `
  -DWAECHTER_BUILD_CLIENT=ON

Write-Host "Configuration complete!"
