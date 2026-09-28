-- Proyecto H² v2.15.1 · Separación de climatización y dosificación
begin;

alter table public.automation_settings add column if not exists ventilation_enabled boolean not null default true;
alter table public.automation_settings add column if not exists dosing_enabled boolean not null default true;

-- Conserva el estado del interruptor único utilizado por v2.15.0.
update public.automation_settings
set ventilation_enabled=enabled,dosing_enabled=enabled
where enabled is not null;

notify pgrst,'reload schema';
commit;
