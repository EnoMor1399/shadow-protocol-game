# SHADOW PROTOCOL v0.8 — Advanced Embassy & Production Combat

## Purpose
v0.8 moves the Embassy vertical slice from a clean competitive prototype toward a production-facing tactical presentation layer. It keeps the intelligence-first Protocol loop intact while making movement, weapon handling, room transitions, breaching, suppression and post-round review easier to read and more atmospheric.

## Browser vertical-slice additions
- Sector-aware Embassy navigation: West Service, Diplomatic Wing, Security Hub, Archive Core, Motor Pool and East Annex.
- Surface profiles: concrete, marble, metal, carpet and gravel.
- Material-aware procedural footsteps with different acoustic signatures.
- Location/surface ribbon in the live HUD and tactical device.
- Low-ready sprint posture, reload weapon tilt/drop, ADS stability and existing recoil/bob integration.
- Muzzle lighting pulse, ejected casing particles and small muzzle sparks.
- Breach dust/spark bursts plus short environmental shock lighting.
- Suppression vignette treatment tied to the combat suppression state.
- Sector lighting profiles and lockdown alarm tint.
- Cinematic Operation Review entrance and four summary cards for stealth, discipline, intelligence and final sector.

## Unreal Engine parity
`ASPEnvironmentZone` defines authorable Embassy zones with a box trigger, zone name, surface profile and lighting profile.

`USPCombatFeedbackComponent` is a replicated presentation bridge. The authoritative server can multicast accepted weapon-fire and suppression feedback while Blueprint/Niagara/MetaSounds provide final client presentation. The component deliberately carries presentation events only; it does not decide hits, damage, score or competitive state.

Production integration should connect:
- `BP_WeaponFired` → first-person animation montage, muzzle Niagara, shell ejection and MetaSound.
- `BP_Suppression` → camera/post-process/audio ducking based on local intensity.
- `BP_BreachImpact` → Chaos/Niagara debris and breach impulse presentation.
- `ASPEnvironmentZone` → physical-material footsteps, room tone, reverb sends and lighting/PP volumes.

## Backend / telemetry
`environment_events` stores trusted match-server presentation/context events for replay diagnostics and balancing. It supports `sector_enter`, `footstep`, `breach_fx`, `suppression_fx` and `weapon_state`. This is telemetry, not authority: clients must never use it to override damage, objective or round state.

## Production gates still open
- Replace procedural browser audio with authored MetaSounds and material/footwear data.
- Build final Embassy modular kit and destructible-door assets in Unreal.
- Add first-person arms, weapon skeletal mesh, recoil/IK and animation blueprint.
- Author Niagara muzzle, dust, spark, smoke and casing systems.
- Add calibrated post-process volumes for room lighting and suppression.
- Compile/test all v0.8 C++ against the target Unreal Engine version.
- Run 10-client PIE and dedicated-server soak tests for presentation RPC volume and relevancy.
