param(
  [string]$Configuration = "Release",
  [string]$Runtime = "win-x64",
  [bool]$SelfContained = $true,
  # Leave the service and desktop app running and let the publish fail on a
  # locked file, instead of stopping them.
  [switch]$NoStop
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$output = Join-Path $root "publish"
$dotnet = Get-Command dotnet -ErrorAction SilentlyContinue
if ($dotnet) {
  $dotnet = $dotnet.Source
} else {
  throw "Install .NET 8 SDK and make sure dotnet is on PATH."
}

# Both targets lock their own DLLs while running, so publishing over a live
# install fails with MSB3027 after ten retries. Stop them first and put back
# whatever was running when we finish.
$serviceWasRunning = $false
$trayWasRunning = $false

if (-not $NoStop) {
  $service = Get-Service -Name "PcBot" -ErrorAction SilentlyContinue
  if ($service -and $service.Status -eq "Running") {
    try {
      Stop-Service -Name "PcBot" -Force -ErrorAction Stop
      $serviceWasRunning = $true
      Write-Host "Stopped the PcBot service."
    } catch {
      throw "The PcBot service is running and could not be stopped. Run this script from an elevated PowerShell, or stop it yourself first."
    }
  }

  $tray = Get-Process -Name "PcBot.Tray" -ErrorAction SilentlyContinue
  if ($tray) {
    $trayWasRunning = $true
    $tray | Stop-Process -Force
    # Give Windows a moment to release the file handles.
    Start-Sleep -Milliseconds 700
    Write-Host "Closed the desktop app."
  }
}

try {
  & $dotnet publish (Join-Path $root "src\PcBot.Service\PcBot.Service.csproj") `
    -c $Configuration -r $Runtime --self-contained $SelfContained `
    -o (Join-Path $output "service")
  if ($LASTEXITCODE -ne 0) { throw "Service publish failed." }

  & $dotnet publish (Join-Path $root "src\PcBot.Tray\PcBot.Tray.csproj") `
    -c $Configuration -r $Runtime --self-contained $SelfContained `
    -o (Join-Path $output "tray")
  if ($LASTEXITCODE -ne 0) { throw "Tray publish failed." }
} finally {
  if ($serviceWasRunning) {
    Start-Service -Name "PcBot"
    Write-Host "Restarted the PcBot service."
  }
  if ($trayWasRunning) {
    Start-Process (Join-Path $output "tray\PcBot.Tray.exe")
    Write-Host "Reopened the desktop app."
  }
}

Write-Host "Windows packages written to $output"
