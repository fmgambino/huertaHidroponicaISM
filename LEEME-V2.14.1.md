# Proyecto H² · Corrección v2.14.1

## Instalación

1. Ejecutar `supabase/MIGRACION_V2141_SETTINGS.sql` en el SQL Editor.
2. Instalar `INSTALAR-Patch-Huerta-H2-v2.14.1.zip` desde Configuración → Actualizaciones, o publicar el paquete completo.
3. Cerrar las pestañas antiguas de la PWA y abrirla nuevamente.

No es necesario volver a desplegar `user-admin` ni `audit-alert` para esta corrección.

## Estado de dispositivos

- `devices.online` ya no se considera suficiente.
- Un dispositivo sólo figura Online cuando `online=true` y `last_seen` corresponde a telemetría recibida durante los últimos 20 segundos.
- El módulo Dispositivos, encabezado y dashboard se actualizan cada cinco segundos.
- La migración limpia indicadores Online antiguos con más de 30 segundos sin actividad.

## EmailJS

- El selector conserva el índice correcto mientras se editan o eliminan templates.
- Al agregar un template se preservan los campos que ya estaban escritos.
- El nombre del selector se actualiza mientras se escribe.
- El envío toma el Template ID directamente de la fila visible seleccionada.
- Los errores indican por separado qué campo falta o el mensaje devuelto por EmailJS.
- El guardado espera confirmación real de la fila `app_settings` en Supabase.

En EmailJS, el campo **To Email** debe usar la misma variable configurada en la PWA. Se recomienda `{{to_email}}` y variable destinatario `to_email`.
