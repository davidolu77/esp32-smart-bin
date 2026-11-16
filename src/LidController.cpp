#include "LidController.h"

LidController::LidController(uint8_t servoPin, uint8_t ledPin,
                             int closedAngle, int openAngle,
                             int minimumPulseUs, int maximumPulseUs)
    : servoPin_(servoPin),
      ledPin_(ledPin),
      closedAngle_(closedAngle),
      openAngle_(openAngle),
      minimumPulseUs_(minimumPulseUs),
      maximumPulseUs_(maximumPulseUs) {}

void LidController::begin() {
  pinMode(ledPin_, OUTPUT);
  digitalWrite(ledPin_, LOW);

  servo_.setPeriodHertz(50);
  servo_.attach(servoPin_, minimumPulseUs_, maximumPulseUs_);
  servo_.write(closedAngle_);
  open_ = false;
  Serial.println("[SERVO] Lid initialized CLOSED");
}

void LidController::open(const char* reason) {
  if (open_) {
    return;
  }

  servo_.write(openAngle_);
  digitalWrite(ledPin_, HIGH);
  open_ = true;
  Serial.printf("[SERVO] OPEN (%s)\n", reason);
}

void LidController::close(const char* reason) {
  if (!open_) {
    return;
  }

  servo_.write(closedAngle_);
  digitalWrite(ledPin_, LOW);
  open_ = false;
  Serial.printf("[SERVO] CLOSED (%s)\n", reason);
}

bool LidController::isOpen() const { return open_; }

const char* LidController::stateName() const {
  return open_ ? "OPEN" : "CLOSED";
}


