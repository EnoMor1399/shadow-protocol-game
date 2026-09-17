# Shadow Protocol v1.0 Changelog

## Professional interface
- Added v1.0 classified-command interface layer.
- Added settings/keybind screen.
- Added persistent HUD density, contrast, reduced-motion, and telemetry preferences.
- Added functional enhanced-contrast and reduced-motion presentation modes.
- Added F1 HUD-density shortcut and Escape settings access.
- Added objective-site confirmation ribbon.
- Added contextual cover/peek indicator.
- Added staged deployment briefing.
- Updated build identity to v1.0 / Embassy Vertical Slice.

## Tactical interaction
- Preserved v0.9 partial-door, leaning, vaulting, optic, weapon-inspection, camera/light destruction, suppression, and vertical-route mechanics.
- Added production-side `USPCoverSystemComponent`.
- `ASPCharacter` now creates the cover component and performs an authority-side cover probe when lean state changes.

## Objective presentation
- Enhanced the existing authoritative `ASPObjectiveSite` rather than introducing a second site class.
- Added replicated site identity, display name, Embassy zone, active-site notification, and Blueprint presentation event.
- Existing `ASPProtocolGameMode::SelectObjectiveSiteForRound()` remains responsible for authoritative site rotation.

## Deployment
- Added replicated `ASPDeploymentDirector` for Authentication → Loadout Check → Insertion → Live sequencing.
- Spawn group validation is limited to ALPHA / BRAVO / CHARLIE.

## Versioning
- `ShadowProtocol-v0.9/` remains the preserved prior snapshot.
- v1.0 development lives under `ShadowProtocol-v1.0/`.

## Validation boundary
The browser v1.0 layer is intended to remain compatible with the existing v0.9 simulation. Unreal additions still require UHT/compiler, PIE, packaged-client, dedicated-server, and multiplayer validation in a full Unreal Engine environment.
