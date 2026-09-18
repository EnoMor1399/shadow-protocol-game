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

Trusted runtime material comes from the dedicated-server process environment:

```text
SERVER_REGISTRATION_SECRET=<registration-bootstrap secret>
SP_NODE_ATTESTATION=<short-lived single-use orchestrator assertion>
```

`MATCH_SERVER_SECRET` is retained only as a v1.0.1 migration fallback name for the registration bootstrap secret. It is not the continuing match-authority credential.

`ConfigureFromRuntime()` refuses to activate outside a dedicated-server process. Production registration additionally requires the orchestrator attestation enforced by the backend.

## Startup lifecycle

On `UGameInstanceSubsystem::Initialize` in a dedicated-server process:

1. Read server identity/public routing from command-line arguments.
2. Read `SERVER_REGISTRATION_SECRET` and the one-time `SP_NODE_ATTESTATION` from the process environment.
3. Read the immutable game network build from `USPBuildInfoLibrary`.
4. Call `POST /v1/servers/register` with bootstrap proof plus the attestation.
5. Backend validates/consumes the attestation and returns a per-node credential.
6. Keep the node credential only in dedicated-server memory and clear the consumed attestation.
7. Start heartbeat and credential-rotation scheduling from backend-provided policy.

The heartbeat interval is approximately one third of the backend TTL, while node credential rotation is scheduled at roughly 75% of credential lifetime.

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

Client allocation produces `allocationId`, `matchId`, `connectToken`, target host/port, server id and network build. The client passes the short-lived allocation envelope through Unreal travel options.

The dedicated server treats the controller as **pending**, not admitted, and calls:

```cpp
DedicatedServerBackend->AdmitConnection(AllocationId, MatchId, ConnectToken);
```

The backend request is authenticated with the dedicated server's **per-node credential**, not the shared registration bootstrap secret. Backend ownership checks bind the request to that node's allocation.

The Unreal bridge correlates both success and failure by allocation/match id. Success is accepted only when the response matches the expected allocation, match, server id and network build. Wrong, expired or replayed allocation tokens never produce a competitive player session.

## Pre-login integration boundary

The pending admission gate is now implemented in `ASPProtocolGameMode`.

1. `USPBackendSessionSubsystem::ConnectToAllocation` performs absolute `ClientTravel` to the allocated host/port and includes only the one-time allocation envelope:
   - `spAllocationId`
   - `spMatchId`
   - `spConnectToken`
   - `spServerId`
   - `spNetworkBuild`
2. `PreLogin` rejects missing/malformed envelopes, wrong server ids, incompatible network builds, unavailable registry state and draining servers.
3. `InitNewPlayer` creates a pending admission record with a short timeout.
4. `HandleStartingNewPlayer_Implementation` suppresses pawn creation while that record exists.
5. `PostLogin` starts asynchronous backend redemption, then clears the plaintext connect token from the pending record.
6. Correlated `OnAdmissionCompleted` promotes the trusted backend `userId`, assigns a team/competitive slot, and resumes `HandleStartingNewPlayer`.
7. Correlated admission failure or timeout kicks the controller before it can enter a competitive slot.
8. Pending disconnects are removed without creating reconnect reservations.

This preserves Unreal's synchronous `PreLogin` contract while moving the authoritative remote check to a safe pre-spawn asynchronous gate.

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

- Shipped clients never receive registration secrets, orchestrator signing keys, node credentials or backend infrastructure credentials.
- The client carries only the short-lived one-time allocation token needed for server admission.
- Production node enrollment requires bootstrap proof plus a single-use orchestrator attestation.
- Continuing server authority uses expiring per-node credentials with bounded rotation overlap.
- Match-scoped server writes are checked against allocation/node ownership.
- Admission success is correlated to allocation + match + server + network build before pawn spawn.
- Pending unauthenticated controllers are excluded from competitive slots and readiness counts.
- Connect tokens are cleared from the server's pending-login record immediately after the admission request starts.

## Current validation boundary

GitHub Actions validates browser syntax, strict backend TypeScript/security behavior, PostgreSQL registry/scheduler/attestation/credential-rotation/admission/reconnect/release, and an Unreal **source-contract** test that asserts the travel envelope and pending-admission gate remain present.

This environment still does not provide Unreal Engine 5.6/UHT, so the C++ additions remain source-validated rather than engine-compiled. Production sign-off still requires:

1. Unreal Header Tool validation;
2. Development Server C++ compilation;
3. packaged client + dedicated-server launch;
4. real travel through `ConnectToAllocation`;
5. simultaneous pending admissions;
6. rejected/expired/replayed-token disconnect behavior;
7. 10-client/5v5 reconnect/failover/credential-rotation soak testing.

## Next hardening step

The backend trust-boundary milestones—per-node credentials, bounded rotation, orchestrator attestation, and pre-spawn admission—are implemented at source/integration-test level.

The next blocker is **UE5.6 runtime validation** plus trusted platform/account bootstrap and real 5v5 network soak testing.

## Per-node credential hardening

The dedicated-server trust boundary now separates registration bootstrap from node authority.

- Set `SERVER_REGISTRATION_SECRET` only on trusted server/orchestrator infrastructure. The legacy `MATCH_SERVER_SECRET` name is accepted only as a v1.0.1 migration fallback.
- `POST /v1/servers/register` is the only endpoint that accepts the shared bootstrap secret.
- Every successful registration rotates and returns a random `nodeCredential`; the backend persists only its SHA-256 hash plus issue/revocation timestamps.
- `USPDedicatedServerBackendSubsystem` keeps that node credential only in dedicated-server memory.
- Heartbeat, drain, admission, allocation release and authoritative match writes send `x-sp-server-id` plus `x-sp-node-credential`.
- Match-scoped calls are checked against `server_allocations.node_id`, so another registered node cannot admit, release or write telemetry for a match it does not own.
- Existing registry rows without node credentials are marked `offline` by the migration and must re-register before scheduling.


## Expiry and zero-downtime rotation

Node credentials are now time-bounded. The backend defaults are:

- `NODE_CREDENTIAL_TTL_MS=21600000` (6 hours)
- `NODE_CREDENTIAL_GRACE_MS=120000` (2 minutes)

The backend clamps unsafe values, excludes expired nodes from new scheduling immediately, and rejects an expired current credential. `POST /v1/servers/rotate-credential` requires the current node credential and rotates to a new random secret while retaining the previous hash only for the bounded grace window. Previous credentials can finish in-flight trusted requests during that window but cannot perform another rotation.

`USPDedicatedServerBackendSubsystem` schedules rotation at approximately 75% of the issued TTL. The plaintext replacement credential remains in dedicated-server memory only. If rotation transport fails, the subsystem retries before expiry; if authentication indicates an uncertain/lost rotation response, it re-registers through the bootstrap channel. A draining node restores drain state after registration recovery so credential recovery does not intentionally return it to the ready pool.

The migration `v101_node_credential_rotation.sql` adds current expiry plus previous-credential hash/grace metadata and gives existing credentialed nodes a bounded migration expiry.


## Orchestrator attestation

Production registration now also requires a short-lived single-use `SP_NODE_ATTESTATION` issued by the trusted orchestration layer. The assertion is bound to the exact server id, region, network build, public host/port and capacity. The backend verifies its HMAC signature, issuer/audience and lifetime, then consumes its unique `jti` transactionally before issuing a node credential.

The dedicated server never receives `ORCHESTRATOR_ATTESTATION_SECRET`. After successful attested registration, `USPDedicatedServerBackendSubsystem` clears its in-memory attestation. If authority is later lost, the process must be relaunched by the orchestrator with a fresh single-use assertion rather than replaying the consumed token.

See `NODE_ATTESTATION_V101.md`.
