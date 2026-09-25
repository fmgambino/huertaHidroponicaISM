-- Proyecto H2 v2.14.0 - firmware, Team Lab público y auditoría
begin;

alter table public.team_members add column if not exists bio text;
alter table public.team_members add column if not exists social_links jsonb not null default '{}'::jsonb;
alter table public.team_members add column if not exists is_public boolean not null default true;

alter table public.team_members enable row level security;
drop policy if exists "team public read" on public.team_members;
create policy "team public read" on public.team_members for select to anon using(is_public=true);
grant select(name,team_role,photo_url,sort_order,team_name,bio,social_links) on public.team_members to anon;

-- Crea las definiciones que coinciden con firmwareHuerta_v02/include/pins.h.
create or replace function public.seed_huerta_firmware_definitions(target_device text)
returns void language plpgsql security definer set search_path='' as $function$
begin
  insert into public.sensor_definitions(id,device_id,name,sensor_type,unit,icon,gpio,enabled,metadata)
  values
    (target_device||'__air_temperature',target_device,'Temperatura ambiente','DHT22','°C','temperature',4,true,'{"channel":"air_temperature","firmware_id":"dht22"}'),
    (target_device||'__humidity',target_device,'Humedad ambiente','HUMIDITY','%','drop',4,true,'{"channel":"humidity","firmware_id":"dht22"}'),
    (target_device||'__water_temperature',target_device,'Temperatura del agua','DS18B20','°C','temperature',5,true,'{"channel":"water_temperature","firmware_id":"ds18b20"}'),
    (target_device||'__ph',target_device,'pH','pH','pH','drop',34,true,'{"channel":"ph","firmware_id":"ph"}'),
    (target_device||'__tds_ppm',target_device,'TDS Meter','TDS','ppm','bolt',35,true,'{"channel":"tds_ppm","firmware_id":"tds"}'),
    (target_device||'__cpu_temperature',target_device,'Temperatura interna ESP32','CPU','°C','temperature',null,true,'{"channel":"cpu_temperature","firmware_id":"cpu"}')
  on conflict(id) do update set device_id=excluded.device_id,name=excluded.name,sensor_type=excluded.sensor_type,unit=excluded.unit,icon=excluded.icon,gpio=excluded.gpio,metadata=excluded.metadata,updated_at=now();

  insert into public.actuator_definitions(id,device_id,name,actuator_type,icon,gpio,current_state,enabled,metadata)
  values
    (target_device||'__extractor_1',target_device,'Extractor 1','extractor','power',16,false,true,'{"firmware_id":"extractor_1","active_low":true}'),
    (target_device||'__extractor_2',target_device,'Extractor 2','extractor','power',17,false,true,'{"firmware_id":"extractor_2","active_low":true}'),
    (target_device||'__ventilador_1',target_device,'Ventilador 1','ventilador','power',18,false,true,'{"firmware_id":"ventilador_1","active_low":true}'),
    (target_device||'__ventilador_2',target_device,'Ventilador 2','ventilador','power',19,false,true,'{"firmware_id":"ventilador_2","active_low":true}'),
    (target_device||'__bomba_agua',target_device,'Bomba de agua','bomba','drop',23,false,true,'{"firmware_id":"bomba_agua","active_low":true}'),
    (target_device||'__lampara_uv',target_device,'Lámpara UV','uv','sun',25,false,true,'{"firmware_id":"lampara_uv","active_low":true}')
  on conflict(id) do update set device_id=excluded.device_id,name=excluded.name,actuator_type=excluded.actuator_type,icon=excluded.icon,gpio=excluded.gpio,metadata=excluded.metadata,updated_at=now();
end
$function$;

create or replace function public.seed_huerta_firmware_on_device()
returns trigger language plpgsql security definer set search_path='' as $function$
begin perform public.seed_huerta_firmware_definitions(new.id);return new;end
$function$;
drop trigger if exists seed_huerta_firmware_after_device on public.devices;
create trigger seed_huerta_firmware_after_device after insert on public.devices for each row execute function public.seed_huerta_firmware_on_device();
do $do$ declare item record;begin for item in select id from public.devices loop perform public.seed_huerta_firmware_definitions(item.id);end loop;end $do$;

-- Auditoría de mutaciones efectivas en la base. Para DELETE la fila anterior
-- queda en details y puede disparar el webhook de alerta.
create or replace function public.audit_table_mutation()
returns trigger language plpgsql security definer set search_path='' as $function$
declare payload jsonb; identifier text;
begin
  payload=case when tg_op='DELETE' then to_jsonb(old) else to_jsonb(new) end;
  identifier=coalesce(payload->>'id',payload->>'name','');
  insert into public.audit_logs(actor_id,action,device_id,details)
  values(auth.uid(),tg_op||' '||tg_table_name,null,jsonb_build_object('table',tg_table_name,'id',identifier,'record',payload,'actor_role',(select role::text from public.profiles where id=auth.uid())));
  if tg_op='DELETE' then return old;end if;
  return new;
end
$function$;

do $do$ declare table_name text;begin
  foreach table_name in array array['profiles','zones','devices','notifications','team_members','role_permissions','sensor_definitions','actuator_definitions'] loop
    execute format('drop trigger if exists audit_mutation on public.%I',table_name);
    execute format('create trigger audit_mutation after insert or update or delete on public.%I for each row execute function public.audit_table_mutation()',table_name);
  end loop;
end $do$;

do $do$ begin
  if not exists(select 1 from pg_publication_tables where pubname='supabase_realtime' and schemaname='public' and tablename='audit_logs') then
    alter publication supabase_realtime add table public.audit_logs;
  end if;
end $do$;

notify pgrst,'reload schema';
commit;
