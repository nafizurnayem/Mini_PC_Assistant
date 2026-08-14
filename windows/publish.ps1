param(
  [string]$Configuration = "Release",
  [string]$Runtime = "win-x64",
  [bool]$SelfContained = $true
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$output = Join-Path $root "publish"
$dotnet = Get-Command dotnet -ErrorAction SilentlyContinue
if ($dotnet) {
  $dotnet = $dotnet.Source
} else {
  $dotnet = Join-Path (Split-Path -Parent $root) ".tools\dotnet\dotnet.exe"
}
if (-not (Test-Path -LiteralPath $dotnet)) {
  throw "Install .NET 8 SDK or place it in .tools\dotnet."
}

& $dotnet publish (Join-Path $root "src\PcBot.Service\PcBot.Service.csproj") `
  -c $Configuration -r $Runtime --self-contained $SelfContained `
  -o (Join-Path $output "service")
if ($LASTEXITCODE -ne 0) { throw "Service publish failed." }
& $dotnet publish (Join-Path $root "src\PcBot.Tray\PcBot.Tray.csproj") `
  -c $Configuration -r $Runtime --self-contained $SelfContained `
  -o (Join-Path $output "tray")
if ($LASTEXITCODE -ne 0) { throw "Tray publish failed." }

Write-Host "Windows packages written to $output"
