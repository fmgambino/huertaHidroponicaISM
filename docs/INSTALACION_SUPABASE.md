# Instalación Supabase · Proyecto H²

1. Abrir el proyecto `jvrrtwlejbymxskijdhj` y entrar en **SQL Editor**.
2. Para una base ya creada, ejecutar primero `supabase/MIGRACION_V23.sql`. Para una instalación nueva, ejecutar completo `supabase/schema.sql`. El esquema usa los valores técnicos singulares `superadmin`, `alumno`, `docente`, `ecociudadano`.
3. En **Authentication → Providers**, habilitar Email y Google.
4. En **Authentication → URL Configuration**, configurar como Site URL la URL de GitHub Pages y agregarla también en Redirect URLs.
5. Crear el primer usuario desde la PWA y ejecutar la sentencia final del esquema para promoverlo a `superadmin`.
6. Registrar el ESP32 antes de recibir lecturas:

```sql
insert into public.devices(id,name,zone_id)
select 'ESP32-AB12','Huerta principal',id from public.zones where name='Invernadero';
```

La clave incluida es `publishable`, apta para frontend. Nunca usar `service_role` en la PWA o el firmware. Para producción, el alta de telemetría anónima debe reemplazarse por una Edge Function o una credencial individual por dispositivo.

## Google OAuth

Crear credenciales OAuth en Google Cloud, agregar el callback mostrado por Supabase y copiar Client ID/Secret en el proveedor Google de Supabase.

## GitHub Pages

Publicar el contenido de `app/` en la raíz configurada de Pages. Los patches instalados desde Configuración se aplican localmente en cada navegador; una actualización global se publica reemplazando los archivos del repositorio.
