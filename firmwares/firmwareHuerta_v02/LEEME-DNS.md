# Firmware 1.5.1 — recuperación DNS

El registro recibido demuestra que la asociación WiFi y DHCP funcionaron (`192.168.0.121`), pero las consultas DNS fallaron para dos dominios independientes. Por eso:

- `Inventario HTTP -1` significa que ni siquiera se abrió la conexión con Supabase; no es una respuesta HTTP, RLS ni una clave inválida.
- MQTT `rc=-2` significa que el cliente no llegó a establecer la conexión de red. Sin MQTT el ESP32 no recibe los comandos de actuadores.
- El warning `remote_reset NOT_FOUND` era un borrado innecesario de una clave NVS inexistente; quedó corregido.

Esta versión imprime IP, gateway, máscara, DNS y RSSI. Primero prueba el DNS de DHCP. Si falla, conserva IP/gateway/máscara de la conexión y configura temporalmente 1.1.1.1 y 8.8.8.8. Reintenta DNS cada 30 segundos y el inventario cada 60 segundos.

## Prueba esperada

Tras grabar y reiniciar, buscar:

    [BOOT] Firmware 1.5.1
    [RED] IP=... Gateway=... DNS1=... DNS2=...
    [DNS] jvrrtwlejbymxskijdhj.supabase.co -> ... (OK)
    [DNS] broker.emqx.io -> ... (OK)
    Inventario HTTP 200: ...
    [MQTT] conectado; suscripcion huertaiot/ESP32-CBB0/command/#: OK

El dispositivo de este registro es `ESP32-CBB0`, con claim `H2-VVB6FEP99QY5`. No usar ESP32-D108 para este equipo. La PWA debe tener o vincular ESP32-CBB0 en su zona. No compartas públicamente el claim: funciona como contraseña de vinculación.

Si ambos DNS continúan fallando, el firmware no puede reparar una red que bloquea DNS o no tiene salida a Internet. Probar el ESP32 con un hotspot del teléfono permite separar un problema del router escolar de uno del firmware. En una PC conectada al mismo WiFi también se puede ejecutar:

    nslookup jvrrtwlejbymxskijdhj.supabase.co
    nslookup broker.emqx.io

Si resuelven en PC pero no en ESP32, compartir todo el bloque `[RED]`/`[DNS]`. Si tampoco resuelven en PC, corregir DNS, filtrado o acceso a Internet del router `ISMrobotica`.

## Instalación

Abrir esta carpeta en PlatformIO y cargar el entorno `esp32dev`. El binario compilado está en `binaries/firmware.bin`; para una carga normal desde PlatformIO usar Upload para que también respete bootloader y particiones. La actualización no cambia `secrets.h`, el claim guardado, la configuración WiFi ni los pines.
