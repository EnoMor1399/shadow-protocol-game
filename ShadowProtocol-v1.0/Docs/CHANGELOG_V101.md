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
- authenticated `/v1/auth/refresh` token rotation for matches longer than the initial 15-minute session lifetime;
- reserved-slot reconnect ticket + reconnect completion flows;
- allocation/reconnect build validation before handing connection data to the travel layer;
- no `SESSION_BOOTSTRAP_SECRET` or `MATCH_SERVER_SECRET` embedded in the game client.

`ShadowProtocol.Build.cs` includes `HTTP`, `Json` and `JsonUtilities` for this bridge.

## Backend protocol 0.8.0

The Fastify backend now enforces v1.0.1 build identity and explicit trust boundaries across authentication, matchmaking and authoritative match services.

- Backend package/protocol advances from `0.7.0` to `0.8.0`.
- Required network build defaults to `SP-1.0.1`.
- `ACCEPTED_NETWORK_BUILDS` defines an explicit server-side compatibility set for controlled rollout windows.
- Unsupported clients receive HTTP `426` with `client-build-incompatible` plus accepted-build metadata.
- Session tokens bind the authenticated user to `build`, region and backend `protocol`.
- `/v1/auth/refresh` rotates a still-valid compatible session and extends PostgreSQL/Redis expiry when configured.
- Match allocation re-validates the signed build/protocol before reserving a server and persists the session network build as `server_build`.
- Matchmaking now requires the signed compatible session; the client no longer controls authoritative `userId`, skill rating or trust score.
- Matchmaking region must match the signed session region; skill/trust are read from server-owned database state when PostgreSQL is configured.
- `MATCH_SERVER_SECRET` now gates anti-cheat, match telemetry, round results, tactical equipment, player slots, fortification, ballistic, kill-feed, overtime, environment/combat and tactical-interaction writes.
- Reconnect ticket issuance and reconnect completion enforce signed compatibility and database-backed slot ownership.
- Ready-state and reconnect mutations fail closed when PostgreSQL is unavailable because ownership cannot be proven safely.
- `/health`, `/v1/compatibility`, allocation/reconnect responses and WebSocket hello messages expose the active release/network/protocol identity.

This makes compatibility, matchmaking identity and authoritative event ownership server-controlled rather than client-asserted.

## CI and integration validation

GitHub Actions runs:

- browser JavaScript syntax checks for `game.js` and `v1.js`;
- strict backend TypeScript typecheck;
- an executable backend integration suite that boots Fastify without PostgreSQL/Redis and verifies:
  - compatibility metadata;
  - rejection of `SP-0.9.0` before session issuance;
  - signed `SP-1.0.1` session creation;
  - authenticated session refresh/rotation;
  - Embassy / Protocol server allocation;
  - authenticated matchmaking identity that ignores spoofed client identity/rating/trust fields;
  - rejection of public-client authoritative match telemetry;
  - acceptance of the same telemetry with the dedicated-server credential;
  - fail-closed reconnect/ready-state ownership behavior when PostgreSQL is unavailable.

The first TypeScript CI pass exposed two real issues in the backend (`ioredis` NodeNext constructor import and an implicit-any WebSocket message parameter); both were corrected without weakening strict TypeScript settings.

## Validation boundary

Browser syntax and backend TypeScript/security behavior are covered by GitHub Actions. The Unreal v1.0.1 files are source-level additions in this environment; Unreal Header Tool, UE C++ compilation, PIE, packaged-client testing and dedicated-server multiplayer testing still require a full Unreal Engine 5 environment.

See `UE_SESSION_BRIDGE_V101.md` for the secure Unreal-to-backend integration flow.
