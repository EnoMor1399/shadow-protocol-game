# Requirements Traceability

This matrix preserves the complete design intent while distinguishing **implemented foundation**, **prototype validation**, and **production content work**.

| # | Requirement | Implementation target | Current repository state |
|---|---|---|---|
| 1 | Core tactical loop | Protocol state machine + planning | Foundation + web prototype |
| 2 | Intelligence Warfare | Intelligence nodes/effects | Foundation + web prototype |
| 3 | 2034 setting | Narrative/content layer | Defined |
| 4 | Directorate Nine / Helix | Faction data/narrative | Defined |
| 5 | First Contact campaign | Mission framework, solo/co-op | Architecture-ready |
| 6 | 10 campaign missions | Mission definitions/content | Production content phase |
| 7 | 5v5 tactical combat | Dedicated-server rules | Foundation |
| 8 | PROTOCOL mode | `ASPProtocolGameMode` | Foundation + web prototype |
| 9 | BLACKSITE extraction | Extraction service/inventory-loss rules | Post-core expansion module |
| 10 | Special Operations co-op | AI mission framework | Architecture-ready |
| 11 | Five tactical roles | role enum/data and equipment rules | Foundation + web prototype |
| 12 | Weapon system | `ASPWeaponBase` + data-driven stats | Foundation + web prototype |
| 13 | Gadgets | gadget interfaces / role equipment | Foundation + web role gadgets + Flash/Smoke tactical equipment |
| 14 | Tactical destruction | `ASPBreachableDoor` / breach surfaces | Foundation + web door breaching |
| 15 | Sound gameplay | noise events / MetaSounds plan | Architecture-ready |
| 16 | Localized health | `USPHealthComponent` | Foundation + web injuries |
| 17 | Downed state | mode-dependent health state | Foundation |
| 18 | Tactical AI | AI controller/state design | Web investigate/search/flank/retreat + allied squad support; UE extension point |
| 19 | Map philosophy | map validation checklist | Defined; Embassy prototype layout |
| 20 | Player customization | profile/cosmetics schema | Backend/database foundation |
| 21 | Progression | level/proficiency/mastery/reputation | Backend/database foundation |
| 22 | Ranked system | ranking schema + matchmaking factors | Backend foundation |
| 23 | Trust Score | reputation/report/ban data | Backend foundation |
| 24 | Task Forces | DB + API route scaffold | Backend foundation |
| 25 | Tournaments | tournament/replay architecture | Planned service module |
| 26 | Economy | Protocol Credits + cosmetic-only rules | DB foundation |
| 27 | Classified Operations battle pass | season/pass schema | DB foundation |
| 28 | Classified military UI | web visual language + UMG spec | Prototype |
| 29 | Operations-centre menu | production UE scene | Architecture-ready |
| 30 | Minimal HUD | web prototype + UMG spec | Prototype |
| 31 | Pre-mission planning | planning component/markers | Foundation + web prototype |
| 32 | Communication | voice/ping/quick-command interfaces | Architecture-ready |
| 33 | Squad Leader | leadership / coordinated order tools | Replicated squad-order foundation + web Follow/Hold/Assault commands |
| 34 | Operation Review replay | event/replay schema | Backend foundation |
| 35 | Objective-based scoring | server score event model | Foundation |
| 36 | Anti-cheat | server authority + telemetry schema | Foundation |
| 37 | Unreal Engine 5 / C++ + Blueprints | UE project scaffold | Included |
| 38 | Multiplayer infrastructure | client → matchmaking → dedicated server → backend | Defined |
| 39 | TS/Go, REST/WS, Postgres, Redis, Docker | Backend scaffold | Included |
| 40 | Game database entities | SQL schema | Included |
| 41 | Phase 1 prototype | one map, teams, weapons, movement, damage, objective, UI | Browser validation build + UE code foundation |
| 42 | Phase 2 vertical slice | Embassy/Protocol/5v5/breach/surveillance/drone/extraction/spectator | Competitive round/tactical-equipment foundation + browser spectator handoff; networked 5v5/full spectator still production milestone |
| 43 | Alpha | 3 maps, weapons, roles, customization, progression, matchmaking | Roadmap |
| 44 | Closed Beta | 6 maps, ranked, replay, battle pass, social, regional servers | Roadmap |
| 45 | Launch | 6–8 maps, Protocol/TDM/Co-op/Custom/Ranked | Roadmap |
| 46 | Premium commercial model | commerce policy | Defined |
| 47 | Creator support | replay/free camera/HUD/screenshot/observer | Replay architecture planned |
| 48 | Brand identity | SHADOW PROTOCOL / S+P symbol direction | Defined |
| 49 | Tagline | “EVERY MOVE IS CLASSIFIED.” | Applied |
| 50 | Franchise | Blacksite, II, Extraction, Shadow Network, World Series | Long-term roadmap |

## Definition of done for the vertical slice
A Phase-2 build is not considered complete until two 5-player teams can join a dedicated server, plan routes, deploy on Embassy, identify the true objective through intelligence interaction, breach at least one curated surface/door, use surveillance/drone information, secure the objective, trigger extraction, spectate after death, and resolve every attacker/defender win condition server-side.

## v0.5 implementation delta
- **Core Multiplayer / PROTOCOL (Sections 7–8):** persistent best-of-nine score, attack/defense round responsibility, side rotation and server-authoritative round cap foundations.
- **Gadgets / Destruction (Sections 13–14):** projectile flash/smoke devices and limited defender barricades at predefined entries; controlled destruction remains tactical rather than structural collapse.
- **Sound / Health / Downed State (Sections 15–17):** prior localized injury systems retained; suppression now reacts to near-miss ballistics and observer behavior is expanded after operator loss.
- **AI Enemies (Section 18):** defense-round attackers navigate toward intelligence stages, fight defenders, breach barricades, capture the package and attempt extraction.
- **Map Philosophy (Section 19):** Embassy entry points now support fortification/breach state and role-specific defensive positioning.
- **Ranked / Tournaments / Replay (Sections 22, 25, 34):** 10 authoritative player slots, round-side telemetry and observer session persistence added to backend foundations.
- **Pre-Mission Planning / Commander (Sections 31, 33):** squad orders retained; defense preparation adds limited fortification decisions.
- **Anti-Cheat / Multiplayer Infrastructure (Sections 36, 38):** fortification, tactical projectile, side, slot and ballistic outcomes are designed to remain server authoritative.

## v0.6 implementation delta
- Core 5v5 competitive structure: 10-slot Tactical Ready Room and authoritative player-slot model.
- Matchmaking/session preparation: ready gate, spawn-group selection, connection/reconnect state foundation.
- Ranked match presentation: match point / deciding round UI and one-time objective overtime.
- Spectator integrity: live-round team-follow restriction; free camera locked during competitive Action/Overtime.
- Competitive observability: kill-feed events, overtime events, ready-state and reconnect contracts in the backend.
- Embassy map philosophy: named tactical callouts and three deployment groups added to planning flow.

## v0.8 traceability update
- **§15 Sound:** browser vertical slice now distinguishes concrete, marble, metal, carpet and gravel footstep profiles; Unreal environment zones expose the surface profile for MetaSound/physical-material implementation.
- **§19 Map Philosophy:** Embassy now exposes six named tactical sectors in the HUD/device and Unreal authoring scaffold.
- **§28 User Interface:** classified-command visual language retained while sector/surface/weapon-state information is presented contextually rather than as permanent center-screen clutter.
- **§30 Match HUD:** current sector and weapon state are added outside the central sight picture; suppression remains primarily visual feedback rather than another numeric HUD meter.
- **§36 Anti-Cheat / authority:** combat-feedback replication remains presentation-only; damage and shot acceptance stay server-authoritative.
- **§37 Technology:** `USPCombatFeedbackComponent` is prepared for Niagara/MetaSounds and `ASPEnvironmentZone` for authored UE environment/physical-material integration.
