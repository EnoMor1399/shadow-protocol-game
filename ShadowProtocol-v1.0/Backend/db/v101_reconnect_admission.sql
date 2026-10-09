-- A transport credential is separate from the reservation ticket. Neither stores plaintext.
alter table match_player_slots
  add column if not exists reconnect_grant_id uuid,
  add column if not exists reconnect_admission_hash text;
create unique index if not exists idx_reconnect_grant on match_player_slots(reconnect_grant_id)
  where reconnect_grant_id is not null;
