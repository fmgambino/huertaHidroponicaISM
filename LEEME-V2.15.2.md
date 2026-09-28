# Proyecto H² v2.15.2 — estabilidad de conexión

## Qué se corrigió

- La PWA espera hasta 45 segundos antes de declarar un dispositivo offline y tolera pequeños desajustes de reloj.
- El cliente MQTT WebSocket reintenta cada 2 segundos, conserva keepalive de 30 segundos y vuelve a suscribirse automáticamente.
- El firmware publica por MQTT cada 5 segundos independientemente de Supabase.
- La sincronización HTTPS con Supabase se ejecuta cada 15 segundos para no bloquear el ciclo MQTT.
- MQTT y Supabase resuelven DNS de manera independiente. Un fallo de Supabase ya no impide conectar al broker.
- Se eliminó la reconfiguración de IP/DNS mientras el ESP32 está conectado por DHCP.
- Se deshabilitó el ahorro de energía Wi-Fi para mejorar estabilidad y se añadió reconexión automática.
- Las claves NVS de los actuadores ahora son cortas; desaparece `KEY_TOO_LONG` para las bombas dosificadoras.

## Instalación

1. Cargar `firmware-v2.15.2.bin` en el ESP32 o compilar la carpeta completa de firmware.
2. Publicar el contenido de `pwa-completa` en GitHub Pages, o instalar `patch-pwa-v2.15.2.zip` desde Configuración > Actualizaciones sobre una PWA v2.15.1.
3. Tras cargar el firmware, abrir el monitor serial a 115200 baudios. Deben aparecer `[MQTT] conectado` y telemetrías `envio=OK`.

No requiere migración nueva de Supabase.
