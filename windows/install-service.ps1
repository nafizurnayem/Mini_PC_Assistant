param(
  [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$principal = [Security.Principal.WindowsPrincipal]::new(
  [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
  throw "Run this installer from an elevated PowerShell terminal."
}

& (Join-Path $root "publish.ps1") -Configuration $Configuration
$publish = Join-Path $root "publish\service"
$tray = Join-Path $root "publish\tray\PcBot.Tray.exe"
$configDirectory = Join-Path $env:ProgramData "PcBot"
New-Item -ItemType Directory -Force -Path $configDirectory | Out-Null
icacls.exe $configDirectory /inheritance:r `
  /grant:r "SYSTEM:(OI)(CI)F" "Administrators:(OI)(CI)F" `
  "$env:USERNAME`:(OI)(CI)M" | Out-Null

$exe = Join-Path $publish "PcBot.Service.exe"
if (Get-Service -Name "PcBot" -ErrorAction SilentlyContinue) {
  Stop-Service PcBot -Force
  sc.exe delete PcBot | Out-Null
}
sc.exe create PcBot binPath= "`"$exe`"" start= auto DisplayName= "PC Assistant Bot" | Out-Null
sc.exe description PcBot "Publishes PC telemetry and handles safe shutdown commands." | Out-Null
sc.exe failure PcBot reset= 86400 actions= restart/5000/restart/15000/none/0 | Out-Null
Start-Service PcBot

$shell = New-Object -ComObject WScript.Shell
$shortcutPath = Join-Path ([Environment]::GetFolderPath("Programs")) "PC Assistant Bot.lnk"
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $tray
$shortcut.WorkingDirectory = Split-Path -Parent $tray
$shortcut.Description = "Configure and diagnose PC Assistant Bot"
$shortcut.Save()

Write-Host "PC Assistant Bot service installed."
Write-Host "Open 'PC Assistant Bot' from the Start menu to configure MQTT and thresholds."
