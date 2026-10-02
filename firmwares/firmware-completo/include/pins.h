#pragma once

// ESP32 DevKit V1 DOIT de 30 pines.
constexpr uint8_t PIN_WIFI_LED = 2;
constexpr uint8_t PIN_DHT22 = 4;
constexpr uint8_t PIN_DS18B20 = 5;
constexpr uint8_t PIN_PH = 34;
constexpr uint8_t PIN_TDS = 35;

constexpr uint8_t PIN_EXTRACTOR_1 = 16;
constexpr uint8_t PIN_EXTRACTOR_2 = 17;
constexpr uint8_t PIN_FAN_1 = 18;
constexpr uint8_t PIN_FAN_2 = 19;
constexpr uint8_t PIN_PUMP = 23;
constexpr uint8_t PIN_UV = 25;
// Bombas de dosificación de 5 V. Usar MOSFET/relé y fuente externa con GND común.
constexpr uint8_t PIN_NUTRIENT_A = 26;  // Solución pH+ / nutriente A.
constexpr uint8_t PIN_NUTRIENT_B = 27;  // Solución pH- / nutriente B.

constexpr bool RELAY_ACTIVE_LOW = true;

