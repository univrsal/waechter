if ($env:CERTIFICATE_BASE64 -and $env:CERTIFICATE_PASSWORD) {
  $certBytes = [System.Convert]::FromBase64String($env:CERTIFICATE_BASE64)
  $certPath = Join-Path $env:TEMP "cert.pfx"
  [IO.File]::WriteAllBytes($certPath, $certBytes)

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

  Remove-Item $certPath
} else {
  Write-Host "Signing skipped - certificate secrets not configured"
}
