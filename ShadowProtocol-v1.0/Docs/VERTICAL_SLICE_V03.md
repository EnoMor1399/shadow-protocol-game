# SHADOW PROTOCOL — PROFESSIONAL VERTICAL SLICE PASS v0.3

This pass moves the prototype from presentation polish toward systems that support the EMBASSY / PROTOCOL vertical slice.

## Implemented in the playable browser build

- Aim-down-sights on right mouse button with reduced weapon spread and an ADS weapon position.
- Sprint on Shift with a regenerating endurance meter and a larger acoustic signature.
- Rotating surveillance cameras with cumulative exposure rather than one-frame detection.
- Four security states: Unaware, Suspicious, Searching, Lockdown.
- Reinforcement/QRF waves when alert escalation reaches higher levels.
- Expanded Helix AI states: patrol, guard, investigate, search, engage and retreat.
- AI sound investigation for gunfire, breaching, sprinting and ordinary footsteps.
- Basic lateral pressure/flanking behavior during engagement.
- Wounded Helix personnel can break contact and retreat.
- Mission-event timeline and enhanced Operation Review telemetry.
- Environmental props including field crates, server racks, desks and practical lighting markers.
- Revised deployment splash and build identity.
- Professional HUD additions for alert level and endurance.

## Unreal Engine 5 foundation added in v0.3

- `ASPAlertDirector`: replicated exposure, alert level, lockdown and reinforcement-wave authority.
- `ESPAwarenessState`: common tactical AI awareness-state enum.
- Replicated aiming, sprinting and stamina fields on `ASPCharacter`.
- Server RPCs for aim and sprint state.
- Weapon dispersion now accounts for ADS and sprint state.
- Right Mouse Button input binding for Aim.

## Backend/database additions

- Backend service version moved to 0.3.0.
- `match_events` table records authoritative tactical events for Operation Review/replay indexing.
- `/v1/matches/events` server-telemetry endpoint scaffold added. In production it must only accept authenticated dedicated-server traffic.

## Vertical-slice next targets

1. Replace placeholder ray-cast world art with the UE5 Embassy environment.
2. Add full defender team logic and networked 5v5 round flow.
3. Implement spatial audio, material-aware footsteps and suppression audio.
4. Add breach charges, destructible-door fragments and selected penetrable surfaces in Chaos.
5. Implement AI Perception + Behavior Trees using the awareness model established here.
6. Build UMG equivalents of the planning screen, HUD, tactical device and Operation Review.
7. Add dedicated-server match telemetry and replay event capture to the backend.
