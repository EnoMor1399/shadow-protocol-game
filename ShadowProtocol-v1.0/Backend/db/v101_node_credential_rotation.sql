-- Shadow Protocol v1.0.1
-- Expiring node credentials with bounded previous-credential overlap for zero-downtime rotation.

alter table game_server_nodes add column if not exists credential_expires_at timestamptz;
alter table game_server_nodes add column if not exists previous_credential_hash text;
alter table game_server_nodes add column if not exists previous_credential_valid_until timestamptz;

alter table game_server_nodes drop constraint if exists game_server_nodes_previous_credential_hash_length;
alter table game_server_nodes add constraint game_server_nodes_previous_credential_hash_length
  check (previous_credential_hash is null or length(previous_credential_hash)=64);

update game_server_nodes
set credential_expires_at=coalesce(credential_expires_at,now()+interval '6 hours'),
    updated_at=now()
where credential_hash is not null and credential_revoked_at is null;

create index if not exists idx_game_server_nodes_credential_expiry
  on game_server_nodes(status,credential_expires_at,previous_credential_valid_until);
