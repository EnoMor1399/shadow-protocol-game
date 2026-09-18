# Dedicated-server OnlineSubsystem integration

## Implemented at source level

`ASPProtocolGameMode` now owns `ASPOnlineGameSession`, a concrete `AGameSession`
wrapper around the world-scoped `IOnlineSession` interface. The game uses
`AGameModeBase` and custom round states, so it explicitly starts the OSS session
before preparation and ends it on match completion. Rounds share one session.

- Dedicated startup creates `NAME_GameSession` with at most ten connections.
- Asynchronous create/start/end callbacks are correlated by name and operation.
- Each operation has a 15-second monotonic timeout; failure blocks new admissions
  and requests backend drain. There is no automatic retry or adoption of an
  existing session belonging to another owner.
- The provider must report a pending, starting or active session for admission.
- Preparation waits for provider start success. NULL can complete synchronously;
  asynchronous providers can complete on a later tick.
- The session is private: no advertising, presence, invites or lobby discovery.
  Only non-secret build metadata is added to settings.
- Delegate handles are removed on completion, failure and teardown. Teardown
  requests destruction only for the session this actor created. Destruction is
  best effort on process/world shutdown; asynchronous completion is not proven.
- The existing allocation-based absolute IP ClientTravel and backend redemption
  remain the route into the game. An OSS session or platform id is not evidence
  of authenticated backend identity.
- Admission promotion rechecks monotonic expiry, backend registration/drain and
  OSS availability. A failed kick falls back to controller destruction. The
  legacy manual session-authorisation helper cannot authenticate dedicated users.
- The native pawn default is `ASPCharacter`, and the project explicitly enables
  `OnlineSubsystemNull`, matching `DefaultPlatformService=NULL`.

`ShadowProtocolServer.Target.cs` adds the missing dedicated-server build target.

## UE 5.6 validation still required

This patch has source-contract coverage, not a successful UHT/Unreal build or
packaged multiplayer test. No Unreal executable is available in the editing
container. Using a UE 5.6 source build with the platform toolchain installed:

```powershell
$UnrealRoot = 'C:\UnrealEngine-5.6'
$Project = (Resolve-Path '.\ShadowProtocol-v1.0\ShadowProtocol.uproject').Path
& "$UnrealRoot\Engine\Build\BatchFiles\Build.bat" ShadowProtocolEditor Win64 Development "-Project=$Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw 'Editor/UHT build failed' }
& "$UnrealRoot\Engine\Build\BatchFiles\Build.bat" ShadowProtocolServer Win64 Development "-Project=$Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw 'Dedicated server build failed' }
& "$UnrealRoot\Engine\Build\BatchFiles\Build.bat" ShadowProtocol Win64 Development "-Project=$Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw 'Game build failed' }
```

Cooking/packaging also requires the Embassy map and referenced game content.
Configure backend node enrollment as documented in `DEDICATED_SERVER_BACKEND_V101.md`.
Validate with packaged clients and a server, not only single-process PIE:

1. Confirm NULL session creation and ten-player capacity before allocation travel.
2. Verify successful, denied, expired and replayed admission tokens; no denied
   controller should gain a pawn, competitive slot or authenticated identity.
3. Delay redemption past twelve seconds, pause/time-dilate the world, and confirm
   the monotonic promotion check still rejects expired admissions.
4. With all ten players ready, confirm asynchronous start completes before the
   planning-to-preparation transition; subsequent rounds reuse the same session.
5. Complete a match, confirm end succeeds, and confirm no new admissions enter it.
6. Force missing provider, immediate operation failure, missing callbacks and
   provider session loss. Verify admission closes and backend drain is requested.
7. Test shutdown during create/start/end, restart and independent PIE worlds for
   stale handles and session-name ownership collisions.

## Remaining integration boundaries

Steam/EOS account verification, trusted backend session bootstrap, provider-specific
client `JoinSession`/discovery and UMG ready/reconnect wiring are not implemented by
this patch. Current sessions are NULL/IP, allocated by the backend. Complete the
UHT, packaged travel and real 10-client soak gates before claiming runtime sign-off.

Reference: [Epic's Session Interface](https://dev.epicgames.com/documentation/en-us/unreal-engine/online-subsystem-session-interface-in-unreal-engine).
