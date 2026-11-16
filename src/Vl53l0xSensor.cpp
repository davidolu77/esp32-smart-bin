#include "Vl53l0xSensor.h"

Vl53l0xSensor::Vl53l0xSensor(uint16_t timeoutMs, uint32_t timingBudgetUs,
                             uint32_t continuousPeriodMs,
                             uint16_t maximumTrackedMm)
    : timeoutMs_(timeoutMs),
      timingBudgetUs_(timingBudgetUs),
      continuousPeriodMs_(continuousPeriodMs),
      maximumTrackedMm_(maximumTrackedMm) {}

bool Vl53l0xSensor::begin() {
  sensor_.setTimeout(timeoutMs_);
  initialized_ = sensor_.init();
  if (!initialized_) {
    return false;
  }

  sensor_.setMeasurementTimingBudget(timingBudgetUs_);
  sensor_.startContinuous(continuousPeriodMs_);
  return true;
}

TofReading Vl53l0xSensor::read() {
  if (!initialized_) {
    return {TofReadingStatus::Invalid, 0};
  }

  const uint16_t distanceMm = sensor_.readRangeContinuousMillimeters();
  const bool timedOut = sensor_.timeoutOccurred();

  // On this breadboard, open space beyond the useful 500 mm control range
  // commonly produces a timeout or an out-of-range value. That is a healthy
  // "no nearby target" condition rather than a proximity measurement error.
  if (timedOut || distanceMm > maximumTrackedMm_) {
    return {TofReadingStatus::ClearOutOfRange, distanceMm};
  }
  if (distanceMm == 0) {
    return {TofReadingStatus::Invalid, distanceMm};
  }
  return {TofReadingStatus::Target, distanceMm};
}

bool Vl53l0xSensor::isInitialized() const { return initialized_; }

