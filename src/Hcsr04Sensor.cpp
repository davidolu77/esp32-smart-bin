#include "Hcsr04Sensor.h"

namespace {
constexpr float speedOfSoundCmPerMicrosecond = 0.0343F;
constexpr float minimumUsefulDistanceCm = 2.0F;
constexpr float maximumUsefulDistanceCm = 400.0F;
}  // namespace

Hcsr04Sensor::Hcsr04Sensor(uint8_t triggerPin, uint8_t echoPin,
                         uint32_t timeoutUs)
    : triggerPin_(triggerPin), echoPin_(echoPin), timeoutUs_(timeoutUs) {}

void Hcsr04Sensor::begin() {
  pinMode(triggerPin_, OUTPUT);
  pinMode(echoPin_, INPUT);
  digitalWrite(triggerPin_, LOW);
}

DistanceReading Hcsr04Sensor::read() {
  digitalWrite(triggerPin_, LOW);
  delayMicroseconds(2);
  digitalWrite(triggerPin_, HIGH);
  delayMicroseconds(10);
  digitalWrite(triggerPin_, LOW);

  const unsigned long echoDurationUs = pulseIn(echoPin_, HIGH, timeoutUs_);
  if (echoDurationUs == 0) {
    return {false, 0.0F};
  }

  const float distanceCm =
      (static_cast<float>(echoDurationUs) * speedOfSoundCmPerMicrosecond) /
      2.0F;
  const bool plausible = distanceCm >= minimumUsefulDistanceCm &&
                         distanceCm <= maximumUsefulDistanceCm;
  return {plausible, distanceCm};
}


