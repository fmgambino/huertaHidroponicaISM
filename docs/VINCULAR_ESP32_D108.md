# Vincular ESP32-D108 con la PWA

## Qué significa el error 409

El firmware está conectado a WiFi y llega a Supabase. La tabla `sensor_readings` exige que el `device_id` exista previamente en `devices`. Por eso `ESP32-D108` debe registrarse una vez desde una cuenta SuperAdmin o Docente.

## Procedimiento

1. Iniciar sesión en la PWA como SuperAdmin.
2. Abrir **Dispositivos** y seleccionar **Nuevo dispositivo**.
3. Cargar exactamente `ESP32-D108` como ID, respetando mayúsculas y guion.
4. Completar nombre, zona, categoría `ESP32 DevKit V1`, IP `192.168.0.233` y número de serie.
5. Guardar y confirmar que la tarjeta aparece en Dispositivos.
6. En Supabase, abrir `public.devices` y verificar que existe la fila `ESP32-D108`.
7. Reiniciar el ESP32 o esperar el próximo envío (15 segundos).
8. En Supabase, abrir `sensor_readings` y verificar filas nuevas con `device_id = ESP32-D108`.
9. Volver al Dashboard, elegir la zona y luego `ESP32-D108` en el selector Device.

## MQTT en tiempo real

La PWA se suscribe a `huertaiot/ESP32-D108/telemetry`. El firmware publica cada 15 segundos. Ambos deben usar el mismo host MQTT configurado en la PWA y en `include/secrets.h`.

## Diagnóstico rápido

- `Supabase HTTP 409 / 23503`: falta la fila exacta en `devices`.
- `Supabase HTTP -1`: falló temporalmente la conexión TLS; comprobar Internet, fecha/hora y memoria libre del ESP32.
- `_handleRequest(): request handler not found`: un navegador pidió una ruta no definida; no impide MQTT ni Supabase. El firmware v1.1.0 devuelve ahora un 404 controlado.
- `MQTT conectado` en la PWA sólo confirma la conexión del navegador al broker. La telemetría confirma que el ESP32 también está publicando.
