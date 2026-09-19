-- Shadow Protocol v1.0.1
-- Regional healthy-server registry used by the Embassy / Protocol allocator.

create table if not exists game_server_nodes (
  id uuid primary key default gen_random_uuid(),
  server_id text unique not null,
  region text not null,
  network_build text not null,
  public_host text not null,
  public_port int not null check(public_port between 1 and 65535),
  status text not null default 'ready' check(status in ('ready','draining','offline')),
  capacity int not null default 1 check(capacity between 1 and 128),
  active_allocations int not null default 0 check(active_allocations >= 0),
  last_heartbeat_at timestamptz not null default now(),
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

create index if not exists idx_game_server_nodes_scheduler
  on game_server_nodes(region,network_build,status,last_heartbeat_at desc);

alter table server_allocations
  add column if not exists node_id uuid references game_server_nodes(id) on delete set null;

create index if not exists idx_server_allocations_node
  on server_allocations(node_id,status,expires_at);
