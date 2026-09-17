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

This is intentionally small and dependency-free so UMG, session creation and dedicated-server validation can consume the same build identity without duplicating literal strings.

## Backend protocol 0.8.0

The Fastify backend now enforces the v1.0.1 network build at the authenticated session boundary and again during server allocation.

- Backend package/protocol advances from `0.7.0` to `0.8.0`.
- Required network build defaults to `SP-1.0.1`.
- `ACCEPTED_NETWORK_BUILDS` can define an explicit server-side compatibility set for controlled rollout windows.
- Unsupported clients receive HTTP `426` with `client-build-incompatible` plus the accepted build metadata.
- Session tokens now bind the authenticated user to both `build` and backend `protocol`.
- Match allocation re-validates the signed build/protocol before reserving a server.
- Allocated matches persist `SP-1.0.1` as `server_build` instead of the old protocol-generation literal.
- Reconnect ticket issuance and reconnect completion also enforce the signed compatibility state.
- `/health`, `/v1/compatibility`, allocation responses and WebSocket hello messages expose the active release/network/protocol identity.

This makes build compatibility server-owned rather than a presentation-only client string.

## Validation boundary

The browser patch has been syntax-checked with Node. The backend changes have been source-reviewed and the updated compatibility flow has been checked against the existing session/allocation structure; a full backend typecheck/runtime integration still requires dependencies and configured PostgreSQL/Redis services. The Unreal files are source-level additions only in this environment; Unreal Header Tool, C++ compilation, PIE and packaged/dedicated-server testing still require a full Unreal Engine 5 environment.
