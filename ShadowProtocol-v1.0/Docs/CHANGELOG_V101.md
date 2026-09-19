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
- Allocation returns `connectHost`, `connectPort` and a short-lived connect token, with the selected target persisted in `server_allocations` when PostgreSQL is enabled.
- `Backend/scripts/apply-db.mjs` provides repeatable `db:init` and `db:upgrade:v101` database paths.
- `Backend/db/v101_connection_target.sql` adds allocation user ownership, connection target columns and token-consumption state.
- `Backend/db/v101_server_registry.sql` adds regional server nodes, heartbeat/capacity state and allocation-to-node linkage.
- Trusted servers can register, heartbeat, drain and release allocations through `/v1/servers/*` endpoints guarded by `MATCH_SERVER_SECRET`.
- Production PostgreSQL allocation selects only same-region/same-build nodes that are `ready`, fresh within `SERVER_HEARTBEAT_TTL_MS` and below capacity.
- Stale or draining nodes are ignored; a region with no eligible node returns `503 no-healthy-game-server`.
- Development retains the static `GAME_SERVER_PUBLIC_HOST` / `GAME_SERVER_PUBLIC_PORT` fallback; production PostgreSQL allocation does not silently fall back to it.
- Capacity is reserved atomically, expired pre-admission reservations are reclaimed, and trusted allocation release decrements node load.
- Allocation tokens are stored only as hashes and are bound to the authenticated session user.
- Trusted dedicated servers redeem `POST /v1/matches/admit` with `MATCH_SERVER_SECRET`, allocation id, match id and the presented connect token before admitting a player.
- Admission atomically verifies allocation identity, token hash, expiry, status and unused state; success marks the allocation `live` and records `connect_token_consumed_at`.
- Replaying a consumed connect token is rejected with `403 admission-denied`.
- Matchmaking requires the signed compatible session; the client no longer controls authoritative `userId`, skill rating or trust score.
- Matchmaking region must match the signed session region; skill/trust are read from server-owned database state when PostgreSQL is configured.
- `MATCH_SERVER_SECRET` gates server registry lifecycle, admission plus anti-cheat, match telemetry, round results, tactical equipment, player slots, fortification, ballistic, kill-feed, overtime, environment/combat and tactical-interaction writes.
- Reconnect ticket issuance and reconnect completion enforce signed compatibility and database-backed slot ownership.
- Ready-state and reconnect mutations fail closed when PostgreSQL is unavailable because ownership cannot be proven safely.
- `/health`, `/v1/compatibility`, allocation/reconnect responses and WebSocket hello messages expose the active release/network/protocol identity; `/health` also exposes scheduler heartbeat policy.

This makes compatibility, matchmaking identity, regional connection routing, connection admission and authoritative event ownership server-controlled rather than client-asserted.

## PostgreSQL migration and initialization

Fresh databases can be initialized with:

```bash
npm run db:init
```

Existing v1.0 databases can apply the complete v1.0.1 migration chain with:

```bash
npm run db:upgrade:v101
```

The upgrade applies `v101_connection_target.sql` and `v101_server_registry.sql` idempotently. Together they add the client connection target, owning user, one-time token consumption timestamp, port validation, regional server registry, heartbeat/capacity state and allocation-node relationship without destructive schema recreation.

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
- production allocation fails closed when no static route exists in the no-database compatibility path;
- authenticated matchmaking identity ignores spoofed client identity/rating/trust fields;
- rejection of public-client authoritative match telemetry;
- acceptance of the same telemetry with the dedicated-server credential;
- fail-closed reconnect/ready-state ownership behavior when PostgreSQL is unavailable.

The PostgreSQL integration job verifies:
- base schema plus both v1.0.1 migrations;
- public server-registration attempts are rejected;
- trusted regional node registration;
- draining nodes are excluded from scheduling;
- stale-heartbeat nodes are excluded from scheduling;
- capacity saturation returns `503 no-healthy-game-server` rather than overbooking;
- selected node id/host/port and hashed connect token are persisted;
- allocation ownership is bound to the authenticated user;
- public admission attempts are rejected;
- incorrect connect tokens are rejected;
- valid dedicated-server admission succeeds once;
- replay of the same connect token is rejected;
- admitted allocation transitions to `live` with consumption timestamp;
- authenticated ready-state ownership;
- hashed 90-second reconnect ticket storage;
- invalid reconnect rejection and valid reconnect recovery;
- reconnect token/deadline cleanup after restoration;
- trusted allocation release returns node capacity to the scheduler.

The first TypeScript CI pass exposed two real issues in the backend (`ioredis` NodeNext constructor import and an implicit-any WebSocket message parameter); both were corrected without weakening strict TypeScript settings.

## Validation boundary

Browser syntax and backend TypeScript/security/database/scheduler behavior are covered by GitHub Actions. The Unreal v1.0.1 files are source-level additions in this environment; Unreal Header Tool, UE C++ compilation, PIE, packaged-client testing and dedicated-server multiplayer testing still require a full Unreal Engine 5 environment.

See `UE_SESSION_BRIDGE_V101.md` for the secure Unreal-to-backend integration flow.


## Per-node dedicated-server credentials

- Adds `v101_node_credentials.sql` to the v1.0.1 migration chain.
- `SERVER_REGISTRATION_SECRET` (with legacy `MATCH_SERVER_SECRET` fallback) now bootstraps only server registration.
- Registration rotates a random per-node credential and persists only its SHA-256 hash, issue timestamp and revocation state.
- Existing registry rows without credentials are marked `offline` and must re-register before scheduling.
- Heartbeat, drain, one-time admission, allocation release and authoritative match writes require `x-sp-server-id` + `x-sp-node-credential`.
- Match-scoped writes are checked against `server_allocations.node_id`, preventing cross-node mutation.
- The no-database path fails authoritative server writes closed because node ownership cannot be proven.
- PostgreSQL integration coverage includes bootstrap-only rejection, cross-node admission/telemetry/release rejection, credential hashing and owned-node success.
- `USPDedicatedServerBackendSubsystem` stores the returned node credential only in dedicated-server memory and automatically uses it after registration.


## Expiring node credential rotation

- Adds `v101_node_credential_rotation.sql`.
- Adds `NODE_CREDENTIAL_TTL_MS` (default 6h) and `NODE_CREDENTIAL_GRACE_MS` (default 2m).
- Scheduler excludes nodes whose current credential has expired.
- Adds `POST /v1/servers/rotate-credential`, authenticated by the current node credential.
- Rotation stores the old credential hash only for a bounded overlap window so in-flight requests survive rotation.
- Previous credentials cannot rotate again; expired credentials must recover through trusted registration.
- PostgreSQL integration tests validate new/old credential overlap, new-token authority, old-token expiry and stale-token rotation rejection.
- Unreal dedicated-server bridge schedules automatic rotation at roughly 75% of TTL and can recover registration after ambiguous credential-auth failures without exposing either credential.


## Orchestrator-backed node attestation

- Adds `v101_node_attestation.sql` and `server_node_attestations` replay tracking.
- Production PostgreSQL registration requires a short-lived orchestrator-signed attestation by default.
- Attestations bind server id, region, network build, public host/port and capacity.
- Issuer, audience, issue/expiry window and HMAC signature are verified before registration.
- Attestation `jti` is consumed in the same transaction as node credential issuance; duplicate use returns `server-attestation-replayed`.
- `game_server_nodes` records the last attestation id/time for audit.
- Unreal dedicated-server bridge reads `SP_NODE_ATTESTATION` from process environment, sends it only during registration and clears it after successful consumption.
- Attested authority recovery fails closed and requires fresh orchestrator launch material instead of replaying an old attestation.


## Unreal pending-admission gate

- Adds `USPBackendSessionSubsystem::BuildAllocationTravelUrl` and `ConnectToAllocation` for validated client travel to the allocated dedicated server.
- Travel options carry only the one-time allocation envelope: allocation id, match id, connect token, target server id and network build.
- Adds allocation/match-correlated dedicated-server admission failure events.
- `ASPProtocolGameMode::PreLogin` rejects malformed envelopes, wrong target servers, incompatible builds and unavailable/draining admission services.
- `InitNewPlayer` records timeout-bound pending admissions.
- `HandleStartingNewPlayer_Implementation` suppresses pawn creation until backend admission succeeds.
- `PostLogin` redeems the one-time token and immediately clears it from pending GameMode state.
- Successful backend admission promotes the trusted backend user id, assigns a competitive team/slot and resumes player startup.
- Failed or timed-out admissions are kicked before competitive participation.
- Pending disconnects do not create reconnect reservations.
- `ASPPlayerState` now carries a replicated backend-authenticated user id.
- Adds `test/unreal-admission-contract.mjs` and a CI step that guards the source-level travel/admission contract until UE5.6 compilation is available.

## Dedicated OnlineSubsystem session pass

- Add `ASPOnlineGameSession` and wire GameModeBase's custom match transitions to
  asynchronous OSS create/start/end, with monotonic timeouts and fail-closed admission.
- Add the UE dedicated-server target and explicitly enable the NULL provider.
- Set the native competitive pawn, recheck expired/lost-authority admission callbacks,
  enforce kick fallback, and disable manual identity promotion on dedicated servers.
- Extend CI from three to six Unreal source-contract checks. See
  `ONLINE_GAME_SESSION_V101.md` for build commands and pending runtime gates.
- UHT, UE compilation, Steam/EOS bootstrap and packaged 10-client validation remain pending.

## Native ready-room continuation

- Add local Ready/Cancel UMG controls showing replicated admission/team/spawn/readiness.
- Add owner-controller ready and spawn RPCs with throttling and GameMode validation.
- Restrict edits to admitted connected planning players; changed valid spawn choices clear readiness.
- Recheck readiness during asynchronous OSS startup before preparation.
- Extend Unreal source contracts to nine checks; UE/UHT and packaged runtime validation remain pending.
- See `READY_ROOM_V101.md` for custom UMG hooks, test cases and remaining integration work.

## Client session-expiry continuation

- Enforce parsed future expiry with UTC and monotonic lifetime checks.
- Add expiry event/status APIs and a native ready-room notice.
- Cancel invalidated requests and ignore stale authenticated/compatibility callbacks.
- Serialize token refresh with match requests; bound HTTP duration and preserve sessions on match-ownership 403.
- Correct core ticker handle types for client expiry and server heartbeat/rotation.
- Extend source checks to twelve; see `SESSION_EXPIRY_V101.md` for runtime gates.

## Allocation travel and failure reporting

- Validate DNS/IPv4/IPv6 host syntax and reject URL injection or embedded ports.
- Recheck allocation expiry/build/token size and local controller ownership before travel.
- Add scoped engine failure callbacks with sanitized persistent error state and UI hooks.
- Compile and execute the shared C++ host validator in CI (49 cases plus ASCII sweep).
- Expand Unreal source contracts to fourteen; document the missing fresh-admission reconnect bridge.

## Bounded reconnect reservation pass

- Prevent reconnect-token replacement and deadline extension through repeated requests.
- Require connected/eligible slots, unfinished matching region/build and live allocations.
- Enforce reconnecting state on redemption and keep restored players unready.
- Add real PostgreSQL race, replay, expiry and closed-match/allocation regression cases.
- Fresh Unreal admission credentials and packaged reconnect remain pending.

## Planned ready-room reconnect admission

- Exchange a reconnect reservation for a fresh single-use transport credential with the original deadline.
- Route Unreal ClientTravel through node-bound admission; correlate asynchronous attempts, restore local reserved team/spawn and preserve roster indices.
- Add migration and PostgreSQL race/replay/expiry/capacity checks.
- Mid-round life-state recovery, automatic backend slot publication and UE runtime verification remain pending; see RECONNECT_RESERVATIONS_V101.md.

## Native UI and control-system pass

- Add match/combat HUD and a scrollable ready-room roster.
- Add Escape controls panel with saved sensitivity/invert-Y and current binding reference.
- Release held actions across menu transitions, preserve input-ignore pairing, fix overlapping lean keys and crouch/sprint state.
- Add a native first-person camera and enable crouch movement.
- Add compiled lean-state regression checks; UE rendering/build verification remains pending (UI_CONTROLS_V101.md).

## Ready-room spawn selection

- Add team-filtered, owner-only spawn choices and a native dropdown.
- Add correlated server confirmation/rejection and bounded pending UI for ready/spawn requests.
- Keep authoritative validation and clear readiness when spawn selection changes.
- Document unconfigured-map and network-delay behavior in UI_CONTROLS_V101.md.

## Action rebinding

- Add action/key picker, saved overrides, conflict feedback and restore-original bindings.
- Validate changes atomically and preserve movement/menu bindings and project config.
- Add Unreal automation coverage (engine execution pending) and a CI source contract.


## Confirmed atomic key-binding swaps

- The native controls panel now distinguishes supported-action conflicts from reserved/invalid-key failures.
- Conflicting supported actions present an explicit **Confirm key swap** step instead of forcing the player through a temporary unused key.
- Confirmed swaps exchange both actions in one candidate and preserve the previous live mapping if validation fails or the conflict changed.
- Pending swap state is cleared when the player changes action, closes controls or restores original bindings.
- Unreal automation coverage now exercises conflict lookup, successful two-action swap and stale-confirmation rollback.
- The Node source contract now requires conflict detection, confirmation UI and atomic swap rollback wiring in CI.

## HUD readability

- Add persisted combat-HUD contrast, crosshair size/visibility and help-text options with a separate reset.
- Add outlined reticle and replicated objective-status strip.
- Engine build and rendered/readability verification remain pending.
