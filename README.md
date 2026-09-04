# Proyecto H² — Huerta Hidropónica IoT v2.10.0 · Edición escolar Supabase-first

Entrega refactorizada: `hardware/` contiene el firmware ESP32-S3 (PlatformIO + Arduino) y `app/` la PWA mobile-first para GitHub Pages, Supabase, MQTT, EmailJS y actualizaciones por patch.

1. Revisar GPIO y credenciales en `hardware/include/` y compilar con PlatformIO.
2. Ejecutar `app/supabase/schema.sql` en Supabase y registrar el ID del dispositivo.
3. Activar Email y Google en Supabase Authentication y configurar la URL de Pages.
4. Publicar el contenido de `app/` como raíz de GitHub Pages.

La publishable key incluida es pública por diseño. No agregar claves `service_role`.
