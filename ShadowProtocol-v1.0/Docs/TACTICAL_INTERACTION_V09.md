# SHADOW PROTOCOL v0.9 — Tactical Interaction & Deeper First-Person Combat

## Goal
v0.9 turns more of EMBASSY into a tactical object rather than a static backdrop. The player can now manipulate sightlines, lighting, cover, posture and vertical routes while retaining the intelligence-first PROTOCOL loop.

## Browser vertical-slice additions
- Lean left/right (`Z` / `C`) changes the camera origin and weapon posture.
- Crouch is on `Ctrl`; sprint remains `Shift`.
- Low-cover vaulting (`Space`) validates a nearby vaultable prop and clear landing point.
- Doors have a three-step state: closed → cracked/peek → fully open. Breach still forces an immediate entry.
- Cameras can be shot out individually; destroying all local camera nodes blinds the network.
- Lamps can be destroyed to create darker local corridors.
- Reflex 1× / magnifier 2× optic switching (`Y`) changes ADS FOV and look sensitivity.
- Weapon inspection (`I`) has a dedicated weapon posture and HUD state.
- Missed rounds produce localized wall/door impact particles.
- Suppressed or badly wounded enemies seek nearby cover objects.
- Embassy includes two linked stair/vertical-route points with floor-state presentation.
- Additional room dressing: stairs, lounge seating and a diplomatic sculpture.

## Unreal foundation additions
### `ASPInteractiveDoor`
Replicated door state (`Closed`, `Peek`, `Open`, `Breached`) with server-authoritative cycling/breach requests and Blueprint presentation hooks.

### `ASPSecurityDevice`
Replicated destructible security-device actor for cameras, lighting nodes and alarm panels. Competitive damage is applied only on authority.

### `ASPCharacter`
Adds replicated lean state, vault state and optic mode; validated server vault traces; optic-cycle RPC; weapon-inspection Blueprint hook; new input mappings.

### `ASPWeaponBase`
Authoritative weapon traces now recognize `ASPSecurityDevice` before continuing character penetration logic.

## Backend telemetry
`tactical_interaction_events` records trusted game-server events for door peeking/opening/breaching, destroyed cameras/lights, vaulting, leaning, optic changes and vertical-route use. These events are diagnostic/replay inputs only; they do not grant client authority.

## Production notes
The browser build represents the tactical intent. Final production behavior should use skeletal first-person animation, physical-material impact tables, replicated door animation curves, real nav links/stairs, destructible light components, MetaSounds and Niagara in Unreal Engine.
