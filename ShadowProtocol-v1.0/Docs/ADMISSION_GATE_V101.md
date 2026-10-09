# Shadow Protocol v1.0.1 — Unreal Pending Admission Gate

## Goal

A network connection is not a gameplay admission. Shadow Protocol now keeps those states separate so a client cannot receive a competitive slot or pawn merely by reaching the dedicated server socket.

## Client handoff

After `OnAllocationCompleted`, the client may call:

```cpp
BackendSession->ConnectToAllocation(PlayerController, Allocation);
```

The bridge validates the backend allocation and performs absolute `ClientTravel` to the selected host/port with:

```text
spAllocationId
spMatchId
spConnectToken
spServerId
spNetworkBuild
```

No session-bootstrap, registration, attestation-signing or node credentials enter that URL.

## Server gate

`ASPProtocolGameMode` uses Unreal's normal connection lifecycle:

1. **PreLogin** — syntax/target/build/registry checks. Rejection here prevents login.
2. **InitNewPlayer** — create an allocation-keyed pending record with a server-side timeout.
3. **HandleStartingNewPlayer** — return without spawning while the controller is pending.
4. **PostLogin** — call `USPDedicatedServerBackendSubsystem::AdmitConnection`.
5. Clear the plaintext connect token from GameMode pending state once the request has started.
6. **Admission success** — verify correlation, trust backend `userId`, assign team and competitive slot, then call the base `HandleStartingNewPlayer_Implementation` to spawn normally.
7. **Admission failure/timeout** — kick the controller.
8. **Pending disconnect** — delete the pending record and do not create a reconnect reservation.

## Concurrency

Pending records are keyed by `allocationId`. Dedicated-server admission callbacks include allocation and match ids, so multiple simultaneous players can be admitted independently without relying on one global "last request" state.

## Security invariants

- A pending controller is unauthenticated.
- A pending controller has no competitive team.
- A pending controller is excluded from competitive slots.
- A pending controller does not spawn a pawn.
- Wrong/replayed/expired connect tokens do not become sessions.
- Backend response must match allocation, match, server id and network build.
- The short-lived connect token is not persisted to PlayerState, SaveGame, config or logs.
- Server infrastructure/node credentials never enter the client travel path.

## Validation

`npm run test:unreal-contract` checks the source contract in CI. It protects against accidental removal of the key integration points but is intentionally not described as a C++ compiler.

Full validation still requires UE5.6/UHT plus packaged client/dedicated-server runtime testing.
