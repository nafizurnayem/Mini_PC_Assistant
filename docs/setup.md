# Deployment guide

## 1. MQTT

Create a TLS-enabled MQTT broker (for example, HiveMQ Cloud) and use TLS port `8883`. Create
separate credentials for the components you use:

| Client | Publish | Subscribe |
|---|---|---|
| ESP32 | `state/bot`, `event/ack`, `command/power` | commands, PC state, telemetry, alerts |
| Windows / Linux | PC state, telemetry, alerts, media, volume, speed-test state, acknowledgements | power, speed-test, volume commands, bot state |
| Mobile | command topics | states, telemetry, alerts, acknowledgements |
| Home Assistant | command topics | states, telemetry, alerts |

All topics are under `pcbot/v1/<deviceId>/`. Use the broker's current trusted
root CA certificate in ESP32 provisioning. Do not reuse credentials and do not
allow wildcard access outside this device root.

## 2. ESP32-S3

Follow the [wiring guide](../firmware/README.md) first. Install PlatformIO,
then build and upload from the repository root (`python3` on Linux):

```powershell
python -m platformio run -d firmware -t upload
python -m platformio device monitor -b 115200
```

On first boot, read the temporary password from the TFT, connect a phone or PC
to the displayed hotspot, and open `http://192.168.4.1`. Enter:

- destination Wi-Fi SSID/password;
- the same device ID used by all other components;
- PC wired-Ethernet MAC and LAN broadcast address;
- the local time zone, used by the power-saver clock screen;
- ESP32-specific MQTT username/password, TLS port, and root CA.

Holding TTP223 for five seconds during boot clears Wi-Fi only. MQTT secrets are
preserved. A factory reset erases all configuration.

## 3. PC app

### Windows

Install .NET 8 and the Ookla Speedtest CLI. From `windows/`:

```powershell
dotnet build .\PcBot.sln -c Release
dotnet run --project .\tests\PcBot.Tests\PcBot.Tests.csproj -c Release
.\install-service.ps1
```

The installer must run elevated. Open the installed desktop app,
enter the Windows-specific MQTT credential, configure alert thresholds, and
restart the service. See the [Windows guide](../windows/README.md). Copy the displayed wired LAN MAC/broadcast values into the
ESP32 portal if needed. Enable Wake-on-LAN in BIOS/UEFI and the Ethernet
adapter's Windows power-management settings.

### Ubuntu Linux

Install the separate Linux app using [Linux guide](../linux/README.md).
It uses the same broker, device ID, phone app, and ESP32 firmware. Its agent
runs as a systemd user service in the desktop session. Enable remote shutdown
in its Monitoring page if desired. The Linux guide includes broker permissions,
setup commands, troubleshooting, and Ubuntu Android build instructions.

## 4. Phone app (optional)

The Android/iOS host projects and permissions are committed:

```powershell
cd mobile
.\bootstrap.ps1
flutter build apk --release
```

Android packaging requires an installed Android SDK and accepted licenses. iOS
signing requires macOS, Xcode, and an Apple developer identity. See the
[mobile guide](../mobile/README.md) for smaller Android builds and signing. Credentials are
stored in Android Keystore or iOS Keychain.

## 5. Home Assistant (optional)

On a Docker host with a public DNS name:

```bash
cd home-assistant
cp .env.example .env
# Set PUBLIC_HOST to a DNS hostname pointed at the VM.
docker compose up -d
```

Allow inbound TCP 80/443 only. Complete Home Assistant onboarding through the
HTTPS hostname and add the MQTT integration with its dedicated TLS credential.
The included YAML dashboard provides status, power, telemetry, speed tests,
history, Wi-Fi reset, and a confirmation-protected factory reset.

Google Assistant is optional. Follow Home Assistant's manual Google Assistant
setup and use `google_assistant.example.yaml` only after creating the required
Google Cloud project/service account. Account linking and Nest testing cannot
be completed from this repository.

## Hardware acceptance

Before permanent installation, verify:

- correct TFT colors and rotation, readable provisioning password, and no
  clipping;
- at least 25 FPS while Wi-Fi, BLE, TLS, and MQTT are active;
- short tap, two-second hold, five-second boot recovery, and buzzer volume;
- WOL from PC-off state and safe shutdown from mobile/Home Assistant;
- sensor behavior when a hardware value is unavailable;
- reconnect after router, broker, ESP32, and PC restarts.
