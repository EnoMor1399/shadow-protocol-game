-- Shadow Protocol v1.0.1
-- Per-node dedicated-server credentials. The shared registration secret is bootstrap-only.

alter table game_server_nodes add column if not exists credential_hash text;
alter table game_server_nodes add column if not exists credential_issued_at timestamptz;
alter table game_server_nodes add column if not exists credential_revoked_at timestamptz;

alter table game_server_nodes drop constraint if exists game_server_nodes_credential_hash_length;
alter table game_server_nodes add constraint game_server_nodes_credential_hash_length
  check (credential_hash is null or length(credential_hash)=64);

update game_server_nodes
set status='offline',updated_at=now()
where credential_hash is null and status<>'offline';

create index if not exists idx_game_server_nodes_credential_state
  on game_server_nodes(server_id,status,credential_revoked_at);
