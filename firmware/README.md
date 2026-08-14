# Pico Assistant firmware (ESP32-S3)

## ILI9341 display wiring

The firmware targets the detected ESP32-S3 N4R2 hardware (4 MB flash and 2 MB
PSRAM) plus a 3.2-inch, 8-pin ILI9341 SPI module driven in portrait
orientation (240x320, `setRotation(0)`). Its 8-pin header is write-only, so a
MISO connection is not required.

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

The interface is built from three source files:

| File | Responsibility |
|---|---|
| `DisplayTheme.cpp` | Palette plus primitives: gradients, frosted glass cards, gradient pills and arcs, neon glow, sheen sweep |
| `LiquidBar.cpp` | Animated liquid fill bar: gradient body, bright leading edge, rising bubbles |
| `Icons.cpp` | Vector icons: CPU, GPU, memory, thermometer, link, power |
| `WelcomeAnimation.cpp` | Boot intro |
| `ClockScreen.cpp` | Power-saver clock shown while the PC is off |
| `BotDisplay.cpp` | Header, summary tiles, metric cards, footer, screen switching |

`TFT_eSPI` drives the ILI9341 at 40 MHz in **portrait** (rotation 0, 240x320).

### Layout

| Band | Content |
|---|---|
| 0–32 | Wordmark and a gradient status pill with a signal icon |
| 36–132 | Full-width network activity graph |
| 138–296 | Three metric cards: gradient icon tile, label, value, liquid bar |
| 300–320 | State label and the hold hint, replaced by the hold bar while pressed |

The network graph is the summary panel: a filled area trace for download and a
line for upload over a four-line grid, with a breathing dot on the newest
sample, the current peak labelled on the scale, signal bars and RSSI in the
title row, and a legend that doubles as the live reading. History is 46 samples,
about four minutes at the 5 s telemetry interval, stored in quarter-Mb steps and
scaled against a decaying peak.

Temperatures live in the CPU and GPU cards below, which use the thermal scale
(30–95 °C) with amber above 80 °C and red above 90 °C.

### Palette and depth

- Neon violet glass: near-black violet base with violet, magenta and electric
  blue accents. Mint marks a healthy link, amber and red mark trouble.
### Glass and motion

There is no alpha channel on an ILI9341, so "frosted glass" is faked and it
has to be faked consistently: `glassCard()` lifts the ground towards a blue-
shifted surface tone, adds a lit top-left rim, a dark bottom-right rim, a
tinted border and a short diagonal sheen. Every tile on the dashboard uses it,
which is what makes the surfaces read as one material.

Everything on screen moves, and each motion means something:

| Element | Motion |
|---|---|
| Metric bars | Liquid fill: gradient body, bright leading edge, small bubbles rising and drifting forward, fading in on entry and out at the surface |
| Values | Glide towards new readings instead of snapping |
| Ring gauge | Gradient stroke with a glint rotating around the track |
| Network graph | Area and line traces scaled to a decaying throughput peak, newest point breathing |
| Cards | Slow sheen sweep, offset between tiles so they never flash in unison |
| Hold gesture | The footer becomes a liquid bar filling towards the 2 s threshold |

Semantic colour is kept separate from the violet-to-blue accent: amber above
75% and red above 88% of full scale, so "hot" can never be mistaken for
decoration.
- Panels use a drop shadow, a gradient body, a sheen line, a light top bevel and
  a dark bottom bevel. `glow()` adds fading concentric outlines for the neon
  bloom around pills and bars.
- Telemetry accents turn amber above 75% and red above 88% of full scale.

### Cost control

- Only the 44x44 face badge redraws every frame (about 3.9 KB per push).
  Telemetry rows repaint when a value changes, the history strip when a sample
  arrives, the header and chip when the mood changes.
- The intro canvas (224x160) is deleted before the dashboard sprites are
  allocated, so the two never occupy memory at once. Peak use is about 72 KB.
- The target frame interval is 25 ms, and 250 ms in power saver.
- Standby dims the whole interface through a single animated factor instead of
  blanking the screen.
- A clear error is shown if sprite allocation fails instead of continuing with
  a corrupted UI.

## Welcome animation

`begin()` no longer flashes red/green/blue test screens. The intro runs for
`kWelcomeMs` (2.6 s) and still proves the panel works, because it exercises the
backdrop gradient, motion, colour and text within the first second:

1. The backdrop gradient and vignette are painted.
2. The orb scales up with an overshoot ease while a halo ring expands and fades.
3. `PICO` slides in with a drop shadow, then `ASSISTANT` fades in letterspaced.
4. The eyes open, a shine band sweeps across the wordmark, and a progress bar
   completes the sequence.

Touching the pad during the intro skips straight to the main UI.

## Power saver and clock screen

When the retained `state/pc` topic reports the PC as off, the assistant waits
`kClockDelayMs` (8 s, long enough to read the shutdown animation) and then
switches to the power-saver clock:

- Resting orb, large seven-segment time with a violet halo, date, and a
  `PC IS OFF / HOLD 2s TO WAKE IT` hint.
- Frame interval drops from 25 ms to 250 ms, and the time area repaints only
  when the displayed string changes.
- The palette dims by 30%.
- The CPU drops to 80 MHz, the lowest frequency Wi-Fi supports, and returns to
  240 MHz as soon as the clock is dismissed.
- Touching the pad returns to the full dashboard; it reappears as a clock 8 s
  after the last interaction. Holding for 2 s still sends Wake-on-LAN.
- MQTT stays connected throughout, so the PC coming back online is picked up
  immediately.

The clock needs a local time zone. It is a POSIX TZ string set in the setup
portal (`Time zone (clock screen)`, default `UTC0`) and stored in NVS. MQTT
timestamps are unaffected: they are generated with `gmtime` and stay UTC.
Switch between 12- and 24-hour display with `kUse24HourClock` in
`include/BoardConfig.h`.

## Now playing and the volume knob

The summary slot shows a now-playing card whenever the PC reports media, and
falls back to the network graph when nothing is playing. The card carries the
player name, a scrolling title, the artist, elapsed/total time and a progress
bar; the position runs forward locally between updates so the bar moves
smoothly rather than stepping once a second.

Data comes from `state/media`, published by the tray app from the Windows media
session — no Spotify account or API key is involved, and any player works.

The volume knob is a 10 kΩ linear potentiometer:

| Pot leg | Wire to |
|---|---|
| Left | GND |
| Wiper (middle) | GPIO 3 |
| Right | 3V3 |

It must stay on ADC1 (GPIO 1–10); ADC2 is unusable while Wi-Fi is running.
GPIO 3 is also a strapping pin (JTAG source select) — a 10 kΩ pot holds it at a
defined level, but if the board ever refuses to boot with the knob attached,
move `kPotPin` to GPIO 5.

Readings are smoothed with an exponential filter, ignored for the first
1.2 s while the ADC settles, and only published when the level moves by
`kVolumeStepPercent` (2%) or hits an end stop, at most every 150 ms. Setpoints
go out on `command/volume` at QoS 0 without retention — an absolute value that
is already stale is worthless. The tray app applies it through Core Audio and
echoes the result on `state/volume`.

A volume overlay pops over the middle card for 1.6 s on every change, and the
footer keeps a small speaker icon and percentage.

## TTP223 touch behaviour

- Tap (release under 600 ms) greets you; hold 2 s toggles PC power.
- While held, an arc around the head fills to show progress towards the hold
  threshold, so the action is never a surprise.
- Input is debounced for 25 ms, and presses between 600 ms and 2 s are ignored
  rather than treated as a tap.
- The pad is sampled on every loop iteration, including setup mode, Wi-Fi
  outages and broker retries.
- The pin uses an internal pulldown so an unpowered or disconnected sensor
  reads a defined low instead of noise.

If the pad does nothing, check these in order:

1. Serial log at boot prints `touch pin 4 idle level=...`. Watch it while
   touching: the level must change.
2. Most TTP223 boards ship in momentary active-high mode. If the `B` pad is
   bridged the output is inverted, so set `kTouchActiveHigh = false` in
   `include/BoardConfig.h`. If the `A` pad is bridged the output latches
   (toggle mode) and the hold gesture repeats.
3. Power the module from 3V3, not 5V, and share ground with the ESP32.

## Configure Wi-Fi from a phone or PC

When Pico Assistant has no valid configuration, it starts a temporary WPA2
setup hotspot and also advertises the existing encrypted BLE service. BLE
advertises as `Pico Assistant <deviceId>`.

1. Read the hotspot name, random password, and URL from the TFT.
2. On a phone or PC, connect to the displayed `PICO-xxxxxx` Wi-Fi network.
3. Accept the captive-portal popup. If it does not appear, open
   `http://192.168.4.1` in a browser.
4. Scan for the destination Wi-Fi or type its SSID, then enter its password.
5. Complete the device, time zone, MQTT TLS, and PC network fields.
6. Select **Save configuration & restart**. The temporary hotspot closes after
   the settings are stored.

The portal has a random per-boot password, allows at most two clients, and is
available only during setup. Credentials travel inside that WPA2-protected
local connection; the setup portal is never exposed through the router.

After a Wi-Fi-only reset, stored MQTT password and root CA can be preserved by
leaving those two portal fields blank. A factory reset erases all credentials.
The Flutter app can still provision the same fields over encrypted BLE.

If incorrect Wi-Fi or MQTT settings leave the device offline, hold the TTP223
while powering it and keep holding for five seconds. This clears only Wi-Fi,
preserves MQTT/device settings, and reopens the browser setup hotspot.

## Build and upload

From the repository root:

```powershell
python -m platformio run -d firmware
python -m platformio run -d firmware -t upload
python -m platformio device monitor -b 115200
```

Before hardware acceptance, verify the exact ESP32-S3 board variant, USB port,
display colors/rotation, touch polarity, backlight current, and frame rate
while Wi-Fi, BLE, and MQTT are all active.
