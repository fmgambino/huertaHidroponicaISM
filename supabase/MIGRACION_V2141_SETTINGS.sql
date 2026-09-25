-- Proyecto H² v2.14.1 - guardado de integraciones por SuperAdmin/Docente
begin;

alter table public.app_settings enable row level security;
grant select,insert,update on public.app_settings to authenticated;

drop policy if exists "settings read" on public.app_settings;
drop policy if exists "settings manage" on public.app_settings;

create policy "settings read"
on public.app_settings for select to authenticated
using(true);

create policy "settings manage"
on public.app_settings for all to authenticated
using(exists(select 1 from public.profiles where id=auth.uid() and role in('superadmin','docente')))
with check(exists(select 1 from public.profiles where id=auth.uid() and role in('superadmin','docente')));

-- Limpia indicadores históricos. La PWA igualmente exige telemetría reciente.
update public.devices
set online=false
where online=true and (last_seen is null or last_seen < now()-interval '30 seconds');

notify pgrst,'reload schema';
commit;
