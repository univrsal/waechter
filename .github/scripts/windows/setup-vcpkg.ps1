# Setup vcpkg on Windows
$ErrorActionPreference = "Stop"

Write-Host "Setting up vcpkg..."
git clone https://github.com/microsoft/vcpkg.git
Set-Location vcpkg
.\bootstrap-vcpkg.bat

# Export VCPKG_ROOT for subsequent steps
$vcpkgRoot = $PWD.Path
Write-Host "VCPKG_ROOT=$vcpkgRoot" >> $env:GITHUB_ENV

Write-Host "vcpkg setup complete!"
