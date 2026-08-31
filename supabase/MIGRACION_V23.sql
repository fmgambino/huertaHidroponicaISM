-- Proyecto H² v2.3 · Ejecutar completa sobre la base existente.
create extension if not exists pgcrypto;
alter table public.profiles add column if not exists updated_at timestamptz not null default now();
alter table public.team_members add column if not exists user_id uuid references auth.users(id) on delete set null;
alter table public.team_members add column if not exists team_name text not null default 'Team Lab';
alter table public.team_members add column if not exists email text;
create unique index if not exists team_members_user_id_uidx on public.team_members(user_id) where user_id is not null;

create or replace function public.handle_new_user() returns trigger language plpgsql security definer set search_path=public as $$
begin
  insert into public.profiles(id,display_name,role,avatar_url)
  values(new.id,coalesce(new.raw_user_meta_data->>'name',new.raw_user_meta_data->>'full_name',split_part(coalesce(new.email,''),'@',1)),'ecociudadano',new.raw_user_meta_data->>'avatar_url')
  on conflict(id) do update set display_name=coalesce(nullif(public.profiles.display_name,''),excluded.display_name),avatar_url=coalesce(public.profiles.avatar_url,excluded.avatar_url),updated_at=now();
  return new;
end$$;
drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created after insert on auth.users for each row execute function public.handle_new_user();

insert into public.profiles(id,display_name,role,avatar_url)
select u.id,coalesce(u.raw_user_meta_data->>'name',u.raw_user_meta_data->>'full_name',split_part(coalesce(u.email,''),'@',1)),'ecociudadano',u.raw_user_meta_data->>'avatar_url'
from auth.users u left join public.profiles p on p.id=u.id where p.id is null on conflict(id) do nothing;

-- Promover el primer SuperAdmin (reemplazar el email y quitar los dos guiones):
-- update public.profiles set role='superadmin',updated_at=now()
-- where id=(select id from auth.users where lower(email)=lower('TU_EMAIL'));

select u.id,u.email,p.display_name,p.role from auth.users u left join public.profiles p on p.id=u.id order by u.created_at;
