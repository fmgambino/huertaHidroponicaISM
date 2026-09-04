# Supabase v2.4

1. Ejecutar `supabase/MIGRACION_V24.sql` en SQL Editor después de la migración v2.3.
2. Instalar Supabase CLI e iniciar sesión.
3. Desde la carpeta `pwa`, vincular el proyecto y desplegar la función:

```bash
supabase link --project-ref jvrrtwlejbymxskijdhj
supabase functions deploy user-admin --no-verify-jwt
```

Supabase inyecta automáticamente `SUPABASE_URL`, `SUPABASE_ANON_KEY` y `SUPABASE_SERVICE_ROLE_KEY` en la función. La service-role nunca debe copiarse a `config.js` ni al frontend.

Ingresar en la PWA con una cuenta real SuperAdmin. Las vistas demo no escriben en la nube porque no poseen JWT autenticado.

La función `user-admin` valida que quien llama tenga rol `superadmin` antes de listar usuarios, crear credenciales, cambiar contraseñas o eliminar cuentas.

Después de desplegar, probar desde la terminal:

```bash
supabase functions list
```

Si el navegador había mostrado un error CORS, recargar la PWA después del despliegue. El archivo `supabase/config.toml` deja `verify_jwt=false` en el gateway porque la función valida el token y el rol internamente.
