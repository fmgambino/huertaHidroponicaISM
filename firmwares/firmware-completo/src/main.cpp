#include <Arduino.h>
#include <ArduinoJson.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>
#include <esp_netif.h>
#include <esp_system.h>

#include "pins.h"
#include "secrets.h"

#ifndef DEVICE_ENROLLMENT_KEY
#define DEVICE_ENROLLMENT_KEY "REEMPLAZAR_CON_CLAVE_DE_FABRICACION"
#endif

namespace {
constexpr char AP_NAME[] = "Huerta Hidroponica IoT ISM";
constexpr char MDNS_NAME[] = "huertaiot";
constexpr uint32_t SAMPLE_INTERVAL_MS = 5000;
constexpr uint32_t PUBLISH_INTERVAL_MS = 5000;
constexpr uint32_t CLOUD_INTERVAL_MS = 10000;
constexpr char FIRMWARE_VERSION[] = "2.15.4";
constexpr bool SIMULATE_SENSORS = false;
bool inventoryRegistered = false;
uint32_t lastEnroll = 0;
uint32_t lastMqttAttempt = 0;
uint32_t lastDnsAttempt = 0;
uint32_t lastWifiReconnect = 0;
uint8_t networkFailures = 0;
IPAddress mqttAddress;

struct Telemetry {
  float airTemp = NAN;
  float humidity = NAN;
  float waterTemp = NAN;
  float ph = NAN;
  float tds = NAN;
  uint32_t sequence = 0;
};

struct Output {
  const char *name;
  const char *nvsKey;
  uint8_t pin;
  bool state;
};

struct AutomationConfig {
  bool ventilationEnabled = true;
  bool dosingEnabled = true;
  float phMin = 5.5f;
  float phMax = 6.5f;
  float tempMin = 22.0f;
  float tempMax = 28.0f;
  float humidityMin = 50.0f;
  float humidityMax = 75.0f;
  uint32_t doseMs = 3000;
  uint32_t cooldownMs = 60000;
} automation;

struct PendingEvent {
  bool pending = false;
  String actuator;
  bool state = false;
  String source;
  String reason;
};
PendingEvent eventQueue[16];
uint8_t eventHead=0,eventTail=0;

Output outputs[] = {
  {"extractor_1", "out_ex1", PIN_EXTRACTOR_1, false},
  {"extractor_2", "out_ex2", PIN_EXTRACTOR_2, false},
  {"ventilador_1", "out_fan1", PIN_FAN_1, false},
  {"ventilador_2", "out_fan2", PIN_FAN_2, false},
  {"bomba_agua", "out_water", PIN_PUMP, false},
  {"lampara_uv", "out_uv", PIN_UV, false},
  {"bomba_nutriente_a", "out_nutra", PIN_NUTRIENT_A, false},
  {"bomba_nutriente_b", "out_nutrb", PIN_NUTRIENT_B, false},
};

DHT dht(PIN_DHT22, DHT22);
OneWire oneWire(PIN_DS18B20);
DallasTemperature waterSensor(&oneWire);
WiFiClient wifiClient;
WiFiClientSecure supabaseClient;
PubSubClient mqtt(wifiClient);
WebServer web(80);
Preferences preferences;
Telemetry telemetry;
String deviceId;
String topicBase;
String claimCode;
String serialNumber;
String bootId;
String resetReason;
uint32_t lastSample = 0;
uint32_t lastPublish = 0;
uint32_t lastCloudPublish = 0;
uint32_t restartCount = 0;
uint32_t doseStopAt = 0;
uint32_t lastDoseAt = 0;
const char *activeDosePump = nullptr;
bool ventilationDemand = false;
bool cloudEventTurn = false;

bool configureFallbackDns() {
  esp_netif_t *station = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (!station) return false;
  esp_netif_dns_info_t dns{};
  dns.ip.type = ESP_IPADDR_TYPE_V4;
  IP4_ADDR(&dns.ip.u_addr.ip4, 1, 1, 1, 1);
  const esp_err_t primary = esp_netif_set_dns_info(station, ESP_NETIF_DNS_MAIN, &dns);
  IP4_ADDR(&dns.ip.u_addr.ip4, 8, 8, 8, 8);
  const esp_err_t backup = esp_netif_set_dns_info(station, ESP_NETIF_DNS_BACKUP, &dns);
  Serial.printf("[DNS] fallback sin alterar DHCP: principal=%s respaldo=%s\n",
                esp_err_to_name(primary), esp_err_to_name(backup));
  return primary == ESP_OK;
}

bool resolveHost(const char *host, IPAddress &resolved) {
  const int result = WiFi.hostByName(host, resolved);
  Serial.printf("[DNS] %s -> %s (%s)\n", host,
                result == 1 ? resolved.toString().c_str() : "sin resolver",
                result == 1 ? "OK" : "ERROR");
  return result == 1;
}

bool ensureDns(const char *host, IPAddress &resolved, bool force = false) {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  const uint32_t now = millis();
  if (!force && resolved != IPAddress() && now - lastDnsAttempt < 300000) return true;
  if (!force && resolved != IPAddress() && now - lastDnsAttempt < 5000) return true;
  lastDnsAttempt = now;
  Serial.printf("[RED] IP=%s Gateway=%s Mascara=%s DNS1=%s DNS2=%s RSSI=%d dBm\n",
                WiFi.localIP().toString().c_str(), WiFi.gatewayIP().toString().c_str(),
                WiFi.subnetMask().toString().c_str(), WiFi.dnsIP(0).toString().c_str(),
                WiFi.dnsIP(1).toString().c_str(), WiFi.RSSI());
  IPAddress candidate;
  if (resolveHost(host, candidate)) {
    resolved = candidate;
    networkFailures = 0;
    return true;
  }
  ++networkFailures;
  if (networkFailures >= 2) {
    configureFallbackDns();
    delay(50);
    if (resolveHost(host, candidate)) {
      resolved = candidate;
      networkFailures = 0;
      return true;
    }
  }
  if (resolved != IPAddress()) {
    Serial.printf("[DNS] Fallo temporal; se reutiliza IP conocida %s para %s\n",
                  resolved.toString().c_str(), host);
    return true;
  }
  Serial.println("[DNS] Sin resolucion ni IP en cache; se reintentara automaticamente.");
  return false;
}

bool writeOutput(Output &output, bool enabled) {
  const bool changed = output.state != enabled;
  output.state = enabled;
  digitalWrite(output.pin, RELAY_ACTIVE_LOW ? !enabled : enabled);
  if (changed) preferences.putBool(output.nvsKey, enabled);
  return changed;
}

Output *findOutput(const char *name) {
  for (auto &output : outputs) if (strcmp(output.name, name) == 0) return &output;
  return nullptr;
}

void queueActuatorEvent(const char *name, bool state, const char *source, const String &reason) {
  JsonDocument event;
  event["actuator"] = name;event["state"] = state;event["source"] = source;event["reason"] = reason;
  event["air_temperature"] = telemetry.airTemp;event["humidity"] = telemetry.humidity;event["ph"] = telemetry.ph;
  String payload;serializeJson(event,payload);
  if (mqtt.connected()) mqtt.publish((topicBase + "/events").c_str(), payload.c_str(), false);
  Serial.printf("[AUTOMATIZACION] %s=%s origen=%s motivo=%s\n",name,state?"ON":"OFF",source,reason.c_str());
  // Los comandos manuales ya son registrados por la PWA al recibir el ACK.
  if(strcmp(source,"automatic")!=0)return;
  const uint8_t next=(eventHead+1)%16;if(next==eventTail){Serial.println("[EVENTO] cola llena; se descarta el más antiguo");eventTail=(eventTail+1)%16;}
  PendingEvent &pending=eventQueue[eventHead];pending.pending=true;pending.actuator=name;pending.state=state;pending.source=source;pending.reason=reason;eventHead=next;
}

void setAutomatedOutput(const char *name, bool enabled, const String &reason) {
  Output *output=findOutput(name);if(output&&writeOutput(*output,enabled))queueActuatorEvent(name,enabled,"automatic",reason);
}

void saveAutomationConfig() {
  preferences.putBool("vent_en",automation.ventilationEnabled);preferences.putBool("dose_en",automation.dosingEnabled);preferences.putFloat("ph_min",automation.phMin);preferences.putFloat("ph_max",automation.phMax);
  preferences.putFloat("t_min",automation.tempMin);preferences.putFloat("t_max",automation.tempMax);preferences.putFloat("h_min",automation.humidityMin);preferences.putFloat("h_max",automation.humidityMax);
  preferences.putUInt("dose_ms",automation.doseMs);preferences.putUInt("cool_ms",automation.cooldownMs);
}

void updateAutomation() {
  const uint32_t now=millis();
  if(activeDosePump&&static_cast<int32_t>(now-doseStopAt)>=0){setAutomatedOutput(activeDosePump,false,"Fin del pulso de dosificación");activeDosePump=nullptr;lastDoseAt=now;}
  if(automation.ventilationEnabled&&!isnan(telemetry.airTemp)&&!isnan(telemetry.humidity)){
    const bool start=telemetry.airTemp>=automation.tempMax||telemetry.humidity>=automation.humidityMax;
    const bool stop=telemetry.airTemp<=automation.tempMin&&telemetry.humidity<=automation.humidityMin;
    if(!ventilationDemand&&start)ventilationDemand=true;else if(ventilationDemand&&stop)ventilationDemand=false;
    const String reason="T="+String(telemetry.airTemp,1)+"C H="+String(telemetry.humidity,1)+"%";
    for(const char *name:{"extractor_1","extractor_2","ventilador_1","ventilador_2"})setAutomatedOutput(name,ventilationDemand,reason);
  }
  else if(!automation.ventilationEnabled&&ventilationDemand){ventilationDemand=false;for(const char *name:{"extractor_1","extractor_2","ventilador_1","ventilador_2"})setAutomatedOutput(name,false,"Automatización de ventilación deshabilitada");}
  if(!automation.dosingEnabled||activeDosePump||isnan(telemetry.ph)||now-lastDoseAt<automation.cooldownMs)return;
  const char *pump=nullptr;String reason;
  if(telemetry.ph<automation.phMin){pump="bomba_nutriente_a";reason="pH bajo "+String(telemetry.ph,2)+" < "+String(automation.phMin,2);}
  else if(telemetry.ph>automation.phMax){pump="bomba_nutriente_b";reason="pH alto "+String(telemetry.ph,2)+" > "+String(automation.phMax,2);}
  if(pump){setAutomatedOutput(pump,true,reason);activeDosePump=pump;doseStopAt=now+automation.doseMs;}
}

String makeDeviceId() {
  const uint64_t mac = ESP.getEfuseMac();
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%04X", static_cast<uint16_t>(mac & 0xFFFF));
  return String("ESP32-") + suffix;
}

String makeClaimCode() {
  constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  String code = "H2-";
  for (uint8_t i = 0; i < 12; ++i) code += alphabet[esp_random() % (sizeof(alphabet) - 1)];
  return code;
}

float readPh() {
  const float voltage = analogReadMilliVolts(PIN_PH) / 1000.0f;
  // Calibración inicial genérica: pH 7 a 2.50 V y pendiente 0.18 V/pH.
  return 7.0f + ((2.50f - voltage) / 0.18f);
}

float readTds(float compensationTemp) {
  const float voltage = analogReadMilliVolts(PIN_TDS) / 1000.0f;
  const float temp = isnan(compensationTemp) ? 25.0f : compensationTemp;
  const float compensation = 1.0f + 0.02f * (temp - 25.0f);
  const float v = voltage / compensation;
  return (133.42f * v * v * v - 255.86f * v * v + 857.39f * v) * 0.5f;
}

void sampleSensors() {
  if (SIMULATE_SENSORS) {
    const float phase = millis() / 30000.0f;
    telemetry.airTemp = 24.0f + 2.0f * sinf(phase);
    telemetry.humidity = 60.0f + 5.0f * sinf(phase * 0.7f);
    telemetry.waterTemp = 22.0f + sinf(phase * 0.4f);
    telemetry.ph = 6.3f + 0.2f * sinf(phase * 0.3f);
    telemetry.tds = 720.0f + 30.0f * sinf(phase * 0.5f);
    telemetry.sequence++;
    return;
  }
  telemetry.airTemp = dht.readTemperature();
  telemetry.humidity = dht.readHumidity();
  waterSensor.requestTemperatures();
  telemetry.waterTemp = waterSensor.getTempCByIndex(0);
  telemetry.ph = readPh();
  telemetry.tds = readTds(telemetry.waterTemp);
  telemetry.sequence++;

  preferences.putFloat("air_temp", telemetry.airTemp);
  preferences.putFloat("humidity", telemetry.humidity);
  preferences.putFloat("water_temp", telemetry.waterTemp);
  preferences.putFloat("ph", telemetry.ph);
  preferences.putFloat("tds", telemetry.tds);
  preferences.putUInt("sequence", telemetry.sequence);
  updateAutomation();
}

String telemetryJson() {
  JsonDocument doc;
  doc["device_id"] = deviceId;
  doc["sequence"] = telemetry.sequence;
  doc["air_temperature"] = roundf(telemetry.airTemp * 100) / 100;
  doc["humidity"] = roundf(telemetry.humidity * 100) / 100;
  doc["water_temperature"] = roundf(telemetry.waterTemp * 100) / 100;
  doc["ph"] = roundf(telemetry.ph * 100) / 100;
  doc["tds_ppm"] = roundf(telemetry.tds * 100) / 100;
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["uptime_ms"] = millis();
  doc["simulated"] = SIMULATE_SENSORS;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["serial_number"] = serialNumber;
  doc["mac"] = WiFi.macAddress();
  doc["ip"] = WiFi.localIP().toString();
  doc["ssid"] = WiFi.SSID();
  doc["mqtt_connected"] = mqtt.connected();
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  doc["cpu_temperature"] = roundf(temperatureRead() * 100) / 100;
  doc["boot_id"] = bootId;
  doc["reset_reason"] = resetReason;
  doc["restart_count"] = restartCount;
  JsonObject actuatorJson = doc["actuators"].to<JsonObject>();
  for (const auto &output : outputs) actuatorJson[output.name] = output.state;
  String payload;
  serializeJson(doc, payload);
  return payload;
}

void publishState() {
  if (!mqtt.connected()) return;
  const String payload = telemetryJson();
  const bool sent = mqtt.publish((topicBase + "/telemetry").c_str(), payload.c_str(), false);
  Serial.printf("[TELEMETRIA] modo=%s envio=%s secuencia=%lu\n", SIMULATE_SENSORS ? "SIMULADO" : "REAL", sent ? "OK" : "ERROR", static_cast<unsigned long>(telemetry.sequence));
  mqtt.publish((topicBase + "/status").c_str(), "online", true);
  if (!sent) mqtt.disconnect();
}

void sendToSupabase() {
  if (!inventoryRegistered) return;
  if (WiFi.status() != WL_CONNECTED || String(SUPABASE_ANON_KEY).startsWith("REEMPLAZAR")) return;
  supabaseClient.stop();
  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(6000);
  const String endpoint = String(SUPABASE_URL) + "/functions/v1/device-enroll";
  if (!http.begin(supabaseClient, endpoint)) return;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("x-enrollment-key", DEVICE_ENROLLMENT_KEY);
  http.addHeader("Prefer", "return=minimal");
  JsonDocument reading;
  deserializeJson(reading, telemetryJson());
  reading["action"] = "telemetry";
  reading["claim_code"] = claimCode;
  String databasePayload;
  serializeJson(reading, databasePayload);
  const int status = http.POST(databasePayload);
  if (status < 200 || status >= 300) Serial.printf("Supabase HTTP %d: %s\n", status, http.getString().c_str());
  http.end();
}

void sendPendingEventToSupabase() {
  if (eventTail==eventHead || !inventoryRegistered || WiFi.status()!=WL_CONNECTED) return;
  PendingEvent &pendingEvent=eventQueue[eventTail];
  supabaseClient.stop();
  HTTPClient http;http.setConnectTimeout(6000);http.setTimeout(6000);
  const String endpoint=String(SUPABASE_URL)+"/functions/v1/device-enroll";
  if(!http.begin(supabaseClient,endpoint))return;
  http.addHeader("Content-Type","application/json");http.addHeader("apikey",SUPABASE_ANON_KEY);http.addHeader("x-enrollment-key",DEVICE_ENROLLMENT_KEY);
  JsonDocument doc;doc["action"]="actuator_event";doc["device_id"]=deviceId;doc["claim_code"]=claimCode;doc["mac"]=WiFi.macAddress();doc["actuator"]=pendingEvent.actuator;doc["state"]=pendingEvent.state;doc["source"]=pendingEvent.source;doc["reason"]=pendingEvent.reason;
  JsonObject snapshot=doc["sensor_snapshot"].to<JsonObject>();snapshot["air_temperature"]=telemetry.airTemp;snapshot["humidity"]=telemetry.humidity;snapshot["water_temperature"]=telemetry.waterTemp;snapshot["ph"]=telemetry.ph;
  String payload;serializeJson(doc,payload);const int status=http.POST(payload);if(status>=200&&status<300){pendingEvent.pending=false;eventTail=(eventTail+1)%16;}else Serial.printf("[EVENTO] Supabase HTTP %d: %s\n",status,http.getString().c_str());http.end();
}

void enrollDevice() {
  lastEnroll = millis();
  if (WiFi.status() != WL_CONNECTED || String(DEVICE_ENROLLMENT_KEY).startsWith("REEMPLAZAR")) {
    Serial.println("Inventario: configure DEVICE_ENROLLMENT_KEY para enrolamiento automatico");
    return;
  }
  IPAddress supabaseAddress;
  if (!ensureDns("jvrrtwlejbymxskijdhj.supabase.co", supabaseAddress)) {
    Serial.println("[INVENTARIO] pospuesto: DNS no disponible; reintento automatico");
    return;
  }
  JsonDocument doc;
  doc["device_id"] = deviceId;
  doc["claim_code"] = claimCode;
  doc["mac"] = WiFi.macAddress();
  doc["serial_number"] = serialNumber;
  doc["ip"] = WiFi.localIP().toString();
  doc["firmware"] = FIRMWARE_VERSION;
  doc["chip_model"] = ESP.getChipModel();
  doc["flash_size"] = ESP.getFlashChipSize();
  String payload;
  serializeJson(doc, payload);
  supabaseClient.stop();
  HTTPClient http;http.setConnectTimeout(6000);http.setTimeout(6000);
  const String endpoint = String(SUPABASE_URL) + "/functions/v1/device-enroll";
  if (!http.begin(supabaseClient, endpoint)) return;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", SUPABASE_ANON_KEY);
  http.addHeader("x-enrollment-key", DEVICE_ENROLLMENT_KEY);
  const int status = http.POST(payload);
  const String response = http.getString();
  Serial.printf("Inventario HTTP %d: %s\n", status, response.c_str());
  inventoryRegistered = status >= 200 && status < 300;
  if (status == 401) Serial.println("[INVENTARIO] Verificar DEVICE_ENROLLMENT_KEY en Supabase Secrets: debe coincidir EXACTAMENTE con include/secrets.h. Reintento en 60 s.");
  http.end();
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String body;
  body.reserve(length);
  for (unsigned int i = 0; i < length; ++i) body += static_cast<char>(payload[i]);
  const String incomingTopic(topic);
  Serial.printf("[MQTT RX] topic=%s payload=%s\n", topic, body.c_str());

  if (incomingTopic == topicBase + "/command/reset" && (body == "1" || body == "true")) {
    preferences.putBool("remote_reset", true);
    mqtt.publish((topicBase + "/events").c_str(), "remote_reset", false);
    delay(250);
    ESP.restart();
  }

  if(incomingTopic==topicBase+"/command/config"){
    JsonDocument config;const auto error=deserializeJson(config,body);if(error){Serial.printf("[CONFIG] JSON inválido: %s\n",error.c_str());return;}
    const float phMin=config["ph_min"]|automation.phMin,phMax=config["ph_max"]|automation.phMax,tempMin=config["temp_min"]|automation.tempMin,tempMax=config["temp_max"]|automation.tempMax,humidityMin=config["humidity_min"]|automation.humidityMin,humidityMax=config["humidity_max"]|automation.humidityMax;
    if(phMin>=phMax||tempMin>=tempMax||humidityMin>=humidityMax){Serial.println("[CONFIG] rechazada: cada MIN debe ser menor que MAX");return;}
    const bool legacyEnabled=config["enabled"]|true;automation.ventilationEnabled=config["ventilation_enabled"]|legacyEnabled;automation.dosingEnabled=config["dosing_enabled"]|legacyEnabled;automation.phMin=phMin;automation.phMax=phMax;automation.tempMin=tempMin;automation.tempMax=tempMax;automation.humidityMin=humidityMin;automation.humidityMax=humidityMax;
    automation.doseMs=constrain(config["dose_ms"]|automation.doseMs,500UL,15000UL);automation.cooldownMs=constrain(config["cooldown_ms"]|automation.cooldownMs,10000UL,3600000UL);saveAutomationConfig();
    JsonDocument ack;ack["type"]="config";ack["ok"]=true;String ackPayload;serializeJson(ack,ackPayload);mqtt.publish((topicBase+"/ack").c_str(),ackPayload.c_str(),false);Serial.println("[CONFIG] automatización actualizada y guardada");return;
  }

  const String prefix = topicBase + "/command/actuator/";
  if (!incomingTopic.startsWith(prefix)) return;
  const String name = incomingTopic.substring(prefix.length());
  body.trim();
  body.toUpperCase();
  if (body != "1" && body != "TRUE" && body != "ON" && body != "0" && body != "FALSE" && body != "OFF") {
    Serial.println("[ACTUADOR] Comando rechazado: valor invalido");
    return;
  }
  const bool enabled = body == "1" || body == "TRUE" || body == "ON";
  for (auto &output : outputs) {
    if (name == output.name) {
      const bool changed=writeOutput(output, enabled);
      Serial.printf("[ACTUADOR APLICADO] %s GPIO=%u estado=%s nivel=%d (sin realimentacion fisica)\n", output.name, output.pin, enabled ? "ON" : "OFF", digitalRead(output.pin));
      JsonDocument ack;
      ack["actuator"] = output.name;
      ack["state"] = enabled;
      String ackPayload;
      serializeJson(ack, ackPayload);
      mqtt.publish((topicBase + "/ack").c_str(), ackPayload.c_str(), false);
      if(changed)queueActuatorEvent(output.name,enabled,"manual","Comando desde PWA");
      publishState();
      return;
    }
  }
  Serial.printf("[ACTUADOR] ID desconocido: %s\n", name.c_str());
}

void connectMqtt() {
  if (mqtt.connected() || WiFi.status() != WL_CONNECTED) return;
  if (millis() - lastMqttAttempt < 5000) return;
  lastMqttAttempt = millis();
  if (!ensureDns(MQTT_HOST, mqttAddress)) return;
  mqtt.setServer(mqttAddress, MQTT_PORT);
  const String clientId = deviceId + "-" + String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);
  const String willTopic = topicBase + "/status";
  const bool connected = mqtt.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD,
                                      willTopic.c_str(), 1, true, "offline");
  if (connected) {
    const bool subscribed = mqtt.subscribe((topicBase + "/command/#").c_str());
    Serial.printf("[MQTT] conectado; suscripcion %s/command/#: %s\n", topicBase.c_str(), subscribed ? "OK" : "ERROR");
    publishState();
  } else Serial.printf("[MQTT] fallo conexion rc=%d\n", mqtt.state());
}

void setupWebServer() {
  web.on("/", HTTP_GET, [] {
    JsonDocument doc;
    doc["name"] = "Huerta Hidroponica IoT ISM";
    doc["device_id"] = deviceId;
    doc["mqtt_topic"] = topicBase;
    doc["telemetry"] = serialized(telemetryJson());
    String response;
    serializeJsonPretty(doc, response);
    web.send(200, "application/json; charset=utf-8", response);
  });
  web.on("/health", HTTP_GET, [] { web.send(200, "text/plain", "ok"); });
  web.onNotFound([] { web.send(404, "application/json", "{\"error\":\"not_found\"}"); });
  web.begin();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  deviceId = makeDeviceId();
  Serial.printf("[BOOT] Firmware %s | Sensores %s\n", FIRMWARE_VERSION, SIMULATE_SENSORS ? "SIMULADOS" : "REALES");
  topicBase = "huertaiot/" + deviceId;

  preferences.begin("huertaiot", false);
  bootId = String(esp_random(), HEX) + "-" + String(esp_random(), HEX);
  const auto cause = esp_reset_reason();
  const bool remote = preferences.getBool("remote_reset", false);
  if (remote) preferences.remove("remote_reset");
  if (remote && cause == ESP_RST_SW) resetReason = "remote";
  else switch (cause) {
    case ESP_RST_POWERON: resetReason = "power_on_or_en"; break;
    case ESP_RST_EXT: resetReason = "external"; break;
    case ESP_RST_SW: resetReason = "software"; break;
    case ESP_RST_PANIC: resetReason = "panic"; break;
    case ESP_RST_INT_WDT: case ESP_RST_TASK_WDT: case ESP_RST_WDT: resetReason = "watchdog"; break;
    case ESP_RST_BROWNOUT: resetReason = "brownout"; break;
    case ESP_RST_DEEPSLEEP: resetReason = "deep_sleep"; break;
    default: resetReason = "unknown";
  }
  Serial.printf("[REINICIO] boot=%s causa=%s\n", bootId.c_str(), resetReason.c_str());
  restartCount = preferences.getUInt("restarts", 0) + 1;
  preferences.putUInt("restarts", restartCount);
  telemetry.sequence = preferences.getUInt("sequence", 0);
  const bool legacyAuto=preferences.getBool("auto_en",true);automation.ventilationEnabled=preferences.getBool("vent_en",legacyAuto);automation.dosingEnabled=preferences.getBool("dose_en",legacyAuto);automation.phMin=preferences.getFloat("ph_min",5.5f);automation.phMax=preferences.getFloat("ph_max",6.5f);automation.tempMin=preferences.getFloat("t_min",22.0f);automation.tempMax=preferences.getFloat("t_max",28.0f);automation.humidityMin=preferences.getFloat("h_min",50.0f);automation.humidityMax=preferences.getFloat("h_max",75.0f);automation.doseMs=preferences.getUInt("dose_ms",3000);automation.cooldownMs=preferences.getUInt("cool_ms",60000);
  claimCode = preferences.getString("claim_code", "");
  if (claimCode.isEmpty()) {
    claimCode = makeClaimCode();
    preferences.putString("claim_code", claimCode);
  }

  pinMode(PIN_WIFI_LED, OUTPUT);
  digitalWrite(PIN_WIFI_LED, LOW);
  for (auto &output : outputs) {
    pinMode(output.pin, OUTPUT);
    const bool dosingPump=strcmp(output.name,"bomba_nutriente_a")==0||strcmp(output.name,"bomba_nutriente_b")==0;
    writeOutput(output,dosingPump?false:preferences.getBool(output.nvsKey,false));
  }

  analogReadResolution(12);
  dht.begin();
  waterSensor.begin();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  String compactMac = WiFi.macAddress();
  compactMac.replace(":", "");
  serialNumber = "ESP" + compactMac.substring(6);
  WiFiManager manager;
  manager.setConfigPortalTimeout(180);
  const bool connected = manager.autoConnect(AP_NAME);
  digitalWrite(PIN_WIFI_LED, connected ? HIGH : LOW);

  if (connected) {
    // Algunos routers anuncian WiFi antes de que su DNS esté utilizable.
    delay(1500);
    ensureDns(MQTT_HOST, mqttAddress, true);
    supabaseClient.setInsecure();  // Canal TLS dedicado; usar setCACert() en producción.
    supabaseClient.setHandshakeTimeout(8);
    if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
    setupWebServer();
    enrollDevice();
  }

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(2048);
  mqtt.setKeepAlive(30);
  mqtt.setSocketTimeout(8);
  Serial.printf("Device: %s | Claim: %s | http://%s.local | MQTT: %s\n", deviceId.c_str(), claimCode.c_str(), MDNS_NAME, topicBase.c_str());
}

void loop() {
  digitalWrite(PIN_WIFI_LED, WiFi.status() == WL_CONNECTED ? HIGH : LOW);
  if (WiFi.status() == WL_CONNECTED) {
    connectMqtt();
    mqtt.loop();
    web.handleClient();
  } else if (millis() - lastWifiReconnect >= 10000) {
    lastWifiReconnect = millis();
    mqttAddress = IPAddress();
    Serial.println("[WIFI] enlace perdido; solicitando reconexion al AP guardado");
    WiFi.reconnect();
  }

  const uint32_t now = millis();
  updateAutomation();
  if (!inventoryRegistered && now - lastEnroll >= 60000) enrollDevice();
  if (now - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = now;
    sampleSensors();
  }
  if (now - lastPublish >= PUBLISH_INTERVAL_MS) {
    lastPublish = now;
    publishState();
  }
  if (now - lastCloudPublish >= CLOUD_INTERVAL_MS) {
    lastCloudPublish = now;
    // Una sola conexión TLS por ciclo: evita agotar sockets y memoria del ESP32.
    if (cloudEventTurn && eventTail != eventHead) sendPendingEventToSupabase();
    else sendToSupabase();
    cloudEventTurn = !cloudEventTurn;
    if (mqtt.connected()) mqtt.loop();
  }
}
