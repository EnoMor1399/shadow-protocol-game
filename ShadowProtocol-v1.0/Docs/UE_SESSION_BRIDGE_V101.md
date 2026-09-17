# Shadow Protocol v1.0.1 — Unreal Backend Session Bridge

## Purpose

`USPBackendSessionSubsystem` connects the Unreal client presentation/session layer to the v1.0.1 backend compatibility, token-rotation, server-allocation and reserved-slot reconnect contract without placing privileged bootstrap credentials inside the shipped game client.

The bridge is intentionally a `UGameInstanceSubsystem` so compatibility state, the authenticated session token and reconnect/allocation callbacks survive map/UI transitions for the life of the current game instance.

## Security boundary

The Unreal client must **never** contain `SESSION_BOOTSTRAP_SECRET` or `MATCH_SERVER_SECRET`.

`POST /v1/auth/game-session` is a trusted identity/platform bootstrap endpoint. A production platform login, account service or other trusted identity integration obtains the signed game-session response and passes only the resulting session values to Unreal:

- session id;
- session token;
- region;
- expiry timestamp.

`InstallAuthenticatedSession(...)` keeps those values in memory. The subsystem does not persist the Bearer token to config, SaveGame or logs.

`MATCH_SERVER_SECRET` belongs only on trusted dedicated-server/backend infrastructure. Authoritative match telemetry endpoints now reject ordinary game clients that do not present that credential.

## Client flow

1. Read local identity from `USPBuildInfoLibrary` (`1.0.1`, `SP-1.0.1`, `EMBASSY-PROTOCOL-101`).
2. Call `CheckCompatibility()` before entering matchmaking.
3. Backend `GET /v1/compatibility` returns the active release, canonical network build, accepted build set and protocol version.
4. `OnCompatibilityChecked` drives the ready-room/UI state.
5. If the local build is not accepted, `OnUpgradeRequired` blocks ranked allocation and should show an update-required UI.
6. A trusted identity/platform layer supplies the signed session through `InstallAuthenticatedSession(...)`.
7. Call `AllocateProtocolServer(region, ranked)`.
8. The subsystem sends the Bearer session token to `POST /v1/matches/allocate`.
9. Successful allocation emits `OnAllocationCompleted` with match/server IDs, connect token, tick rate, expiry, network build and backend protocol.
10. HTTP `426` or an allocation build mismatch emits `OnUpgradeRequired` instead of joining the server.

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

`POST /v1/matchmaking/queue` now requires the signed compatible game session. The client submits only latency, party size and region. User identity comes from the signed session, while skill/trust values are read from server-owned database state when PostgreSQL is configured.

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
- `OnAllocationCompleted` — transition to the connection/travel layer using the returned server allocation data;
- `OnReconnectTicketIssued` — start/update the reconnect countdown and persist the token only in memory;
- `OnReconnectCompleted` — restore the recovered player slot and resume the connection/travel path;
- `OnUpgradeRequired` — block matchmaking/reconnect and present required build/protocol details;
- `OnRequestFailed` — present transport/auth/allocation/reconnect failures without treating them as successful lobby state.

Recommended UMG sequence:

**Ready Room → Compatibility → Identity Session → Allocate → Connect → Refresh Session as Needed → Live Match**

Recovery sequence:

**Transport Loss → Refresh Session if Needed → Reconnect Ticket → Reserved Slot Countdown → Reconnect → Restore Slot → Resume Match**

The subsystem intentionally stops before transport-specific server travel. A connection address or OnlineSubsystem session handle must come from the dedicated-server registry/allocation layer; `serverId` alone must not be guessed into an address.

## Build compatibility behavior

The backend owns the accepted-build policy through `ACCEPTED_NETWORK_BUILDS`. The client checks whether its immutable `SP-1.0.1` build is present in that policy before requesting allocation, refresh or reconnect operations.

The allocated/reconnected server build is then validated with `USPBuildInfoLibrary::IsNetworkBuildCompatible(...)`. Ranked flows therefore fail closed if returned server metadata does not match the local executable.

## Dedicated-server authority

The following event families are now guarded by `x-match-server-secret` and must not be authored by public clients:

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

The browser and backend paths are validated by GitHub Actions, including compatibility/session/allocation, session refresh, authenticated matchmaking identity, dedicated-server telemetry authorization and fail-closed ownership behavior without PostgreSQL. The Unreal bridge has been source-reviewed only in the current environment. It still requires Unreal Header Tool, UE C++ compilation, PIE and packaged-client testing before it can be treated as production-compiled code.

## Next integration tasks

1. Compile the module in Unreal Engine 5.6 and fix any UHT/compiler-specific issues.
2. Bind ready-room/session-expiry/reconnect UMG widgets to subsystem delegates.
3. Implement the trusted platform/account bootstrap that supplies the signed game session.
4. Add the dedicated-server registry/connection address to allocation responses.
5. Hand successful allocations/reconnects to OnlineSubsystem/client travel.
6. Add backend reconnect integration coverage against a disposable PostgreSQL instance.
7. Run 10-client dedicated-server compatibility, token-refresh, reconnect and round-transition tests.
