#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

#include "project_config.h"
#include "secrets.h"

namespace {
struct Actuator { const char *id; uint8_t gpio; bool state; };

Actuator actuators[] = {
  {"extractor_1", GPIO_EXTRACTOR_1, false}, {"extractor_2", GPIO_EXTRACTOR_2, false},
  {"ventilador_1", GPIO_VENTILADOR_1, false}, {"ventilador_2", GPIO_VENTILADOR_2, false},
  {"bomba_agua", GPIO_BOMBA_AGUA, false},  {"lampara_uv", GPIO_LAMPARA_UV, false}
};

Preferences preferences;
WiFiClientSecure mqttTls;
PubSubClient mqtt(mqttTls);
WiFiManager wifiManager;
String deviceId, macAddress, claimCode, bootId;
uint32_t sequenceNumber = 0;
uint32_t restartCount = 0;
uint32_t lastTelemetry = 0, lastMqttAttempt = 0, lastEnrollAttempt = 0;
uint32_t wifiResetPressedAt = 0;
uint32_t lastWifiPortalAttempt = 0;
bool enrolled = false;
bool remoteResetRequested = false;

String topic(const String &suffix) { return String(MQTT_ROOT) + "/" + deviceId + "/" + suffix; }

String randomToken(size_t length) {
  static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  String out; out.reserve(length);
  for (size_t i = 0; i < length; ++i) out += alphabet[esp_random() % (sizeof(alphabet) - 1)];
  return out;
}

String randomHex(size_t length) {
  static const char hex[] = "0123456789abcdef";
  String out; out.reserve(length);
  for (size_t i = 0; i < length; ++i) out += hex[esp_random() & 0x0F];
  return out;
}

const char *resetReason() {
  if (remoteResetRequested || preferences.getBool("remote_reset", false)) return "remote";
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "power_on_or_en";
    case ESP_RST_EXT: return "external";
    case ESP_RST_SW: return "software";
    case ESP_RST_PANIC: return "panic";
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    case ESP_RST_DEEPSLEEP: return "deep_sleep";
    default: return "unknown";
  }
}

void writeRelay(Actuator &a) {
  digitalWrite(a.gpio, RELAY_ACTIVE_LOW ? !a.state : a.state);
}

void saveActuator(const Actuator &a) { preferences.putBool(a.id, a.state); }

void initializeIdentity() {
  macAddress = WiFi.macAddress(); macAddress.toUpperCase();
  String compact = macAddress; compact.replace(":", "");
  deviceId = "ESP32-" + compact.substring(compact.length() - 4);
  claimCode = preferences.getString("claim_code", "");
  if (claimCode.isEmpty()) { claimCode = randomToken(16); preferences.putString("claim_code", claimCode); }
  bootId = randomHex(8) + "-" + randomHex(4) + "-4" + randomHex(3) + "-a" + randomHex(3) + "-" + randomHex(12);
  restartCount = preferences.getULong("restart_count", 0) + 1;
  preferences.putULong("restart_count", restartCount);
}

bool connectWifiPortal() {
  lastWifiPortalAttempt = millis();
  const String portalName = "Huerta-H2-" + deviceId.substring(deviceId.length() - 4);
  wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SECONDS);
  wifiManager.setConnectTimeout(30);
  wifiManager.setConnectRetries(3);
  wifiManager.setWiFiAutoReconnect(true);
  wifiManager.setTitle("Huerta Hidroponica Inteligente");
  Serial.printf("[WiFiManager] Red de configuración: %s\n", portalName.c_str());
  const bool connected = wifiManager.autoConnect(portalName.c_str());
  Serial.printf("[WiFiManager] %s | IP %s\n", connected ? "Conectado" : "Sin conexión", WiFi.localIP().toString().c_str());
  return connected;
}

void handleWifiResetButton() {
  if (digitalRead(WIFI_RESET_BUTTON_GPIO) == LOW) {
    if (wifiResetPressedAt == 0) wifiResetPressedAt = millis();
    if (millis() - wifiResetPressedAt >= WIFI_RESET_HOLD_MS) {
      Serial.println("[WiFiManager] Borrando Wi-Fi y reiniciando...");
      wifiManager.resetSettings(); delay(300); ESP.restart();
    }
  } else wifiResetPressedAt = 0;
}

void publishAck(const Actuator &a) {
  JsonDocument doc; doc["actuator"] = a.id; doc["state"] = a.state;
  doc["gpio"] = a.gpio; doc["device_id"] = deviceId;
  char payload[192]; serializeJson(doc, payload, sizeof(payload));
  mqtt.publish(topic("ack").c_str(), payload, false);
}

void mqttCallback(char *rawTopic, byte *payload, unsigned int length) {
  String incomingTopic(rawTopic), body;
  body.reserve(length); for (unsigned int i = 0; i < length; ++i) body += char(payload[i]);
  body.trim();
  if (incomingTopic == topic("command/reset") && (body.equalsIgnoreCase("true") || body == "1")) {
    Serial.println("[MQTT] Reset remoto confirmado");
    preferences.putBool("remote_reset", true);
    mqtt.publish(topic("ack").c_str(), "{\"reset\":true}", false);
    delay(250); ESP.restart();
  }
  const String prefix = topic("command/actuator/");
  if (!incomingTopic.startsWith(prefix)) return;
  const String id = incomingTopic.substring(prefix.length());
  for (auto &a : actuators) if (id == a.id) {
    if (!body.equalsIgnoreCase("ON") && !body.equalsIgnoreCase("OFF")) return;
    a.state = body.equalsIgnoreCase("ON"); writeRelay(a); saveActuator(a); publishAck(a);
    Serial.printf("[GPIO] %s GPIO %u = %s\n", a.id, a.gpio, a.state ? "ON" : "OFF");
    return;
  }
}

bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED || mqtt.connected()) return mqtt.connected();
  if (millis() - lastMqttAttempt < MQTT_RETRY_MS) return false;
  lastMqttAttempt = millis();
  const String clientId = deviceId + "-" + String(uint32_t(esp_random()), HEX);
  const String statusTopic = topic("status");
  if (!mqtt.connect(clientId.c_str(), statusTopic.c_str(), 0, true, "offline")) {
    Serial.printf("[MQTT] Error %d\n", mqtt.state()); return false;
  }
  mqtt.subscribe(topic("command/#").c_str());
  mqtt.publish(statusTopic.c_str(), "online", true);
  Serial.println("[MQTT] Conectado y suscripto a comandos");
  return true;
}

void addCommonPayload(JsonDocument &doc) {
  doc["device_id"] = deviceId; doc["mac"] = macAddress; doc["claim_code"] = claimCode;
  doc["firmware"] = FIRMWARE_VERSION; doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID(); doc["chip_model"] = ESP.getChipModel(); doc["flash_size"] = ESP.getFlashChipSize();
}

int postJson(const String &json, String &response) {
  WiFiClientSecure https; https.setInsecure(); // Prototipo: ver README para CA raíz en producción.
  HTTPClient http;
  if (!http.begin(https, ENROLL_ENDPOINT)) return -1;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-enrollment-key", DEVICE_ENROLLMENT_KEY2);
  const int status = http.POST(json); response = status > 0 ? http.getString() : http.errorToString(status);
  http.end(); return status;
}

bool enroll() {
  if (WiFi.status() != WL_CONNECTED) return false;
  JsonDocument doc; addCommonPayload(doc);
  String json, response; serializeJson(doc, json);
  const int status = postJson(json, response);
  Serial.printf("[Enroll] HTTP %d %s\n", status, response.c_str());
  return status == 200 || status == 201;
}

float noise(float amplitude) { return (int32_t(esp_random() % 2001) - 1000) * amplitude / 1000.0f; }
float wave(float center, float amplitude, float periodSeconds, float phase = 0) {
  return center + amplitude * sinf((millis() / 1000.0f + phase) * TWO_PI / periodSeconds);
}
float clampf(float value, float low, float high) { return value < low ? low : (value > high ? high : value); }

void makeTelemetry(JsonDocument &doc) {
  const float air = clampf(wave(25.4f, 2.3f, 420.0f) + noise(.18f), 18, 35);
  const float humidity = clampf(wave(63.0f, 7.5f, 510.0f, 80) + noise(.6f), 40, 90);
  const float water = clampf(wave(22.2f, 1.1f, 700.0f, 130) + noise(.08f), 17, 29);
  const float ph = clampf(wave(6.25f, .18f, 600.0f, 40) + noise(.025f), 5.5f, 7.0f);
  const float tds = clampf(wave(820.0f, 85.0f, 650.0f, 110) + noise(8.0f), 550, 1100);
  doc["action"] = "telemetry"; addCommonPayload(doc);
  doc["boot_id"] = bootId; doc["sequence"] = sequenceNumber++; doc["uptime_ms"] = millis();
  doc["reset_reason"] = resetReason(); doc["restart_count"] = restartCount;
  doc["air_temperature"] = air; doc["humidity"] = humidity; doc["water_temperature"] = water;
  doc["ph"] = ph; doc["tds_ppm"] = tds; doc["cpu_temperature"] = temperatureRead();
  doc["wifi_rssi"] = WiFi.RSSI(); doc["mqtt_connected"] = mqtt.connected(); doc["simulated"] = true;
  JsonObject states = doc["actuators"].to<JsonObject>();
  for (const auto &a : actuators) states[a.id] = a.state;
}

void sendTelemetry() {
  JsonDocument doc; makeTelemetry(doc);
  String json; serializeJson(doc, json);
  if (mqtt.connected()) mqtt.publish(topic("telemetry").c_str(), json.c_str(), false);
  String response; const int status = postJson(json, response);
  Serial.printf("[Telemetry #%lu] HTTP %d %s\n", static_cast<unsigned long>(sequenceNumber - 1), status, response.c_str());
  if (status == 409 && response.indexOf("Enrolar primero") >= 0) enrolled = false;
  if (preferences.getBool("remote_reset", false)) preferences.putBool("remote_reset", false);
}
} // namespace

void setup() {
  Serial.begin(115200); delay(300);
  preferences.begin("huerta-h2", false);
  pinMode(WIFI_RESET_BUTTON_GPIO, INPUT_PULLUP);
  for (auto &a : actuators) { a.state = preferences.getBool(a.id, false); pinMode(a.gpio, OUTPUT); writeRelay(a); }
  WiFi.mode(WIFI_STA);
  initializeIdentity();
  connectWifiPortal();
  mqttTls.setInsecure(); mqtt.setServer(MQTT_HOST, MQTT_PORT); mqtt.setCallback(mqttCallback); mqtt.setBufferSize(2048); mqtt.setKeepAlive(30);
  Serial.printf("\nHuerta H2 | %s | %s | Claim: %s | Firmware %s\n", deviceId.c_str(), macAddress.c_str(), claimCode.c_str(), FIRMWARE_VERSION);
}

void loop() {
  handleWifiResetButton();
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiPortalAttempt >= WIFI_PORTAL_RETRY_MS) connectWifiPortal();
  if (WiFi.status() == WL_CONNECTED) {
    if (!enrolled && (lastEnrollAttempt == 0 || millis() - lastEnrollAttempt >= ENROLL_RETRY_MS)) { lastEnrollAttempt = millis(); enrolled = enroll(); }
    connectMqtt(); mqtt.loop();
    if (enrolled && (lastTelemetry == 0 || millis() - lastTelemetry >= TELEMETRY_INTERVAL_MS)) { lastTelemetry = millis(); sendTelemetry(); }
  }
  delay(5);
}
