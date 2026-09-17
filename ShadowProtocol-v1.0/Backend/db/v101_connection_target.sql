-- Shadow Protocol v1.0.1
-- Persist the client-connectable target returned by the allocation service.

alter table server_allocations
  add column if not exists connect_host text;

alter table server_allocations
  add column if not exists connect_port int;

alter table server_allocations
  drop constraint if exists server_allocations_connect_port_range;

alter table server_allocations
  add constraint server_allocations_connect_port_range
  check (connect_port is null or connect_port between 1 and 65535);
