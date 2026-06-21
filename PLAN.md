# PC Assistant Bot

## Summary
Build greenfield system using ESP32-S3, SH1106 OLED, TTP223 touch module, buzzer, Windows agent, Flutter mobile app, HiveMQ Cloud Free, and Home Assistant on Oracle Free VM.

Flow: phone/Nest/HA → HiveMQ TLS → ESP32-S3 sends local WOL packet → PC wakes. Shutdown always uses Windows agent; unavailable agent means safe failure.

## Implementation

- **ESP32-S3 firmware:** PlatformIO/Arduino, SH1106 128×64 driver, BLE Wi-Fi provisioning, secure MQTT, UDP WOL, TTP223 gestures, buzzer, OTA-ready structure.
- Touch tap triggers pet/emotion animation. Two-second hold toggles PC power.
- Send WOL three times to LAN broadcast. Never expose router UDP ports.
- Animations cover boot, provisioning, Wi-Fi/MQTT connect/disconnect, waking, shutdown, speed test, warning, critical alert, reset, petting, and idle sleep.
- Protect OLED against burn-in through dimming and pixel-position changes.

- **Windows 10/11 agent:** `.NET 8` service plus tray UI.
- Collect CPU/GPU temperatures and loads, RAM usage, drive temperature/space, network throughput, uptime, and PC state through LibreHardwareMonitor and Windows APIs.
- Run manually requested internet speed tests on PC, not Oracle VM.
- Process graceful shutdown commands with expiry, duplicate-command protection, acknowledgement, and safe failure.
- Provide Wi-Fi reset, factory reset, thresholds, MQTT setup, WOL MAC, and diagnostics.

- **Flutter app:** shared Android/iOS source.
- Dashboard, PC power confirmation, live telemetry, history, speed test, settings, BLE provisioning, Wi-Fi reset, and protected factory reset.
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

Require sustained violation, hysteresis, and cooldown to prevent repeated alarms. Alerts appear through bot OLED/buzzer only; dashboards retain alert history without phone push notifications.

## Test Plan
- Unit-test MQTT schemas, expiry/idempotency, thresholds, alert cooldown, WOL packets, reset rules, and telemetry normalization.
- Test firmware using mocked Wi-Fi/MQTT plus physical SH1106, TTP223, buzzer, reconnect, and power-cycle checks.
- Verify Android BLE provisioning, remote power, telemetry, reset confirmation, and credential storage.
- Test Windows startup service, sensor failures, shutdown permissions, throughput, and speed-test cancellation.
- Validate Home Assistant configuration, Google account linking, Nest power/temperature commands, broker ACL isolation, TLS reconnect, and PC-off WOL.
- Acceptance: remote wake succeeds, graceful shutdown never forces power loss, alerts animate correctly, and bot enters sleep animation after 60 seconds idle.

## Assumptions
- PC uses wired Ethernet with BIOS/NIC WOL enabled; ESP32-S3 shares its LAN.
- Display controller is `SH1106`; “SSH1106” treated as naming typo.
- Single-owner personal installation.
- Oracle Free capacity and HiveMQ Free limits remain available; neither provides uptime SLA.
- Google limitations follow current [Home Assistant integration](https://www.home-assistant.io/integrations/google_assistant/) and [Conversational Actions sunset](https://developers.google.com/assistant/ca-sunset) documentation.
