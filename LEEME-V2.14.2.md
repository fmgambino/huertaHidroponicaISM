# Proyecto H² · Corrección v2.14.2

## Instalación

1. Ejecutar `supabase/MIGRACION_V2142_EMAILJS.sql` en SQL Editor.
2. Instalar `INSTALAR-Patch-Huerta-H2-v2.14.2.zip` o publicar el paquete completo.
3. Volver a desplegar `audit-alert`, porque ahora lee la configuración independiente `app_settings/emailjs`:

```powershell
npx supabase functions deploy audit-alert --project-ref jvrrtwlejbymxskijdhj --no-verify-jwt
```

## Cambios

- EmailJS se guarda en una fila independiente para evitar que otra sincronización sobrescriba sus templates.
- Después de guardar, la PWA relee la fila y verifica que coincida exactamente.
- Enviar prueba guarda primero la configuración activa.
- La confirmación diferencia solicitud aceptada por EmailJS de entrega final.
- Cada módulo muestra un spinner con fondo difuso mientras actualiza datos.

Para la entrega del correo, configurar en EmailJS **To Email** como `{{to_email}}` y en la PWA usar `to_email` como variable destinatario. Si EmailJS responde 200 pero no llega, revisar Email History dentro de EmailJS: allí figura si el proveedor rechazó, rebotó o bloqueó el mensaje.
