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

## Backend note

The Fastify backend package and current `/health` response remain on the existing `0.7.0` service/protocol generation. v1.0.1 does not silently rename that service version. The next networking pass should import the game build identity into authenticated session/allocation compatibility checks and then deliberately advance the backend protocol version.

## Validation boundary

The browser patch has been syntax-checked with Node. The Unreal files are source-level additions only in this environment; Unreal Header Tool, C++ compilation, PIE and packaged/dedicated-server testing still require a full Unreal Engine 5 environment.
