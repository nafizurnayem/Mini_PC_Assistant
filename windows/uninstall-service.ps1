$ErrorActionPreference = "Stop"
if (Get-Service -Name "PcBot" -ErrorAction SilentlyContinue) {
  Stop-Service PcBot -Force
  sc.exe delete PcBot | Out-Null
}
$shortcutPath = Join-Path ([Environment]::GetFolderPath("Programs")) "PC Assistant Bot.lnk"
if (Test-Path -LiteralPath $shortcutPath) {
  Remove-Item -LiteralPath $shortcutPath -Force
}
Write-Host "PC Assistant Bot service removed. Configuration remains in ProgramData\PcBot."
