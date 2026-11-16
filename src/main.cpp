#include <Arduino.h>
#include <Wire.h>

#include "Config.h"
#include "DebouncedButton.h"
#include "Hcsr04Sensor.h"
#include "LidController.h"
#include "NetworkManager.h"
#include "SmartBinState.h"
#include "Vl53l0xSensor.h"
#include "secrets.h"

Vl53l0xSensor proximitySensor(Timing::tofTimeoutMs, Timing::tofTimingBudgetUs,
                              Timing::tofContinuousPeriodMs,
                              Proximity::tofMaximumTrackedMm);
Hcsr04Sensor fillSensor(Pins::ultrasonicTrigger, Pins::ultrasonicEcho,
                       Timing::ultrasonicTimeoutUs);
DebouncedButton manualButton(Pins::pushButton, Timing::buttonDebounceMs);
LidController lid(Pins::servoSignal, Pins::statusLed,
                  Lid::closedAngleDegrees, Lid::openAngleDegrees,
                  Lid::servoMinimumPulseUs, Lid::servoMaximumPulseUs);
NetworkManager networkManager(NetworkConfig::httpPort);

TofReading latestTofReading{TofReadingStatus::Invalid, 0};
DistanceReading latestFillReading{false, 0.0F};
OperatingMode operatingMode = OperatingMode::Auto;
uint32_t lastTofReadMs = 0;
uint32_t lastFillReadMs = 0;
uint32_t lastSerialReportMs = 0;
uint32_t triggerStartedMs = 0;
uint32_t clearStartedMs = 0;
bool triggerCandidate = false;
bool areaClear = false;
bool tofAvailable = false;
bool tofHealthKnown = false;
bool lastTofHealthy = false;
uint8_t consecutiveInvalidTofReadings = 0;
bool fillValidityKnown = false;
bool lastFillValid = false;

void resetAutomaticTimers() {
  triggerCandidate = false;
  triggerStartedMs = 0;
  areaClear = false;
  clearStartedMs = 0;
}

void logTofHealthChange(TofReadingStatus status) {
  const bool healthy = status != TofReadingStatus::Invalid;
  if (tofHealthKnown && healthy == lastTofHealthy) {
    return;
  }

  tofHealthKnown = true;
  lastTofHealthy = healthy;
  Serial.println(healthy ? "[TOF] Sensor healthy"
                         : "[TOF] Invalid measurement");
}

void logFillValidityChange(bool valid) {
  if (fillValidityKnown && valid == lastFillValid) {
    return;
  }

  fillValidityKnown = true;
  lastFillValid = valid;
  Serial.println(valid ? "[FILL] HC-SR04 measurements valid"
                       : "[FILL] Invalid HC-SR04 measurement/no echo");
}

void updateProximityState(uint32_t nowMs) {
  if (latestTofReading.status == TofReadingStatus::Invalid) {
    // A genuine malformed reading cannot open the lid or start a clear timer.
    triggerCandidate = false;
    triggerStartedMs = 0;
    return;
  }

  const bool targetDetected =
      latestTofReading.status == TofReadingStatus::Target;
  const bool targetInTriggerRange =
      targetDetected &&
      latestTofReading.millimetres <= Proximity::tofTriggerMm;
  const bool areaIsClear =
      latestTofReading.status == TofReadingStatus::ClearOutOfRange ||
      (targetDetected &&
       latestTofReading.millimetres >= Proximity::tofClearMm);

  if (!lid.isOpen()) {
    areaClear = false;
    clearStartedMs = 0;

    if (targetInTriggerRange) {
      if (!triggerCandidate) {
        triggerCandidate = true;
        triggerStartedMs = nowMs;
        Serial.printf("[TOF] Object detected at %u mm; confirming\n",
                      latestTofReading.millimetres);
      } else if (nowMs - triggerStartedMs >= Timing::triggerConfirmationMs) {
        Serial.println("[TOF] Object confirmed; opening lid");
        lid.open("ToF proximity trigger");
        triggerCandidate = false;
      }
    } else {
      triggerCandidate = false;
      triggerStartedMs = 0;
    }
    return;
  }

  triggerCandidate = false;
  triggerStartedMs = 0;

  // Hysteresis: >= clear threshold starts the timer; <= trigger threshold
  // cancels it. Readings between the thresholds retain the current state.
  if (!areaClear && areaIsClear) {
    areaClear = true;
    clearStartedMs = nowMs;
    if (latestTofReading.status == TofReadingStatus::ClearOutOfRange) {
      Serial.println(
          "[TOF] Area clear beyond 500 mm/no target; close timer started");
    } else {
      Serial.printf("[TOF] Area cleared at %u mm; close timer started\n",
                    latestTofReading.millimetres);
    }
  } else if (areaClear && targetInTriggerRange) {
    areaClear = false;
    clearStartedMs = 0;
    Serial.println("[TOF] Object returned; close timer cancelled");
  }

  if (areaClear && nowMs - clearStartedMs >= Timing::clearBeforeCloseMs) {
    lid.close("ToF area clear timeout");
    resetAutomaticTimers();
  }
}

void readTofWhenDue(uint32_t nowMs) {
  if (!tofAvailable || nowMs - lastTofReadMs < Timing::tofIntervalMs) {
    return;
  }

  lastTofReadMs = nowMs;
  const TofReading newReading = proximitySensor.read();
  if (newReading.status == TofReadingStatus::Invalid) {
    if (consecutiveInvalidTofReadings <
        Timing::tofInvalidConfirmationCount) {
      ++consecutiveInvalidTofReadings;
    }
    if (consecutiveInvalidTofReadings <
        Timing::tofInvalidConfirmationCount) {
      return;
    }
  } else {
    consecutiveInvalidTofReadings = 0;
  }

  latestTofReading = newReading;
  logTofHealthChange(latestTofReading.status);
  if (operatingMode == OperatingMode::Auto) {
    updateProximityState(nowMs);
  }
}

void readFillSensorWhenDue(uint32_t nowMs) {
  if (nowMs - lastFillReadMs < Timing::fillSensorIntervalMs) {
    return;
  }

  lastFillReadMs = nowMs;
  latestFillReading = fillSensor.read();
  logFillValidityChange(latestFillReading.valid);
}

float calculateFillPercent(float distanceCm) {
  const float spanCm = FillCalibration::BIN_EMPTY_DISTANCE_CM -
                       FillCalibration::BIN_FULL_DISTANCE_CM;
  const float percentage =
      (FillCalibration::BIN_EMPTY_DISTANCE_CM - distanceCm) * 100.0F / spanCm;
  return constrain(percentage, 0.0F, 100.0F);
}

SmartBinSnapshot provideSmartBinSnapshot() {
  return {
      lid.isOpen(),
      operatingMode,
      latestTofReading,
      latestFillReading.valid,
      latestFillReading.valid ? latestFillReading.centimetres : 0.0F,
      latestFillReading.valid
          ? calculateFillPercent(latestFillReading.centimetres)
          : 0.0F,
      millis(),
  };
}

void handleNetworkCommand(NetworkCommand command) {
  resetAutomaticTimers();
  switch (command) {
    case NetworkCommand::Open:
      operatingMode = OperatingMode::Auto;
      lid.open("HTTP API temporary open");
      Serial.println("[MODE] AUTO; temporary remote open");
      break;
    case NetworkCommand::Close:
      operatingMode = OperatingMode::Auto;
      lid.close("HTTP API temporary close");
      Serial.println("[MODE] AUTO; temporary remote close");
      break;
    case NetworkCommand::HoldOpen:
      operatingMode = OperatingMode::ManualOpen;
      lid.open("HTTP API hold open");
      Serial.println("[MODE] MANUAL_OPEN; ToF lid control paused");
      break;
    case NetworkCommand::HoldClosed:
      operatingMode = OperatingMode::ManualClosed;
      lid.close("HTTP API hold closed");
      Serial.println("[MODE] MANUAL_CLOSED; ToF lid control paused");
      break;
    case NetworkCommand::Auto:
      operatingMode = OperatingMode::Auto;
      Serial.println("[MODE] AUTO; ToF control restored");
      break;
  }
}

void reportStatusWhenDue(uint32_t nowMs) {
  if (nowMs - lastSerialReportMs < Timing::serialReportIntervalMs) {
    return;
  }

  lastSerialReportMs = nowMs;
  Serial.print("[STATUS] ToF: ");
  if (latestTofReading.status == TofReadingStatus::Target) {
    Serial.printf("%u mm", latestTofReading.millimetres);
  } else if (latestTofReading.status ==
             TofReadingStatus::ClearOutOfRange) {
    Serial.print("CLEAR (>500 mm/no target)");
  } else {
    Serial.print("INVALID");
  }

  Serial.print(" | Fill sensor: ");
  if (latestFillReading.valid) {
    const float fillPercent =
        calculateFillPercent(latestFillReading.centimetres);
    Serial.printf("%.1f cm | Fill: %.0f%%", latestFillReading.centimetres,
                  fillPercent);
  } else {
    Serial.print("INVALID/NO ECHO | Fill: UNKNOWN");
  }
  Serial.printf(" | Lid: %s | Mode: %s\n", lid.stateName(),
                operatingModeName(operatingMode));
}

void handleButton(uint32_t nowMs) {
  if (!manualButton.pressed(nowMs)) {
    return;
  }

  Serial.println("[BUTTON] Debounced press detected; toggling lid");
  resetAutomaticTimers();
  if (operatingMode != OperatingMode::Auto) {
    operatingMode = OperatingMode::Auto;
    Serial.println("[MODE] Physical button restored AUTO mode");
  }
  if (lid.isOpen()) {
    lid.close("manual button");
  } else {
    lid.open("manual button");
  }
}

void setup() {
  Serial.begin(115200);
  delay(250);  // Brief startup settling only; runtime logic is non-blocking.
  Serial.println("\nESP32 Smart Bin starting");
  Serial.printf("Firmware version: %s\n", NetworkConfig::firmwareVersion);

  fillSensor.begin();
  manualButton.begin();
  lid.begin();

  // Explicit ESP32 I2C wiring: Wire.begin(SDA, SCL) = GPIO 21, GPIO 22.
  Wire.begin(Pins::tofSda, Pins::tofScl);
  tofAvailable = proximitySensor.begin();
  if (tofAvailable) {
    Serial.println("[TOF] VL53L0X initialized on SDA 21 / SCL 22");
  } else {
    Serial.println(
        "[ERROR] VL53L0X initialization failed; proximity opening disabled");
  }

  Serial.printf(
      "ToF trigger <= %u mm for %lu ms; clear >= %u mm; track to %u mm\n",
      Proximity::tofTriggerMm,
      static_cast<unsigned long>(Timing::triggerConfirmationMs),
      Proximity::tofClearMm,
      Proximity::tofMaximumTrackedMm);
  Serial.printf("Fill calibration: empty %.1f cm, full %.1f cm (temporary)\n",
                FillCalibration::BIN_EMPTY_DISTANCE_CM,
                FillCalibration::BIN_FULL_DISTANCE_CM);

  networkManager.begin(
      NetworkConfig::hostname, WIFI_SSID, WIFI_PASSWORD, SMART_BIN_API_TOKEN,
      NetworkConfig::reconnectIntervalMs, provideSmartBinSnapshot,
      handleNetworkCommand);
}

void loop() {
  const uint32_t nowMs = millis();
  handleButton(nowMs);
  readTofWhenDue(nowMs);
  readFillSensorWhenDue(nowMs);
  reportStatusWhenDue(nowMs);
  networkManager.update(nowMs);
}

