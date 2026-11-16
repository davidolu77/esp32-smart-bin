#pragma once

#include <Arduino.h>

#include "DistanceSensor.h"

class Hcsr04Sensor final : public DistanceSensor {
 public:
  Hcsr04Sensor(uint8_t triggerPin, uint8_t echoPin, uint32_t timeoutUs);

  void begin() override;
  DistanceReading read() override;

 private:
  uint8_t triggerPin_;
  uint8_t echoPin_;
  uint32_t timeoutUs_;
};


