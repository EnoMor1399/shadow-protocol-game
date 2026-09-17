# Shadow Protocol v1.0.1 — Stability Patch

v1.0.1 is a focused stabilization and production-readiness patch for the Embassy / Protocol vertical slice. It does not replace the v1.0 gameplay foundation or expand scope into a new map/mode.

## Browser vertical slice

- Updates visible build identity to `v1.0.1 // STABILITY PATCH` and `SP-1.0.1`.
- Migrates presentation settings from the v1.0 storage namespace while remaining safe when browser storage is blocked or unavailable.
- Adds persistent Input Hints and Intel Notices preferences.
- Fixes Escape handling so the settings surface can be closed predictably during live play.
- Hardens repeated deployment transitions by clearing stale step timers before a new deployment sequence begins.
- Adds client build integrity diagnostics to Settings and the ready room.
- Adds live secure-session status to the HUD and mirrors reconnect/degraded states.
- Prevents objective-site notification timers from stacking when objective state changes quickly.
- Adds cleanup for observers, polling timers and managed timeouts during page teardown.
- Keeps v1.0 reduced-motion and enhanced-contrast behavior intact.

## Unreal Engine foundation

Added `USPBuildInfoLibrary` to expose a single Blueprint/C++ source for:

- release version: `1.0.1`;
- network build id: `SP-1.0.1`;
- content revision: `EMBASSY-PROTOCOL-101`;
- exact network-build compatibility checks for ranked/dev integration.

Added `USPBackendSessionSubsystem` as the Unreal client bridge for the backend contract:

- public `/v1/compatibility` checks before matchmaking;
- Blueprint/UMG delegates for verified, upgrade-required, allocation-success, session-refresh, reconnect and request-failure states;
- in-memory installation of a signed game-session token supplied by a trusted identity/platform layer;
- Bearer-authenticated Embassy / Protocol server allocation;
- Blueprint-visible `ConnectHost` / `ConnectPort` plus connect token on successful allocation;
- client-side rejection of allocation responses without a valid host/port target;
- authenticated `/v1/auth/refresh` token rotation for matches longer than the initial 15-minute session lifetime;
- reserved-slot reconnect ticket + reconnect completion flows;
- allocation/reconnect build validation before handing connection data to the travel layer;
- no `SESSION_BOOTSTRAP_SECRET` or `MATCH_SERVER_SECRET` embedded in the game client.

`ShadowProtocol.Build.cs` includes `HTTP`, `Json` and `JsonUtilities` for this bridge.

## Backend protocol 0.8.0

The Fastify backend now enforces v1.0.1 build identity and explicit trust boundaries across authentication, matchmaking, allocation, admission and authoritative match services.

- Backend package/protocol advances from `0.7.0` to `0.8.0`.
- Required network build defaults to `SP-1.0.1`.
- `ACCEPTED_NETWORK_BUILDS` defines an explicit server-side compatibility set for controlled rollout windows.
- Unsupported clients receive HTTP `426` with `client-build-incompatible` plus accepted-build metadata.
- Session tokens bind the authenticated user to `build`, region and backend `protocol`.
- `/v1/auth/refresh` rotates a still-valid compatible session and extends PostgreSQL/Redis expiry when configured.
- Match allocation re-validates the signed build/protocol before reserving a server and persists the session network build as `server_build`.
- Allocation returns `connectHost`, `connectPort` and a short-lived connect token; configured targets are persisted with `server_allocations` when PostgreSQL is enabled.
- Development may use `127.0.0.1:7777`; production has no localhost fallback and returns `503 game-server-connect-target-not-configured` when routing configuration is absent or invalid.
- `Backend/scripts/apply-db.mjs` provides repeatable `db:init` and `db:upgrade:v101` database paths.
- `Backend/db/v101_connection_target.sql` adds allocation user ownership, connection target columns and token-consumption state for existing databases.
- Allocation tokens are stored only as hashes and are bound to the authenticated session user.
- Trusted dedicated servers redeem `POST /v1/matches/admit` with `MATCH_SERVER_SECRET`, allocation id, match id and the presented connect token before admitting a player.
- Admission atomically verifies allocation identity, token hash, expiry, status and unused state; success marks the allocation `live` and records `connect_token_consumed_at`.
- Replaying a consumed connect token is rejected with `403 admission-denied`.
- Matchmaking requires the signed compatible session; the client no longer controls authoritative `userId`, skill rating or trust score.
- Matchmaking region must match the signed session region; skill/trust are read from server-owned database state when PostgreSQL is configured.
- `MATCH_SERVER_SECRET` gates admission plus anti-cheat, match telemetry, round results, tactical equipment, player slots, fortification, ballistic, kill-feed, overtime, environment/combat and tactical-interaction writes.
- Reconnect ticket issuance and reconnect completion enforce signed compatibility and database-backed slot ownership.
- Ready-state and reconnect mutations fail closed when PostgreSQL is unavailable because ownership cannot be proven safely.
- `/health`, `/v1/compatibility`, allocation/reconnect responses and WebSocket hello messages expose the active release/network/protocol identity.

This makes compatibility, matchmaking identity, connection routing, connection admission and authoritative event ownership server-controlled rather than client-asserted.

## PostgreSQL migration and initialization

Fresh databases can now be initialized with:

```bash
npm run db:init
```

Existing v1.0 databases can apply only the v1.0.1 allocation/admission migration with:

```bash
npm run db:upgrade:v101
```

The migration adds the client connection target, owning user, one-time token consumption timestamp, port validation and allocation lookup index without requiring destructive schema recreation.

## CI and integration validation

GitHub Actions runs three validation jobs:

- browser JavaScript syntax checks for `game.js` and `v1.js`;
- strict backend TypeScript plus no-database compatibility/security tests;
- a PostgreSQL 16 integration job that initializes the real schema and validates persistent production paths.

The no-database suite verifies:
- compatibility metadata;
- rejection of `SP-0.9.0` before session issuance;
- signed `SP-1.0.1` session creation;
- authenticated session refresh/rotation;
- Embassy / Protocol server allocation with `connectHost` / `connectPort`;
- production allocation fails closed when public server routing is not configured;
- authenticated matchmaking identity ignores spoofed client identity/rating/trust fields;
- rejection of public-client authoritative match telemetry;
- acceptance of the same telemetry with the dedicated-server credential;
- fail-closed reconnect/ready-state ownership behavior when PostgreSQL is unavailable.

The PostgreSQL integration job verifies:
- base schema plus v1.0.1 migration initialization;
- allocation host/port and hashed connect-token persistence;
- allocation ownership bound to the authenticated user;
- public admission attempts rejected;
- incorrect connect tokens rejected;
- valid dedicated-server admission succeeds once;
- replay of the same connect token is rejected;
- admitted allocation transitions to `live` with consumption timestamp;
- authenticated ready-state ownership;
- hashed 90-second reconnect ticket storage;
- invalid reconnect rejection and valid reconnect recovery;
- reconnect token/deadline cleanup after restoration.

The first TypeScript CI pass exposed two real issues in the backend (`ioredis` NodeNext constructor import and an implicit-any WebSocket message parameter); both were corrected without weakening strict TypeScript settings.

## Validation boundary

Browser syntax and backend TypeScript/security/database behavior are covered by GitHub Actions. The Unreal v1.0.1 files are source-level additions in this environment; Unreal Header Tool, UE C++ compilation, PIE, packaged-client testing and dedicated-server multiplayer testing still require a full Unreal Engine 5 environment.

See `UE_SESSION_BRIDGE_V101.md` for the secure Unreal-to-backend integration flow.
