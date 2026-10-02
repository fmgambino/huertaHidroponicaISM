# Firmware Huerta H² v2.15.0

Firmware para ESP32 DevKit V1 con sensores reales y control autónomo.

- DHT22: GPIO4.
- DS18B20: GPIO5, pull-up de 4,7 kΩ.
- pH: GPIO34, entrada máxima 3,3 V.
- Bomba nutriente A / pH+: GPIO26.
- Bomba nutriente B / pH−: GPIO27.

Las bombas requieren etapa de potencia externa. Al arrancar, ambas salidas de dosificación se fuerzan a OFF. Los rangos llegan por MQTT en `huertaiot/<DEVICE_ID>/command/config` y se guardan en NVS.

Compilar y subir:

```powershell
platformio run
platformio run --target upload
platformio device monitor
```
