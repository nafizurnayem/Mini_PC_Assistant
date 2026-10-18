$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Push-Location $root
try {
  npm.cmd test
  if ($LASTEXITCODE -ne 0) { throw "Contract tests failed." }
  npm.cmd run validate
  if ($LASTEXITCODE -ne 0) { throw "Configuration validation failed." }
  npm.cmd run validate:yaml
  if ($LASTEXITCODE -ne 0) { throw "YAML validation failed." }

  $env:PLATFORMIO_SETTING_ENABLE_TELEMETRY = "No"
  python -m platformio run -d firmware
  if ($LASTEXITCODE -ne 0) { throw "Firmware build failed." }

  $env:DOTNET_CLI_TELEMETRY_OPTOUT = "1"
  dotnet build (Join-Path $root "windows\PcBot.sln") -c Release
  if ($LASTEXITCODE -ne 0) { throw "Windows build failed." }
  dotnet run --project (Join-Path $root "windows\tests\PcBot.Tests\PcBot.Tests.csproj") -c Release
  if ($LASTEXITCODE -ne 0) { throw "Windows tests failed." }

  if (Get-Command flutter -ErrorAction SilentlyContinue) {
    Push-Location (Join-Path $root "mobile")
    try {
      flutter pub get
      if ($LASTEXITCODE -ne 0) { throw "Flutter dependency setup failed." }
      flutter analyze
      if ($LASTEXITCODE -ne 0) { throw "Flutter analysis failed." }
      flutter test
      if ($LASTEXITCODE -ne 0) { throw "Flutter tests failed." }
    } finally {
      Pop-Location
    }
  } else {
    Write-Warning "Flutter SDK not on PATH; mobile analysis/tests skipped."
  }

  git diff --check
  if ($LASTEXITCODE -ne 0) { throw "Git whitespace check failed." }
  Write-Host "PC Assistant Bot verification passed."
} finally {
  Pop-Location
}
