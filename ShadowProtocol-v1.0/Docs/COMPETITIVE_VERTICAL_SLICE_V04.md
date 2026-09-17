# Shadow Protocol — Competitive Vertical Slice v0.4

## Purpose
v0.4 turns the professional prototype into a clearer competitive **PROTOCOL** round. It keeps the intelligence-war loop at the center while making the match presentation and tactical decisions resemble the intended 5v5 product.

## Browser prototype additions
- Preparation phase before the round goes live.
- Directorate Nine attacker versus Helix defender score presentation.
- Action-phase banner and round state.
- Flash grenades that disrupt Helix AI and reward coordinated pushes.
- Smoke grenades that interrupt AI visual channels and create temporary screening.
- Squad leader orders: Follow, Hold, Assault.
- Bravo and Charlie allied AI support with visible squad states.
- Defender cover props to reinforce the concept of prepared defensive positions.
- Expanded Operation Review metrics for tactical-equipment usage and squad orders.
- Short post-death spectator handoff to a surviving Spectre teammate, validating the observer flow before full network spectator cameras.

## Unreal Engine additions
- Replicated `ESPRoundState` and expanded `ESPMatchPhase` values.
- Replicated round number, team round wins and match-complete state.
- Preparation → Action → PostRound → NextRound server-authoritative lifecycle.
- Best-of-9-ready default target through `RoundsToWin = 5`.
- `USPTacticalEquipmentComponent` for replicated Flash/Smoke inventory and deployment requests.
- Replicated `ESPSquadOrder` on the player character.
- Input mappings for equipment cycle, equipment throw and squad order.

## Backend additions
- `match_rounds` persistence for per-round result and objective state.
- `tactical_equipment_events` persistence for server-authored Flash/Smoke telemetry.
- `/v1/matches/rounds` trusted-server result endpoint.
- `/v1/matches/equipment-events` trusted-server telemetry endpoint.
- Backend health version advanced to 0.4.0.

## Competitive rules represented
Attackers win by securing the required intelligence and reaching extraction. Defenders win by eliminating the attack or allowing the timer to expire. Objective progression remains mandatory even if defenders are neutralized, preserving Shadow Protocol's principle that kills are not the sole win condition.

## Next production milestone
The next Unreal-focused pass should replace the browser abstractions with authored Embassy geometry, networked 5v5 player controllers, defender preparation interactions, barricade/deployable-cover actors, flash/smoke projectile actors with authoritative traces and area effects, full spectator/free-camera controls, round-side swapping, spawn groups, replay event capture, and dedicated-server integration tests.
