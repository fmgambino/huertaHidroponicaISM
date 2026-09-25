-- Proyecto H2 v2.13.1
-- Ejecutar una sola vez en Supabase > SQL Editor.
-- Es idempotente y no cambia los roles ya asignados.

begin;

-- Todo usuario nuevo, incluido Google OAuth, nace como EcoCiudadano.
-- Nunca se acepta un rol enviado en metadata por el navegador/proveedor.
create or replace function public.handle_new_user()
returns trigger
language plpgsql
security definer
set search_path = ''
as $function$
begin
  insert into public.profiles(id,display_name,role,avatar_url)
  values(
    new.id,
    coalesce(new.raw_user_meta_data->>'name',new.raw_user_meta_data->>'full_name',split_part(coalesce(new.email,''),'@',1),''),
    'ecociudadano'::public.app_role,
    coalesce(new.raw_user_meta_data->>'avatar_url',new.raw_user_meta_data->>'picture')
  )
  on conflict(id) do update set
    display_name=coalesce(nullif(public.profiles.display_name,''),excluded.display_name),
    avatar_url=coalesce(public.profiles.avatar_url,excluded.avatar_url),
    updated_at=now();
  return new;
end
$function$;

drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created
after insert on auth.users
for each row execute function public.handle_new_user();

-- Repara cuentas Auth/Google existentes que todavía no tengan perfil. No toca
-- perfiles existentes ni degrada roles asignados manualmente.
insert into public.profiles(id,display_name,role,avatar_url)
select
  u.id,
  coalesce(u.raw_user_meta_data->>'name',u.raw_user_meta_data->>'full_name',split_part(coalesce(u.email,''),'@',1),''),
  'ecociudadano'::public.app_role,
  coalesce(u.raw_user_meta_data->>'avatar_url',u.raw_user_meta_data->>'picture')
from auth.users u
left join public.profiles p on p.id=u.id
where p.id is null
on conflict(id) do nothing;

-- Comprobación de rol sin recursión RLS.
create or replace function public.current_app_role()
returns public.app_role
language sql stable security definer
set search_path = ''
set row_security = 'off'
as $function$
  select role from public.profiles where id=auth.uid()
$function$;
revoke all on function public.current_app_role() from public,anon;
grant execute on function public.current_app_role() to authenticated,service_role;

-- Reemplaza las políticas históricas de profiles para evitar 42P17 y permitir
-- que un SuperAdmin reciba cambios de perfiles por Realtime.
do $do$
declare policy_name text;
begin
  for policy_name in select policyname from pg_policies where schemaname='public' and tablename='profiles'
  loop execute format('drop policy if exists %I on public.profiles',policy_name); end loop;
end
$do$;

alter table public.profiles enable row level security;
create policy "profiles_read_own" on public.profiles for select to authenticated using(id=auth.uid());
create policy "profiles_read_superadmin" on public.profiles for select to authenticated using(public.current_app_role()='superadmin'::public.app_role);
create policy "profiles_update_own" on public.profiles for update to authenticated using(id=auth.uid()) with check(id=auth.uid());
revoke update on table public.profiles from authenticated;
grant select on table public.profiles to authenticated;
grant update(display_name,avatar_url,updated_at) on table public.profiles to authenticated;

-- Incorpora a la publicación únicamente las tablas que existen y que todavía
-- no fueron añadidas. Esto habilita INSERT/UPDATE/DELETE sin duplicar entradas.
do $do$
declare v_table text;
begin
  foreach v_table in array array[
    'devices','zones','sensor_readings','sensor_definitions','actuator_definitions',
    'notifications','team_members','role_permissions','app_settings','profiles','device_inventory'
  ]
  loop
    if to_regclass(format('public.%I',v_table)) is not null
       and not exists(
         select 1 from pg_publication_tables
         where pubname='supabase_realtime' and schemaname='public' and tablename=v_table
       ) then
      execute format('alter publication supabase_realtime add table public.%I',v_table);
    end if;
  end loop;
end
$do$;

-- UPDATE/DELETE entregan la fila completa cuando las políticas lo permiten.
alter table public.profiles replica identity full;
alter table public.devices replica identity full;
alter table public.zones replica identity full;

notify pgrst, 'reload schema';
commit;

-- Diagnóstico: Google/email deben crear una fila con ecociudadano.
select u.id,u.email,u.raw_app_meta_data->>'provider' as provider,p.role
from auth.users u left join public.profiles p on p.id=u.id
order by u.created_at desc;
