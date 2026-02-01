# Sign Windows executable
# Expects environment variables: CERTIFICATE_BASE64, CERTIFICATE_PASSWORD

if ($env:CERTIFICATE_BASE64 -and $env:CERTIFICATE_PASSWORD) {
    Write-Host "Certificate credentials found, proceeding with signing..."

    # Decode certificate from base64
    $certBytes = [System.Convert]::FromBase64String($env:CERTIFICATE_BASE64)
    $certPath = Join-Path $env:TEMP "cert.pfx"
    [IO.File]::WriteAllBytes($certPath, $certBytes)

    # Sign the executable
    $exePath = "build/Source/Gui/Release/waechter.exe"
    if (Test-Path $exePath) {
        & "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\signtool.exe" sign `
            /f $certPath `
            /p $env:CERTIFICATE_PASSWORD `
            /tr http://timestamp.digicert.com `
            /td SHA256 `
            /fd SHA256 `
            $exePath

        Write-Host "Executable signed successfully"
    } else {
        Write-Host "Executable not found at $exePath"
        exit 1
    }

    # Clean up certificate
    Remove-Item $certPath
} else {
    Write-Host "Signing skipped - certificate secrets not configured"
}
