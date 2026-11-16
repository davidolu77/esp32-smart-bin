#include "NetworkManager.h"

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#include <cstring>

#include "Config.h"

namespace {
constexpr char apiKeyHeader[] = "X-API-Key";

bool isPlaceholder(const char* value, const char* placeholder) {
  return value == nullptr || value[0] == '\0' || strcmp(value, placeholder) == 0;
}

const char* tofStateName(TofReadingStatus status) {
  switch (status) {
    case TofReadingStatus::Target:
      return "target";
    case TofReadingStatus::ClearOutOfRange:
      return "clear";
    case TofReadingStatus::Invalid:
    default:
      return "invalid";
  }
}
}  // namespace

NetworkManager::NetworkManager(uint16_t httpPort) : server_(httpPort) {}

void NetworkManager::begin(const char* hostname, const char* ssid,
                           const char* password, const char* apiToken,
                           uint32_t reconnectIntervalMs,
                           SnapshotProvider snapshotProvider,
                           NetworkCommandHandler commandHandler) {
  hostname_ = hostname;
  ssid_ = ssid;
  password_ = password;
  apiToken_ = apiToken;
  reconnectIntervalMs_ = reconnectIntervalMs;
  snapshotProvider_ = snapshotProvider;
  commandHandler_ = commandHandler;

  configurationReady_ =
      !isPlaceholder(ssid_, "your_wifi_name") &&
      !isPlaceholder(password_, "your_wifi_password");
  apiTokenReady_ =
      !isPlaceholder(apiToken_, "replace-with-a-long-random-token") &&
      !isPlaceholder(apiToken_, "replace-me");

  if (!configurationReady_) {
    Serial.println(
        "[WIFI] Disabled: fill in include/secrets.h before uploading");
    return;
  }

  if (!apiTokenReady_) {
    Serial.println(
        "[API] State-changing endpoints disabled: configure API token");
  }

  const char* headers[] = {apiKeyHeader};
  server_.collectHeaders(headers, 1);
  registerRoutes();
  configureOta();

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(hostname_);
  WiFi.setAutoReconnect(false);
  startWifiConnection(millis());
}

void NetworkManager::registerRoutes() {
  server_.on("/health", HTTP_GET, [this]() { handleHealth(); });
  server_.on("/status", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/open", HTTP_POST,
             [this]() { handleCommand(NetworkCommand::Open, "OPEN"); });
  server_.on("/close", HTTP_POST,
             [this]() { handleCommand(NetworkCommand::Close, "CLOSE"); });
  server_.on("/hold-open", HTTP_POST, [this]() {
    handleCommand(NetworkCommand::HoldOpen, "HOLD_OPEN");
  });
  server_.on("/hold-closed", HTTP_POST, [this]() {
    handleCommand(NetworkCommand::HoldClosed, "HOLD_CLOSED");
  });
  server_.on("/auto", HTTP_POST,
             [this]() { handleCommand(NetworkCommand::Auto, "AUTO"); });
  server_.onNotFound([this]() {
    sendJson(404, "{\"ok\":false,\"error\":\"not_found\"}");
  });
}

void NetworkManager::configureOta() {
  if (!apiTokenReady_) {
    otaConfigured_ = false;
    Serial.println("[OTA] Disabled until SMART_BIN_API_TOKEN is configured");
    return;
  }

  ArduinoOTA.setHostname(hostname_);
  ArduinoOTA.setPassword(apiToken_);
  ArduinoOTA.onStart([this]() {
    lastOtaProgressPercent_ = 255;
    Serial.println("[OTA] Update started");
  });
  ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
    const uint8_t percent =
        total == 0 ? 0 : static_cast<uint8_t>((progress * 100U) / total);
    if (percent != lastOtaProgressPercent_ && percent % 10 == 0) {
      lastOtaProgressPercent_ = percent;
      Serial.printf("[OTA] Progress: %u%%\n", percent);
    }
  });
  ArduinoOTA.onEnd([]() { Serial.println("[OTA] Update complete; rebooting"); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[OTA] Error %u: ", error);
    switch (error) {
      case OTA_AUTH_ERROR:
        Serial.println("authentication failed");
        break;
      case OTA_BEGIN_ERROR:
        Serial.println("begin failed");
        break;
      case OTA_CONNECT_ERROR:
        Serial.println("connection failed");
        break;
      case OTA_RECEIVE_ERROR:
        Serial.println("receive failed");
        break;
      case OTA_END_ERROR:
        Serial.println("end failed");
        break;
      default:
        Serial.println("unknown error");
        break;
    }
  });
  otaConfigured_ = true;
}

void NetworkManager::startWifiConnection(uint32_t nowMs) {
  lastConnectAttemptMs_ = nowMs;
  Serial.println("[WIFI] Connecting to configured network...");
  WiFi.disconnect();
  WiFi.begin(ssid_, password_);
}

void NetworkManager::handleWifiConnected() {
  connected_ = true;
  Serial.printf("[WIFI] Connected; IP address: %s\n",
                WiFi.localIP().toString().c_str());

  if (MDNS.begin(hostname_)) {
    MDNS.addService("http", "tcp", 80);
    mdnsActive_ = true;
    Serial.printf("[MDNS] Ready: http://%s.local\n", hostname_);
  } else {
    Serial.println("[MDNS] Initialization failed; IP access still available");
  }

  if (!httpStarted_) {
    server_.begin();
    httpStarted_ = true;
    Serial.println("[HTTP] Server ready on port 80");
  }

  if (otaConfigured_) {
    ArduinoOTA.begin();
    Serial.printf("[OTA] Ready at %s.local\n", hostname_);
  }
}

void NetworkManager::handleWifiDisconnected() {
  connected_ = false;
  if (mdnsActive_) {
    MDNS.end();
    mdnsActive_ = false;
  }
  Serial.println("[WIFI] Disconnected; local bin control remains active");
}

void NetworkManager::update(uint32_t nowMs) {
  if (!configurationReady_) {
    return;
  }

  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiConnected && !connected_) {
    handleWifiConnected();
  } else if (!wifiConnected && connected_) {
    handleWifiDisconnected();
  }

  if (!wifiConnected) {
    if (nowMs - lastConnectAttemptMs_ >= reconnectIntervalMs_) {
      Serial.println("[WIFI] Reconnecting...");
      startWifiConnection(nowMs);
    }
    return;
  }

  server_.handleClient();
  ArduinoOTA.handle();
}

bool NetworkManager::authorizeStateChange() {
  if (!apiTokenReady_) {
    sendJson(503,
             "{\"ok\":false,\"error\":\"api_token_not_configured\"}");
    return false;
  }
  if (!server_.hasHeader(apiKeyHeader) ||
      server_.header(apiKeyHeader) != apiToken_) {
    sendJson(401, "{\"ok\":false,\"error\":\"unauthorized\"}");
    return false;
  }
  return true;
}

void NetworkManager::handleCommand(NetworkCommand command,
                                   const char* commandName) {
  if (!authorizeStateChange()) {
    return;
  }
  if (commandHandler_ == nullptr) {
    sendJson(503, "{\"ok\":false,\"error\":\"control_unavailable\"}");
    return;
  }

  Serial.printf("[API] %s command received\n", commandName);
  commandHandler_(command);
  String response = "{\"ok\":true,\"command\":\"";
  response += commandName;
  response += "\"}";
  sendJson(200, response);
}

void NetworkManager::handleHealth() {
  sendJson(200, "{\"ok\":true,\"device\":\"esp32-smart-bin\"}");
}

void NetworkManager::handleStatus() { sendJson(200, buildStatusJson()); }

String NetworkManager::buildStatusJson() const {
  if (snapshotProvider_ == nullptr) {
    return "{\"ok\":false,\"error\":\"status_unavailable\"}";
  }

  const SmartBinSnapshot state = snapshotProvider_();
  String json;
  json.reserve(300);
  json += "{\"device\":\"esp32-smart-bin\",\"firmware\":\"";
  json += NetworkConfig::firmwareVersion;
  json += "\",\"lid\":\"";
  json += state.lidOpen ? "open" : "closed";
  json += "\",\"mode\":\"";
  json += operatingModeName(state.mode);
  json += "\",\"tof_mm\":";
  if (state.tof.status == TofReadingStatus::Target) {
    json += state.tof.millimetres;
  } else {
    json += "null";
  }
  json += ",\"tof_state\":\"";
  json += tofStateName(state.tof.status);
  json += "\",\"fill_distance_cm\":";
  if (state.fillValid) {
    json += String(state.fillDistanceCm, 1);
  } else {
    json += "null";
  }
  json += ",\"fill_percent\":";
  if (state.fillValid) {
    json += String(state.fillPercent, 0);
  } else {
    json += "null";
  }
  json += ",\"fill_valid\":";
  json += state.fillValid ? "true" : "false";
  json += ",\"wifi_rssi\":";
  if (WiFi.status() == WL_CONNECTED) {
    json += WiFi.RSSI();
  } else {
    json += "null";
  }
  json += ",\"uptime_ms\":";
  json += state.uptimeMs;
  json += "}";
  return json;
}

void NetworkManager::sendJson(int statusCode, const String& body) {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(statusCode, "application/json", body);
}

bool NetworkManager::isConnected() const { return connected_; }

