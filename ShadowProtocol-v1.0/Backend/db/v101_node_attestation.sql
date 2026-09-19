-- Shadow Protocol v1.0.1
-- Orchestrator-backed, single-use server registration attestations.

create table if not exists server_node_attestations (
  id uuid primary key,
  server_id text not null,
  region text not null,
  network_build text not null,
  issued_at timestamptz not null,
  expires_at timestamptz not null,
  consumed_at timestamptz not null default now()
);

create index if not exists idx_server_node_attestations_expiry
  on server_node_attestations(expires_at);

create index if not exists idx_server_node_attestations_server
  on server_node_attestations(server_id,consumed_at desc);

alter table game_server_nodes
  add column if not exists last_attestation_id uuid references server_node_attestations(id) on delete set null;

alter table game_server_nodes
  add column if not exists last_attested_at timestamptz;

create index if not exists idx_game_server_nodes_attestation
  on game_server_nodes(last_attested_at desc);
