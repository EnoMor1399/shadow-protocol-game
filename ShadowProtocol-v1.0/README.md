# SHADOW PROTOCOL — Tactical Interaction Vertical Slice v0.9
### Every Move Is Classified.

This repository is an implementation foundation for **Shadow Protocol**, a realistic tactical first-person shooter built around intelligence warfare, planning, infiltration, breaching, objective recovery, extraction, and server-authoritative competitive play.

## What is included now

1. **Playable browser vertical-slice simulator** (`PrototypeWeb/`) for validating the core loop: Observe → Plan → Infiltrate → Execute → Secure → Extract. The current v0.9 build includes a 10-player ready-room presentation, five-member local fireteam simulation, rotating Embassy intelligence sites, attack/defense round flow, spawn-group selection, ready checks, ADS, sprint/endurance, tactical equipment, squad orders, surveillance/alert escalation, QRF behavior, objective overtime, kill feed, headshots, assists, friendly-fire discipline, reconnect presentation, team-restricted spectating, match review and network telemetry.
2. **Unreal Engine 5 C++ project scaffold** with server-authoritative foundations for Protocol mode, authenticated competitive slots, round/side state, weapons, localized injuries, headshots/assists/team-damage policy, lag-compensation hooks, intelligence objectives, tactical planning, breaching, fortification, surveillance, roles, scoring, observer state and objective-site rotation.
3. **Backend service scaffold** (`Backend/`) using TypeScript/Fastify, PostgreSQL, Redis, WebSockets, signed game sessions, server allocation records, hashed reconnect tickets and trusted-server combat telemetry.
4. **Database schema** covering accounts, profiles, characters, loadouts, weapons, cosmetics, inventory, matches, player slots, rounds, rankings, Task Forces, parties, reports/bans, seasons, missions, contracts, transactions, anti-cheat events, reconnect state, server allocations, combat events and replay metadata.
5. **Requirements traceability and phase documentation** mapping the design document into production modules and staged implementation milestones.

## Play the immediate prototype

Open `PrototypeWeb/index.html` in a modern desktop browser.

### Core controls
- `WASD` — move
- Mouse or `←/→` — look/turn
- Left click — fire
- Right mouse button — aim down sights
- `R` — reload
- `E` — interact/open door
- `H` — hack/recover intelligence
- `B` — breach nearby breachable entry
- `Ctrl` — crouch / lower movement signature
- `Z` / `C` — lean left / right
- `Shift` — sprint / consume endurance
- `Q` — use role gadget
- `G` — cycle Flash / Smoke
- `F` — throw selected tactical grenade
- `V` — cycle squad order (Follow / Hold / Assault)
- `X` — fortify a valid entry during DEFENSE preparation
- `Tab` — tactical map
- `N` — cycle surviving teammate spectator target after death
- `M` — observer-mode toggle when permitted
- `Space` — vault low cover when available
- `I` — inspect weapon
- `Y` — switch Reflex 1× / Magnifier 2× optic

The browser build is a **mechanics, UX and match-flow simulator**. It is not a substitute for the Unreal Engine production client or a claim that ten real network clients are already connected.

## v0.7 competitive flow

1. Select a tactical specialization.
2. Enter the authenticated 10-slot ready-room simulation.
3. Select ALPHA / BRAVO / CHARLIE Embassy deployment group.
4. Mark ready and open tactical planning.
5. Deploy into Preparation, then Action.
6. On attack, discover which of three intelligence sites is active, secure the data and extract.
7. On defense, fortify, monitor and deny intelligence recovery until elimination or timer victory.
8. Match score persists across side rotations; match-point, deciding-round and objective-overtime states are represented.
9. Competitive observer rules restrict live-round information exposure.

## Unreal Engine project

Open `ShadowProtocol.uproject` in Unreal Engine 5.6 or update `EngineAssociation` if your installed UE5 version differs.

Core classes are under `Source/ShadowProtocol/`.

Production content still requiring Unreal Editor authoring includes final Embassy geometry, animation blueprints, skeletal meshes, material/destruction profiles, Niagara effects, MetaSounds/spatial audio, UMG production widgets, nav meshes, online subsystem integration and packaged dedicated-server targets.

### v0.7 authority boundaries
The client requests actions; the server owns competitive results. The C++ foundation now contains authenticated-session gating, server shot-age validation, server damage policy, headshot/assist attribution, friendly-fire escalation, reconnect slot reclamation, objective-site selection and a short location-history component for future lag compensation.

The current lag-compensation implementation is intentionally a **foundation**, not final skeletal hitbox rewind. Full ranked production still requires historical hitboxes, clock calibration, reconciliation, packet-loss/latency soak testing and ten-client dedicated-server validation.

## Backend

```bash
cd Backend
cp .env.example .env
npm install
npm run dev
```

A local PostgreSQL + Redis environment can be started with:

```bash
docker compose up -d
```

Apply `db/schema.sql` to PostgreSQL before starting persistent services.

Important v0.7 security variables include `SESSION_SIGNING_SECRET`, `SESSION_BOOTSTRAP_SECRET`, and `MATCH_SERVER_SECRET`. Replace development values with securely generated secrets and rotate them in production.

## Development order

The project intentionally follows the staged requirements approach:
- Phase 1: prototype combat and objective loop.
- Phase 2: polished **EMBASSY + PROTOCOL + 5v5** vertical slice.
- Phase 3: Alpha systems and content expansion.
- Phase 4: Closed Beta, ranked/replay/social/regional services.
- Phase 5: launch content and hardening.
- Post-launch: BLACKSITE extraction mode and franchise extensions.

Key documentation:
- `Docs/REQUIREMENTS_TRACEABILITY.md`
- `Docs/ARCHITECTURE.md`
- `Docs/AUTHORITATIVE_MULTIPLAYER_V07.md`
- `Docs/COMPETITIVE_MATCH_LAYER_V06.md`
- `Docs/MATCH_SYSTEMS_V05.md`
- `Docs/COMPETITIVE_VERTICAL_SLICE_V04.md`
- `Docs/VERTICAL_SLICE_V03.md`
- `Docs/PROFESSIONAL_POLISH_V02.md`
- `Docs/CHANGELOG_V07.md`

## v0.9 validation status

The browser build has been exercised through the professional pre-match flow in an in-memory Chromium page because normal localhost/file navigation is restricted in this execution environment. No browser JavaScript page/console errors were produced; v0.9 optic and weapon-inspection presentation states were verified. JavaScript syntax, DOM selector references, duplicate IDs, C++ structural checks and archive integrity are also validated.

The Unreal additions have **not** been compiled or 10-client PIE/dedicated-server tested in this environment. The backend scaffold has been syntax-checked but its dependencies/services have not been runtime-integrated here. Those are the next production validation gates.


## v0.7 / v0.7.1 — Authoritative Combat + Professional Interface
The current revision adds authenticated-session and reconnect foundations, five-player team simulation, headshots, assists, team-damage discipline, server-authority combat hooks, rotating Embassy intelligence sites, and a complete professional UI pass across the operations menu, role selection, ready room, planning device, live HUD, tactical overlays, kill feed and Operation Review. See `Docs/CHANGELOG_V07.md` and `Docs/PROFESSIONAL_INTERFACE_V071.md`.


## v0.8 — Advanced Embassy & Production Combat
The current revision adds sector-aware Embassy navigation, material-aware footsteps, surface/room telemetry, improved sprint/reload weapon posture, muzzle/casing/spark feedback, breach debris, suppression/lockdown treatment and a more cinematic Operation Review. Unreal now includes `USPCombatFeedbackComponent` and `ASPEnvironmentZone`; the backend includes trusted environment/presentation telemetry. See `Docs/ADVANCED_EMBASSY_COMBAT_V08.md` and `Docs/CHANGELOG_V08.md`.


## v0.9 — Tactical Interaction & Deeper First-Person Combat
This revision adds camera-position leaning, validated low-cover vaulting, cracked/partially open doors, destructible surveillance cameras and lighting, optic switching, weapon inspection, impact feedback, cover-seeking AI and a simulated Embassy vertical route. Unreal now includes replicated door/security-device actors plus lean/vault/optic state, and the backend includes trusted tactical-interaction telemetry. See `Docs/TACTICAL_INTERACTION_V09.md` and `Docs/CHANGELOG_V09.md`.
