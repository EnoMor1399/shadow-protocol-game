# Shadow Protocol v0.6 — Competitive Match Layer

## Goal
v0.6 converts the vertical slice from a local round simulation into a clearer 5v5 competitive match foundation. It does not claim production networking is complete; it establishes the UI, authoritative state model, persistence contracts and gameplay rules needed for the next dedicated-server pass.

## Playable prototype additions
- 10-slot Tactical Ready Room with five Directorate Nine and five Helix positions.
- Local ready/unready gate before tactical planning.
- Three selectable spawn groups with distinct Embassy approaches.
- Spawn selection carried into the live round and Operation Review.
- Ranked-style team-follow spectator restriction during live rounds.
- Live kill feed for player, squad and enemy eliminations.
- Match-point / deciding-round presentation.
- Objective-based overtime: a secured intelligence package can extend a round once for 30 seconds.
- Browser online/offline connection-state presentation and slot-reclaim messaging foundation.
- Embassy tactical callouts added to planning maps.

## Unreal Engine foundation
- `ESPConnectionState` and `FSPCompetitivePlayerSlot` replicated data model.
- `ASPPlayerState`: ready state, spawn group and connection state.
- `ASPProtocolGameState`: 10-player readiness, overtime, match point and team-only spectating state.
- `ASPProtocolGameMode`: player-slot population, ready gating, spawn selection, spawn-point choice, overtime transition and match-point calculation.
- Observer controller denies free-camera activation during Action/Overtime when team-only competitive spectating is enabled.
- Reconnecting player slots are retained in replicated state as reservations pending real session/backend ticket validation.

## Backend / persistence
- `match_player_slots` now supports ready state and reconnect metadata.
- `kill_feed_events` records authoritative elimination telemetry.
- `match_overtime_events` records overtime triggers and duration.
- Ready-state, reconnect, kill-feed and overtime API contracts added.
- These endpoints are architecture foundations; production writes must be authenticated as either the slot owner or a trusted dedicated server, as appropriate.

## Remaining for production multiplayer
- Online subsystem/session integration and authenticated identities.
- Dedicated-server allocation and actual 10-client replication tests.
- Reconnect-ticket issuance/rotation and grace-period cleanup worker.
- Server-side team balancing/party preservation.
- Network prediction/rewind/lag compensation.
- Production spawn actors tagged for every map/spawn group.
- Round-ready checks after side switching if competitive rules require them.
- Anti-cheat integration and signed trusted-server telemetry.
