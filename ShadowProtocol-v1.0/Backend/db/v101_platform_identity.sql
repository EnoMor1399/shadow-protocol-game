-- Shadow Protocol v1.0.1
-- Trusted platform identity assertion exchange and stable backend-account binding.

create table if not exists platform_identities (
  provider text not null,
  provider_subject_hash text not null,
  user_id uuid not null references users(id) on delete cascade,
  created_at timestamptz not null default now(),
  last_seen_at timestamptz not null default now(),
  primary key(provider,provider_subject_hash),
  constraint platform_identity_subject_hash_length check(length(provider_subject_hash)=64)
);

create index if not exists idx_platform_identities_user
  on platform_identities(user_id,provider);

create table if not exists platform_identity_assertions (
  id uuid primary key,
  provider text not null,
  provider_subject_hash text not null,
  issued_at timestamptz not null,
  expires_at timestamptz not null,
  consumed_at timestamptz not null default now(),
  constraint platform_assertion_subject_hash_length check(length(provider_subject_hash)=64)
);

create index if not exists idx_platform_identity_assertions_expiry
  on platform_identity_assertions(expires_at);

alter table game_sessions
  add column if not exists auth_provider text;

alter table game_sessions
  add column if not exists identity_assertion_id uuid references platform_identity_assertions(id) on delete set null;

create unique index if not exists idx_game_sessions_identity_assertion
  on game_sessions(identity_assertion_id)
  where identity_assertion_id is not null;
