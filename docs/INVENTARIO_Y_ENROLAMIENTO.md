# Inventario y enrolamiento automático

## Instalación en Supabase

1. Ejecutar `supabase/MIGRACION_V28_INVENTARIO.sql` en SQL Editor.
2. Generar una clave de fabricación aleatoria de al menos 32 caracteres.
3. Guardarla como secreto de la función:

   `supabase secrets set DEVICE_ENROLLMENT_KEY=SU_CLAVE_ALEATORIA`

4. Desplegar la función:

   `supabase functions deploy device-enroll --no-verify-jwt`

5. Copiar el mismo valor en `firmware-esp32-devkit-v1/include/secrets.h`:

   `#define DEVICE_ENROLLMENT_KEY "SU_CLAVE_ALEATORIA"`

6. Compilar y cargar el firmware.

## Flujo

En el primer arranque, el ESP32 genera un código de vinculación aleatorio y lo conserva en Preferences. Al conectarse a Internet envía por HTTPS el Device ID, código, MAC y versión a `device-enroll`. La función valida la clave de fabricación y escribe el inventario usando la Service Role, que nunca se incluye en el firmware.

La tabla sólo puede leerse por SuperAdmin. El módulo Inventario genera una etiqueta con Device ID, contraseña, Code 128 y QR. El QR contiene un JSON con `device_id` y `password`.

## Recomendación para comercialización

La clave de fabricación compartida es adecuada para prototipo y lotes controlados. Para producción comercial conviene provisionar una clave única o certificado por placa y cifrar el código de vinculación mediante KMS/Vault. Después de vincular un equipo, el código debe rotarse o invalidarse.
