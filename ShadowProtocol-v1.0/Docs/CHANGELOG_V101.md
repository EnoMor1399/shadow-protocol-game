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
- Blueprint/UMG delegates for verified, upgrade-required, allocation-success and request-failure states;
- in-memory installation of a signed game-session token supplied by a trusted identity/platform layer;
- Bearer-authenticated Embassy / Protocol server allocation;
- allocation build validation before handing connection data to the travel layer;
- no `SESSION_BOOTSTRAP_SECRET` embedded in the game client.

`ShadowProtocol.Build.cs` now includes `HTTP`, `Json` and `JsonUtilities` for this bridge.

## Backend protocol 0.8.0

The Fastify backend now enforces the v1.0.1 network build at the authenticated session boundary and again during server allocation.

- Backend package/protocol advances from `0.7.0` to `0.8.0`.
- Required network build defaults to `SP-1.0.1`.
- `ACCEPTED_NETWORK_BUILDS` can define an explicit server-side compatibility set for controlled rollout windows.
- Unsupported clients receive HTTP `426` with `client-build-incompatible` plus the accepted build metadata.
- Session tokens now bind the authenticated user to both `build` and backend `protocol`.
- Match allocation re-validates the signed build/protocol before reserving a server.
- Allocated matches persist the network build as `server_build` instead of the old protocol-generation literal.
- Reconnect ticket issuance and reconnect completion also enforce the signed compatibility state.
- `/health`, `/v1/compatibility`, allocation responses and WebSocket hello messages expose the active release/network/protocol identity.

This makes build compatibility server-owned rather than a presentation-only client string.

## CI and integration validation

GitHub Actions now runs:

- browser JavaScript syntax checks for `game.js` and `v1.js`;
- strict backend TypeScript typecheck;
- an executable compatibility-handshake integration test that boots the backend without PostgreSQL/Redis, verifies the compatibility contract, rejects `SP-0.9.0`, issues a signed `SP-1.0.1` session and allocates an Embassy / Protocol server with the expected network/protocol metadata.

The first TypeScript CI pass exposed two real issues in the backend (`ioredis` NodeNext constructor import and an implicit-any WebSocket message parameter); both were corrected without weakening strict TypeScript settings.

## Validation boundary

Browser syntax and backend TypeScript/compatibility behavior are covered by GitHub Actions. The Unreal v1.0.1 files are source-level additions in this environment; Unreal Header Tool, UE C++ compilation, PIE, packaged-client testing and dedicated-server multiplayer testing still require a full Unreal Engine 5 environment.

See `UE_SESSION_BRIDGE_V101.md` for the secure Unreal-to-backend integration flow.
