#include "DebouncedButton.h"

DebouncedButton::DebouncedButton(uint8_t pin, uint32_t debounceMs)
    : pin_(pin), debounceMs_(debounceMs) {}

void DebouncedButton::begin() {
  pinMode(pin_, INPUT_PULLUP);
  const bool initialPressed = digitalRead(pin_) == LOW;
  stablePressed_ = initialPressed;
  lastRawPressed_ = initialPressed;
}

bool DebouncedButton::pressed(uint32_t nowMs) {
  const bool rawPressed = digitalRead(pin_) == LOW;

  if (rawPressed != lastRawPressed_) {
    lastRawPressed_ = rawPressed;
    lastRawChangeMs_ = nowMs;
  }

  if (rawPressed != stablePressed_ &&
      nowMs - lastRawChangeMs_ >= debounceMs_) {
    stablePressed_ = rawPressed;
    return stablePressed_;
  }

  return false;
}


