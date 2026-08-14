# GIF Loop Display Feature

Like NZXT Kraken / Lian Li LCD coolers — upload a short animated GIF from the
tray app, enable it over MQTT, and the display shows a continuous fullscreen
portrait loop instead of the dashboard.

Replaces the earlier video/PCVF plan. Reasoning is in "Why GIF, not video"
below.

---

## How It Works (end-to-end)

```
User picks a GIF in the tray app
  → already 240x320 and under the size cap?  upload as-is, no FFmpeg
  → otherwise FFmpeg scales/pads to 240x320 and re-optimises the palette
  → HTTP POST to the ESP32 at its LAN address        (one-time per GIF)
  → "Enable GIF mode" publishes command/display → gif_on
  → ESP32 stops the dashboard and loops the GIF fullscreen from LittleFS
  → "Disable" publishes gif_off → dashboard resumes
```

---

## Why GIF, not video

The ceiling is SPI bandwidth, not decoding. A full 240x320 frame is 153,600
bytes; at 40 MHz that is ~31 ms, capping full-frame playback near 32 fps before
any decode work. JPEG frame sequences push the whole screen every frame, so
they land at 15–20 fps in practice.

GIF wins on two structural points:

- **Partial frames.** GIF stores only the changed rectangle per frame. Anime
  loops with a static background push a fraction of 153 KB per frame, so the
  effective frame rate goes up instead of down.
- **LZW plus a 256-colour palette** compresses flat cel-shaded art far better
  than JPEG, so more seconds of loop fit in the same flash.

Decoding is also cheap: bitbank2's `AnimatedGIF` decodes 240x320 in roughly
8–15 ms on the S3 and streams from LittleFS through file callbacks, so the file
never has to fit in RAM.

The trade: 256 colours per frame means smooth dark gradients can band. FFmpeg's
`palettegen` / `paletteuse` with per-frame palettes removes most of it.

---

## User Review Required

> [!IMPORTANT]
> **"HD" has to be resized on the PC.** A 1080p GIF is 10–50 MB, flash offers
> ~2.4 MB, and the decoder draws at native size — there is no on-device scaler.
> Anything above 240x320 buys nothing on a 240x320 panel. Oversized files get
> converted; correctly-sized ones upload untouched.

> [!IMPORTANT]
> **FFmpeg is optional, not required.** It is only invoked when the chosen file
> is not already 240x320 or does not fit. Pre-made 240x320 loops work with no
> FFmpeg installed.

> [!WARNING]
> **OTA partition removed.** `min_spiffs.csv` gives SPIFFS only 128 KB and keeps
> a second OTA slot (1.9 MB) this project never uses. The new table drops OTA-1
> and gives **~2.4 MB** to LittleFS. Firmware is still flashed over USB;
> OTA-over-Wi-Fi was never implemented.

> [!IMPORTANT]
> **Loop length: roughly 15–25 seconds** at 240x320 and 10 fps, depending on how
> much of the frame changes. 3–8 second loops look best and leave margin.

> [!IMPORTANT]
> **The ESP32's LAN address must be entered once** in the tray app so the
> uploader knows where to POST. The device prints its IP to serial on boot; it
> is also in the router's DHCP table.

---

## Proposed Changes

### Firmware

#### [NEW] `firmware/partitions.csv`
1.5 MB app (firmware is currently 1.22 MB, so ~280 KB headroom), 2.44 MB
LittleFS.

```
# Name,    Type, SubType, Offset,   Size
nvs,       data, nvs,     0x9000,   0x5000
otadata,   data, ota,     0xe000,   0x2000
app0,      app,  ota_0,   0x10000,  0x180000
littlefs,  data, spiffs,  0x190000, 0x270000
```

#### [MODIFY] `firmware/platformio.ini`
- `board_build.partitions = ../partitions.csv`
- `board_build.filesystem = littlefs`
- `lib_deps += bitbank2/AnimatedGIF@^2.1.1`

#### [NEW] `firmware/include/GifScreen.h` / `src/GifScreen.cpp`
```cpp
class GifScreen {
 public:
  bool begin();                       // mount LittleFS
  bool hasGif() const;                // /loop.gif exists and opens
  void start();                       // rewind to frame 0
  void tick(TFT_eSPI& tft, uint32_t now);  // decode + push the next frame
  void stop();                        // close the decoder, free buffers
};
```
- File callbacks (`open`/`close`/`read`/`seek`) stream from LittleFS.
- Frame buffer and the LZW dictionary come from PSRAM.
- `tick()` honours each frame's own delay, applies GIF disposal, and pushes
  only the frame's rectangle. Loops at the end.

#### [MODIFY] `firmware/src/main.cpp`
1. `WebServer server(80)` started once Wi-Fi is up:
   - `POST /gif` — stream the body to LittleFS `/loop.gif`, reject if it would
     overflow the partition, respond with frame count and size
   - `GET /status` — `{"hasGif":bool,"gifMode":bool,"ip":"...","freeBytes":n}`
2. `command/display` handler for `gif_on` / `gif_off`, acknowledged on
   `event/ack` like every other command.
3. Subscribe to `command/display` in `connectMqtt()`.
4. In `loop()`: when GIF mode is on and a GIF exists, call `gif.tick()` instead
   of `bot.tick()`. Touch still works — a tap exits GIF mode so the dashboard
   is always reachable without the PC.

#### [MODIFY] `firmware/include/BotDisplay.h`
Add `TFT_eSPI& panel()` so `main.cpp` can hand the display to `GifScreen`.

### Windows — Core

#### [MODIFY] `windows/src/PcBot.Core/Models.cs`
`public string DisplayCommand => $"{_root}/command/display";`

### Windows — Tray

#### [MODIFY] `MainForm.cs`
New **Display** section:

| Control | Purpose |
|---|---|
| Text "Device IP" | where the ESP32 HTTP server listens |
| Text "FFmpeg path" | defaults to `ffmpeg` on PATH, only used when conversion is needed |
| Numeric "Target FPS" | 5–20, default 10, conversion only |
| Button **Upload GIF…** | file picker → convert if needed → POST with progress |
| Button **Enable GIF mode** | publish `command/display { action: "gif_on" }` |
| Button **Disable** | publish `gif_off` |
| Label | result and remaining flash |

Upload flow:
1. `OpenFileDialog` (GIF, MP4, MOV, WebM…)
2. If it is a GIF, already ≤240x320, and fits — skip straight to step 4
3. Otherwise FFmpeg:
   `-vf "fps=N,scale=240:320:force_original_aspect_ratio=decrease,pad=240:320:(ow-iw)/2:(oh-ih)/2,split[a][b];[a]palettegen=stats_mode=single[p];[b][p]paletteuse=new=1"`
4. `HttpClient` POST to `http://{ip}/gif`, streamed with progress
5. Report `Uploaded N frames (X.X MB), Y KB free`

#### [MODIFY] `PcBot.Tray.csproj`
No new packages — `System.Diagnostics.Process` and `HttpClient` only.

---

## Verification Plan

**Firmware build** — `pio run` with the new partition table, confirm the app
fits 1.5 MB with headroom.

**Upload** — `curl -X POST http://{ip}/gif --data-binary @loop.gif`; serial
should log the byte count and parsed frame count.

**Playback** — publish `command/display { action: "gif_on" }`, confirm the loop
runs fullscreen and repeats seamlessly; `gif_off` restores the dashboard.

**Recovery** — tap the pad during playback and confirm the dashboard returns
without the PC being involved.

**Measure** — log the achieved frame interval over a 10 s loop and compare to
the GIF's declared delays, to see the real frame rate on a representative file.
