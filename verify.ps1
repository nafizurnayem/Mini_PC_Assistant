$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $root
try {
  npm.cmd test
  npm.cmd run validate
  npm.cmd run validate:yaml

  $env:PLATFORMIO_CORE_DIR = Join-Path $root ".platformio"
  $env:PLATFORMIO_SETTING_ENABLE_TELEMETRY = "No"
  python -m platformio run -d firmware

  $dotnet = Join-Path $root ".tools\dotnet\dotnet.exe"
  if (-not (Test-Path -LiteralPath $dotnet)) { $dotnet = "dotnet" }
  $env:DOTNET_CLI_HOME = Join-Path $root ".tools\dotnet-home"
  $env:NUGET_PACKAGES = Join-Path $root ".tools\nuget"
  $env:DOTNET_CLI_TELEMETRY_OPTOUT = "1"
  & $dotnet build (Join-Path $root "windows\PcBot.sln") -c Release
  & $dotnet run --project (Join-Path $root "windows\tests\PcBot.Tests\PcBot.Tests.csproj") -c Release

  $flutter = Join-Path $root ".tools\flutter\bin\flutter.bat"
  if (Test-Path -LiteralPath $flutter) {
    Push-Location (Join-Path $root "mobile")
    try {
      & $flutter pub get
      & $flutter analyze
      & $flutter test
    } finally {
      Pop-Location
    }
  } else {
    Write-Warning "Flutter SDK not found; mobile analysis/tests skipped."
  }

  git diff --check
  Write-Host "PC Assistant Bot verification passed."
} finally {
  Pop-Location
}
