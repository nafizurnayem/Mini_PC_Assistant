# PC Assistant Bot

PC Assistant Bot is a physical and remote control system for a Windows PC. An
ESP32-S3 drives a 3.2-inch ILI9341 display, handles local Wake-on-LAN, and
provides touch/buzzer feedback. A Windows service publishes hardware telemetry
and performs graceful shutdown. Flutter and Home Assistant provide remote
control through a TLS MQTT broker.

## Repository

| Component | Purpose | Validation |
|---|---|---|
| `firmware/` | ESP32-S3 display, touch, provisioning, TLS MQTT, WOL | PlatformIO build and physical boot |
| `windows/` | Telemetry service, alerts, shutdown, speed test, control panel | .NET Release build and behavior tests |
| `mobile/` | Android/iOS dashboard and encrypted BLE provisioning | Dart analysis and Flutter tests |
| `home-assistant/` | HTTPS container deployment, dashboard, MQTT entities/actions | YAML and contract tests |
| `contracts/` | Versioned MQTT topics and command envelope | Node tests and validator |

## Hardware

- ESP32-S3 N4R2: 4 MB flash and 2 MB PSRAM
- ILI9341 320×240 SPI TFT: CS 10, MOSI 11, SCLK 12, DC 7, RST 6
- TTP223 touch input: GPIO 4
- Buzzer: GPIO 2

See [firmware/README.md](firmware/README.md) for safe wiring and backlight notes.

## MQTT choice

Use [HiveMQ Cloud Serverless Free](https://www.hivemq.com/products/mqtt-cloud-broker/)
for the normal installation. It provides TLS, authorization rules, up to 100
connections, and 10 GB monthly traffic without a credit card, but has no uptime
SLA. Create separate credentials for the ESP32, Windows agent, mobile app, and
Home Assistant.

An old Android phone running Termux can host an experimental local broker, but
it is not recommended for this system's remote control path: Android background
limits, changing LAN addresses, battery failure, TLS certificate management,
and router exposure make it less reliable and less secure. Keep the phone
option for LAN-only testing; do not forward an unencrypted MQTT port.

## Deployment order

1. Create the MQTT broker and dedicated credentials.
2. Power the ESP32 and connect to the TFT-displayed `PCBOT-xxxxxx` hotspot.
3. Open `http://192.168.4.1`, enter Wi-Fi, MQTT TLS, MAC, and broadcast details.
4. Build and open the Windows control panel, save its dedicated credential, and
   install the service from an elevated terminal.
5. Build/install the Flutter app or use Home Assistant.
6. Deploy `home-assistant/compose.yaml` on the Oracle VM and configure its
   dedicated MQTT credential through Home Assistant.

Detailed commands and external account steps are in
[DEPLOYMENT.md](DEPLOYMENT.md).

## Verify

Run from PowerShell:

```powershell
.\verify.ps1
```

The script validates contracts/YAML, builds firmware and Windows projects, runs
Windows behavior tests, and analyzes/tests Flutter when the local SDK is
available.
