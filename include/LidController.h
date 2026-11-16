#pragma once

#include <Arduino.h>
#include <ESP32Servo.h>

class LidController {
 public:
  LidController(uint8_t servoPin, uint8_t ledPin, int closedAngle,
                int openAngle, int minimumPulseUs, int maximumPulseUs);

  void begin();
  void open(const char* reason);
  void close(const char* reason);
  bool isOpen() const;
  const char* stateName() const;

 private:
  Servo servo_;
  uint8_t servoPin_;
  uint8_t ledPin_;
  int closedAngle_;
  int openAngle_;
  int minimumPulseUs_;
  int maximumPulseUs_;
  bool open_ = false;
};


