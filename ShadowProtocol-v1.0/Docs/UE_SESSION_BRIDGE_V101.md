# Shadow Protocol v1.0.1 — Unreal Backend Session Bridge

## Purpose

`USPBackendSessionSubsystem` connects the Unreal client presentation/session layer to the v1.0.1 backend compatibility, token-rotation, server-allocation and reserved-slot reconnect contract without placing privileged bootstrap credentials inside the shipped game client.

The bridge is intentionally a `UGameInstanceSubsystem` so compatibility state, the authenticated session token and reconnect/allocation callbacks survive map/UI transitions for the life of the current game instance.

## Security boundary

`USPBackendSessionSubsystem` is a shipped-client component. It may hold only the signed game-session token supplied by trusted account/platform integration plus short-lived allocation/reconnect tokens.

It must never receive:

- `SESSION_BOOTSTRAP_SECRET`;
- `SERVER_REGISTRATION_SECRET`;
- `ORCHESTRATOR_ATTESTATION_SECRET`;
- `SP_NODE_ATTESTATION`;
- per-node dedicated-server credentials.

The dedicated-server bridge owns server-only trust material. Production node enrollment uses a one-time orchestrator attestation; subsequent authoritative calls use the node's expiring credential.

## Client flow

1. Read local identity from `USPBuildInfoLibrary` (`1.0.1`, `SP-1.0.1`, `EMBASSY-PROTOCOL-101`).
2. Call `CheckCompatibility()` before entering matchmaking.
3. Backend `GET /v1/compatibility` returns the active release, canonical network build, accepted build set and protocol version.
4. `OnCompatibilityChecked` drives the ready-room/UI state.
5. If the local build is not accepted, `OnUpgradeRequired` blocks ranked allocation and should show an update-required UI.
6. A trusted identity/platform layer supplies the signed session through `InstallAuthenticatedSession(...)`.
7. Call `AllocateProtocolServer(region, ranked)`.
8. The subsystem sends the Bearer session token to `POST /v1/matches/allocate`.
9. Successful allocation emits `OnAllocationCompleted` with match/server IDs, connect token, `ConnectHost`, `ConnectPort`, tick rate, expiry, network build and backend protocol.
10. HTTP `426`, invalid connection metadata or an allocation build mismatch fails closed instead of entering travel.

## Dedicated-server connection handoff

v1.0.1 has an explicit connection-target contract between allocation and Unreal. With PostgreSQL in production, that target now comes from the regional healthy-server registry rather than a hard-coded address.

A dedicated server registers its infrastructure-owned identity and route using `POST /v1/servers/register`, then keeps the route eligible with `POST /v1/servers/heartbeat`. Registration includes:

- stable `serverId`;
- region;
- network build;
- public host and port;
- concurrent allocation capacity.

The allocator only selects a node when it matches the player's signed region/build, is `ready`, has a recent heartbeat and has free capacity. `SERVER_HEARTBEAT_TTL_MS` defaults to 30 seconds. Operators can remove a node from new allocations with `POST /v1/servers/drain` without invalidating allocations already assigned to it.

For local development or the no-database prototype path, static routing remains available through:

- `GAME_SERVER_PUBLIC_HOST`
- `GAME_SERVER_PUBLIC_PORT`

Development defaults to `127.0.0.1:7777`. Production PostgreSQL allocation does **not** silently fall back to that address. If no eligible registered node exists, `/v1/matches/allocate` returns `503 no-healthy-game-server`.

Allocation responses include:

- `allocationId`
- `matchId`
- `connectHost`
- `connectPort`
- `connectToken`
- `serverId`
- network/protocol identity and expiry metadata.

`FSPMatchAllocation` exposes `ConnectHost` and `ConnectPort` to Blueprint. `USPBackendSessionSubsystem` verifies both values before broadcasting `OnAllocationCompleted`, preventing UMG/client-travel code from treating an address-less allocation as success.

When PostgreSQL is enabled, the backend persists the owning user, selected node, target host/port, allocation status, expiry and **hash** of the connect token. The plaintext token remains only in the allocation response/client connection path.

Existing databases apply the complete v1.0.1 migration chain through:

```bash
npm run db:upgrade:v101
```

Fresh databases use:

```bash
npm run db:init
```

The migration chain includes both `v101_connection_target.sql` and `v101_server_registry.sql`.

## Server registry lifecycle

1. Orchestration launches a dedicated-server process with routing arguments, `SERVER_REGISTRATION_SECRET`, and a fresh `SP_NODE_ATTESTATION`.
2. The server registers through `POST /v1/servers/register`.
3. Backend verifies and consumes the attestation, then returns a per-node credential.
4. Heartbeat, drain, allocation release, admission and authoritative match writes use `x-sp-server-id` plus `x-sp-node-credential`.
5. Node credentials rotate before expiry with a short previous-token overlap.
6. Planned maintenance uses drain so no new allocations target the node.
7. Expired reservations are reclaimed and released capacity returns to the scheduler.

The shared registration secret is bootstrap-only; it is not an ongoing match authority credential.

## One-time connection admission

After allocation, call:

```cpp
BackendSession->ConnectToAllocation(PlayerController, Allocation);
```

The client bridge validates allocation/match ids, server/build option values, target host/port and token shape, then performs absolute `ClientTravel` with the one-time admission envelope.

On the dedicated server, `ASPProtocolGameMode` now implements the pre-spawn gate:

- `PreLogin` validates the URL envelope and server/build target.
- `InitNewPlayer` creates a timeout-bound pending admission.
- `HandleStartingNewPlayer_Implementation` refuses pawn creation while pending.
- `PostLogin` asks `USPDedicatedServerBackendSubsystem` to redeem the one-time token.
- admission success must correlate to allocation, match, server and build;
- only then is the backend user identity trusted, a competitive slot assigned, and pawn spawning resumed;
- failure/timeout kicks the controller.

The connect token is cleared from the pending GameMode state immediately after the backend request begins.

## Session rotation

Game sessions use a 15-minute signed lifetime. A competitive match may last longer than that, so v1.0.1 adds explicit rotation rather than relying on a stale token throughout the match.

1. Call `RefreshAuthenticatedSession()` before the current expiry window becomes critical.
2. The subsystem sends the current Bearer token to `POST /v1/auth/refresh`.
3. The backend verifies the signature, expiry, build and backend protocol before rotating the token.
4. PostgreSQL session expiry and Redis session TTL are extended when those services are configured.
5. The returned token replaces the previous in-memory token; it is never exposed through a Blueprint event.
6. `OnSessionRefreshed` emits only the session id and new expiry timestamp for UMG/controller scheduling.
7. HTTP `401`/`403` clears the local session; HTTP `426` routes to `OnUpgradeRequired`.

Recommended policy: refresh while still comfortably inside the valid window, and refresh before attempting a reserved-slot reconnect if the session is close to expiry. Do not store the refreshed Bearer token in SaveGame, config files, analytics or logs.

## Matchmaking trust boundary

`POST /v1/matchmaking/queue` requires the signed compatible game session. The client submits only latency, party size and region. User identity comes from the signed session, while skill/trust values are read from server-owned database state when PostgreSQL is configured.

Client-supplied `userId`, `skillRating` or `trustScore` fields cannot become authoritative matchmaking identity. Region must match the signed session region.

## Reserved-slot reconnect flow

When a connected client loses transport but still owns its competitive slot:

1. Call `RequestReconnectTicket(matchId, roundNumber, slotIndex)` while the authenticated game session is still valid.
2. Backend `/v1/matches/reconnect-ticket` verifies compatibility and database-backed slot ownership, changes that slot to `reconnecting`, clears ready state and returns a short-lived reconnect token plus the reconnect deadline and grace period.
3. `OnReconnectTicketIssued` exposes the token/deadline/seconds remaining to the client controller. The token should be kept in memory only.
4. Call `ReconnectToReservedSlot(...)` with the returned token.
5. Backend `/v1/matches/reconnect` verifies session ownership, token hash and deadline before restoring the slot to `connected`.
6. `OnReconnectCompleted` returns slot index, user/team/spawn metadata and network/protocol identity.
7. Any `426` or build mismatch routes to `OnUpgradeRequired`; expired/invalid tokens route to `OnRequestFailed`.

The backend contract currently reserves the slot for **90 seconds**. Reconnect and ready-state mutations fail closed with `503 database-not-configured` when PostgreSQL is unavailable because ownership cannot be proven safely.

## Blueprint/UMG events

Bind the ready-room or online-session controller to:

- `OnCompatibilityChecked` — display Verified / Update Required state;
- `OnSessionRefreshed` — update the displayed/session-controller expiry timer without exposing the Bearer token;
- `OnAllocationCompleted` — hand `ConnectHost`, `ConnectPort`, `AllocationId`, `MatchId` and `ConnectToken` to the connection/travel layer;
- `OnReconnectTicketIssued` — start/update the reconnect countdown and keep the token only in memory;
- `OnReconnectCompleted` — restore the recovered player slot and resume the connection/travel path;
- `OnUpgradeRequired` — block matchmaking/reconnect and present required build/protocol details;
- `OnRequestFailed` — present transport/auth/allocation/reconnect failures without treating them as successful lobby state.

Recommended UMG sequence:

**Ready Room → Compatibility → Identity Session → Allocate → Registry-selected Host/Port → Connect → Server Redeems Admission Token → Admit Player → Refresh Session as Needed → Live Match**

Recovery sequence:

**Transport Loss → Refresh Session if Needed → Reconnect Ticket → Reserved Slot Countdown → Reconnect → Restore Slot → Resume Match**

`ConnectToAllocation` performs validated absolute IP `ClientTravel`; the GameMode redeems the admission token before pawn startup. `ASPOnlineGameSession` supplies the dedicated-server OnlineSubsystem lifecycle. Provider-specific Steam/EOS identity bootstrap and client `JoinSession` discovery are still pending; the current route uses backend allocation with the NULL/IP transport.

## Build compatibility behavior

The backend owns the accepted-build policy through `ACCEPTED_NETWORK_BUILDS`. The client checks whether its immutable `SP-1.0.1` build is present in that policy before requesting allocation, refresh or reconnect operations.

The allocated/reconnected server build is then validated with `USPBuildInfoLibrary::IsNetworkBuildCompatible(...)`. Ranked flows therefore fail closed if returned server metadata does not match the local executable.

## Dedicated-server authority

The following operations are guarded by `x-match-server-secret` and must not be authored by public clients:

- server registration, heartbeat, drain and allocation release;
- one-time allocation-token admission;
- anti-cheat events;
- generic match telemetry;
- round results;
- tactical equipment events;
- player-slot provisioning;
- fortification events;
- ballistic/suppression events;
- kill feed;
- overtime;
- environment/combat telemetry;
- tactical interactions.

This credential is infrastructure-only. It must be injected into trusted server processes through deployment secrets and never packaged into the client.

## Database validation

The backend has repeatable database commands:

- `npm run db:init` — base schema plus all v1.0.1 migrations;
- `npm run db:upgrade:v101` — idempotent v1.0.1 connection/admission + registry migrations;
- `npm run test:postgres` — persistent scheduler/allocation/admission/ready/reconnect/release integration test.

GitHub Actions runs the PostgreSQL test against a disposable PostgreSQL 16 service. It verifies server registration authorization, draining/stale-node exclusion, capacity enforcement, selected-node persistence, hashed allocation-token persistence, one-time admission and replay rejection, database-owned ready state, reconnect token hashing/deadline behavior, successful slot restoration and allocation release/capacity recovery.

## Unreal modules

v1.0.1 adds the following module dependencies in `ShadowProtocol.Build.cs`:

- `HTTP`
- `Json`
- `JsonUtilities`

Primary files:

- `Public/SPBackendSessionSubsystem.h`
- `Private/SPBackendSessionSubsystem.cpp`
- `Public/SPBuildInfoLibrary.h`
- `Private/SPBuildInfoLibrary.cpp`

## Validation boundary

GitHub Actions now includes an Unreal admission source-contract test in addition to backend/browser validation. It verifies the expected C++ integration points remain present, but it does **not** replace UHT or UE compilation.

UE5.6 remains required for UHT, Development Server compilation, PIE/package validation, real `ClientTravel`, socket/login behavior and 5v5 soak testing.

## Next integration tasks

1. Compile all v1.0.1 Unreal networking additions in UE5.6 and resolve UHT/compiler issues.
2. Run packaged client → dedicated-server `ConnectToAllocation` travel.
3. Validate concurrent successful, denied, expired and replayed admissions before pawn spawn.
4. Bind ready-room, session-expiry and reconnect UMG widgets.
5. Implement trusted platform/account bootstrap for signed game sessions.
6. Run orchestrator launch, node rotation, scheduler failover, reconnect and latency soak tests with 10 clients.

## Per-node server identity update

The registry no longer uses one shared credential for all server authority. `SERVER_REGISTRATION_SECRET` bootstraps only `POST /v1/servers/register` (with `MATCH_SERVER_SECRET` retained only as a v1.0.1 migration fallback). Registration returns a fresh per-node credential while persisting only its SHA-256 hash.

After registration, trusted server requests use:

```text
x-sp-server-id: <registered server id>
x-sp-node-credential: <in-memory node credential>
```

Heartbeat, drain, one-time admission, allocation release and authoritative match writes require this node identity. Match-scoped calls are verified against the allocation's `node_id`, preventing one dedicated server from mutating another node's match. Shipped clients never receive either the registration bootstrap secret or node credentials.


## Orchestrator-attested registration

For production dedicated servers, registration now has a control-plane proof in addition to the bootstrap secret. The orchestrator injects `SP_NODE_ATTESTATION` into the dedicated-server process. `USPDedicatedServerBackendSubsystem` sends it only as `x-sp-node-attestation` during `POST /v1/servers/register`; it is not exposed through Blueprint or logs.

The backend validates the signed server identity/region/build/public route/capacity and consumes the attestation id once. When registration reports that the attestation was consumed, the Unreal subsystem clears its in-memory copy. An attested server that later loses node authority fails closed and requires orchestrator relaunch with a new attestation rather than replaying the old proof.


## Shared 5v5 roster authority

The allocation/admission boundary now carries server-owned competitive roster authority.

For production PostgreSQL matches, each allocation is assigned a round-1 slot before travel. The backend keeps a ten-player shared match roster and returns `slotIndex`, `team` and `tacticalSide`. The dedicated server receives the same metadata only after successful one-time admission.

First-time admission in `ASPProtocolGameMode` validates the slot range and accepted team names, stores the slot in replicated `ASPPlayerState::CompetitiveSlotIndex`, and uses the backend-provided team. Connection order no longer decides first-time teams. `RefreshCompetitiveSlots()` preserves that authoritative index when building replicated ready-room slots.

The current v1.0.1 solo assembly uses deterministic fixed sides: slots 0–4 are Directorate Nine / attack and slots 5–9 are Helix / defense. Party-preserving and skill-balanced team construction remain later matchmaking work; they must not be inferred as already implemented.

Expired, unconsumed pre-admission reservations are removed from the pre-match roster, allowing a replacement allocation to reclaim the exact vacant slot without shifting existing players.
