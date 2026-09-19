# Native UI and controls

The native Protocol GameMode now selects `ASPCombatHUD`. It displays replicated
round/time/score, health, stamina, movement stance, magazine count, selected
throwable and a crosshair. Empty weapon and missing-pawn states are explicit.
The HUD is hidden behind ready-room/controls UI and scales to the viewport.
The counter after magazine ammo is magazine capacity, not reserve ammo.

The ready room now has a bounded, scrollable deployment panel, readable team
roster with text connection/readiness states, an accent Ready button, and a
Controls button. It refreshes at 10 Hz instead of reconstructing text each frame.
Admission/expiry/connection notices remain present. Readiness still goes through
the existing owning-controller RPC and authoritative GameMode checks.

Press Escape in gameplay or the ready room to open controls. Escape or Return to
game closes it. Tab navigates UMG controls; sensitivity and invert-Y have native
slider/checkbox controls. Settings apply immediately and save to local
GameUserSettings config on closing or controller teardown. The settings currently
apply to the machine profile, not separate split-screen player profiles. Sensitivity
is clamped to 0.25–3.0, including validation of invalid/non-finite config values.
Reset restores only mouse preferences. Key rebinding and gamepad navigation are
not implemented in this pass. The control reference reads current input mappings,
and excludes config-only actions without native implementation.

Opening/closing UI releases aim, sprint, crouch and both lean directions, then
flushes held keys. A paired input-ignore state prevents stack accumulation while
preserving ignores belonging to other systems. Closing controls restores the
ready room if it is still active; otherwise gameplay receives input. The online
match is never paused by the controls panel.

Native pawn changes enable crouching, add a control-rotation first-person camera,
apply mouse preferences, and fix simultaneous lean-key release order. Crouching
cancels sprint locally and on the server. These changes do not add automatic fire,
weapon assets, interaction/hacking, a tactical map, lean-camera animation or
server-side combat permission changes. Existing weapon/fortification/equipment
content must be configured for their corresponding actions to produce gameplay.

## Validation

CI compiles and executes the pawn's shared held-lean state for overlapping keys,
release order and modal-reset regression cases. Source contracts check modal
release/focus wiring and HUD authority boundaries alongside existing admission
checks. These are not Unreal compilation or rendered UI validation.

Required UE5.6 checks (engine unavailable in this workspace):

1. Run UHT and Development Client/Server builds; check native widget creation.
2. At 1280x720, 1920x1080 and ultrawide, inspect readable layout and scroll to the
   final controls/roster entry. Verify keyboard focus and slider/checkbox access.
3. Hold aim/sprint/crouch/lean, open/close controls repeatedly, then press fresh
   inputs. Confirm no stuck actions, movement or mouse-look behind the UI.
4. Enter controls during planning and allow the phase to change while open;
   close it and verify gameplay focus/cursor state. Do the inverse on a new lobby.
5. Test ready/cancel on two clients and verify roster, health, ammo and clock
   reflect replication rather than local optimistic state.
6. Change sensitivity/invert, restart and check persistence. Reset and restart.
7. Test downed/eliminated players, possession changes and travel while menus
   are open. Inspect Blueprint pawn subclasses for camera overrides before shipping.
