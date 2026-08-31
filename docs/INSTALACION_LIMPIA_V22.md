# Instalación limpia v2.2

Esta versión reemplaza completamente cualquier copia anterior basada en Salamandra.

1. Detener Live Server.
2. Vaciar la carpeta publicada y copiar **el contenido** de `pwa/` o del ZIP PWA, dejando `index.html` en la raíz publicada.
3. Iniciar nuevamente Live Server o publicar esos archivos en GitHub Pages.
4. Abrir la aplicación y recargar una vez. El Service Worker v2.2 elimina automáticamente caches anteriores, incluidos patches incompatibles.

No copiar el ZIP dentro de la carpeta que se publica ni extraerlo creando una ruta `app/app`.

La configuración de Supabase se encuentra en `assets/js/config.js`. El esquema ejecutable está en `supabase/schema.sql`.
