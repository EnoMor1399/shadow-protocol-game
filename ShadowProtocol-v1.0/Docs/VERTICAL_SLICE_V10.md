# SHADOW PROTOCOL v1.0 — Embassy Vertical Slice

## Purpose
v1.0 consolidates the browser training build and Unreal production foundation into a clearer vertical-slice milestone. The goal is not to claim a finished commercial shooter; it is to make the EMBASSY / PROTOCOL experience coherent from operations menu through deployment, intelligence discovery, tactical engagement, extraction, spectating, and round review.

## Browser presentation layer
The v1.0 browser build keeps the validated v0.9 simulation and adds a production UX layer in `PrototypeWeb/v1.js` and `PrototypeWeb/v1.css`.

New presentation systems include:
- professional settings and keybind screen;
- persistent HUD-density preference;
- optional enhanced-contrast and reduced-motion presentation modes;
- optional network-telemetry visibility;
- F1 HUD-density shortcut and Escape settings access;
- objective-site confirmation ribbon when verified SIGINT reveals ARCHIVE-A, VAULT-B, or SAFE-C;
- contextual cover/peek indicator driven by the existing door, vault, and lean states;
- staged deployment briefing with authentication, loadout, insertion, and live phases;
- updated v1.0 build identity while preserving the classified military command visual language.

The existing v0.9 simulation remains responsible for movement, ADS, sprint/endurance, localized injury effects, leaning, vaulting, door peeking, breaching, surveillance, destructible cameras/lights, flash/smoke, AI response, attack/defense rounds, overtime, five-member team simulation, objective rotation, kill feed, reconnect presentation, and Operation Review.

## Unreal production additions
### `USPCoverSystemComponent`
Replicated cover state for the owning character. The server owns whether the player is considered in cover, the normalized cover surface normal, and the current peek amount. `ASPCharacter` now creates this component and performs an authority-side forward cover trace when lean state changes.

### `ASPObjectiveSiteActor`
Authorable and replicated objective-site identity. Each site carries a `SiteId`, human-readable display name, Embassy zone, and server-owned active-site state. Blueprint presentation can react through `BP_OnObjectiveSiteStateChanged`.

### `ASPDeploymentDirector`
Replicated deployment state for Authentication → Loadout Check → Insertion → Live. Spawn groups are constrained to ALPHA, BRAVO, or CHARLIE before the sequence begins.

## Authority boundary
The client may request presentation or movement intent, but competitive truth remains server-owned. Objective activation, cover state, deployment phase, combat resolution, player state, and match state must not depend on untrusted client-only values in ranked production.

## Remaining production gates
v1.0 still requires Unreal Engine 5 compilation/UHT validation, production Embassy geometry, animation blueprints, first-person arms/weapon assets, UMG implementation of the browser UX, Niagara/MetaSounds, nav-mesh authoring, online-subsystem integration, packaged dedicated-server builds, ten-client soak tests, latency/packet-loss testing, anti-cheat integration, and final content/performance optimization.
