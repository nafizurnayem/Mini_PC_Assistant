#pragma once

#include <Arduino.h>

namespace BoardConfig {
constexpr uint8_t kOledSda = 8;
constexpr uint8_t kOledScl = 9;
constexpr uint8_t kTouchPin = 4;
constexpr uint8_t kBuzzerPin = 5;
constexpr uint8_t kOledAddress = 0x3C;
constexpr uint16_t kMqttPort = 8883;
constexpr uint32_t kTouchHoldMs = 2000;
constexpr uint32_t kIdleSleepMs = 60000;
constexpr uint32_t kTelemetryLostMs = 30000;
constexpr uint32_t kReconnectMs = 5000;
constexpr uint32_t kDisplayFrameMs = 100;
constexpr char kBleServiceUuid[] = "b7a70001-ec76-4b4f-9a06-8b865ce25170";
constexpr char kBleConfigUuid[] = "b7a70002-ec76-4b4f-9a06-8b865ce25170";
constexpr char kBleStatusUuid[] = "b7a70003-ec76-4b4f-9a06-8b865ce25170";
}

