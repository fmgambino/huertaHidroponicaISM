# Proyecto H² · PWA v2.14.0

## Instalación

1. Realizar un respaldo de Supabase.
2. Ejecutar `supabase/MIGRACION_V2140_FIRMWARE_TEAM_AUDIT.sql` en SQL Editor.
3. Desplegar las funciones:

```powershell
supabase functions deploy user-admin --no-verify-jwt
supabase functions deploy audit-alert --no-verify-jwt
```

4. Crear los secretos de `audit-alert`:

```powershell
supabase secrets set ALERT_EMAIL=fernando.m.gambino@gmail.com AUDIT_WEBHOOK_SECRET=CAMBIAR_POR_UN_SECRETO_LARGO
```

`EMAILJS_PRIVATE_KEY` es opcional y sólo se necesita si la cuenta EmailJS exige clave privada.

5. En Supabase → Database → Webhooks crear un webhook para `public.audit_logs`, evento `INSERT`, dirigido a:

`https://<PROJECT_REF>.supabase.co/functions/v1/audit-alert`

Agregar el header `x-webhook-secret` con el mismo valor de `AUDIT_WEBHOOK_SECRET`.
Este enlace servidor-servidor permite avisar eliminaciones aunque ninguna PWA esté abierta.

6. En Configuración → EmailJS guardar Service ID, Public key y uno o más templates. Para el template de alertas seleccionar el uso **Alerta de eliminación** y escribir el nombre exacto de su variable destinatario (por ejemplo `to_email`).
7. Instalar `INSTALAR-Patch-Huerta-H2-v2.14.0.zip` desde Configuración → Actualizaciones o publicar el ZIP completo en GitHub Pages.

## Incluye

- Sensores del firmware: DHT22 temperatura/humedad (GPIO 4), DS18B20 (GPIO 5), pH (GPIO 34), TDS (GPIO 35) y temperatura interna.
- Actuadores del firmware: extractores 1/2 (GPIO 16/17), ventiladores 1/2 (GPIO 18/19), bomba (GPIO 23) y lámpara UV (GPIO 25), todos activos en LOW.
- Alta y actualización automática de definiciones al registrar dispositivos.
- Múltiples templates EmailJS con propósito y variable destinatario configurables.
- Prueba EmailJS compatible con `to_email`, `email`, `recipient`, `user_email` y el campo personalizado.
- Perfiles públicos del Team Lab desde el login, redes sociales y botones para compartir.
- Auditoría en tiempo real con nombre/email del usuario y captura de cambios efectivos en base de datos.
- Alerta por email ante eliminaciones realizadas por SuperAdmin.

## Requisito de EmailJS

El campo **To Email** del template de EmailJS debe contener la variable configurada en la PWA, por ejemplo `{{to_email}}`. No debe quedar vacío ni contener sólo texto sin variable.
