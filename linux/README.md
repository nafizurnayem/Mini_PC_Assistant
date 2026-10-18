# Pico Assistant for Ubuntu Linux

A separate Linux agent and native desktop control panel, using the existing
`pcbot/v1/<deviceId>/...` protocol. Connect it to the same TLS MQTT broker as
your ESP32 bot, Flutter phone app, and Home Assistant. The Windows build stays
in `windows/`.

## Install on Ubuntu Desktop

Requires Python 3.10 or newer and a systemd desktop session. Run these commands
from the repository root in your normal desktop terminal:

```bash
sudo apt update
sudo apt install python3-venv python3-tk pulseaudio-utils playerctl
cd linux
bash install.sh
```

Open **Pico Assistant** from Ubuntu's Applications menu, or run:

```bash
~/.local/bin/pico-assistant
```

In **Connection**, enter the same **device ID**, **MQTT host**, and **TLS port**
(usually 8883) used by the phone and ESP32. Enter a dedicated Linux MQTT
username/password. Save settings, then select **Start agent** on the Dashboard.
Do not run the installer or the app with `sudo`.

The installer copies the app into `~/.local/share/pico-assistant`, creates an
isolated Python environment, an application launcher, and a systemd **user**
service. Moving the repository afterwards does not break the installation.
The installed agent starts at graphical login and stops at logout. Closing
the installed control panel leaves the service running. It is not a
before-login/headless system service; audio and MPRIS belong to your desktop
session. Run the installer again after updating the source, then start the
agent from the control panel.

## Run directly from this folder

After installing the Ubuntu packages above:

```bash
cd linux  # if you are still at the repository root
bash setup.sh
bash run.sh
```

This keeps Python dependencies in `linux/.venv`. It does not install a service
or menu entry. Use **Start agent** in the window; closing this source-mode
window stops the agent it started. Alternatively, run it in a terminal after
saving settings:

```bash
bash run.sh agent
```

Only one agent can run per user. Do not run Windows and Linux agents at the
same time with the same device ID on separate computers.

## Cloud, phone, and bot setup

### Recover after changing your router password

Open **Bot Wi-Fi setup** on the Linux Dashboard (also available as its own tab).
It works without MQTT or Internet access:

1. On firmware 1.6+, the bot opens setup after 60 seconds without Wi-Fi. You can
   also hold its touch pad for five seconds while it is offline.
2. On older firmware, hold the touch pad while powering on for five seconds.
3. Join the `PICO-xxxxxx` hotspot using the password displayed on the bot.
4. Enter your home Wi-Fi name and new password, then choose **Save Wi-Fi to bot**.
   For older firmware or first-time setup, choose **Open setup page** instead.
5. Reconnect your PC to home Wi-Fi after the bot restarts.

The in-app Wi-Fi-only save requires firmware 1.6+ and preserves the bot's
dedicated MQTT credential, certificate, device ID and PC settings. It does not
send the Linux MQTT password to the bot. **Enable setup on online bot** is a
separate cloud action and cannot reach an offline bot.

### Shared connection

The phone app's existing telemetry, power, alerts, and speed-test screens work
with the Linux message formats. There is no separate Linux phone app and no
new cloud server to deploy. If the app is already installed, update its
connection settings to match the Linux agent. Keep the ESP32 connected to your
home LAN so it can wake the PC while Linux is powered off.

All permissions below are relative to `pcbot/v1/<deviceId>/`:

| Linux credential | Topics |
|---|---|
| Publish | `state/pc`, `telemetry/system`, `state/media`, `state/volume`, `state/speedtest`, `event/alert`, `event/ack` |
| Subscribe | `command/power`, `command/speedtest`, `command/volume`, `state/bot` |
| Publish for desktop buttons | `command/speedtest`, `command/reset` |

Use the broker's trusted public certificate or set **Custom CA file** to a PEM
file for your own broker. TLS certificate and hostname validation are always
enabled. No incoming PC port or router port-forward is required. The broker
must allow the publish and subscribe operations above; rejected subscription
permissions appear on the Dashboard.

To build the existing Android app on Ubuntu, install Flutter and the Android
SDK, then use the committed host project:

```bash
cd mobile  # from the repository root
flutter pub get
flutter analyze
flutter test
flutter build apk --release
```

APK: `mobile/build/app/outputs/flutter-apk/app-release.apk`. See
[mobile/README.md](../mobile/README.md) for signing. Building iOS requires macOS.

## Features and limits

| Feature | Linux implementation |
|---|---|
| CPU / memory / disk / network | psutil; default sampling every five seconds |
| CPU temperature | Available kernel sensors, including AMD k10temp and Intel coretemp |
| GPU | First NVIDIA GPU through `nvidia-smi`, or available AMD/nouveau sensors and DRM busy counters |
| Bot volume knob | Default sink through `pactl`, with `wpctl` fallback when pactl is absent |
| Now playing on bot | MPRIS-compatible players through `playerctl`; track, artist, album, position |
| Alerts | Temperature, memory and free-space thresholds with sustain, cooldown and recovery hysteresis |
| Remote shutdown | Explicit opt-in; normal `systemctl poweroff` through Ubuntu's existing authorization |
| Wake | ESP32 sends Wake-on-LAN on your local network |
| Speed tests | Optional Ookla **Speedtest CLI**, executable `speedtest` |
| Bot Wi-Fi/factory reset | Confirmed desktop actions over MQTT |

Missing sensors are reported as `null`, rather than fabricated zero readings.
Drive temperature is currently unavailable. Missing media/volume utilities
do not stop telemetry. Telemetry omits trailing disk entries and long media
text is shortened when needed to fit the bot's 2048-byte MQTT buffer. The
control panel does not yet include the Windows
firmware updater or GIF/video upload workflow.

Install Ookla's CLI if you want speed tests. From the repository root:

```bash
python3 linux/install-speedtest.py
```

This downloads the official binary over HTTPS to
`~/.local/share/pico-assistant/bin/speedtest`, without sudo or apt repository
changes. Leave **Ookla Speedtest executable** set to `speedtest`; the agent
finds this binary even when the systemd service has a different PATH.
Ubuntu's similarly named Python `speedtest-cli` is not a compatible substitute. The configured
executable is run with `--accept-license --accept-gdpr --format=json` when you
request a test. Without it, both the desktop and phone show installation
instructions. Other failures include the CLI's error message.

The Dashboard shows live download/upload traffic in Mbps, the selected
interface, and speed-test progress/results. Live traffic is averaged over the
Monitoring sampling interval (five seconds by default; set two for more frequent
updates). It reflects current usage, so low values while idle are normal.
For a fixed adapter, enter its name in **Network interface**; leave blank for
automatic selection, including active VPN interfaces.

## Shutdown and Wake-on-LAN

Enable **Allow graceful shutdown from phone / bot** under Monitoring, save,
and stop/start the agent. Remote shutdown is disabled by default. Ubuntu
logind/polkit and shutdown inhibitors still apply; denied requests produce a
rejected acknowledgement. The app never adds sudoers/polkit overrides and
never forces power off. Multiple logged-in users or a blocked shutdown may
require local action.

Enable Wake-on-LAN in BIOS/UEFI and your wired network connection settings.
Copy the Ethernet MAC and LAN broadcast address shown under Device into the
ESP32 provisioning settings. Wake support depends on your motherboard,
Ethernet adapter and network configuration; ordinary Wi-Fi is not a substitute
for wired Wake-on-LAN. This app does not change your network settings.

## Settings, service, and troubleshooting

Settings: `~/.config/pico-assistant/config.json` (owner-only permissions).
They contain your MQTT password in a local JSON file; this is not keyring
encryption. The app respects `XDG_CONFIG_HOME` and `XDG_STATE_HOME`; if you
customize them, use the same environment for the GUI and user service.
Runtime status and the persistent duplicate-command cache are under
`~/.local/state/pico-assistant/`. The cache prevents replay across restarts.

```bash
systemctl --user status pico-assistant.service
journalctl --user -u pico-assistant.service -n 80 --no-pager
systemctl --user restart pico-assistant.service
~/.local/bin/pico-assistant diagnose
~/.local/bin/pico-assistant snapshot
~/.local/bin/pico-assistant check
```

Source-mode logs: `~/.local/state/pico-assistant/agent.log`. Prefix a source
command with `bash run.sh` in place of the installed launcher. A custom config
can be selected with `bash run.sh --config /path/to/config.json agent`.

If cloud is disconnected, check host/credentials, device ID, broker ACLs,
outbound TLS connectivity, and the machine's clock. If audio or media is
missing, ensure the app runs as the same user as your desktop player and that
`pactl info` / `playerctl status` work in that user's terminal.

Uninstall the service and launchers (preserves settings and app files):

```bash
bash install.sh --uninstall
```

## Verification

```bash
bash setup.sh
bash verify.sh
```

The 43 automated tests cover command validation/replay, shutdown authorization
failure, reconnect callbacks, TLS configuration, volume backends, missing OS
facilities, telemetry contract fields, speed-test results, and alert recovery.
Hardware side effects and MQTT delivery are mocked in behavior tests; tests
never shut down the machine. The verification script also reads a real local
telemetry snapshot and diagnostics.

The desktop window startup and network display have been smoke-tested on a
virtual display. A speed-test request through the configured cloud broker
completed successfully on the installed agent, with live traffic updating
during the test. This does not establish results for other networks.

Live acceptance still requires checking the physical phone/bot displays,
turning the bot's knob, login autostart, network interruption/recovery, and an
intentional shutdown followed by Wake-on-LAN.

Implementation references: [Paho MQTT client](https://eclipse.dev/paho/files/paho.mqtt.python/html/client.html),
[psutil](https://psutil.io/), [playerctl](https://github.com/altdesktop/playerctl).
