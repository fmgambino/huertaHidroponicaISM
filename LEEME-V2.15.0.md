# Proyecto H² · Automatización v2.15.0

## Instalación

1. Respaldar Supabase.
2. Ejecutar `supabase/MIGRACION_V2150_AUTOMATIZACION.sql`.
3. Desplegar la Edge Function actualizada:

```powershell
npx supabase functions deploy device-enroll --project-ref jvrrtwlejbymxskijdhj --no-verify-jwt
```

4. Publicar la PWA completa o instalar `INSTALAR-Patch-Huerta-H2-v2.15.0.zip`.
5. Compilar/subir el firmware v2.15.0 al ESP32.
6. En el dashboard, hacer clic en el icono de información de una bomba, ventilador o extractor para configurar rangos y consultar historial.

## Automatización

- pH menor que MIN: pulso de la bomba A / pH+.
- pH mayor que MAX: pulso de la bomba B / pH−.
- Las bombas son mutuamente excluyentes, trabajan por pulsos y respetan un tiempo de espera.
- Ventiladores y extractores se activan si temperatura o humedad alcanzan MAX.
- Se desactivan cuando temperatura y humedad vuelven por debajo de sus MIN.
- Los valores se guardan en Supabase y en la NVS del ESP32.
- Cada cambio queda en `actuator_events` y genera una notificación PWA.

## Cableado

| Elemento | GPIO |
|---|---:|
| DHT22 | 4 |
| DS18B20 | 5 |
| pH analógico | 34 |
| Bomba general | 23 |
| Bomba nutriente A / pH+ | 26 |
| Bomba nutriente B / pH− | 27 |
| Extractores | 16 y 17 |
| Ventiladores | 18 y 19 |

El DS18B20 requiere normalmente una resistencia pull-up de 4,7 kΩ entre DATA y 3,3 V. La salida del módulo de pH no debe superar 3,3 V en GPIO34.

Las bombas de 5 V no se conectan directamente al ESP32: usar relé o MOSFET, fuente externa de 5 V, diodo flyback cuando corresponda y masa común. Verificar si el módulo de relé es activo en LOW antes de energizar las bombas.
