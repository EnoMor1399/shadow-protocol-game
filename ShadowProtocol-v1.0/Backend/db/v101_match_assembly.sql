-- Shadow Protocol v1.0.1
-- Shared 5v5 PROTOCOL match assembly metadata.

alter table matches add column if not exists assembly_state text;
alter table matches add column if not exists target_players int;
alter table matches add column if not exists assembled_at timestamptz;
alter table matches add column if not exists created_at timestamptz not null default now();

update matches set assembly_state=coalesce(assembly_state,'ready');
update matches set target_players=coalesce(target_players,10);

alter table matches alter column assembly_state set default 'ready';
alter table matches alter column assembly_state set not null;
alter table matches alter column target_players set default 10;
alter table matches alter column target_players set not null;

alter table matches drop constraint if exists matches_assembly_state_check;
alter table matches add constraint matches_assembly_state_check
  check (assembly_state in ('assembling','ready','live','closed','failed'));

alter table matches drop constraint if exists matches_target_players_check;
alter table matches add constraint matches_target_players_check
  check (target_players between 1 and 10);

create index if not exists idx_matches_assembly
  on matches(region,server_build,mode,map_code,ranked,assembly_state,created_at)
  where ended_at is null;

create index if not exists idx_server_allocations_user_active
  on server_allocations(user_id,status)
  where user_id is not null;
