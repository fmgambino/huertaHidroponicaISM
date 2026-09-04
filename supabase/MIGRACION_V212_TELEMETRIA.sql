-- Ejecutar una vez sobre el esquema existente (requiere MIGRACION_V28).
begin;
alter table public.device_inventory add column if not exists serial_number text;
update public.device_inventory set serial_number = 'ESP' || right(replace(upper(mac), ':', ''), 6)
where serial_number is null;
create unique index if not exists inventory_serial_unique on public.device_inventory(serial_number);
alter table public.sensor_readings add column if not exists simulated boolean not null default false;
alter table public.sensor_readings add column if not exists cpu_temperature real;
alter table public.sensor_readings add column if not exists boot_id text;
-- La secuencia del firmware se reinicia por arranque; ahora se identifica por boot_id.
alter table public.sensor_readings drop constraint if exists sensor_readings_device_id_sequence_key;
create unique index if not exists readings_boot_sequence on public.sensor_readings(device_id,boot_id,sequence);
create index if not exists readings_device_date on public.sensor_readings(device_id,created_at);
create table if not exists public.device_boot_events (
  device_id text not null references public.device_inventory(id),
  boot_id text not null,
  reason text not null,
  restart_count bigint,
  created_at timestamptz not null default now(),
  primary key(device_id,boot_id)
);
alter table public.device_boot_events enable row level security;
drop policy if exists "boot events read" on public.device_boot_events;
create policy "boot events read" on public.device_boot_events for select to authenticated
using (exists (select 1 from public.devices d where d.id=device_id));
grant select on public.device_boot_events to authenticated;
grant all on public.device_boot_events to service_role;

-- Sólo el backend autenticado con service_role puede ingerir telemetría.
create or replace function public.ingest_device_telemetry(p jsonb) returns jsonb
language plpgsql security definer set search_path = '' as $$
declare device_key text := p->>'device_id'; stamp timestamptz := now();
begin
  perform 1 from public.device_inventory where id=device_key for update;
  if not found then raise exception 'Dispositivo no enrolado'; end if;
  insert into public.device_boot_events(device_id,boot_id,reason,restart_count,created_at)
  values(device_key,p->>'boot_id',p->>'reset_reason',(p->>'restart_count')::bigint,
    stamp - ((p->>'uptime_ms')::double precision * interval '1 millisecond'))
  on conflict do nothing;
  update public.device_inventory set last_seen=stamp,firmware=p->>'firmware' where id=device_key;
  update public.devices set last_seen=stamp, online=true,
    serial_number=coalesce(nullif(serial_number,''),p->>'serial_number'),
    metadata=metadata || jsonb_build_object('ip',p->>'ip','mac',p->>'mac','ssid',p->>'ssid',
      'firmware',p->>'firmware','mqtt_connected',p->'mqtt_connected','rssi',p->'wifi_rssi',
      'uptime_ms',p->'uptime_ms','cpu_temperature',p->'cpu_temperature','simulated',p->'simulated')
  where id=device_key;
  if not found then return jsonb_build_object('ok',true,'pending_link',true); end if;
  insert into public.sensor_readings(device_id,sequence,air_temperature,humidity,water_temperature,
    ph,tds_ppm,wifi_rssi,uptime_ms,actuators,simulated,cpu_temperature,boot_id,created_at)
  values(device_key,(p->>'sequence')::bigint,(p->>'air_temperature')::real,(p->>'humidity')::real,
    (p->>'water_temperature')::real,(p->>'ph')::real,(p->>'tds_ppm')::real,(p->>'wifi_rssi')::integer,
    (p->>'uptime_ms')::bigint,p->'actuators',(p->>'simulated')::boolean,
    (p->>'cpu_temperature')::real,p->>'boot_id',stamp)
  on conflict (device_id,boot_id,sequence) do nothing;
  return jsonb_build_object('ok',true);
end $$;
revoke all on function public.ingest_device_telemetry(jsonb) from public,anon,authenticated;
grant execute on function public.ingest_device_telemetry(jsonb) to service_role;
notify pgrst, 'reload schema';
commit;
