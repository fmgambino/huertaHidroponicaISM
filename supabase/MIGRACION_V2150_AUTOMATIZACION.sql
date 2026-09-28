-- Proyecto H² v2.15.0 · Control automático de pH, temperatura y humedad
begin;

create table if not exists public.automation_settings(
  device_id text primary key references public.devices(id) on delete cascade,
  enabled boolean not null default true,
  ventilation_enabled boolean not null default true,
  dosing_enabled boolean not null default true,
  ph_min real not null default 5.5,
  ph_max real not null default 6.5,
  temp_min real not null default 22,
  temp_max real not null default 28,
  humidity_min real not null default 50,
  humidity_max real not null default 75,
  dose_ms integer not null default 3000 check(dose_ms between 500 and 15000),
  cooldown_ms integer not null default 60000 check(cooldown_ms between 10000 and 3600000),
  updated_by uuid references auth.users(id),
  updated_at timestamptz not null default now(),
  constraint automation_ph_range check(ph_min>=0 and ph_max<=14 and ph_min<ph_max),
  constraint automation_temp_range check(temp_min<temp_max),
  constraint automation_humidity_range check(humidity_min>=0 and humidity_max<=100 and humidity_min<humidity_max)
);

alter table public.actuator_events add column if not exists source text not null default 'manual';
alter table public.actuator_events add column if not exists reason text not null default '';
alter table public.actuator_events add column if not exists sensor_snapshot jsonb not null default '{}'::jsonb;

alter table public.automation_settings enable row level security;
drop policy if exists "automation read" on public.automation_settings;
drop policy if exists "automation manage" on public.automation_settings;
create policy "automation read" on public.automation_settings for select to authenticated using(true);
create policy "automation manage" on public.automation_settings for all to authenticated
using(public.current_role() in('superadmin','docente')) with check(public.current_role() in('superadmin','docente'));
grant select,insert,update,delete on public.automation_settings to authenticated;

insert into public.actuator_definitions(id,device_id,name,actuator_type,icon,gpio,current_state,enabled,metadata)
select d.id||'__bomba_nutriente_a',d.id,'Bomba nutriente A / pH+','nutriente','flask',26,false,true,'{"firmware_id":"bomba_nutriente_a","active_low":true,"automation":"ph_low"}'::jsonb from public.devices d
on conflict(id) do update set name=excluded.name,actuator_type=excluded.actuator_type,icon=excluded.icon,gpio=excluded.gpio,metadata=excluded.metadata,updated_at=now();
insert into public.actuator_definitions(id,device_id,name,actuator_type,icon,gpio,current_state,enabled,metadata)
select d.id||'__bomba_nutriente_b',d.id,'Bomba nutriente B / pH−','nutriente','beaker',27,false,true,'{"firmware_id":"bomba_nutriente_b","active_low":true,"automation":"ph_high"}'::jsonb from public.devices d
on conflict(id) do update set name=excluded.name,actuator_type=excluded.actuator_type,icon=excluded.icon,gpio=excluded.gpio,metadata=excluded.metadata,updated_at=now();

create or replace function public.seed_automation_for_device() returns trigger language plpgsql security definer set search_path='' as $fn$
begin
  insert into public.automation_settings(device_id) values(new.id) on conflict do nothing;
  insert into public.actuator_definitions(id,device_id,name,actuator_type,icon,gpio,metadata) values
    (new.id||'__bomba_nutriente_a',new.id,'Bomba nutriente A / pH+','nutriente','flask',26,'{"firmware_id":"bomba_nutriente_a","active_low":true,"automation":"ph_low"}'),
    (new.id||'__bomba_nutriente_b',new.id,'Bomba nutriente B / pH−','nutriente','beaker',27,'{"firmware_id":"bomba_nutriente_b","active_low":true,"automation":"ph_high"}')
  on conflict(id) do nothing;
  return new;
end $fn$;
drop trigger if exists seed_automation_after_device on public.devices;
create trigger seed_automation_after_device after insert on public.devices for each row execute function public.seed_automation_for_device();
insert into public.automation_settings(device_id) select id from public.devices on conflict do nothing;

create or replace function public.notify_actuator_change() returns trigger language plpgsql security definer set search_path='' as $fn$
declare label text;
begin
  if new.actuator not in('bomba_nutriente_a','bomba_nutriente_b','ventilador_1','ventilador_2','extractor_1','extractor_2') then return new;end if;
  label=case new.actuator when 'bomba_nutriente_a' then 'Bomba nutriente A / pH+' when 'bomba_nutriente_b' then 'Bomba nutriente B / pH−' when 'ventilador_1' then 'Ventilador 1' when 'ventilador_2' then 'Ventilador 2' when 'extractor_1' then 'Extractor 1' else 'Extractor 2' end;
  insert into public.notifications(title,message,level) values(label||case when new.state then ' activado' else ' desactivado' end,'Dispositivo '||coalesce(new.device_id,'—')||'. '||coalesce(new.reason,'')||' · Origen: '||new.source,case when new.state then 'warning' else 'success' end);
  return new;
end $fn$;
drop trigger if exists notify_actuator_change_after_insert on public.actuator_events;
create trigger notify_actuator_change_after_insert after insert on public.actuator_events for each row execute function public.notify_actuator_change();

do $do$ begin
  if not exists(select 1 from pg_publication_tables where pubname='supabase_realtime' and schemaname='public' and tablename='automation_settings') then alter publication supabase_realtime add table public.automation_settings;end if;
  if not exists(select 1 from pg_publication_tables where pubname='supabase_realtime' and schemaname='public' and tablename='actuator_events') then alter publication supabase_realtime add table public.actuator_events;end if;
end $do$;

notify pgrst,'reload schema';
commit;
