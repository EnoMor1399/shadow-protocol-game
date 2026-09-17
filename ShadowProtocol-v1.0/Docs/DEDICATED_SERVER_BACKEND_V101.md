# Shadow Protocol v1.0.1 — Dedicated Server Backend Lifecycle

## Purpose

`USPDedicatedServerBackendSubsystem` is the Unreal dedicated-server bridge for the v1.0.1 backend registry, heartbeat, drain, admission and allocation-release contract.

It is intentionally separate from `USPBackendSessionSubsystem`:

- `USPBackendSessionSubsystem` belongs to the game client/session layer and must never receive infrastructure credentials.
- `USPDedicatedServerBackendSubsystem` activates only in a dedicated-server process and loads its infrastructure credential at runtime.

The server bridge does not embed `MATCH_SERVER_SECRET` in source, config, Blueprint or logs.

## Runtime configuration

Supply non-secret server identity/routing values through command-line arguments:

```text
-SPBackendUrl=https://api.example.com
-SPServerId=ACC-PRIMARY
-SPRegion=acc
-SPPublicHost=203.0.113.10
-SPPublicPort=7777
-SPCapacity=10
```

Supply the infrastructure credential only through the dedicated-server process environment:

```text
MATCH_SERVER_SECRET=<strong infrastructure secret>
```

`ConfigureFromRuntime()` refuses to activate outside a dedicated-server process and requires all routing values plus the runtime secret.

## Startup lifecycle

On `UGameInstanceSubsystem::Initialize` in a dedicated-server process:

1. Read command-line server identity and public routing values.
2. Read `MATCH_SERVER_SECRET` from the process environment.
3. Read the immutable game network build from `USPBuildInfoLibrary`.
4. Call `POST /v1/servers/register`.
5. Validate that the response contains a node id and server id.
6. Mark the server registered.
7. Start heartbeat scheduling using the backend-provided heartbeat TTL.

The heartbeat interval is approximately one third of the backend TTL and is clamped to a maximum of 10 seconds.

## Healthy-node heartbeat

While the node is registered and not draining, the subsystem calls:

```text
POST /v1/servers/heartbeat
```

The request reports the configured `serverId` and `ready` state. The backend remains the authority for capacity and active-allocation counters.

If heartbeat requests fail, the subsystem reports the failure through `OnRequestFailed`; it does not silently invent a healthy state.

## Drain lifecycle

Before planned shutdown, maintenance or deployment replacement, call `MarkDraining()`.

The subsystem:

1. Stops the heartbeat ticker.
2. Calls `POST /v1/servers/drain`.
3. Marks local drain state only after a successful backend response.
4. Emits `OnDrainChanged(true)`.

During subsystem deinitialization, a best-effort drain request is also sent if the node was still registered and not already draining. Orchestration should still call `MarkDraining()` before terminating the server so the backend has time to remove the node from new allocations.

## One-time connection admission

A client allocation contains:

- `allocationId`
- `matchId`
- `connectToken`
- `connectHost`
- `connectPort`
- `serverId`

The dedicated server must treat a socket/transport connection as **pending**, not admitted, until backend redemption succeeds.

Call:

```cpp
DedicatedServerBackend->AdmitConnection(AllocationId, MatchId, ConnectToken);
```

The subsystem calls:

```text
POST /v1/matches/admit
```

with the infrastructure credential and the presented one-time allocation token.

On success it additionally verifies that:

- the backend returned `admitted=true`;
- the returned `serverId` equals this dedicated server's configured id;
- the returned `networkBuild` equals `USPBuildInfoLibrary::GetNetworkBuildId()`.

Only after `OnAdmissionCompleted` fires should the game-mode/session layer create or possess the authoritative player pawn.

Wrong, expired or replayed allocation tokens remain backend failures and must not produce a player session.

## Pre-login integration boundary

Unreal's normal `PreLogin` path is synchronous while the backend admission request is asynchronous. Do not fake success in `PreLogin` and redeem later.

The production integration should place connecting clients into a pending admission gate:

1. Parse `allocationId`, `matchId` and `connectToken` from the connection handshake/options.
2. Do not spawn or possess the player yet.
3. Call `AdmitConnection(...)`.
4. On `OnAdmissionCompleted`, bind the returned backend `userId` to the pending connection and continue authoritative login/spawn.
5. On `OnRequestFailed`, reject/close the pending connection.
6. Apply a short server-side timeout so abandoned pending admissions cannot consume resources indefinitely.

This gate still needs to be wired into the final dedicated-server GameMode / OnlineSubsystem connection flow and validated in UE5.6.

## Allocation release

When the match/allocation is finished, call:

```cpp
DedicatedServerBackend->ReleaseAllocation(AllocationId, MatchId, false);
```

Use `true` for the final argument when the allocation failed.

The subsystem calls:

```text
POST /v1/servers/release-allocation
```

and emits `OnAllocationReleased` with the resulting status and node active-allocation count.

This is required so the scheduler can return node capacity after closed or failed matches.

## Security properties

- The shipped game client never receives or persists `MATCH_SERVER_SECRET`.
- The dedicated-server secret is read only from the process environment.
- The secret is never exposed by a Blueprint getter, event payload or log statement.
- Connection tokens remain one-time backend-validated admission material.
- Server/build identity is checked again after admission before gameplay continuation.
- Registration/heartbeat/drain/release remain backend-authoritative operations.

## Current validation boundary

The backend registry, capacity accounting, admission, reconnect and release behavior is covered by GitHub Actions/PostgreSQL integration tests.

`USPDedicatedServerBackendSubsystem` is a source-level Unreal addition. This environment does not provide UE5.6/UHT, so the new C++ still requires:

1. Unreal Header Tool validation;
2. Development Server C++ compilation;
3. dedicated-server startup with runtime command-line/environment configuration;
4. real heartbeat and drain verification;
5. pending-connection admission integration;
6. 10-client/5v5 admission, reconnect and failover soak testing.

## Next hardening step

The current backend still authenticates trusted server calls with one shared infrastructure credential. The next trust-boundary upgrade should rotate registration into **per-node credentials** so a registered server can act only as its own node and only on allocations assigned to that node.
