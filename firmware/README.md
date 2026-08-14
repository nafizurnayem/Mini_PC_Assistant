# ESP32-S3 firmware

## ILI9341 display wiring

The firmware targets the detected ESP32-S3 N4R2 hardware (4 MB flash and 2 MB
PSRAM) plus a 3.2-inch, 320x240, 8-pin ILI9341 SPI module in landscape
orientation. Its 8-pin header is write-only, so a MISO connection is not
required.

GPIO 13 is reserved in firmware as a dummy MISO input because this ESP32-S3
HSPI implementation requires a valid input pin. Leave GPIO 13 physically
unconnected from the display.

| ILI9341 pin | ESP32-S3 Super Mini | Purpose |
|---|---:|---|
| VCC | 5V | Display power |
| GND | GND | Common ground |
| CS | GPIO 10 | SPI chip select |
| RESET / RST | GPIO 6 | Display reset |
| DC / RS | GPIO 7 | Data/command select |
| MOSI / SDI | GPIO 11 | SPI data to display |
| SCK / CLK | GPIO 12 | SPI clock |
| LED | 3V3(OUT) | Backlight power |

The TTP223 remains on GPIO 4 and the buzzer uses GPIO 2. All ESP32-S3
signals use 3.3 V logic. Check whether the module includes a backlight resistor;
if it does not, add the resistor or transistor circuit recommended by the
module vendor instead of connecting the LED pin directly.

The pin macros and 40 MHz SPI speed are defined in `platformio.ini`. If the
display shows corruption on longer jumper wires, lower `SPI_FREQUENCY` to
27 MHz before changing application code.

## UI rendering

- `TFT_eSPI` drives the ILI9341 in landscape mode.
- The animated command-core panel renders into a 304x126 RGB565 sprite and is
  pushed as one frame. This avoids visible tearing from clearing and redrawing
  individual face elements.
- Telemetry cards redraw only when their value changes.
- The target frame interval is 33 ms (about 30 FPS).
- Warning, critical, offline, boot, pairing, wake, shutdown, speed-test,
  interaction, reset, and standby states use distinct accent colors and motion.
- The sprite needs about 77 KB of heap. A clear error is shown if allocation
  fails instead of continuing with a corrupted UI.

## Configure Wi-Fi from a phone or PC

When the bot has no valid configuration, it starts a temporary WPA2 setup
hotspot and also advertises the existing encrypted BLE service.

1. Read the hotspot name, random password, and URL from the TFT.
2. On a phone or PC, connect to the displayed `PCBOT-xxxxxx` Wi-Fi network.
3. Accept the captive-portal popup. If it does not appear, open
   `http://192.168.4.1` in a browser.
4. Scan for the destination Wi-Fi or type its SSID, then enter its password.
5. Complete the device, MQTT TLS, and PC network fields.
6. Select **Save configuration & restart**. The temporary hotspot closes after
   the settings are stored.

The portal has a random per-boot password, allows at most two clients, and is
available only during setup. Credentials travel inside that WPA2-protected
local connection; the setup portal is never exposed through the router.

After a Wi-Fi-only reset, stored MQTT password and root CA can be preserved by
leaving those two portal fields blank. A factory reset erases all credentials.
The Flutter app can still provision the same fields over encrypted BLE.

If incorrect Wi-Fi or MQTT settings leave the bot offline, hold the TTP223 while
powering the bot and keep holding for five seconds. This clears only Wi-Fi,
preserves MQTT/device settings, and reopens the browser setup hotspot.

## Build and upload

From the repository root:

```powershell
python -m platformio run -d firmware
python -m platformio run -d firmware -t upload
python -m platformio device monitor -b 115200
```

Before hardware acceptance, verify the exact ESP32-S3 board variant, USB port,
display colors/rotation, touch polarity, backlight current, and 30 FPS behavior
while Wi-Fi, BLE, and MQTT are all active.
