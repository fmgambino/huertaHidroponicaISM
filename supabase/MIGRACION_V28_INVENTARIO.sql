-- Inventario privado para el enrolamiento automático de equipos fabricados.
create table if not exists public.device_inventory (
  id text primary key check (id ~ '^ESP32-[0-9A-F]{4}$'),
  mac text not null unique,
  claim_code text not null,
  secret_hash text not null,
  status text not null default 'available' check (status in ('available','claimed','blocked','retired')),
  firmware text,
  first_seen timestamptz not null default now(),
  last_seen timestamptz not null default now(),
  claimed_by uuid references auth.users(id) on delete set null,
  claimed_at timestamptz,
  metadata jsonb not null default '{}'
);

alter table public.device_inventory enable row level security;
drop policy if exists "inventory superadmin read" on public.device_inventory;
drop policy if exists "inventory superadmin update" on public.device_inventory;
create policy "inventory superadmin read" on public.device_inventory for select to authenticated
using (public.current_role() = 'superadmin');
create policy "inventory superadmin update" on public.device_inventory for update to authenticated
using (public.current_role() = 'superadmin') with check (public.current_role() = 'superadmin');
grant select,update on public.device_inventory to authenticated;

update public.role_permissions
set permissions = array_append(permissions,'inventory'), updated_at=now()
where role='superadmin' and not ('inventory'=any(permissions));
