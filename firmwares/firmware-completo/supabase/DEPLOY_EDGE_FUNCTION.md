# Actualizar la Edge Function

En **Edge Functions > Secrets**, crear exactamente:

```text
Name:  DEVICE_ENROLLMENT_KEY2
Value: 4dc65ce6477b8f973184a21cc43fb12301cf2f65ee736bb370412cb0ac1d5c19
```

Luego reemplazar `device-enroll/index.ts` por el archivo incluido y desplegar la función sin verificación JWT:

```bash
supabase functions deploy device-enroll --no-verify-jwt
```

También puede editar y desplegar la función desde el panel de Supabase. No coloque `SUPABASE_SERVICE_ROLE_KEY` en el ESP32: Supabase la inyecta únicamente en la Edge Function.
