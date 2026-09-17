# SHADOW PROTOCOL — v0.5 Match Systems Vertical Slice

## Playable prototype
- Persistent best-of-nine match session; first team to five rounds wins.
- Automatic attack/defense side rotation between rounds.
- Defense-round objective loop: deny communications access, defend the SP data package, intercept the carrier before extraction.
- Preparation-phase fortification with limited barricade inventory (`X`).
- Grid navigation for attacking AI and barricade breaching.
- Projectile flash/smoke grenades with visible flight arc, gravity and bounce.
- Selected-material bullet penetration with reduced post-penetration damage.
- Near-miss suppression behavior and telemetry.
- Observer improvements: teammate cycling (`N`) and free camera (`M`).
- Round review now carries Directorate Nine / Helix score between rounds.
- Dynamic faction naming/visual treatment when the player changes side.

## Unreal Engine foundation
- Replicated attacking/defending team state and configurable side rotation.
- Best-of-nine cap remains server authoritative.
- `ASPBarricade`: replicated deployable fortification with health and destruction state.
- `ASPTacticalProjectile`: replicated bouncing/fused tactical projectile foundation.
- `ASPObserverPlayerController`: replicated observer target/free-camera state.
- Weapon penetration supports tagged penetrable surfaces with damage falloff.
- Suppression hook emitted to characters near the authoritative shot segment.
- `FSPSpawnGroup` foundation for team/round spawn groups.

## Backend
- Backend service version raised to `0.5.0`.
- Added authoritative 10-slot round player assignments.
- Added fortification event telemetry.
- Added ballistic penetration/suppression telemetry.
- Added observer session persistence model.
- Added trusted-server endpoints for player slots, fortifications and ballistic events.

## Validation
- Browser JavaScript syntax: passed `node --check`.
- HTML duplicate IDs / JavaScript element references: validated.
- Simulated runtime smoke test: boot -> preparation -> action -> round win -> Round 2 defense -> barricade placement -> defense action passed.
- Unreal code is source-complete but requires an Unreal Engine 5 toolchain for compilation and PIE/network testing.
