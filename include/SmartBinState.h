#pragma once

#include <Arduino.h>

#include "Vl53l0xSensor.h"

enum class OperatingMode : uint8_t {
  Auto,
  ManualOpen,
  ManualClosed,
};

enum class NetworkCommand : uint8_t {
  Open,
  Close,
  HoldOpen,
  HoldClosed,
  Auto,
};

struct SmartBinSnapshot {
  bool lidOpen;
  OperatingMode mode;
  TofReading tof;
  bool fillValid;
  float fillDistanceCm;
  float fillPercent;
  uint32_t uptimeMs;
};

inline const char* operatingModeName(OperatingMode mode) {
  switch (mode) {
    case OperatingMode::ManualOpen:
      return "manual_open";
    case OperatingMode::ManualClosed:
      return "manual_closed";
    case OperatingMode::Auto:
    default:
      return "auto";
  }
}

