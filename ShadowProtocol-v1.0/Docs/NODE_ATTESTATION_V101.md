# Shadow Protocol v1.0.1 — Orchestrator Node Attestation

## Purpose

Production PostgreSQL server registration now requires proof that the dedicated-server process was launched by trusted orchestration. The backend no longer treats possession of the shared registration bootstrap secret as sufficient production enrollment authority.

A production node must present **both**:

- `x-match-server-secret` containing `SERVER_REGISTRATION_SECRET`; and
- `x-sp-node-attestation` containing a short-lived HMAC-signed orchestration assertion.

The dedicated server never receives `ORCHESTRATOR_ATTESTATION_SECRET`. Only the orchestration control plane and backend verifier hold that signing material.

## Attestation claims

The token is `base64url(JSON).base64url(HMAC-SHA256)` and contains:

- `jti` — unique UUID for one-time replay protection;
- `iss` — `shadow-protocol-orchestrator`;
- `aud` — `shadow-protocol-server-registration`;
- `iat` / `exp` — millisecond timestamps;
- `serverId`;
- `region`;
- `networkBuild`;
- `publicHost`;
- `publicPort`;
- `capacity`.

Every registration field must exactly match the signed claims. The backend also bounds the assertion lifetime with `SERVER_ATTESTATION_MAX_TTL_MS` (120 seconds by default) and a small clock-skew allowance.

## Replay protection

`v101_node_attestation.sql` adds `server_node_attestations`.

During registration, the backend transaction first inserts the attestation `jti`. A duplicate ID returns `409 server-attestation-replayed` before any node credential can be rotated or replaced. Successful registration records the last attestation id/time on `game_server_nodes` for audit.

## Production defaults

When `NODE_ENV=production` and PostgreSQL is configured, server attestation is required by default.

Backend environment:

```text
REQUIRE_SERVER_ATTESTATION=true
ORCHESTRATOR_ATTESTATION_SECRET=<backend/orchestrator shared verifier secret>
SERVER_ATTESTATION_MAX_TTL_MS=120000
```

Dedicated-server process environment:

```text
SERVER_REGISTRATION_SECRET=<registration bootstrap secret>
SP_NODE_ATTESTATION=<single-use signed attestation>
```

The Unreal server bridge reads `SP_NODE_ATTESTATION` from the process environment and sends it only to `POST /v1/servers/register`. After an attested registration succeeds and the backend confirms consumption, the subsystem clears its in-memory copy.

## Recovery behavior

A consumed attestation is deliberately not reusable. If an attested server later loses node authority because its credential state becomes uncertain or invalid, the Unreal bridge fails closed instead of replaying the original registration assertion.

Production orchestration should terminate/relaunch that server with a fresh attestation. This preserves one-time registration semantics and gives the orchestration layer control over server replacement.

Local/non-production backends may set `REQUIRE_SERVER_ATTESTATION=false` for development-only registration, but that mode is not the production trust boundary.

## Validation boundary

GitHub/PostgreSQL integration tests cover:

- missing attestation rejection;
- signed-field mismatch rejection;
- valid attested registration;
- one-time attestation consumption;
- replay rejection;
- persisted attestation audit metadata;
- compatibility with per-node credential issue/rotation and regional scheduling.

The Unreal bridge remains source-reviewed only until UE5.6/UHT and packaged dedicated-server testing are available.
