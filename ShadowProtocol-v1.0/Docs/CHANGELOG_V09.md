# CHANGELOG — v0.9

## Added
- Lean left/right camera and weapon posture.
- Low-cover vault action.
- Closed → peek → open door interaction.
- Individual destructible surveillance cameras.
- Individual destructible Embassy lights and local shadow treatment.
- Reflex/magnifier optic configuration.
- Weapon inspection animation state.
- Wall/entry impact particles for misses.
- Cover-seeking behavior for suppressed/wounded AI.
- Simulated Embassy vertical route and floor indicator.
- Stair, sofa and sculpture environment props.
- Unreal `ASPInteractiveDoor` and `ASPSecurityDevice`.
- Replicated Unreal lean/vault/optic state.
- Trusted backend tactical-interaction telemetry.

## Changed
- Crouch moved to `Ctrl`; `Z/C` are dedicated lean controls.
- `Space` prioritizes low-cover vault and falls back to fire in the browser training build.
- Camera-network disabled state can now be achieved by physical destruction as well as electronic override.
- Dynamic FOV now follows the selected optic while aiming.
- Door rays/line-of-sight support partially open door slabs.

## Validation
- Browser JavaScript parses with `node --check`.
- HTML IDs and JavaScript selector references are consistent.
- In-memory Chromium flow reached the competitive UI with no page/console JavaScript errors and validated optic/inspection state rendering.
- C++ source/header braces were structurally checked.
- Backend TypeScript parsing reached dependency/type resolution; full type-check cannot complete in this environment because npm dependencies and Node type packages are not installed.
- Unreal compilation and multiplayer PIE/dedicated-server testing remain required.
