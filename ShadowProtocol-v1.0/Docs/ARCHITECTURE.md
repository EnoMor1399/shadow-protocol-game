# Shadow Protocol Architecture

## Product identity
- Tactical FPS; near-future 2034 setting.
- Primary differentiator: **Intelligence Warfare**.
- Core loop: **Observe → Plan → Infiltrate → Execute → Secure → Extract**.
- Competitive centerpiece: **PROTOCOL**, 5 attackers versus 5 defenders, one life per ranked round.
- Production engine: Unreal Engine 5, C++ plus Blueprints.

## Runtime layers

```text
Unreal Game Client
  ├─ Input / Camera / Movement / Weapon presentation
  ├─ Tactical planning UI
  ├─ HUD / audio / accessibility
  └─ cosmetic rendering only for non-authoritative data
        │
        ▼
Regional Matchmaking / Session Service
        │
        ▼
Authoritative Dedicated Game Server
  ├─ movement validation
  ├─ weapon hit validation
  ├─ damage / injury / downed-state rules
  ├─ Protocol objective state machine
  ├─ intelligence discovery state
  ├─ breach/destruction authority
  ├─ AI authority for co-op / Blacksite
  ├─ scoring and round win conditions
  └─ anti-cheat telemetry
        │
        ▼
Game Backend (REST + WebSocket)
  ├─ identity/auth
  ├─ player profiles
  ├─ loadouts/inventory/cosmetics
  ├─ progression/mastery/reputation
  ├─ matchmaking/ranks
  ├─ Task Forces/social
  ├─ seasons/battle pass
  ├─ reports/trust score/bans
  ├─ replay metadata
  └─ analytics/anti-cheat signals
        │
        ├─ PostgreSQL (durable state)
        ├─ Redis (queues/cache/presence/matchmaking)
        └─ Object Storage (replays/assets)
```

## Core gameplay domains

### 1. Intelligence Warfare
Every intelligence source grants a **specific, server-authoritative tactical effect** rather than generic score only.

Examples:
- Communications terminal → reveal true objective zone.
- Security node → disable/loop cameras for a timed window.
- Encrypted device → unlock alternate extraction.
- Surveillance archive → reveal coarse enemy movement zones, never exact wallhacks.
- Building schematic → expose breachable surfaces and alternate routes.

The `ASPIntelligenceNode` actor encapsulates source type, capture duration, team ownership, once-only/repeatable rules, and an effect tag. `ASPProtocolGameMode` applies the result to match state.

### 2. PROTOCOL match state machine

```text
Planning
  -> Infiltration
  -> Intelligence Search
  -> Objective Identified
  -> Intelligence Secured
  -> Extraction Active
  -> Extracted / Attackers Eliminated / Timer Expired
```

Defenders can win by elimination, preventing extraction, or timer expiry. Attackers must both secure intelligence and extract.

### 3. Roles without hero powers
- Breacher: reinforced entry, breach efficiency, ballistic equipment.
- Recon: drones, motion/surveillance tools.
- Tech: hacking, camera/EW interaction.
- Support: ammunition, medical, deployable defense.
- Marksman: optics, long-range observation, overwatch.

Role bonuses are constrained to equipment access/handling and interaction speed. They do not create supernatural abilities.

### 4. Weapon and damage model
Weapons are fictional modern-platform analogues with recoil, mass/handling, magazine capacity, penetration, sound profile, reload speed, and accuracy.

Damage is localized:
- arm injury → stability penalty;
- leg injury → movement penalty;
- severe injury → incapacitation where the mode allows it.

The server owns health, injury, death and revive state.

### 5. Tactical destruction
Destruction is **curated**, not global. Every breachable surface has an explicit data asset/material profile. Full-building collapse is out of scope for competitive maps.

### 6. Sound as gameplay
MetaSounds should emit physically readable cues by material class (metal/concrete/wood/glass/water/gravel). AI perception and enemy human players consume the same noise events conceptually; crouch/silent movement reduces noise radius.

### 7. AI
AI state tree / behavior tree goals:
- patrol and preserve formations;
- investigate suspicious sound;
- use cover and suppression;
- flank when confidence is high;
- retreat/reposition when exposed;
- guard objectives;
- react to camera loss and missing patrols;
- call reinforcements when mission rules allow.

### 8. Planning / squad command
Before deployment, teammates can add entry, route, breach, sniper, objective and rally markers. The Squad Leader gets richer coordination controls but no combat-stat advantage.

### 9. Scoring
Score events are objective-weighted. Kills are intentionally not sufficient to top the scoreboard. Events include intelligence, hacking, recon assists, revive/stabilize, defense time, coordinated actions, objective secure and extraction.

### 10. Security and anti-cheat
- never trust client inventory/progression;
- validate movement and weapon requests server side;
- issue short-lived authenticated session tokens;
- record anomalous movement/aim/economy signals;
- persist replay metadata and report evidence;
- keep enforcement policy regional/legal-aware.

## UI direction
Charcoal / graphite / black / white / tactical grey with controlled red accent. Information is contextual and minimal in-match. The operations-centre main menu is a 3D diegetic hub whose camera moves between Play, Operators, Armory, Task Force, Career, Intelligence, Store and Settings stations.

## Content architecture
Use data assets for weapons, roles, gadgets, mission definitions, map objective sets, intelligence nodes, cosmetics, rank thresholds and seasonal content. This lets design/balance change without recompiling core rules.

## v0.7 Authoritative Competitive Boundary

The competitive client is treated as an input/request source, never as the authority for ranked outcomes. The target chain is:

`Authenticated Client → Regional Allocation → Dedicated UE Server → Trusted Match Events → Backend Persistence`

The dedicated game server validates player readiness, team/slot ownership, spawn group, movement/combat requests, shot age, damage, eliminations, assists, friendly-fire discipline, objective-site state, round result and reconnect reclamation. The backend issues signed short-lived game sessions, stores server allocations and hashed reconnect tickets, and accepts combat telemetry only from trusted match-server credentials.

v0.7 adds the first lag-compensation history hook but does not yet implement final skeletal hitbox rewind. Ranked release requires historical hitboxes, time synchronization, abuse limits, reconciliation, movement validation and dedicated-server soak tests.
