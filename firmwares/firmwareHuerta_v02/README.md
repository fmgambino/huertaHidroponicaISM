# Firmware Huerta Hidropónica Inteligente — ESP32 DevKit V1

Firmware PlatformIO/Arduino compatible con la PWA 2.12.1 adjunta. Configura Wi-Fi mediante WiFiManager, publica datos simulados cada 15 segundos por MQTT y los persiste mediante la Edge Function de Supabase.

## Funciones incluidas

- Identidad automática `ESP32-XXXX` derivada de la MAC.
- Portal WiFiManager `Huerta-H2-XXXX`, sin SSID ni contraseña cableados en el código.
- Código de vinculación persistente en NVS/Preferences.
- Enrolamiento por `device-enroll` sin incluir `service_role` ni `anon key` en el ESP32.
- Temperatura ambiente, humedad, temperatura de agua, pH y TDS simulados con variaciones suaves y ruido acotado.
- Campo `simulated: true`, temperatura interna, RSSI, uptime, `boot_id`, secuencia y causa de reinicio.
- MQTT TLS, LWT online/offline, reconexión y publicación en `huertaiot/<ID>/telemetry`.
- Seis salidas y ACK JSON compatible con la confirmación de la PWA.
- Reset remoto y persistencia del estado de los actuadores.

## GPIO de actuadores

| Actuador | GPIO |
|---|---:|
| Extractor 1 | 13 |
| Extractor 2 | 14 |
| Ventilador 1 | 16 |
| Ventilador 2 | 17 |
| Bomba de agua | 25 |
| Lámpara UV | 26 |

Las salidas se inicializan antes de conectarse. Para módulos activos en nivel bajo, cambiar `RELAY_ACTIVE_LOW` a `true` en `include/project_config.h`. Use transistor/driver, diodo flyback y fuente separada adecuada; no alimente bobinas directamente desde GPIO.

El GPIO3 usado en su prueba queda libre por defecto porque es RX0 y puede interferir con carga/monitor serie. Si su PCB obliga a utilizarlo, cambie en `project_config.h` solamente la constante del actuador correspondiente a `3`.

## Instalación

1. Ejecute en Supabase `MIGRACION_V28_INVENTARIO.sql` y luego `MIGRACION_V212_TELEMETRIA.sql` incluidos en la PWA.
2. Cree en Edge Functions > Secrets el valor `DEVICE_ENROLLMENT_KEY2` indicado en `supabase/DEPLOY_EDGE_FUNCTION.md`.
3. Reemplace y despliegue la Edge Function incluida con `--no-verify-jwt`.
4. Abra esta carpeta en VS Code con PlatformIO, compile el entorno `esp32dev` y cargue la placa.
5. Desde el celular conéctese a `Huerta-H2-XXXX`, abra `192.168.4.1` si el portal no aparece y seleccione la red Wi-Fi.
6. Abra el monitor serie a 115200 baud. En Inventario aparecerán el Device ID y código; vincule el equipo desde la PWA.

Para borrar las credenciales Wi-Fi, mantenga presionado el botón **BOOT/GPIO0 durante 5 segundos** con la placa ya iniciada.

Comandos equivalentes:

```bash
pio run
pio run --target upload
pio device monitor
```

## Binarios incluidos

- `release/firmware.bin`: actualización de aplicación en offset `0x10000`.
- `release/huerta-h2-v2.0.0-full.bin`: imagen completa para grabar desde offset `0x0`.
- `release/SHA256SUMS.txt`: integridad de todos los binarios.

Ejemplo de grabación completa (reemplazar el puerto):

```bash
python -m esptool --chip esp32 --port COM3 --baud 460800 write_flash 0x0 release/huerta-h2-v2.0.0-full.bin
```

## Tópicos MQTT

- Telemetría: `huertaiot/<DEVICE_ID>/telemetry`
- Estado retained/LWT: `huertaiot/<DEVICE_ID>/status`
- Actuador: `huertaiot/<DEVICE_ID>/command/actuator/<actuator_id>` con `ON` u `OFF`
- Confirmación: `huertaiot/<DEVICE_ID>/ack`
- Reset: `huertaiot/<DEVICE_ID>/command/reset` con `true`

## Nota de seguridad

La solución replica el diseño de prototipo de la PWA. HTTPS y MQTT usan `setInsecure()` para facilitar la puesta en marcha con el broker público. Para producción, instale las CA raíz, use un broker privado autenticado y provisione credenciales únicas por dispositivo. No coloque nunca la `service_role` de Supabase en el firmware.
