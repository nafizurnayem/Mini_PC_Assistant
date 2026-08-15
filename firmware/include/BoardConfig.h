#pragma once

#include <Arduino.h>

namespace BoardConfig {
// Product branding. The MQTT topic namespace and NVS keys intentionally keep
// their original identifiers so existing brokers, Home Assistant packages and
// stored configurations keep working after the rename.
constexpr char kProductName[] = "PICO";
constexpr char kProductSuffix[] = "ASSISTANT";
constexpr char kProductFullName[] = "Pico Assistant";
constexpr char kSetupApPrefix[] = "PICO-";
// Bump on every release. Published in state/bot so the desktop app can tell
// whether an update actually took.
constexpr char kFirmwareVersion[] = "1.5.0";

// Over-the-air updates. The device serves an authenticated upload endpoint on
// the LAN; the desktop app pushes a firmware.bin to it.
constexpr uint16_t kOtaPort = 80;
constexpr uint32_t kOtaTokenBytes = 16;

// ILI9341 is configured through TFT_eSPI build flags in platformio.ini:
// VCC=5V, LED=3V3, CS=10, RST=6, DC=7, MOSI=11, SCLK=12. GPIO13 is reserved
// as the SPI controller's dummy MISO input; the 8-pin display has no MISO
// pin, so leave GPIO13 physically unconnected.
constexpr uint8_t kTouchPin = 4;
constexpr uint8_t kBuzzerPin = 2;
// Potentiometer wiper → ADC1_CH2. Safe to use with Wi-Fi active (ADC1 only).
// Wire: left leg = GND, right leg = 3V3, middle wiper = GPIO 3.
// GPIO 3 is also an ESP32-S3 strapping pin (JTAG source select). A 10k pot
// holds it at a defined level, which is fine, but if the board ever refuses to
// boot with the knob attached, move this to GPIO 5 (ADC1_CH4).
constexpr uint8_t kPotPin = 3;
constexpr uint16_t kPotAdcMax = 4095;
constexpr uint32_t kVolumeSampleMs = 40;
constexpr uint32_t kVolumePublishMs = 150;
// Percent change needed before a new setpoint is sent, which keeps ADC noise
// off the broker.
constexpr uint8_t kVolumeStepPercent = 2;
// Ignore readings until the ADC settles, so boot never yanks the PC volume.
constexpr uint32_t kVolumeSettleMs = 1200;
// How long the volume overlay stays up after the knob stops moving.
constexpr uint32_t kVolumeOverlayMs = 1600;

constexpr uint16_t kMqttPort = 8883;

// TTP223 modules leave the factory in momentary/active-high mode. Bridging the
// "B" pad inverts the output; set kTouchActiveHigh to false for those boards.
constexpr bool kTouchActiveHigh = true;
// The module output is push-pull, but a pulldown keeps the input defined when
// the sensor is unpowered or the wire is loose, instead of reading noise.
constexpr uint8_t kTouchPinMode = INPUT_PULLDOWN;
constexpr uint32_t kTouchDebounceMs = 25;
constexpr uint32_t kTouchHoldMs = 2000;
// Presses longer than a tap but shorter than a hold are ignored rather than
// being treated as a tap, so a slow finger never fires the wrong action.
constexpr uint32_t kTouchTapMaxMs = 600;

constexpr uint32_t kIdleSleepMs = 60000;
constexpr uint32_t kTelemetryLostMs = 30000;
constexpr uint32_t kReconnectMs = 5000;
constexpr uint32_t kWifiRetryMs = 15000;
constexpr uint32_t kDisplayFrameMs = 25;
// The telemetry cards push about 85 KB per pass, so they animate on their own
// slower cadence while the rest of the interface keeps the 25 ms budget.
constexpr uint32_t kCardFrameMs = 50;
constexpr uint32_t kWelcomeMs = 2600;
constexpr uint32_t kTransientMoodMs = 2600;

// Power saver: once the PC reports itself off, the assistant stops animating a
// full dashboard for data that is no longer arriving and shows a clock instead.
// The delay leaves the shutdown animation on screen long enough to be read.
constexpr uint32_t kClockDelayMs = 8000;
constexpr uint32_t kPowerSaverFrameMs = 250;
constexpr float kPowerSaverDim = 0.30f;
constexpr uint32_t kActiveCpuMhz = 240;
constexpr uint32_t kPowerSaverCpuMhz = 80;  // Lowest frequency Wi-Fi supports.
constexpr bool kUse24HourClock = false;

constexpr char kBleServiceUuid[] = "b7a70001-ec76-4b4f-9a06-8b865ce25170";
constexpr char kBleConfigUuid[] = "b7a70002-ec76-4b4f-9a06-8b865ce25170";
constexpr char kBleStatusUuid[] = "b7a70003-ec76-4b4f-9a06-8b865ce25170";
}
