create extension if not exists pgcrypto;

create table if not exists users (
  id uuid primary key default gen_random_uuid(), email text unique not null,
  password_hash text, status text not null default 'active', trust_score numeric(5,2) not null default 75,
  created_at timestamptz not null default now(), last_seen_at timestamptz
);
create table if not exists profiles (
  user_id uuid primary key references users(id) on delete cascade, callsign varchar(32) unique,
  display_name varchar(64), account_level int not null default 1, xp bigint not null default 0,
  faction_reputation jsonb not null default '{}'::jsonb, operator_proficiency jsonb not null default '{}'::jsonb,
  settings jsonb not null default '{}'::jsonb
);
create table if not exists characters (
  id uuid primary key default gen_random_uuid(), user_id uuid not null references users(id) on delete cascade,
  face_key text, uniform_key text, helmet_key text, gloves_key text, boots_key text, vest_key text,
  patch_key text, watch_key text, radio_key text, role text not null default 'breacher'
);
create table if not exists weapons (
  id uuid primary key default gen_random_uuid(), code text unique not null, category text not null, base_stats jsonb not null
);
create table if not exists weapon_cosmetics (
  id uuid primary key default gen_random_uuid(), weapon_id uuid references weapons(id), code text unique not null,
  cosmetic_type text not null, rarity text not null default 'standard'
);
create table if not exists inventory (
  user_id uuid references users(id) on delete cascade, item_type text not null, item_id uuid not null,
  quantity int not null default 1, protected boolean not null default false, primary key(user_id,item_type,item_id)
);
create table if not exists loadouts (
  id uuid primary key default gen_random_uuid(), user_id uuid not null references users(id) on delete cascade,
  name text not null, role text not null, primary_weapon_id uuid references weapons(id), secondary_weapon_id uuid references weapons(id),
  gadgets jsonb not null default '[]'::jsonb, cosmetics jsonb not null default '{}'::jsonb
);
create table if not exists matches (
  id uuid primary key default gen_random_uuid(), mode text not null, map_code text not null, region text,
  ranked boolean not null default false, started_at timestamptz, ended_at timestamptz, winner_team text,
  server_build text, outcome_reason text, objective_state jsonb not null default '{}'::jsonb
);
create table if not exists match_players (
  match_id uuid references matches(id) on delete cascade, user_id uuid references users(id), team text not null,
  role text, kills int not null default 0, deaths int not null default 0, assists int not null default 0,
  tactical_score int not null default 0, intel_recovered int not null default 0, objective_actions int not null default 0,
  route_trace jsonb, primary key(match_id,user_id)
);
create table if not exists statistics (
  user_id uuid primary key references users(id) on delete cascade, matches bigint not null default 0,
  wins bigint not null default 0, losses bigint not null default 0, eliminations bigint not null default 0,
  assists bigint not null default 0, intel_recovered bigint not null default 0, extractions bigint not null default 0,
  revives bigint not null default 0, hacks bigint not null default 0
);
create table if not exists rankings (
  user_id uuid primary key references users(id) on delete cascade, season_id uuid,
  skill_rating numeric(10,2) not null default 1000, rank_code text not null default 'RECRUIT',
  recent_performance jsonb not null default '[]'::jsonb, updated_at timestamptz not null default now()
);
create table if not exists task_forces (
  id uuid primary key default gen_random_uuid(), name varchar(48) unique not null, tag varchar(6) unique not null,
  emblem_key text, commander_user_id uuid not null references users(id), stats jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now()
);
create table if not exists task_force_members (
  task_force_id uuid references task_forces(id) on delete cascade, user_id uuid references users(id) on delete cascade,
  role text not null default 'member', joined_at timestamptz not null default now(), primary key(task_force_id,user_id)
);
create table if not exists friends (
  user_id uuid references users(id) on delete cascade, friend_user_id uuid references users(id) on delete cascade,
  state text not null default 'pending', primary key(user_id,friend_user_id), check(user_id<>friend_user_id)
);
create table if not exists parties (
  id uuid primary key default gen_random_uuid(), leader_user_id uuid references users(id), region text, state text not null default 'lobby', created_at timestamptz default now()
);
create table if not exists party_members (
  party_id uuid references parties(id) on delete cascade, user_id uuid references users(id) on delete cascade,
  joined_at timestamptz default now(), primary key(party_id,user_id)
);
create table if not exists reports (
  id uuid primary key default gen_random_uuid(), reporter_user_id uuid references users(id), target_user_id uuid references users(id),
  match_id uuid references matches(id), category text not null, details text, evidence jsonb not null default '{}'::jsonb,
  status text not null default 'open', created_at timestamptz default now()
);
create table if not exists bans (
  id uuid primary key default gen_random_uuid(), user_id uuid references users(id), reason text not null, scope text not null,
  starts_at timestamptz default now(), ends_at timestamptz, hardware_reference_hash text, active boolean not null default true
);
create table if not exists seasons (
  id uuid primary key default gen_random_uuid(), code text unique not null, name text not null, starts_at timestamptz not null,
  ends_at timestamptz not null, operation_theme jsonb not null default '{}'::jsonb
);
create table if not exists battle_pass_progress (
  user_id uuid references users(id) on delete cascade, season_id uuid references seasons(id) on delete cascade,
  xp bigint not null default 0, tier int not null default 0, premium boolean not null default false,
  claimed_rewards jsonb not null default '[]'::jsonb, primary key(user_id,season_id)
);
create table if not exists missions (
  id uuid primary key default gen_random_uuid(), code text unique not null, title text not null, mission_type text not null,
  definition jsonb not null, active boolean not null default true
);
create table if not exists contracts (
  id uuid primary key default gen_random_uuid(), code text unique not null, faction text, definition jsonb not null,
  starts_at timestamptz, ends_at timestamptz
);
create table if not exists rewards (
  id uuid primary key default gen_random_uuid(), code text unique not null, reward_type text not null, payload jsonb not null
);
create table if not exists transactions (
  id uuid primary key default gen_random_uuid(), user_id uuid references users(id), currency text not null,
  amount bigint not null, transaction_type text not null, item_reference text, provider_reference text,
  created_at timestamptz default now()
);
create table if not exists anti_cheat_events (
  id bigserial primary key, match_id uuid references matches(id), user_id uuid references users(id), signal text not null,
  severity smallint not null, evidence jsonb not null default '{}'::jsonb, created_at timestamptz default now()
);
create table if not exists match_replays (
  id uuid primary key default gen_random_uuid(), match_id uuid unique references matches(id) on delete cascade,
  object_key text not null, duration_seconds int, event_index jsonb not null default '[]'::jsonb,
  created_at timestamptz default now()
);
create index if not exists idx_matches_started on matches(started_at desc);
create index if not exists idx_ac_user_created on anti_cheat_events(user_id,created_at desc);
create index if not exists idx_reports_target on reports(target_user_id,status);

-- v0.3 tactical telemetry used by Operation Review and replay indexing.
create table if not exists match_events (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  user_id uuid references users(id) on delete set null,
  event_type text not null,
  game_time_ms int not null,
  position jsonb,
  payload jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now()
);
create index if not exists idx_match_events_match_time on match_events(match_id,game_time_ms);

-- v0.4 competitive round model. Dedicated servers remain authoritative.
create table if not exists match_rounds (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  round_number int not null,
  attacking_team text not null,
  defending_team text not null,
  winner_team text,
  outcome_reason text,
  started_at timestamptz,
  ended_at timestamptz,
  objective_state jsonb not null default '{}'::jsonb,
  unique(match_id,round_number)
);
create index if not exists idx_match_rounds_match on match_rounds(match_id,round_number);

create table if not exists tactical_equipment_events (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  round_number int not null,
  user_id uuid references users(id) on delete set null,
  equipment_type text not null,
  position jsonb,
  affected_entities jsonb not null default '[]'::jsonb,
  game_time_ms int not null,
  created_at timestamptz not null default now()
);
create index if not exists idx_tactical_equipment_match on tactical_equipment_events(match_id,round_number,game_time_ms);


-- v0.5 authoritative 5v5 slot, fortification, ballistics and observer telemetry.
create table if not exists match_player_slots (
  match_id uuid not null references matches(id) on delete cascade,
  round_number int not null,
  slot_index int not null check(slot_index between 0 and 9),
  user_id uuid references users(id) on delete set null,
  team text not null,
  tactical_side text not null check(tactical_side in ('attack','defense')),
  spawn_group text,
  connection_state text not null default 'connected',
  joined_at timestamptz not null default now(),
  primary key(match_id,round_number,slot_index)
);
create index if not exists idx_match_player_slots_user on match_player_slots(user_id,match_id);

create table if not exists fortification_events (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  round_number int not null,
  user_id uuid references users(id) on delete set null,
  action text not null check(action in ('placed','damaged','destroyed')),
  fortification_type text not null default 'barricade',
  position jsonb not null,
  health_remaining numeric(7,2),
  game_time_ms int not null,
  created_at timestamptz not null default now()
);
create index if not exists idx_fortification_match on fortification_events(match_id,round_number,game_time_ms);

create table if not exists ballistic_events (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  round_number int not null,
  shooter_user_id uuid references users(id) on delete set null,
  event_type text not null check(event_type in ('penetration','suppression')),
  material text,
  target_user_id uuid references users(id) on delete set null,
  position jsonb,
  payload jsonb not null default '{}'::jsonb,
  game_time_ms int not null,
  created_at timestamptz not null default now()
);
create index if not exists idx_ballistic_match on ballistic_events(match_id,round_number,game_time_ms);

create table if not exists observer_sessions (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  user_id uuid references users(id) on delete set null,
  round_number int not null,
  mode text not null check(mode in ('team-follow','free-camera')),
  target_user_id uuid references users(id) on delete set null,
  started_at timestamptz not null default now(),
  ended_at timestamptz
);


-- v0.6 competitive ready-room, reconnect and live match telemetry.
alter table match_player_slots add column if not exists ready boolean not null default false;
alter table match_player_slots add column if not exists reconnect_token_hash text;
alter table match_player_slots add column if not exists reconnect_deadline timestamptz;

create table if not exists kill_feed_events (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  round_number int not null,
  killer_user_id uuid references users(id) on delete set null,
  victim_user_id uuid references users(id) on delete set null,
  killer_team text not null,
  victim_team text not null,
  weapon_code text,
  headshot boolean not null default false,
  game_time_ms int not null,
  created_at timestamptz not null default now()
);
create index if not exists idx_kill_feed_match on kill_feed_events(match_id,round_number,game_time_ms);

create table if not exists match_overtime_events (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  round_number int not null,
  trigger_reason text not null,
  duration_seconds int not null default 30,
  started_at timestamptz not null default now(),
  ended_at timestamptz
);
create index if not exists idx_overtime_match on match_overtime_events(match_id,round_number);

-- v0.7 authenticated game sessions, dedicated-server allocation and authoritative combat telemetry.
create table if not exists game_sessions (
  id uuid primary key,
  user_id uuid not null references users(id) on delete cascade,
  region text not null,
  build text not null,
  device_nonce_hash text not null,
  created_at timestamptz not null default now(),
  expires_at timestamptz not null,
  revoked_at timestamptz
);
create index if not exists idx_game_sessions_user_exp on game_sessions(user_id,expires_at desc);

create table if not exists server_allocations (
  id uuid primary key,
  match_id uuid not null references matches(id) on delete cascade,
  server_id text not null,
  region text not null,
  status text not null check(status in ('reserved','starting','ready','live','draining','closed','failed')),
  connect_token_hash text not null,
  created_at timestamptz not null default now(),
  expires_at timestamptz not null,
  started_at timestamptz,
  ended_at timestamptz
);
create index if not exists idx_server_allocations_match on server_allocations(match_id,status);
create index if not exists idx_server_allocations_region on server_allocations(region,status,expires_at);

create table if not exists combat_events (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  round_number int not null check(round_number between 1 and 9),
  shooter_user_id uuid not null references users(id) on delete cascade,
  victim_user_id uuid not null references users(id) on delete cascade,
  event_type text not null check(event_type in ('damage','elimination','assist','team_damage','headshot')),
  weapon_code text not null,
  damage numeric(8,2) not null default 0,
  body_zone text not null default 'torso',
  client_shot_age_ms numeric(8,2) not null default 0,
  server_shot_time_ms bigint not null,
  friendly_fire boolean not null default false,
  payload jsonb not null default '{}'::jsonb,
  created_at timestamptz not null default now()
);
create index if not exists idx_combat_events_match_round on combat_events(match_id,round_number,server_shot_time_ms);
create index if not exists idx_combat_events_shooter on combat_events(shooter_user_id,created_at desc);
alter table match_rounds add column if not exists objective_site_code text;

-- v0.8 environment / presentation telemetry used for replay diagnostics and balancing.
create table if not exists environment_events (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  round_number int not null check(round_number between 1 and 9),
  user_id uuid references users(id) on delete set null,
  event_type text not null check(event_type in ('sector_enter','footstep','breach_fx','suppression_fx','weapon_state')),
  sector_code text,
  surface_profile text,
  position jsonb,
  payload jsonb not null default '{}'::jsonb,
  game_time_ms int not null,
  created_at timestamptz not null default now()
);
create index if not exists idx_environment_events_match_round on environment_events(match_id,round_number,game_time_ms);

-- v0.9 tactical interaction telemetry. Competitive state is still decided by the dedicated server.
create table if not exists tactical_interaction_events (
  id bigserial primary key,
  match_id uuid not null references matches(id) on delete cascade,
  round_number int not null check(round_number between 1 and 9),
  user_id uuid references users(id) on delete set null,
  event_type text not null check(event_type in ('door_peek','door_open','door_breach','camera_destroyed','light_destroyed','vault','lean','optic_change','vertical_route')),
  object_code text,
  sector_code text,
  position jsonb,
  payload jsonb not null default '{}'::jsonb,
  game_time_ms int not null,
  created_at timestamptz not null default now()
);
create index if not exists idx_tactical_interaction_match on tactical_interaction_events(match_id,round_number,game_time_ms);
