param(
  [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$publish = Join-Path $root "publish\service"

dotnet publish (Join-Path $root "src\PcBot.Service\PcBot.Service.csproj") `
  -c $Configuration -r win-x64 --self-contained false -o $publish

$exe = Join-Path $publish "PcBot.Service.exe"
if (Get-Service -Name "PcBot" -ErrorAction SilentlyContinue) {
  Stop-Service PcBot -Force
  sc.exe delete PcBot | Out-Null
}
sc.exe create PcBot binPath= "`"$exe`"" start= auto DisplayName= "PC Assistant Bot" | Out-Null
sc.exe description PcBot "Publishes PC telemetry and handles safe shutdown commands." | Out-Null
Start-Service PcBot
Write-Host "PC Assistant Bot service installed."

