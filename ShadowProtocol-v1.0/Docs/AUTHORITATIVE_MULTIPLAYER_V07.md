# Shadow Protocol v0.7 — Authoritative Multiplayer & Combat Layer

## Goal
v0.7 advances the Embassy / Protocol vertical slice from competitive-state scaffolding into an implementation foundation for authenticated, server-authoritative 5v5 combat. The browser build remains a local mechanics simulator; the Unreal and backend layers define the production authority boundaries.

## Playable prototype additions
- Five-person friendly element: the local operator plus four squad members.
- Three rotating possible intelligence sites: `ARCHIVE-A`, `VAULT-B`, and `SAFE-C`.
- Communications intelligence reveals the actual site for the round instead of always pointing to one fixed package location.
- Server-validation simulation for each shot using connection state and client-shot age.
- Network telemetry HUD: session authority, server, tick rate, latency, packet loss, and sync state.
- 90-second reconnect-grace presentation with authenticated slot reservation.
- Headshot detection, differentiated headshot damage, headshot kill-feed marker, and Operation Review count.
- Damage-contribution assists when a squad member finishes an enemy recently damaged by the player.
- Friendly-fire discipline: reduced team damage, score penalty, incident tracking, and reverse-friendly-fire escalation after repeated incidents.
- Five-member live team-status strip.
- Correct spawn-side derivation now uses the current match round rather than stale prior-round state.

## Unreal Engine additions
- `USPLagCompensationComponent` stores short server-side location history and provides rewound capsule-location samples.
- `ASPWeaponBase` validates shot timestamps before accepting a fire request.
- Current-position traces retain penetration and suppression; a rewound capsule-location fallback provides a first lag-compensation hook.
- `ASPProtocolGameMode` owns friendly-fire scaling, reverse-friendly-fire policy, damage contribution, elimination and assist resolution.
- `ASPPlayerState` replicates authenticated session ID/state, eliminations, deaths, assists, headshots, team damage, friendly-fire incidents and discipline state.
- Authenticated ready gating can be required before a ranked match starts.
- Session-based slot reclaim restores team and spawn-group assignment after an authenticated reconnect.
- Reconnect reservations have server-side expiry enforcement.
- Team-tag and occupancy validation added to spawn selection.
- `ASPObjectiveSite` supports multiple replicated objective sites per map; the authoritative game mode selects one per round and publishes its ID through game state.
- `ASPProtocolGameState` now publishes server instance, tick rate, ranked-rules state and active objective site.

## Backend additions
- Signed short-lived game-session tokens created only through a trusted bootstrap/identity boundary.
- Match allocation endpoint returns match/server IDs, 60 Hz target tick rate, short-lived connect token and allocation expiry.
- Persistent `game_sessions` and `server_allocations` models.
- Reconnect tickets are random server-issued secrets; only hashes are stored.
- Reconnect validation requires both an authenticated game session and ownership of the reserved slot.
- Trusted dedicated-server combat event endpoint for damage, eliminations, assists, team damage and headshots.
- `combat_events` captures weapon, body zone, damage, shot age, server shot time and friendly-fire state.
- Match-round persistence carries the selected objective-site code.

## Authority rules
The production client may request actions, but it must not author competitive outcomes. The dedicated server validates movement, shot age, damage, eliminations, assists, team damage, objective state, inventory, round results and reconnect ownership. Backend combat endpoints are trusted-server writes only.

## Lag compensation status
v0.7 adds a short location-history component and rewound capsule-location fallback. This is deliberately not described as final hitbox rewind. Production networking should add skeletal hitbox history, interpolation-aware rewind, server/client clock calibration, maximum rewind policy, penetration history rules, projectile rewind rules and abuse telemetry before ranked launch.

## Still required before production multiplayer
- Compile and PIE-test all new C++ classes in the target Unreal Engine version.
- Run 10 real network clients against a dedicated server.
- Integrate the chosen platform/online identity provider with `AuthorizePlayerSession`.
- Build a server allocator/health lifecycle around the allocation records.
- Add signed dedicated-server credentials and rotation.
- Implement full skeletal hitbox rewind and reconciliation.
- Add packet-loss, latency and server-load soak tests.
- Add server-side movement prediction validation and anti-speed/teleport checks.
- Add production kill-feed multicast/UI and assist attribution windows.
- Add secure map-defined D9/HELIX PlayerStart tags for all spawn groups.
