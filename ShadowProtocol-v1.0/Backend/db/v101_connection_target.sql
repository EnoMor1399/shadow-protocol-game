-- Shadow Protocol v1.0.1
-- Persist the client-connectable target and one-time admission state returned by the allocation service.

alter table server_allocations
  add column if not exists user_id uuid references users(id) on delete set null;

alter table server_allocations
  add column if not exists connect_host text;

alter table server_allocations
  add column if not exists connect_port int;

alter table server_allocations
  add column if not exists connect_token_consumed_at timestamptz;

alter table server_allocations
  drop constraint if exists server_allocations_connect_port_range;

alter table server_allocations
  add constraint server_allocations_connect_port_range
  check (connect_port is null or connect_port between 1 and 65535);

create index if not exists idx_server_allocations_user
  on server_allocations(user_id,status,expires_at);
