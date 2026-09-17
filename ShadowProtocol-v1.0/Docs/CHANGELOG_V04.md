# Shadow Protocol v0.4 — Competitive Vertical Slice

## Gameplay
- Added Preparation → Action → Round Complete structure.
- Added Directorate Nine attacker / Helix defender round presentation.
- Added Flash grenades with defender disruption.
- Added Smoke grenades with temporary visual-channel blocking.
- Added Follow / Hold / Assault squad-leader orders.
- Added Bravo and Charlie allied combat support.
- Added Helix defensive cover props.
- Added short post-death spectator handoff.
- Expanded Operation Review telemetry.

## Presentation
- Added competitive round header and team score state.
- Added action/preparation phase banners.
- Added tactical-equipment HUD and key prompts.
- Refined compass placement to avoid HUD collision.
- Added spectator identification overlay.

## Reliability
- Fixed the inherited startup temporal-dead-zone error caused by initializing the `once` event cache after the initial `resetGame()` call.
- Reset smoke/flash/phase effects correctly when returning to Operations.
- Verified every JavaScript `#id` reference exists in the HTML and that IDs are unique.
- Passed JavaScript syntax validation, DOM boot smoke testing, and a simulated runtime flow test covering Preparation → Action, equipment cycling/use, and squad orders.

## Unreal foundation
- Added `ESPRoundState`, tactical-equipment and squad-order enums.
- Expanded replicated Protocol game state with round number, team round wins and match completion.
- Added server-authoritative Preparation/Action/PostRound lifecycle and round-to-win handling.
- Added `USPTacticalEquipmentComponent` with replicated Flash/Smoke counts and server deployment requests.
- Added replicated squad orders to `ASPCharacter` and new input mappings.

## Backend
- Backend version advanced to 0.4.0.
- Added `match_rounds` persistence.
- Added `tactical_equipment_events` persistence.
- Added trusted-server round-result and equipment-event endpoints.
