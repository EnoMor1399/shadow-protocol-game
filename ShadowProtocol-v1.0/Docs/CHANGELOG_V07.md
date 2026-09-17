# v0.7 Change Log

## Competitive/network
- Added authenticated-session status and network telemetry to the playable build.
- Added 90-second reconnect-grace UI and session reservation behavior.
- Fixed side/spawn lookup so round rotation cannot reuse stale previous-round side state.
- Added five-player friendly team simulation and live team-status strip.
- Added Unreal authenticated ready gating and session-based slot reclaim.
- Added reconnect reservation expiry and safer spawn validation.

## Combat
- Added shot-age validation in prototype and Unreal server code.
- Added headshots, headshot feed marker and review telemetry.
- Added damage-contribution assists.
- Added friendly-fire penalties and reverse-friendly-fire escalation.
- Added Unreal replicated K/D/A/headshot/team-damage statistics.
- Added lag-compensation location history and a rewound capsule fallback hook.

## Objective gameplay
- Added three possible Embassy intelligence sites.
- Objective site rotates by round in the playable build and authoritative Unreal game state.
- Round-result backend model now stores objective-site code.

## Backend/security
- Added signed short-lived game sessions.
- Added dedicated-server allocation contract and persistent allocation records.
- Reworked reconnect flow so clients receive a random ticket while only its hash is stored.
- Added authenticated slot ownership check on reconnect.
- Added trusted-server combat-event telemetry.


## v0.7.1 interface continuation
- Rebuilt the visual hierarchy across Operations, Role Assignment, Ready Room, Planning, Live HUD, Tactical Device and Operation Review.
- Added a consistent command-console surface system with restrained glass, technical registration marks and classified-status accents.
- Improved ready-room roster density, spawn-selection feedback and secure-session presentation.
- Improved HUD separation between round state, compass, team status, objectives and combat telemetry.
- Improved weapon/ammunition, health/endurance, kill-feed and interaction-prompt readability.
- Preserved the design-document rule that red/glow effects remain controlled rather than filling every screen.
