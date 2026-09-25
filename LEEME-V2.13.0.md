# Proyecto H² · PWA v2.13.0

## Actualización desde la PWA

1. Iniciar sesión como SuperAdmin.
2. Abrir **Configuración → Actualizaciones**.
3. Seleccionar `INSTALAR-Patch-Huerta-H2-v2.13.0.zip`.
4. Verificar que el paquete indique la versión 2.13.0 y confirmar.
5. Al finalizar, pulsar **Continuar** una sola vez.

El nuevo service worker elimina automáticamente shells, staging y respaldos
obsoletos. Conserva únicamente el patch activo necesario cuando la actualización
se instala localmente sobre GitHub Pages.

## Google OAuth

La PWA nunca solicita ni almacena el Client Secret.

1. En Google Cloud crear un cliente OAuth web.
2. Copiar en Google la URL callback que muestra Supabase para el proveedor.
3. En Supabase abrir **Authentication → Providers → Google**.
4. Activar el proveedor e ingresar allí el Client ID y Client Secret.
5. En **Authentication → URL Configuration** agregar:
   `https://fmgambino.github.io/huertaHidroponicaISM/index.html`
6. En la PWA abrir **Configuración → Integraciones**, comprobar la URL y guardar.

## EmailJS

Abrir **Configuración → EmailJS**, ingresar Service ID, Template ID, Public Key y
un correo de prueba. La configuración global se guarda en `app_settings` de
Supabase; no se utiliza el almacenamiento local de la aplicación.

## Estado de conectividad

El indicador WiFi/conectado requiere una lectura de telemetría con antigüedad
menor a 20 segundos. El campo histórico `devices.online` no alcanza por sí solo
para mostrar el dispositivo conectado.

La aplicación no guarda dispositivos, roles ni telemetría en `localStorage`.
Supabase puede conservar su token de autenticación mediante su cliente oficial
para mantener la sesión iniciada.
