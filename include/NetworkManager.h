#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include "SmartBinState.h"

using SnapshotProvider = SmartBinSnapshot (*)();
using NetworkCommandHandler = void (*)(NetworkCommand command);

class NetworkManager {
 public:
  explicit NetworkManager(uint16_t httpPort);

  void begin(const char* hostname, const char* ssid, const char* password,
             const char* apiToken, uint32_t reconnectIntervalMs,
             SnapshotProvider snapshotProvider,
             NetworkCommandHandler commandHandler);
  void update(uint32_t nowMs);
  bool isConnected() const;

 private:
  void registerRoutes();
  void startWifiConnection(uint32_t nowMs);
  void handleWifiConnected();
  void handleWifiDisconnected();
  void configureOta();
  bool authorizeStateChange();
  void handleStatus();
  void handleHealth();
  void handleCommand(NetworkCommand command, const char* commandName);
  void sendJson(int statusCode, const String& body);
  String buildStatusJson() const;

  WebServer server_;
  const char* hostname_ = nullptr;
  const char* ssid_ = nullptr;
  const char* password_ = nullptr;
  const char* apiToken_ = nullptr;
  uint32_t reconnectIntervalMs_ = 0;
  uint32_t lastConnectAttemptMs_ = 0;
  SnapshotProvider snapshotProvider_ = nullptr;
  NetworkCommandHandler commandHandler_ = nullptr;
  bool configurationReady_ = false;
  bool apiTokenReady_ = false;
  bool connected_ = false;
  bool httpStarted_ = false;
  bool mdnsActive_ = false;
  bool otaConfigured_ = false;
  uint8_t lastOtaProgressPercent_ = 255;
};

