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
slider/checkbox controls. Mouse/HUD settings apply immediately and save to local
GameUserSettings config on closing or controller teardown. The settings currently
apply to the machine profile, not separate split-screen player profiles. Sensitivity
is clamped to 0.25–3.0, including validation of invalid/non-finite config values.
Separate reset buttons cover mouse, HUD, and all key bindings. Movement edits require Apply; action rebinding saves immediately. Details follow; gamepad navigation remains pending. The control reference reads current input mappings,
and excludes config-only actions without native implementation.

Opening/closing UI releases aim, sprint, crouch and both lean directions, then
flushes held keys. A paired input-ignore state prevents stack accumulation while
preserving ignores belonging to other systems. Closing controls restores the
ready room if it is still active; otherwise gameplay receives input. The online
match is never paused by the controls panel.

Native pawn changes enable crouching, add a control-rotation first-person camera,
apply mouse preferences, and fix simultaneous lean-key release order. Crouching
cancels sprint locally and on the server. These changes do not add automatic fire,
weapon assets, interaction/hacking, a tactical map or lean-camera animation.
Character health-based action checks are described under incapacitated controls. Existing weapon/fortification/equipment
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
control and Restore all original key bindings button. Select an action, click its
key and press a keyboard key or mouse button. Escape cancels capture before it
closes the panel. Successful changes immediately refresh the displayed bindings.

Rebinding supports one keyboard/mouse binding per supported action. It replaces
that action's existing keyboard/mouse alternatives; gamepad mappings are retained.
Movement keyboard bindings have a separate editor below. Menu controls, mouse-look
axes, modifier combinations and gamepad remapping remain fixed. Movement/look axes, console keys, reserved navigation keys and
unsupported/config-only actions still fail closed without changing live mappings.

When the selected key is already owned by another supported combat/tactics action,
the panel now identifies that action and exposes an explicit **Confirm key swap**
step. Confirming exchanges the two actions' current keyboard/mouse keys in one
atomic candidate; there is no temporary unbound action. If the conflict changes
before confirmation, either action becomes invalid, or full-candidate validation
fails, the previous live bindings remain intact. Changing the selected action,
closing the panel or restoring defaults clears any pending swap confirmation.
Saved swaps are loaded and validated as one atomic candidate.

Action overrides and movement keys are saved to GameUserSettings; project Input defaults
are not rewritten. RebuildKeymaps applies the runtime change. Restore all resets both
action and axis bindings to the process's original project mappings and preserves
mouse and HUD settings. Resetting both together avoids conflicts with reclaimed keys.
Settings remain machine-wide, including all PIE/local-player instances in that
process. Invalid saved overrides are ignored with feedback in the controls panel;
Restore recovers the baseline. Reopening PIE after editing project defaults may
require restarting the editor to recapture the baseline.

Run UE automation `ShadowProtocol.Controls.AtomicRebinding` on the stock project
input configuration to verify conflict rejection, unchanged state on failure,
valid replacement, conflict lookup, confirmed two-action swaps and stale-confirmation rollback. The test restores runtime mappings and does not
save config. This test has been added but cannot run here without UE. Also test
capture cancellation, keyboard focus, mouse-button capture, restart persistence,
reset, and held-action release in packaged clients before runtime sign-off.

API references: [key selector](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/UMG/UInputKeySelector)
and [input settings](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/UInputSettings).

## HUD readability preferences

Controls now contains combat-HUD high contrast, crosshair visibility/size and help
text toggles. The high-contrast HUD uses opaque black panels and a yellow accent;
the crosshair has a black outline for visibility against bright backgrounds. Size
is clamped to 0.75–2.5x, with non-finite config values falling back to 1x. Preferences
apply when returning to gameplay and save with the other controls settings on menu
close or controller teardown. Reset HUD preferences leaves mouse and key bindings
unchanged. These settings affect the combat HUD, not a full menu/theme overhaul.

Objective status uses existing replicated readiness, round and objective flags.
It does not reveal hidden locations or introduce new client authority. Hiding help
text does not hide health, ammunition, score or objective status. Existing rules
still suppress the crosshair while downed, eliminated, sprinting or without a weapon.

UE visual checks remain required: contrast on bright/dark maps, crosshair sizes and
visibility, small/ultrawide viewports, menu close/reopen and restart persistence,
separate resets, and objective text across waiting/preparation/action/post-round.

## 5v5 scoreboard

By default, hold F2 during gameplay to view the two team rosters, K/D/A and tactical scores.
Release F2 to return. A saved toggle option and alternate key binding are available. This is a read-only overlay: movement and gameplay continue,
no mouse capture or pause is added. Opening a modal clears the held scoreboard
state; closing settings cannot leave it stuck open. The final roster appears
automatically at match completion and remains accessible without a pawn.

Rows use authoritative slot order, highlight the local player and show explicit
connection state. Statistics come from replicated PlayerStates. Reserved slots
without a current PlayerState show `--`, not invented zero statistics; durable
post-match/disconnected-player result storage remains separate work. Names are
length-limited and stripped of line breaks/tabs for layout. The overlay exposes no
positions, health or backend account/session IDs. HUD high contrast applies.

F2 is the default scoreboard binding; it can be rebound or swapped in controls. Tab remains available for menu navigation and the future tactical
map. Verify two-client score replication, reconnect/missing-PlayerState rows,
match completion without a pawn, F2 hold/release and Escape transitions, and
720p/1080p/ultrawide layout in UE. Engine compilation/rendering remain unverified
in this environment; CI checks source wiring and existing regression suites.

### Scoreboard binding continuation

The scoreboard action now supports saved keyboard/mouse rebinding and confirmed swaps. F2 remains the default. HUD hold/release prompts read current action mappings, including modifier labels for project defaults. Reset restores the original mapping. UE validation pending: rebind scoreboard, swap with a combat action, reopen settings, restart client, and check hold/release and menu cleanup with the new key.

### Scoreboard toggle preference

Added a saved press-to-toggle scoreboard option in HUD preferences. Hold remains the default. Both help prompts use the selected mode and live binding. Key release closes only hold mode; a second press closes toggle mode. Opening the controls or ready-room modal clears either mode. HUD reset restores hold without resetting action bindings. The final roster remains visible at match completion. UE runtime validation pending: both modes, remapped keys, menu transitions, saved preference after restart, HUD reset and match completion.

### Incapacitated character controls

Character action RPCs now recheck alive/not-downed state for firing, reload, aim, sprint, silent movement, lean, vault, optic changes, squad orders and fortification. Release requests remain accepted. Authority clears aim/sprint/silent/lean and cover peek while incapacitated; the local control transition releases held inputs once instead of sending releases every frame. Lean input is a reliable press/release RPC and rejects nonfinite values.

Validation: Node source contracts cover the handler gates and cleanup wiring. UE5.6 compilation and runtime testing remain pending. Test downing/elimination while holding each action, delayed press packets after downing, recovery requiring fresh presses, menu transitions, and lean release under packet loss. This change covers character input handlers; it is not a complete audit of independently callable weapon/equipment/cover component RPCs or server movement validation.

### Separate aim sensitivity

Added a saved aim sensitivity multiplier (0.10–1.00x normal mouse sensitivity) to controls. Both yaw and pitch use it only while aiming; invert-Y remains applied. Default 1.00 preserves existing behavior. Mouse reset restores normal sensitivity, aim multiplier and invert-Y together; HUD/key resets preserve them. Values loaded from config are clamped and nonfinite values fall back to 1.00. Saves with existing controls close/end-play persistence. UE validation pending: horizontal/vertical aim, release back to normal speed, invert-Y, restart persistence and independent resets. This changes input scaling only, not camera FOV, weapon accuracy or optic zoom.

### Toggle aiming

Added saved toggle aiming (default off). First aim press enters aim, second exits; release exits only in hold mode. Menu and incapacitation cleanup call unconditional EndAim, so toggle cannot retain aim through control loss. Aim still cancels sprint and uses the saved aim-sensitivity multiplier. Mouse reset restores hold aiming; HUD/key resets preserve the preference. UE runtime checks pending: hold/toggle presses and releases, sprint conflict, menu open/close, downing/recovery, saved restart behavior and mouse reset.

### Preserve alternate bindings during swaps

Confirmed swaps now require exactly one unmodified keyboard/mouse binding per action. Multiple bindings and modifier chords are rejected before mutation, preserving both actions and saved overrides. Gamepad alternatives are excluded from this count and retained. Direct single-key reassignment remains an explicit replacement. The Unreal atomic-rebinding test now disables persistence for swaps and checks alternate keys on either action plus modifier-chord rejection. Engine test execution remains pending; source checks are not a substitute for Unreal automation.


## Keyboard movement editor

The movement section provides four labelled key selectors: forward, backward,
strafe left and strafe right. WASD and arrow-key presets fill a draft. **Apply
movement keys** validates and saves all four directions together; editing or
selecting a preset has no effect until Apply. **Discard movement draft** reloads
the live layout, which remains visible in the active-layout summary. Closing the
menu also discards the draft. Escape cancels key capture before closing the panel.

Applying replaces all keyboard movement alternatives with the four chosen keys,
using axis scales +1/-1. Mouse look, other axes and gamepad/analog movement mappings
are preserved. This does not implement gamepad navigation or gamepad remapping.
Only single keyboard keys are accepted: duplicates, mouse buttons, analog input,
modifier combinations, menu/console keys and conflicts with action/other-axis
bindings are rejected. Failed validation changes neither live maps nor saved keys.

Action and movement candidates are validated together before either map is
installed. This allows moving from WASD to arrows, then assigning the freed W key
to a combat action. Returning to WASD is rejected until that action moves elsewhere.
**Restore all original key bindings** resets both action and movement overrides
together without changing mouse/aim/HUD preferences. It also recovers malformed
saved layouts. Like action bindings, the original axis map is captured once per
process; restart the editor after changing project defaults.

### Validation matrix (engine execution pending)

Run `ShadowProtocol.Controls.AtomicRebinding` and
`ShadowProtocol.Controls.MovementRebinding` with the stock project input defaults.
Both tests restore action/axis runtime snapshots and disable config persistence.
The movement test covers signed directions, duplicate/reserved/occupied/invalid
keys, partial layouts, saved-layout reload, direction swaps, cross-conflicts,
full reset and preference isolation. Existing look/gamepad mappings are checked
when present in the fixture.

| Packaged-client check | Expected behavior |
| --- | --- |
| Fill arrow preset, then discard or close | Original movement remains active |
| Apply arrows, close, restart | Arrow keys move in the correct directions |
| Swap forward/back and left/right in one draft | All four switch on Apply |
| Enter duplicate or action-bound keys | Feedback explains rejection; active layout stays intact |
| Move to arrows, bind Reload to W, then apply WASD | Rejected until W is freed; Restore all remains available |
| Capture a key and press Escape | Capture cancels; a subsequent Escape closes controls |
| Apply while movement/aim was held before menu entry | No action remains held; new inputs work after closing |
| Use custom mouse axes or gamepad movement defaults | Those mappings survive keyboard layout changes |
| Use 720p, 1080p, ultrawide and keyboard navigation | All selectors, presets, feedback and reset remain reachable |

Node source contracts pass locally, but neither these engine automation tests nor
rendered/input behavior can be verified here without UE5.6.
