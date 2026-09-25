-- Proyecto H² v2.14.2 - configuración EmailJS independiente y verificable
begin;

alter table public.app_settings enable row level security;
grant select,insert,update on public.app_settings to authenticated;

insert into public.app_settings(id,value,updated_at)
select 'emailjs',coalesce(value->'settings'->'emailjs','{}'::jsonb),now()
from public.app_settings
where id='global' and value->'settings'->'emailjs' is not null
on conflict(id) do nothing;

notify pgrst,'reload schema';
commit;
