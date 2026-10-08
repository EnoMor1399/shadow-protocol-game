# Native gamepad controls — v1.0.1 continuation

This pass extends the native Unreal control layer with a source-level controller profile while preserving the existing keyboard/mouse override model and all server-authoritative gameplay checks.

## Default controller profile

- Left stick: move forward/back and strafe.
- Right stick: camera yaw/pitch through dedicated gamepad axes.
- Right trigger: fire.
- Left trigger: aim.
- X / Square: reload.
- A / Cross: vault.
- Y / Triangle: cycle optic.
- B / Circle: inspect weapon.
- Left stick click: sprint.
- Right stick click: crouch / silent movement.
- D-pad up: cycle equipment.
- D-pad down: cycle squad order.
- D-pad left/right: lean left/right.
- Right shoulder: throw equipment.
- Left shoulder: fortify.
- Menu / Start: controls panel.
- View / Back: scoreboard.

The exact display names vary by controller platform, but Unreal receives the same logical gamepad keys.

## Look behavior

Mouse delta input remains on the existing `Turn` and `LookUp` axes. Controller camera input uses separate `TurnGamepad` and `LookUpGamepad` axes so stick input can be scaled by frame time rather than being treated like mouse delta.

Controller look now has its own persisted tuning profile in the native Controls panel: normal look sensitivity (0.25–3.00x), ADS sensitivity multiplier (0.10–1.00x), right-stick dead zone (5–50%) and vertical inversion. Defaults are 1.00x normal look, 0.75x ADS, 18% dead zone and non-inverted Y. Invalid or non-finite saved values are clamped/fallback safely before use.

The right-stick dead zone is applied before look-rate scaling, then the remaining stick range is renormalized so reaching full deflection still produces the configured maximum turn rate. Mouse sensitivity and mouse inversion are no longer read by the controller look handlers.

All look handlers still call `CanUseLocalControls()`, so ready-room/controls UI, downed/eliminated state and other local gameplay gates block controller camera input the same way they block mouse input.

## Rebinding boundary

The existing action and movement override system intentionally rewrites only keyboard/mouse mappings. Gamepad alternatives are preserved when keyboard actions are rebound, swapped, reset or when keyboard movement is changed.

The native Controls panel now also provides saved controller action-button remapping for the supported combat/tactics actions. Controller overrides are stored separately from keyboard/mouse overrides. A controller rebind is rejected before mutation if the key is not a gamepad button, is used by movement/camera axes, or is already owned by another supported/default action. **Restore original controller buttons** clears only controller action overrides; look sensitivity, dead zone, inversion and keyboard/mouse mappings are preserved.

Occupied supported controller buttons now enter an explicit **Confirm controller swap** state. Confirmation rechecks the current owner, requires exactly one gamepad mapping for each action, swaps the two buttons as one candidate, and rolls back the saved override set if full input validation fails. Changing the selected controller action, restoring bindings, or closing Controls clears pending confirmation. Platform-specific glyph switching and deeper platform navigation remain runtime work.

## CI contract

`Backend/test/gamepad-controls-contract.mjs` now runs with the existing Unreal source-contract suite. It checks required stock controller mappings, dedicated right-stick look handlers, frame-time scaling, gameplay gating, independent sensitivity/ADS/inversion/dead-zone settings, safe clamping, native UI wiring, controller action override isolation, confirmed atomic swaps with rollback/stale-owner protection, and preservation of keyboard/gamepad alternatives across resets.

## Runtime validation still required

UE5.6/UHT and packaged client checks remain required. Test at minimum 5/18/50% dead zones, small-stick drift, diagonals, right-stick look at 30/60/120+ FPS, independent mouse/controller sensitivity, ADS scaling, controller-only inversion, persistence/reset, modal transitions with held sticks/triggers, scoreboard/menu buttons and keyboard rebinding followed by controller use.
