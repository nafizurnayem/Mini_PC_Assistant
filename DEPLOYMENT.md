# Deployment guide

## 1. MQTT

Create a HiveMQ Cloud Serverless Free cluster and use TLS port `8883`. Create
four different credentials:

| Client | Publish | Subscribe |
|---|---|---|
| ESP32 | `state/bot`, `event/ack`, `command/power` | commands, PC state, telemetry, alerts |
| Windows | PC state, telemetry, alerts, speed-test state, acknowledgements | power and speed-test commands |
| Mobile | command topics | states, telemetry, alerts, acknowledgements |
| Home Assistant | command topics | states, telemetry, alerts |

All topics are under `pcbot/v1/<deviceId>/`. Use the broker's current trusted
root CA certificate in ESP32 provisioning. Do not reuse credentials and do not
allow wildcard access outside this device root.

## 2. ESP32-S3

Build and upload:

```powershell
$env:PLATFORMIO_CORE_DIR="$PWD\.platformio"
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

## 3. Windows

Install .NET 8 and the Ookla Speedtest CLI. From `windows/`:

```powershell
dotnet build .\PcBot.sln -c Release
dotnet run --project .\tests\PcBot.Tests\PcBot.Tests.csproj -c Release
.\install-service.ps1
```

The installer must run elevated. Open **PC Assistant Bot** from the Start menu,
enter the Windows-specific MQTT credential, configure alert thresholds, and
restart the service. Copy the displayed wired LAN MAC/broadcast values into the
ESP32 portal if needed. Enable Wake-on-LAN in BIOS/UEFI and the Ethernet
adapter's Windows power-management settings.

## 4. Flutter

The Android/iOS host projects and permissions are committed:

```powershell
cd mobile
.\bootstrap.ps1
flutter build apk --release
```

Android packaging requires an installed Android SDK and accepted licenses. iOS
signing requires macOS, Xcode, and an Apple developer identity. Credentials are
stored in Android Keystore or iOS Keychain.

## 5. Home Assistant

On the Oracle VM:

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
