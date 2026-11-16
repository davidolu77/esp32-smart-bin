#pragma once

#include <Arduino.h>

namespace Pins {
constexpr uint8_t ultrasonicTrigger = 5;
constexpr uint8_t ultrasonicEcho = 18;
constexpr uint8_t servoSignal = 13;
constexpr uint8_t statusLed = 2;
constexpr uint8_t pushButton = 4;
constexpr uint8_t tofSda = 21;
constexpr uint8_t tofScl = 22;
}  // namespace Pins

namespace Timing {
constexpr uint32_t tofIntervalMs = 100;
constexpr uint32_t fillSensorIntervalMs = 250;
constexpr uint32_t serialReportIntervalMs = 1000;
constexpr uint32_t triggerConfirmationMs = 150;
constexpr uint32_t clearBeforeCloseMs = 3000;
constexpr uint32_t buttonDebounceMs = 40;
constexpr uint32_t ultrasonicTimeoutUs = 30000;
constexpr uint16_t tofTimeoutMs = 60;
constexpr uint32_t tofTimingBudgetUs = 33000;
constexpr uint32_t tofContinuousPeriodMs = 50;
constexpr uint8_t tofInvalidConfirmationCount = 3;
}  // namespace Timing

namespace Proximity {
constexpr uint16_t tofTriggerMm = 180;
constexpr uint16_t tofClearMm = 250;
constexpr uint16_t tofMaximumTrackedMm = 500;
}  // namespace Proximity

namespace FillCalibration {
// Top-down sensor gaps: large gap = empty, small gap = full.
constexpr float BIN_EMPTY_DISTANCE_CM = 28.0F;
constexpr float BIN_FULL_DISTANCE_CM = 4.0F;
static_assert(BIN_EMPTY_DISTANCE_CM > BIN_FULL_DISTANCE_CM,
              "Empty distance must be greater than full distance");
}  // namespace FillCalibration

namespace Lid {
constexpr int closedAngleDegrees = 0;
constexpr int openAngleDegrees = 90;
constexpr int servoMinimumPulseUs = 500;
constexpr int servoMaximumPulseUs = 2400;
}  // namespace Lid

namespace NetworkConfig {
constexpr char hostname[] = "esp32-smart-bin";
constexpr char firmwareVersion[] = "1.2.0";
constexpr uint16_t httpPort = 80;
constexpr uint32_t reconnectIntervalMs = 15000;
}  // namespace NetworkConfig

