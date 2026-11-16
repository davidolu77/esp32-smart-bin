#pragma once

#include <Arduino.h>

class DebouncedButton {
 public:
  DebouncedButton(uint8_t pin, uint32_t debounceMs);

  void begin();
  bool pressed(uint32_t nowMs);

 private:
  uint8_t pin_;
  uint32_t debounceMs_;
  bool stablePressed_ = false;
  bool lastRawPressed_ = false;
  uint32_t lastRawChangeMs_ = 0;
};


