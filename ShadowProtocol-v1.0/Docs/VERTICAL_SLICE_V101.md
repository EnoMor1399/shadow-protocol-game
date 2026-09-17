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

Ranked/dev compatibility is server-owned and exact-match by default. The backend can expose a controlled compatibility window only through the explicit `ACCEPTED_NETWORK_BUILDS` configuration; the client never decides compatibility on its own.

### Session presentation

The browser simulator distinguishes three player-facing states:

1. **Verified** — required client anchors exist and the simulated network/auth states are healthy.
2. **Reconnect window** — a reserved competitive slot is being restored.
3. **Degraded** — a critical interface anchor is missing or the session presentation is not healthy.

These remain presentation/diagnostic states. They do not claim a real dedicated-server connection in the browser simulator.

### Backend protocol 0.8.0

The Fastify backend now treats `SP-1.0.1` as an enforceable network contract.

Authenticated session creation validates the submitted build before issuing a signed session. The signed token carries both the network build and backend protocol. Match allocation validates both fields again before reserving a server, preventing a stale session from crossing a protocol rollout boundary.

Compatibility metadata is exposed through:

- `GET /health`
- `GET /v1/compatibility`
- `POST /v1/auth/game-session`
- `POST /v1/matches/allocate`
- reconnect ticket/reconnect responses
- WebSocket `/v1/live` hello payloads

Unsupported clients receive HTTP `426` with `client-build-incompatible`, the received build, the active release/network/content identifiers, backend protocol, and the accepted build set.

The default accepted build set is `SP-1.0.1`. Operations can explicitly provide a comma-separated `ACCEPTED_NETWORK_BUILDS` value for a controlled patch transition. This is a server policy only; the shipped client still advertises its immutable `SP-1.0.1` identity.

## Next production targets after v1.0.1

1. Compile the Unreal module and consume `USPBuildInfoLibrary` from the UMG ready-room and online session layer.
2. Connect the UE client to `/v1/compatibility`, authenticated session bootstrap and allocation failure handling.
3. Convert browser diagnostics into production UMG network failure/reconnect flows.
4. Add automated backend tests for accepted/rejected build IDs, expired protocol sessions and reconnect compatibility.
5. Run 10-client dedicated-server soak tests with reconnect and round-transition cases.
6. Author final Embassy geometry, materials, animation, audio and destruction profiles.
7. Move to Alpha content only after vertical-slice networking and failure paths are validated.
