$ErrorActionPreference = "Stop"
if (-not (Get-Command flutter -ErrorAction SilentlyContinue)) {
  throw "Flutter SDK is not installed or not on PATH."
}

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
  flutter pub get
  flutter analyze
  flutter test
  Write-Host "Flutter dependencies, analysis, and tests completed."
} finally {
  Pop-Location
}
