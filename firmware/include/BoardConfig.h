#pragma once

#include <Arduino.h>

namespace BoardConfig {
// ILI9341 is configured through TFT_eSPI build flags in platformio.ini:
// VCC=5V, LED=3V3, CS=10, RST=6, DC=7, MOSI=11, SCLK=12. GPIO13 is reserved
// as the SPI controller's dummy MISO input; the 8-pin display has no MISO
// pin, so leave GPIO13 physically unconnected.
constexpr uint8_t kTouchPin = 4;
constexpr uint8_t kBuzzerPin = 2;
constexpr uint16_t kMqttPort = 8883;
constexpr uint32_t kTouchHoldMs = 2000;
constexpr uint32_t kIdleSleepMs = 60000;
constexpr uint32_t kTelemetryLostMs = 30000;
constexpr uint32_t kReconnectMs = 5000;
constexpr uint32_t kDisplayFrameMs = 33;
constexpr char kBleServiceUuid[] = "b7a70001-ec76-4b4f-9a06-8b865ce25170";
constexpr char kBleConfigUuid[] = "b7a70002-ec76-4b4f-9a06-8b865ce25170";
constexpr char kBleStatusUuid[] = "b7a70003-ec76-4b4f-9a06-8b865ce25170";
}
