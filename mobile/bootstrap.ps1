$ErrorActionPreference = "Stop"
if (-not (Get-Command flutter -ErrorAction SilentlyContinue)) {
  throw "Flutter SDK is not installed or not on PATH."
}

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
  flutter create --org com.pcbot --project-name pc_bot --platforms android,ios .
  flutter pub get
  Write-Host "Add Android Bluetooth permissions and iOS Bluetooth usage text described in mobile/README.md."
} finally {
  Pop-Location
}

