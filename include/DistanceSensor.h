#pragma once

struct DistanceReading {
  bool valid;
  float centimetres;
};

// A small interface keeps the control logic independent of the sensor type.
class DistanceSensor {
 public:
  virtual ~DistanceSensor() = default;
  virtual void begin() = 0;
  virtual DistanceReading read() = 0;
};


