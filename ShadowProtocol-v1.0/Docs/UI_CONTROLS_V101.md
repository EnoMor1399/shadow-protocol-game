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
Reset restores only mouse preferences. Action rebinding is described below; gamepad navigation remains pending. The control reference reads current input mappings,
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

## Ready-room spawn selection and feedback

The owning controller receives only the server-filtered spawn group names for its
admitted team during editable planning. A native dropdown submits the selected
name through the existing authoritative GameMode validation. Changing the spawn
still clears readiness. The UI never sets replicated player state locally.
Configure GameMode `SpawnGroups` and corresponding tagged PlayerStarts in the map;
no choices are fabricated for an unconfigured map. Empty/unavailable choices leave
the dropdown disabled, with explanatory text.

Ready/spawn requests allow one outstanding operation, retain local/server rate
limits, and return an owning-client acknowledgement with a matching request ID.
Controls disable while awaiting confirmation. A four-second timeout reports that
confirmation is unknown, allowing a retry; late acknowledgements cannot overwrite
a newer request's status. Replicated state remains the source of truth. The server
still validates every mutation even if displayed choices are stale. Choice lists
refresh server-side every 250 ms, including team, phase and service-health changes.

Additional UE checks: two clients on different teams see only eligible groups;
selecting a different spawn clears readiness; rejected/stale choices do not mutate
state; keyboard dropdown navigation does not trigger requests during replication;
delayed, dropped and reordered acknowledgements leave controls recoverable. Source
checks cover the wiring; engine/UHT and packaged UI verification remain pending.

## Action key rebinding

The combat/tactics section now includes an action picker, native key-capture
control and Restore original action bindings button. Select an action, click its
key and press a keyboard key or mouse button. Escape cancels capture before it
closes the panel. Successful changes immediately refresh the displayed bindings.

Rebinding supports one keyboard/mouse binding per supported action. It replaces
that action's existing keyboard/mouse alternatives; gamepad mappings are retained.
Movement axes, menu controls, modifier combinations and gamepad remapping remain
outside this pass. Conflicts with other actions (including config-only actions),
movement/look axes, console and reserved navigation keys are rejected without
changing the current mapping. To swap two keys interactively, first move one action
to an unused key. Saved swaps are loaded and validated as an atomic candidate.

Only our action overrides are saved to GameUserSettings; project Input defaults
are not rewritten. RebuildKeymaps applies the runtime change. Restore resets action
bindings to the process's original project mappings and preserves mouse settings.
Settings remain machine-wide, including all PIE/local-player instances in that
process. Invalid saved overrides are ignored with feedback in the controls panel;
Restore recovers the baseline. Reopening PIE after editing project defaults may
require restarting the editor to recapture the baseline.

Run UE automation `ShadowProtocol.Controls.AtomicRebinding` on the stock project
input configuration to verify conflict rejection, unchanged state on failure,
valid replacement and saved swaps. The test restores runtime mappings and does not
save config. This test has been added but cannot run here without UE. Also test
capture cancellation, keyboard focus, mouse-button capture, restart persistence,
reset, and held-action release in packaged clients before runtime sign-off.

API references: [key selector](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UInputKeySelector)
and [input settings](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UInputSettings).
