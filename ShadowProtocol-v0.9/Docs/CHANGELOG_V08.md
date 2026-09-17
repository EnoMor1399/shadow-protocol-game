# v0.8 Changelog — Advanced Embassy & Production Combat

## Presentation
- Added Embassy room/sector identification to top status, HUD and tactical device.
- Added material/surface identification for concrete, marble, metal, carpet and gravel.
- Added material-aware procedural footsteps.
- Added suppression vignette and lockdown lighting treatment.
- Added muzzle light pulse, casing particles and muzzle spark feedback.
- Added breach dust/spark particles and environmental shock pulse.
- Added low-ready sprint and animated reload weapon posture.
- Added richer Operation Review summary cards and staged entrance animation.

## Gameplay / UX
- Existing movement noise now shares the same surface-aware step cadence as audio feedback.
- Current sector and surface update live as the player crosses the Embassy.
- Weapon state now reports READY, ADS, LOW READY, RELOADING or SUPPRESSED.
- Existing intelligence, surveillance, alert, round, overtime and 5v5-ready-room systems remain intact.

## Unreal / backend
- Added `USPCombatFeedbackComponent` replicated presentation event bridge.
- Added `ASPEnvironmentZone` and `ESPSurfaceProfile` for authorable Embassy sectors.
- Weapon fire now emits accepted server-side presentation multicast after validation/ammo checks.
- Near-miss suppression emits replicated feedback without moving damage authority to clients.
- Added `environment_events` database telemetry and `/v1/matches/environment-events` backend endpoint.

## Validation
- Browser JS syntax checked with Node.
- DOM IDs checked for duplicates.
- Inlined Chromium/Playwright smoke flow reached Ready Room, Planning, Deployment and Action without page errors.
- New location/surface HUD state verified in Action.
- Unreal C++ is structurally checked only in this environment; full UE compilation remains required.
