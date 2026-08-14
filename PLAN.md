# PC Assistant Bot

Implementation status: all five code components are present. Firmware has been
flashed to the detected N4R2 board; automated contract, provisioning, Windows,
Flutter, and YAML checks are included. Cloud-account creation, private
credentials, Android/iOS signing, Oracle deployment, and final physical
hardware acceptance remain deployment activities rather than repository code.

## Summary
Build a polished PC companion system using ESP32-S3, a 3.2-inch ILI9341 SPI TFT LCD module, TTP223 touch module, buzzer, Windows agent, Flutter mobile app, HiveMQ Cloud Free, and Home Assistant on Oracle Free VM.

Flow: phone/Nest/HA → HiveMQ TLS → ESP32-S3 sends local WOL packet → PC wakes. Shutdown always uses Windows agent; unavailable agent means safe failure.

## Implementation

- **ESP32-S3 firmware:** PlatformIO/Arduino, ILI9341 320×240 SPI driver, BLE Wi-Fi provisioning, secure MQTT, UDP WOL, TTP223 gestures, buzzer, OTA-ready structure.
- Provide one temporary WPA2 captive setup portal that works from any phone or PC browser, alongside encrypted BLE provisioning for the mobile app. Display the per-boot SSID, password, and actual setup URL on the TFT.
- Limit the setup AP to two clients, stop it after successful configuration, never expose it through the router, never return stored passwords through the configuration API, and preserve MQTT secrets during a Wi-Fi-only recovery.
- Holding TTP223 for five seconds during boot clears Wi-Fi only and reopens setup, allowing recovery from an incorrect SSID without needing MQTT connectivity.
- Build a professional, gaming-inspired UI with layered cards, crisp typography, neon-accent status states, and smooth motion.
- Use landscape orientation with a fixed status header, animated command-core panel, three readable telemetry cards, and a compact interaction footer.
- Target 30 FPS with a sprite-buffered animated region and dirty-region telemetry updates; do not clear and repaint the full screen on every frame.
- Touch tap triggers pet/emotion animation. Two-second hold toggles PC power.
- Send WOL three times to LAN broadcast. Never expose router UDP ports.
- Animations cover boot, provisioning, Wi-Fi/MQTT connect/disconnect, waking, shutdown, speed test, warning, critical alert, reset, petting, and idle sleep.
- Use 3.3 V logic and document the exact eight-pin wiring. Treat MISO as intentionally absent and validate backlight current against the purchased module.

- **Windows 10/11 agent:** `.NET 8` service plus tray UI.
- Collect CPU/GPU temperatures and loads, RAM usage, drive temperature/space, network throughput, uptime, and PC state through LibreHardwareMonitor and Windows APIs.
- Run manually requested internet speed tests on PC, not Oracle VM.
- Process graceful shutdown commands with expiry, duplicate-command protection, acknowledgement, and safe failure.
- Provide Wi-Fi reset, factory reset, thresholds, MQTT setup, WOL MAC, and diagnostics.

- **Flutter app:** shared Android/iOS source.
- Dashboard, PC power confirmation, live telemetry, history, speed test, settings, BLE provisioning, Wi-Fi reset, and protected factory reset.
- Use the same visual language as the device UI: sleek, high-contrast, gaming-inspired surfaces with responsive layout and smooth transitions.
- Store credentials in Android Keystore/iOS Keychain.
- Verify Android build now. Keep iOS project ready, but defer signing and device/BLE verification until macOS/Xcode becomes available.

- **Cloud/home automation:** HiveMQ Cloud Free with TLS, separate credentials and topic permissions for ESP32, Windows agent, app, and Home Assistant.
- Oracle VM runs Home Assistant Container behind Caddy HTTPS using a free DNS hostname.
- HA provides dashboards, retained telemetry, PC power switch, speed-test button/result, reset actions, and metric history.
- Manual Google Home integration exposes PC power plus CPU, GPU, and drive temperature sensors to Nest Mini.
- Unsupported metrics remain available through HA app/Assist. Nest Mini cannot answer custom RAM/disk/load questions because Google Conversational Actions ended in 2023.

## MQTT Interface
Use `pcbot/v1/{deviceId}/...`:

- `command/power`, `command/speedtest`, `command/reset`
- `state/pc`, `state/bot`, `state/speedtest`
- `telemetry/system`, `event/alert`, `event/ack`
- Commands use QoS 1, are never retained, and contain `commandId`, `source`, `action`, `issuedAt`, and `expiresAt`.
- States use retained messages. ESP32 and Windows agent publish last-will offline states.
- Factory reset publishes acknowledgement first, then erases Wi-Fi, MQTT credentials, pairing, thresholds, and settings. Wi-Fi reset preserves all except network credentials.

## Alerts
Default configurable limits: CPU 90°C, GPU 88°C, drive 70°C, RAM 95% sustained, free disk below 10%, and telemetry loss.

Require sustained violation, hysteresis, and cooldown to prevent repeated alarms. Alerts appear through the bot TFT/buzzer only; dashboards retain alert history without phone push notifications.

## Test Plan
- Unit-test MQTT schemas, expiry/idempotency, thresholds, alert cooldown, WOL packets, reset rules, and telemetry normalization.
- Compile firmware in CI and test it using mocked Wi-Fi/MQTT plus a physical ILI9341 TFT, TTP223, buzzer, reconnect, animation timing, heap monitoring, and power-cycle checks.
- Verify Android, iOS, Windows, and macOS captive-portal detection; manual navigation to `192.168.4.1`; Wi-Fi scanning; open and WPA2 destination networks; invalid form rejection; secret preservation; AP shutdown after save; and boot-hold recovery.
- Measure frame cadence while Wi-Fi, BLE, TLS, and MQTT are active. Pass when the animated panel sustains at least 25 FPS, input reacts within 100 ms, and no sprite allocation or watchdog reset occurs.
- Check every screen for clipping at 320×240, legibility at normal desk distance, correct landscape rotation, stable colors, and no visible full-screen flicker.
- Verify Android BLE provisioning, remote power, telemetry, reset confirmation, and credential storage.
- Test Windows startup service, sensor failures, shutdown permissions, throughput, and speed-test cancellation.
- Validate Home Assistant configuration, Google account linking, Nest power/temperature commands, broker ACL isolation, TLS reconnect, and PC-off WOL.
- Acceptance: remote wake succeeds, graceful shutdown never forces power loss, alerts animate correctly, UI stays smooth and responsive, and bot enters sleep animation after 60 seconds idle.

## Assumptions
- PC uses wired Ethernet with BIOS/NIC WOL enabled; ESP32-S3 shares its LAN.
- Display controller is `ILI9341` over SPI; old SH1106 references are legacy only.
- Default ESP32-S3 DevKitC-1 wiring is CS 10, MOSI 11, SCLK 12, DC 7, RST 6, TTP223 4, and buzzer 5; re-map these before assembly if the selected board reserves a pin.
- Single-owner personal installation.
- Oracle Free capacity and HiveMQ Free limits remain available; neither provides uptime SLA.
- Google limitations follow current [Home Assistant integration](https://www.home-assistant.io/integrations/google_assistant/) and [Conversational Actions sunset](https://developers.google.com/assistant/ca-sunset) documentation.
