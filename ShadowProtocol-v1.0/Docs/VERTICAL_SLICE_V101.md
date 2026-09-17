# SHADOW PROTOCOL — Embassy Vertical Slice v1.0.1

## Release intent

v1.0.1 is the first post-v1.0 hardening pass. The goal is to stabilize the interface/session boundary before larger Alpha content work begins.

The v1.0 gameplay loop remains:

**Observe → Plan → Infiltrate → Execute → Secure → Extract**

## Patch architecture

### Browser client

`PrototypeWeb/v1.js` remains the additive production-UX controller above the existing gameplay simulator. v1.0.1 adds safe settings persistence, deterministic deployment transition cleanup, objective-notification de-duplication, session/build diagnostics and page-lifecycle cleanup.

`PrototypeWeb/v101.css` contains patch-only presentation rules. Keeping the patch rules separate avoids reworking the large v1.0 stylesheet and makes the v1.0.1 delta easy to audit or revert.

### Build identity

The browser presents two identifiers:

- Product release: `1.0.1`
- Network build: `SP-1.0.1`

The Unreal `USPBuildInfoLibrary` exposes the same identifiers plus the content revision `EMBASSY-PROTOCOL-101`.

For ranked integration, build compatibility is exact-match in v1.0.1. A future backend policy may define compatible patch windows, but the client should never assume compatibility on its own.

### Session presentation

The browser simulator now distinguishes three player-facing states:

1. **Verified** — required client anchors exist and the simulated network/auth states are healthy.
2. **Reconnect window** — a reserved competitive slot is being restored.
3. **Degraded** — a critical interface anchor is missing or the session presentation is not healthy.

These are presentation/diagnostic states. They do not claim a real dedicated-server connection in the browser simulator.

### Backend protocol boundary

The existing Fastify backend remains on its 0.7.0 service/protocol generation in this patch. v1.0.1 deliberately does not overwrite that identifier without wiring build compatibility into authenticated game-session and allocation flows. The next backend pass should make `SP-1.0.1` a server-enforced compatibility policy rather than a presentation-only string.

## Next production targets after v1.0.1

1. Compile the Unreal module and consume `USPBuildInfoLibrary` from the UMG ready-room and session layer.
2. Add server-owned build compatibility enforcement to authenticated session allocation.
3. Convert browser diagnostics into UMG/network failure flows.
4. Run 10-client dedicated-server soak tests with reconnect and round-transition cases.
5. Author final Embassy geometry, materials, animation, audio and destruction profiles.
6. Move to Alpha content only after the vertical-slice networking and failure paths are validated.
