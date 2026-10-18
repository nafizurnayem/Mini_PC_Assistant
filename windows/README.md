# Windows agent and desktop app

Two processes with different jobs:

| Process | Session | Responsibility |
|---|---|---|
| `PcBot.Service` | Session 0 (Windows service) | Hardware telemetry, alerts, graceful shutdown, speed tests |
| `PcBot.Tray` | Interactive user session | Desktop app: settings, live dashboard, now-playing, PC volume |

The split is not cosmetic. The Windows media session and the audio endpoint both
belong to the logged-in user, so a session 0 service cannot read what is playing
or change the volume. Those duties live in the desktop app.

## The desktop app

A dark, four-page window matching the device's own palette:

| Page | Contents |
|---|---|
| Dashboard | Broker and media status pills, now-playing card with progress, PC volume with a live slider, quick actions |
| Connection | Device ID, broker host/port, credentials, save and test |
| Monitoring | Sampling intervals and alert thresholds |
| Device | LAN diagnostics, Wi-Fi reset, factory reset |

Closing the window hides it to the notification area — the media and volume
agent keeps running, because that is the whole point of it. Quit from the tray
icon's menu to stop it. Double-click the icon to reopen.

The agent publishes retained `state/media` and `state/volume`, and subscribes to
`command/volume` so the bot's knob drives the PC. The dashboard slider and the
knob are two views of the same value.

## Build

```powershell
dotnet build .\PcBot.sln -c Release
dotnet run --project .\tests\PcBot.Tests\PcBot.Tests.csproj -c Release
```

## Publish

```powershell
.\publish.ps1
```

`publish.ps1` stops the running service and desktop app before publishing, then
restarts whichever was running. Run it from an elevated PowerShell if the
service is active. Use `-NoStop` only when you have stopped both yourself.

Install the Ookla Speedtest CLI and set its executable path on the Monitoring
page. Run `install-service.ps1` from an elevated PowerShell terminal to publish
and install self-contained service and desktop executables. Open
`PcBot.Tray.exe`, save a dedicated TLS MQTT credential, then restart the
service.

The service performs graceful Windows shutdown only. The ESP32 handles wake by
sending a Wake-on-LAN packet on the local network.
