#pragma once

#include <Arduino.h>
#include <VL53L0X.h>

enum class TofReadingStatus : uint8_t {
  Target,
  ClearOutOfRange,
  Invalid,
};

struct TofReading {
  TofReadingStatus status;
  uint16_t millimetres;
};

class Vl53l0xSensor {
 public:
  Vl53l0xSensor(uint16_t timeoutMs, uint32_t timingBudgetUs,
                uint32_t continuousPeriodMs, uint16_t maximumTrackedMm);

  bool begin();
  TofReading read();
  bool isInitialized() const;

 private:
  VL53L0X sensor_;
  uint16_t timeoutMs_;
  uint32_t timingBudgetUs_;
  uint32_t continuousPeriodMs_;
  uint16_t maximumTrackedMm_;
  bool initialized_ = false;
};

