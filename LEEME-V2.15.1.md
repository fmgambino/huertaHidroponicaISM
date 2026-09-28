# Proyecto H² · Automatización separada v2.15.1

## Corrección

- Extractores y ventiladores muestran únicamente temperatura MIN/MAX y humedad MIN/MAX.
- Bombas dosificadoras muestran únicamente pH MIN/MAX, duración del pulso y espera entre dosis.
- Cada subsistema tiene su propio interruptor: `ventilation_enabled` y `dosing_enabled`.
- Deshabilitar dosificación no modifica la climatización; deshabilitar climatización no modifica las bombas.

## Instalación desde v2.15.0

1. Ejecutar `supabase/MIGRACION_V2151_AUTOMATIZACION_SEPARADA.sql`.
2. Instalar `INSTALAR-Patch-Huerta-H2-v2.15.1.zip`.
3. Cargar el firmware v2.15.1 en el ESP32.

No es necesario volver a desplegar `device-enroll` si ya se desplegó la versión incluida con v2.15.0.
