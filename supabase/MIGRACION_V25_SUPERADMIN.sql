-- Vincula el usuario Auth existente con profiles y lo promueve a SuperAdmin.
-- Ejecutar en Supabase SQL Editor.
insert into public.profiles(id,display_name,role,avatar_url,created_at,updated_at)
select id,'Fernando Gambino','superadmin',raw_user_meta_data->>'avatar_url',coalesce(created_at,now()),now()
from auth.users
where lower(email)=lower('fernando.m.gambino@gmail.com')
on conflict(id) do update set display_name='Fernando Gambino',role='superadmin',updated_at=now();

select u.id,u.email,p.display_name,p.role
from auth.users u join public.profiles p on p.id=u.id
where lower(u.email)=lower('fernando.m.gambino@gmail.com');
