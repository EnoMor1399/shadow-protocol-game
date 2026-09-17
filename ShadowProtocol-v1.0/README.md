# SHADOW PROTOCOL — Embassy Vertical Slice v1.0.1
### Every Move Is Classified.

**Shadow Protocol** is a tactical first-person shooter development foundation built around intelligence warfare, planning, infiltration, breaching, objective recovery, extraction, and server-authoritative competitive play.

The `ShadowProtocol-v1.0/` directory now carries the **v1.0.1 stability patch** on top of the v1.0 Embassy / Protocol vertical slice. The previous `ShadowProtocol-v0.9/` directory remains preserved as the earlier validated snapshot.

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
- `USPBuildInfoLibrary` — shared release version, network build id, content revision and exact-match compatibility helpers for Blueprint/C++ session integration.

Existing foundations include server-authoritative Protocol round state, player slots, authenticated sessions, weapons, health/injuries, lag-compensation hooks, tactical equipment, intelligence nodes, alert state, fortification, doors/security devices, observer rules and competitive scoring.

## Backend

`Backend/` uses TypeScript/Fastify with PostgreSQL, Redis and WebSocket foundations.

```bash
cd Backend
cp .env.example .env
npm install
npm run dev
```

For local PostgreSQL + Redis:

```bash
docker compose up -d
```

Apply `db/schema.sql` to PostgreSQL before starting persistent services. Replace all development secrets before any production deployment.

The current backend service/protocol generation remains `0.7.0`. v1.0.1 deliberately does **not** rename that service version without first wiring `SP-1.0.1` into authenticated game-session and server-allocation compatibility enforcement.

## Documentation

Start with:
- `Docs/VERTICAL_SLICE_V101.md`
- `Docs/CHANGELOG_V101.md`
- `Docs/VERTICAL_SLICE_V10.md`
- `Docs/CHANGELOG_V10.md`
- `Docs/REQUIREMENTS_TRACEABILITY.md`
- `Docs/ARCHITECTURE.md`
- `Docs/AUTHORITATIVE_MULTIPLAYER_V07.md`
- `Docs/TACTICAL_INTERACTION_V09.md`

## Production validation boundary

The browser v1.0.1 patch has been syntax-checked independently, but the Unreal additions still require a full Unreal Engine environment for Unreal Header Tool validation, C++ compilation, PIE, packaged-client testing and dedicated-server multiplayer testing.

Production content still to author includes final Embassy geometry, skeletal meshes and first-person arms, animation blueprints, UMG production widgets, Niagara effects, MetaSounds/spatial audio, physical material/destruction profiles, nav meshes, online subsystem integration, anti-cheat integration and 10-client network soak testing.

## Development path after v1.0.1

The next production milestones are:
1. compile and integrate `USPBuildInfoLibrary` into UMG/session code;
2. enforce server-owned client/server build compatibility during authenticated session allocation;
3. author the production Embassy map and objective sites;
4. convert browser diagnostics and settings language into UMG widgets;
5. add final first-person character/weapon animation and spatial audio;
6. perform real dedicated-server 5v5 replication, reconnect and latency testing;
7. expand from the hardened vertical slice toward Alpha content.
