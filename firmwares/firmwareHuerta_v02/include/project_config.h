#pragma once

#define FIRMWARE_VERSION "2.0.0-wm-sim"
#define SUPABASE_URL "https://jvrrtwlejbymxskijdhj.supabase.co"
#define ENROLL_ENDPOINT SUPABASE_URL "/functions/v1/device-enroll"

#define MQTT_HOST "broker.emqx.io"
#define MQTT_PORT 8883
#define MQTT_ROOT "huertaiot"

#define TELEMETRY_INTERVAL_MS 15000UL
#define ENROLL_RETRY_MS 60000UL
#define WIFI_RETRY_MS 10000UL
#define WIFI_PORTAL_RETRY_MS 300000UL
#define MQTT_RETRY_MS 5000UL
#define WIFI_PORTAL_TIMEOUT_SECONDS 180
#define WIFI_RESET_BUTTON_GPIO 0
#define WIFI_RESET_HOLD_MS 5000UL

// false: relé activo en HIGH. Cambiar a true para módulos de relé activos en LOW.
#define RELAY_ACTIVE_LOW false

// Mapeo centralizado de salidas. GPIO3 queda libre porque comparte RX0/USB-Serial.
#define GPIO_EXTRACTOR_1 13
#define GPIO_EXTRACTOR_2 14
#define GPIO_VENTILADOR_1 16
#define GPIO_VENTILADOR_2 17
#define GPIO_BOMBA_AGUA 25
#define GPIO_LAMPARA_UV 26
