# Windows agent and control panel

The .NET 8 Windows service publishes hardware telemetry and handles graceful
shutdown and speed-test commands. The tray control panel stores MQTT settings,
alert thresholds, timing, and shows the active adapter's MAC and broadcast
address for ESP32 provisioning.

## Build

```powershell
dotnet build .\PcBot.sln -c Release
dotnet run --project .\tests\PcBot.Tests\PcBot.Tests.csproj -c Release
```

Install the Ookla Speedtest CLI and set its executable path in the control
panel. Run `install-service.ps1` from an elevated PowerShell terminal to publish
and install self-contained service/tray executables. Open `PcBot.Tray.exe`,
save a dedicated TLS MQTT credential, then restart the service.

The service performs graceful Windows shutdown only. The ESP32 handles wake by
sending a Wake-on-LAN packet on the local network.
