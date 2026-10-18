# Pico Assistant 🤖

**Pico Assistant** is an ESP32-powered desk companion for your computer. It sits under your monitor, displays real-time PC telemetry (CPU/GPU temperatures, memory and network usage, and currently playing media), and lets you power your PC on or off with a touch sensor and adjust volume with a dial.

Works seamlessly with **Ubuntu Linux** and **Windows 10/11**, with an optional **mobile app** (Android/iOS) and **Home Assistant** integration.

---

## ✨ Features

- **Live PC Dashboard**: 3.2" color screen displaying CPU/GPU temperatures & load, RAM usage, network throughput, and media playback.
- **One-Touch Power Control**: Built-in Wake-on-LAN (WoL) turns your PC on remotely with a touch gesture, plus graceful shutdown support.
- **Smooth Volume Knob**: Dedicated physical rotary dial for instant PC master volume adjustments.
- **Smart Standby Clock**: Automatically transitions to a sleek digital desk clock when your PC is powered off.
- **Audio & Visual Alerts**: Buzzer sounds and visual warnings for critical system temperatures or high memory usage.
- **Cloud & Local Connectivity**: Communicates securely over TLS via MQTT (free cloud brokers like HiveMQ Cloud supported).
- **Mobile Companion App**: Monitor stats, run speed tests, and control power right from your phone.
- **Home Assistant Ready**: Pre-built Lovelace dashboards and automation packages included.

---

## 🛠️ Hardware Requirements

To build the physical desk bot, you will need:

| Component | Recommended Model | Note |
|---|---|---|
| **Microcontroller** | ESP32-S3 N4R2 (Super Mini) | 4 MB Flash, 2 MB PSRAM |
| **Display** | 3.2" ILI9341 SPI TFT (240×320) | Driven in portrait orientation |
| **Touch Button** | TTP223 Capacitive Touch Sensor | Mounts invisibly behind front wall |
| **Buzzer** | 5V / 3.3V Passive Buzzer | For audio alerts and haptic feedback |
| **Volume Knob** | 10 kΩ Potentiometer or EC11 Encoder | (Optional) Analog or digital volume dial |
| **Case** | 3D-printed enclosure | [Print files & CAD included](hardware/) |

> **Note:** Your PC must be connected via Ethernet with **Wake-on-LAN (WoL)** enabled in BIOS/UEFI to allow the bot to turn it on from an off state.

---

## 🚀 Quick Start Guide

Setting up Pico Assistant takes four simple steps:

### 1. Set Up an MQTT Broker
The bot and PC communicate through a secure MQTT broker.
- Create a free cloud broker at [HiveMQ Cloud](https://www.hivemq.com/cloud/) (or host your own via Mosquitto).
- Create separate credentials for the **Bot**, **PC**, and **Phone**.
- Choose a shared Device ID (e.g., `desktop-01`).

### 2. Flash & Assemble the Bot
- Flash the ESP32-S3 firmware using the [Firmware Guide](firmware/README.md).
- On first boot, connect your phone or laptop to the bot's Wi-Fi hotspot (`PICO-xxxxxx`).
- Open `http://192.168.4.1` in your browser to enter your Wi-Fi details, MQTT credentials, and PC MAC address.

### 3. Install the PC Agent
Run the native companion client on your computer:
- **Windows**: Follow the [Windows Guide](windows/README.md) to install the background service and system tray app.
- **Linux**: Follow the [Linux Guide](linux/README.md) to set up the lightweight background daemon and status app.

### 4. Optional: Mobile App & Home Assistant
- **Mobile App**: Install the Flutter phone app to check stats and control power anywhere on your home network ([Mobile Guide](mobile/README.md)).
- **Home Assistant**: Drop in the pre-configured package and Lovelace dashboard ([Home Assistant Guide](home-assistant/README.md)).

For comprehensive deployment instructions, check out the [Full Setup Guide](docs/setup.md).

---

## 🕹️ Everyday Controls

- **Short Tap Touch Pad**: Wake screen / dismiss active alert.
- **Hold Touch Pad for 2 Seconds**: Power ON the PC (via Wake-on-LAN) or request safe shutdown.
- **Turn Knob**: Smoothly adjust master system volume up or down.
- **Hold Touch Pad for 5 Seconds on Boot**: Reset Wi-Fi credentials to enter setup hotspot mode (preserves your MQTT keys).

---

## 📂 Project Structure

```
├── firmware/         # ESP32-S3 C++ firmware, display drivers & captive portal
├── windows/          # Windows .NET 8 background service & system tray app
├── linux/            # Linux Python daemon, system tray monitor & installer
├── mobile/           # Cross-platform Flutter companion app (Android & iOS)
├── hardware/         # 3D-printable case, Fusion 360 parametric CAD & diagrams
├── home-assistant/   # Docker Compose stack, YAML packages & Lovelace dashboard
├── contracts/        # Standard MQTT message specifications & JSON schemas
├── docs/             # Comprehensive deployment & development documentation
├── tests/            # Protocol, contract, and recovery automated test suites
└── tools/            # Verification & validation scripts
```

---

## 💻 Development & Contributing

- Run validation and contract checks:
  ```bash
  npm test
  npm run validate
  python3 tools/validate-yaml.py
  ```
- See the [Development Guide](docs/development.md) for full testing workflows and coding guidelines.
