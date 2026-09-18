# Client session expiry and response ownership

## Implemented

`USPBackendSessionSubsystem` validates a future ISO-8601 expiry before installing
or rotating a session. `HasAuthenticatedSession()` now checks remaining lifetime,
using the earlier of UTC expiry and a monotonic deadline captured at installation.
A backwards system-clock change cannot extend that installed lifetime. This is
client-side fail-closed behavior; the backend remains the authority on validity.

A core ticker clears expired credentials and broadcasts `OnSessionExpired` once.
It is independent of world pause/time dilation and removed on deinitialization.
Blueprint UI can query `GetSessionSecondsRemaining()` and `HasExpiredSession()`.
The native ready-room panel displays an expiry notice. This does not revoke an
already admitted server player or force a client out of an active match.

Clearing/replacing a session cancels tracked requests and removes their callbacks.
Every authenticated callback must still belong to the active request list, carry
the current bearer token, and arrive within the current lifetime. Delayed refresh,
allocation and reconnect responses cannot restore a cleared session or deliver
results to a replacement session. Compatibility requests are separately tracked
so old responses cannot validate a changed backend URL. Changing the backend URL
also clears authentication and compatibility state.

Refresh is single-flight and may only start after active authenticated requests
finish. While it runs, new match requests fail with a retry message. Authenticated
HTTP requests have a 15-second timeout. Callers must explicitly retry after
`OnSessionRefreshed` or handle `OnRequestFailed`; there is no automatic retry,
refresh scheduling or identity bootstrap in this patch.

HTTP 401 invalidates the current session. HTTP 403 invalidates it only for refresh;
a forbidden match/reconnect operation does not itself imply expired credentials.
An invalid refresh lifetime fails closed. Explicit sign-out clears expiry status;
a successfully installed replacement session clears the notice.

Ticker handles now use `FTSTicker::FDelegateHandle`, including the existing
server heartbeat/credential-rotation handles. These are different from the generic
multicast-delegate handle type. See [Epic's FTSTicker API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/FTSTicker).

## Validation still required

Twelve source-contract checks run locally and in CI, but no UE runtime is installed
in the editing environment. Compile with UE 5.6/UHT, then test:

- Missing/malformed/past expiry must never install credentials.
- Expiry during world pause, forward/backward clock adjustments and map travel.
- Sign out or install another identity while refresh/allocation/reconnect is pending;
  delayed success and unauthorized responses must not alter the replacement session.
- Concurrent refresh attempts, blocked operations during refresh and retry after completion.
- HTTP 401 versus match-ownership 403; verify only the intended case expires the session.
- Backend URL change while compatibility is pending; no old-origin compatibility acceptance.
- Shutdown while ticker/HTTP callbacks are pending; no late UI callbacks or restored credentials.
- A live admitted player can finish the match when client backend credentials expire;
  matchmaking/reconnect stays blocked until trusted sign-in installs a fresh session.

Packaged reconnect transport, trusted platform sign-in, automatic refresh policy,
and a persistent in-match expiry screen remain separate pending work.
