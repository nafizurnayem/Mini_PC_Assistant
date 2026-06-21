$ErrorActionPreference = "Stop"
if (Get-Service -Name "PcBot" -ErrorAction SilentlyContinue) {
  Stop-Service PcBot -Force
  sc.exe delete PcBot | Out-Null
}
Write-Host "PC Assistant Bot service removed. Configuration remains in ProgramData\PcBot."
