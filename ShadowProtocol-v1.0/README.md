# SHADOW PROTOCOL — Embassy Vertical Slice v1.0.1
### Every Move Is Classified.

**Shadow Protocol** is a tactical first-person shooter development foundation built around intelligence warfare, planning, infiltration, breaching, objective recovery, extraction, and server-authoritative competitive play.

The `ShadowProtocol-v1.0/` directory carries the **v1.0.1 stability patch** on top of the v1.0 Embassy / Protocol vertical slice. The previous `ShadowProtocol-v0.9/` directory remains preserved as the earlier validated snapshot.

## What v1.0.1 contains

### Playable browser vertical slice
`PrototypeWeb/` contains the immediate mechanics and UX simulator. It preserves the v1.0 combat/match systems and adds the v1.0.1 stability and diagnostic layer.

Core flow:

**Observe → Plan → Infiltrate → Execute → Secure → Extract**

Current systems include:
- 10-player competitive ready-room presentation;
- five-member local fireteam simulation;
- attack/defense side rotation and best-of-nine match structure;
- ALPHA / BRAVO / CHARLIE deployment groups;
- rotating Embassy intelligence sites;
- tactical planning and persistent tactical map;
- ADS, sprint/endurance, crouch, lean, vault, weapon inspection and 1×/2× optics;
- cracked/partially open doors and dynamic breaching;
- surveillance cameras, security nodes and destructible local lighting;
- Flash / Smoke tactical equipment;
- suppression, material penetration, localized injury effects and friendly-fire discipline;
- Helix alert escalation, search behavior and QRF response;
- objective overtime, kill feed, reconnect presentation and team-restricted spectating;
- Operation Review and tactical scoring;
- classified-command settings/keybind interface, HUD-density modes, contrast/motion preferences, objective-site confirmation and deployment briefing;
- v1.0.1 Input Hints and Intel Notices preferences;
- v1.0.1 client build-integrity diagnostics and secure-session/reconnect presentation;
- deterministic deployment timer cleanup and safer settings persistence.

Open `PrototypeWeb/index.html` in a modern desktop browser to run the simulator.

## Controls

| Action | Control |
| --- | --- |
| Move | `WASD` |
| Aim | Right mouse button |
| Fire | Left mouse button |
| Reload | `R` |
| Interact / door / vertical route | `E` |
| Hack / recover intelligence | `H` |
| Breach | `B` |
| Sprint | `Shift` |
| Crouch | `Ctrl` |
| Lean / peek | `Z` / `C` |
| Vault | `Space` |
| Tactical map | `Tab` |
| Role gadget | `Q` |
| Cycle throwable | `G` |
| Throw | `F` |
| Squad order | `V` |
| Fortify | `X` |
| Switch optic | `Y` |
| Inspect weapon | `I` |
| HUD density | `F1` |
| Settings | `Esc` |

The browser build is a **mechanics, UX and match-flow simulator**. It is not a claim that ten live network clients are already connected.

## v1.0.1 production interface patch

The additive production-UX layer is implemented in:
- `PrototypeWeb/v1.js`
- `PrototypeWeb/v1.css`
- `PrototypeWeb/v101.css`

v1.0.1 keeps the existing v1.0 gameplay core intact while hardening local preference storage, deployment sequencing, settings close behavior, objective notice timing, client/session diagnostics and page-lifecycle cleanup.

Browser build identity is `SP-1.0.1`.

## Unreal Engine 5 foundation

Open `ShadowProtocol.uproject` in Unreal Engine 5.6, or update `EngineAssociation` if another compatible UE5 version is installed.

Core source lives in `Source/ShadowProtocol/`.

v1.0 introduced:
- `USPCoverSystemComponent` — replicated server-owned cover normal and peek state;
- `ASPObjectiveSiteActor` — replicated authorable objective-site identity and active state;
- `ASPDeploymentDirector` — replicated Authentication → Loadout Check → Insertion → Live sequence;
- `ASPCharacter` integration for server-side cover probing when lean/peek state changes.

v1.0.1 adds:
- `USPBuildInfoLibrary` — shared release version, network build id, content revision and exact-match compatibility helpers for Blueprint/C++ session integration;
- `USPBackendSessionSubsystem` — GameInstance-scoped HTTP/JSON bridge for compatibility checks, signed-session rotation, authenticated Embassy/Protocol allocation, update-required handling and reserved-slot reconnect;
- Blueprint delegates for compatibility, session refresh, allocation, reconnect-ticket, reconnect-complete, upgrade-required and request-failure UI states;
- Blueprint-visible allocation `ConnectHost`, `ConnectPort` and connect-token data with client-side validation before allocation success;
- in-memory session/reconnect token handling with no privileged bootstrap or match-server secret embedded in the shipped client.

`ShadowProtocol.Build.cs` includes `HTTP`, `Json` and `JsonUtilities` for the backend bridge.

Existing foundations include server-authoritative Protocol round state, player slots, authenticated sessions, weapons, health/injuries, lag-compensation hooks, tactical equipment, intelligence nodes, alert state, fortification, doors/security devices, observer rules and competitive scoring.

## Backend

`Backend/` uses TypeScript/Fastify with PostgreSQL, Redis and WebSocket foundations.

For a fresh local backend:

```bash
cd Backend
cp .env.example .env
npm install
npm run db:init
npm run dev
```

For local PostgreSQL + Redis, start the services before `db:init`:

```bash
docker compose up -d
```

`npm run db:init` applies the base schema plus the v1.0.1 connection/admission and server-registry migrations in order. To upgrade an existing v1.0 database without reapplying the full base schema:

```bash
npm run db:upgrade:v101
```

Replace every development secret before any production deployment.

The backend protocol is **`0.8.0`** and server-owned compatibility enforcement is active. The default accepted network build is **`SP-1.0.1`**. Authenticated session creation rejects unsupported builds with HTTP `426`, signed sessions carry build/protocol identity, and match allocation validates both values again before reserving a server. `ACCEPTED_NETWORK_BUILDS` may be used for an explicit controlled rollout policy.

### Regional dedicated-server registry and allocation

PostgreSQL-backed production allocation now uses a health-aware regional server registry instead of a static address. Trusted dedicated servers register themselves through:

```text
POST /v1/servers/register
POST /v1/servers/heartbeat
POST /v1/servers/drain
POST /v1/servers/release-allocation
x-match-server-secret: <infrastructure-only secret>
```

A registered node declares its stable `serverId`, region, network build, public host/port and allocation capacity. The allocator selects only nodes that:

- are in the authenticated session region;
- advertise the same network build;
- are in `ready` state rather than `draining`/`offline`;
- have a heartbeat newer than `SERVER_HEARTBEAT_TTL_MS` (30 seconds by default);
- still have allocation capacity.

Capacity reservation is updated atomically in PostgreSQL. Expired pre-admission reservations are reclaimed, and the trusted release endpoint decrements node load when an allocation closes or fails. Production returns `503 no-healthy-game-server` when no eligible node exists instead of silently routing to a stale or incompatible server.

For local development only, the legacy static target remains as a fallback:

```bash
GAME_SERVER_PUBLIC_HOST=127.0.0.1
GAME_SERVER_PUBLIC_PORT=7777
```

When PostgreSQL is not configured, allocation continues to use that static development target. With PostgreSQL in production, healthy registered nodes are required.

Each persistent allocation stores its owning user, selected node/server id, target host/port, expiry and a **hash** of the short-lived connect token. The plaintext connect token is returned only to the client connection layer.

### Dedicated-server connection admission

Before accepting the connection, the trusted dedicated server must redeem the presented token through:

```text
POST /v1/matches/admit
x-match-server-secret: <infrastructure-only secret>
```

The admission request supplies `allocationId`, `matchId` and the presented `connectToken`. The backend verifies the token hash, allocation/match identity, expiry and unconsumed state atomically. A successful redemption binds the connection to the allocated `userId`, marks the allocation `live`, records `connect_token_consumed_at`, and returns the selected server/network identity. The same token cannot be replayed; a second redemption is rejected with `403 admission-denied`.

The game client must never contain `MATCH_SERVER_SECRET`. That credential belongs only to backend/dedicated-server infrastructure.

Compatibility metadata is exposed through `/health`, `/v1/compatibility`, authenticated session/allocation responses, reconnect responses and the WebSocket hello payload. Session refresh rotates still-valid 15-minute Bearer sessions; reconnect endpoints enforce session ownership, hashed reconnect tokens and a 90-second reserved-slot deadline. Matchmaking identity/rating/trust are server-owned, and authoritative match telemetry requires the infrastructure-only match-server credential.

### Backend validation

Fast checks:

```bash
cd Backend
npm install
npm run typecheck
npm run test:compatibility
```

PostgreSQL-backed validation:

```bash
npm run db:init
npm run test:postgres
```

GitHub Actions runs all of the above against a disposable PostgreSQL 16 service. The suite validates build compatibility, session issuance/rotation, development/static routing behavior, authenticated matchmaking, server-only telemetry, regional node registration, draining/stale-node exclusion, capacity enforcement, allocation persistence, hashed connect tokens, one-time dedicated-server admission/replay rejection, database-owned ready-state, reserved-slot reconnect recovery and trusted allocation release.

## Documentation

Start with:
- `Docs/VERTICAL_SLICE_V101.md`
- `Docs/CHANGELOG_V101.md`
- `Docs/UE_SESSION_BRIDGE_V101.md`
- `Docs/VERTICAL_SLICE_V10.md`
- `Docs/CHANGELOG_V10.md`
- `Docs/REQUIREMENTS_TRACEABILITY.md`
- `Docs/ARCHITECTURE.md`
- `Docs/AUTHORITATIVE_MULTIPLAYER_V07.md`
- `Docs/TACTICAL_INTERACTION_V09.md`

## Production validation boundary

Browser syntax and backend TypeScript/security/allocation/database behavior are validated in GitHub Actions. The Unreal v1.0.1 additions still require a full Unreal Engine environment for Unreal Header Tool validation, C++ compilation, PIE, packaged-client testing and dedicated-server multiplayer testing.

Production content still to author includes final Embassy geometry, skeletal meshes and first-person arms, animation blueprints, UMG production widgets, Niagara effects, MetaSounds/spatial audio, physical material/destruction profiles, nav meshes, online subsystem integration, anti-cheat integration and 10-client network soak testing.

## Development path after v1.0.1

The next production milestones are:
1. compile the v1.0.1 Unreal networking additions in UE5.6 and resolve any UHT/compiler-specific issues;
2. bind ready-room, session-expiry and reconnect UMG widgets to `USPBackendSessionSubsystem` delegates;
3. implement the trusted platform/account bootstrap that supplies signed game sessions to Unreal;
4. wire `ConnectHost`, `ConnectPort` and the short-lived connect token into OnlineSubsystem/client travel;
5. have the dedicated-server connection handler redeem `/v1/matches/admit` before spawning/possessing the player;
6. integrate the regional server registry with the real dedicated-server process/orchestrator and replace the shared infrastructure credential with per-node identity;
7. author the production Embassy map and objective sites;
8. add final first-person character/weapon animation and spatial audio;
9. perform real dedicated-server 5v5 replication, token-refresh, admission, reconnect, scheduler-failover, round-transition and latency testing;
10. expand from the hardened vertical slice toward Alpha content.
